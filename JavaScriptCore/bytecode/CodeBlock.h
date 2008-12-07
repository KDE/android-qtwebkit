/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CodeBlock_h
#define CodeBlock_h

#include "EvalCodeCache.h"
#include "Instruction.h"
#include "JSGlobalObject.h"
#include "JumpTable.h"
#include "Nodes.h"
#include "RegExp.h"
#include "UString.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace JSC {

    class ExecState;

    enum CodeType { GlobalCode, EvalCode, FunctionCode };

    static ALWAYS_INLINE int missingThisObjectMarker() { return std::numeric_limits<int>::max(); }

    struct HandlerInfo {
        uint32_t start;
        uint32_t end;
        uint32_t target;
        uint32_t scopeDepth;
        void* nativeCode;
    };

    struct ExpressionRangeInfo {
        enum {
            MaxOffset = (1 << 7) - 1, 
            MaxDivot = (1 << 25) - 1
        };
        uint32_t instructionOffset : 25;
        uint32_t divotPoint : 25;
        uint32_t startOffset : 7;
        uint32_t endOffset : 7;
    };

    struct LineInfo {
        uint32_t instructionOffset;
        int32_t lineNumber;
    };

    struct StructureStubInfo {
        StructureStubInfo(unsigned bytecodeIndex)
            : bytecodeIndex(bytecodeIndex)
            , stubRoutine(0)
            , callReturnLocation(0)
            , hotPathBegin(0)
        {
        }
    
        unsigned bytecodeIndex;
        void* stubRoutine;
        void* callReturnLocation;
        void* hotPathBegin;
    };

    struct CallLinkInfo {
        CallLinkInfo()
            : callReturnLocation(0)
            , hotPathBegin(0)
            , hotPathOther(0)
            , coldPathOther(0)
            , callee(0)
        {
        }
    
        unsigned bytecodeIndex;
        void* callReturnLocation;
        void* hotPathBegin;
        void* hotPathOther;
        void* coldPathOther;
        CodeBlock* callee;
        unsigned position;
        
        void setUnlinked() { callee = 0; }
        bool isLinked() { return callee; }
    };

    inline void* getStructureStubInfoReturnLocation(StructureStubInfo* structureStubInfo)
    {
        return structureStubInfo->callReturnLocation;
    }

    inline void* getCallLinkInfoReturnLocation(CallLinkInfo* callLinkInfo)
    {
        return callLinkInfo->callReturnLocation;
    }

    // Binary chop algorithm, calls valueAtPosition on pre-sorted elements in array,
    // compares result with key (KeyTypes should be comparable with '--', '<', '>').
    // Optimized for cases where the array contains the key, checked by assertions.
    template<typename ArrayType, typename KeyType, KeyType(*valueAtPosition)(ArrayType*)>
    inline ArrayType* binaryChop(ArrayType* array, size_t size, KeyType key)
    {
        // The array must contain at least one element (pre-condition, array does conatin key).
        // If the array only contains one element, no need to do the comparison.
        while (size > 1) {
            // Pick an element to check, half way through the array, and read the value.
            int pos = (size - 1) >> 1;
            KeyType val = valueAtPosition(&array[pos]);
            
            // If the key matches, success!
            if (val == key)
                return &array[pos];
            // The item we are looking for is smaller than the item being check; reduce the value of 'size',
            // chopping off the right hand half of the array.
            else if (key < val)
                size = pos;
            // Discard all values in the left hand half of the array, up to and including the item at pos.
            else {
                size -= (pos + 1);
                array += (pos + 1);
            }

            // 'size' should never reach zero.
            ASSERT(size);
        }
        
        // If we reach this point we've chopped down to one element, no need to check it matches
        ASSERT(size == 1);
        ASSERT(key == valueAtPosition(&array[0]));
        return &array[0];
    }

    class CodeBlock {
        friend class JIT;
    public:
        CodeBlock(ScopeNode* ownerNode, CodeType, PassRefPtr<SourceProvider>, unsigned sourceOffset);
        ~CodeBlock();

#if ENABLE(JIT) 
        void unlinkCallers();
#endif

        void addCaller(CallLinkInfo* caller)
        {
            caller->callee = this;
            caller->position = m_linkedCallerList.size();
            m_linkedCallerList.append(caller);
        }

        void removeCaller(CallLinkInfo* caller)
        {
            unsigned pos = caller->position;
            unsigned lastPos = m_linkedCallerList.size() - 1;

            if (pos != lastPos) {
                m_linkedCallerList[pos] = m_linkedCallerList[lastPos];
                m_linkedCallerList[pos]->position = pos;
            }
            m_linkedCallerList.shrink(lastPos);
        }

        inline bool isKnownNotImmediate(int index)
        {
            if (index == m_thisRegister)
                return true;

            if (isConstantRegisterIndex(index))
                return !JSImmediate::isImmediate(getConstant(index));

            return false;
        }

        ALWAYS_INLINE bool isConstantRegisterIndex(int index)
        {
            return index >= m_numVars && index < m_numVars + m_numConstants;
        }

        ALWAYS_INLINE JSValue* getConstant(int index)
        {
            return m_constantRegisters[index - m_numVars].getJSValue();
        }

        ALWAYS_INLINE bool isTemporaryRegisterIndex(int index)
        {
            return index >= m_numVars + m_numConstants;
        }

#if !defined(NDEBUG) || ENABLE_OPCODE_SAMPLING
        void dump(ExecState*) const;
        void printStructures(const Instruction*) const;
        void printStructure(const char* name, const Instruction*, int operand) const;
#endif
        int expressionRangeForVPC(const Instruction*, int& divot, int& startOffset, int& endOffset);
        int lineNumberForVPC(const Instruction* vPC);
        bool getHandlerForVPC(const Instruction* vPC, Instruction*& target, int& scopeDepth);
        void* nativeExceptionCodeForHandlerVPC(const Instruction* handlerVPC);

        void mark();
        void refStructures(Instruction* vPC) const;
        void derefStructures(Instruction* vPC) const;

        StructureStubInfo& getStubInfo(void* returnAddress)
        {
            return *(binaryChop<StructureStubInfo, void*, getStructureStubInfoReturnLocation>(m_propertyAccessInstructions.begin(), m_propertyAccessInstructions.size(), returnAddress));
        }

        CallLinkInfo& getCallLinkInfo(void* returnAddress)
        {
            return *(binaryChop<CallLinkInfo, void*, getCallLinkInfoReturnLocation>(m_callLinkInfos.begin(), m_callLinkInfos.size(), returnAddress));
        }


        Vector<Instruction>& instructions() { return m_instructions; }
#if ENABLE(JIT)
        void setJITCode(void* jitCode) { m_jitCode = jitCode; }
        void* jitCode() { return m_jitCode; }
        ExecutablePool* executablePool() { return m_executablePool.get(); }
        void setExecutablePool(ExecutablePool* pool) { m_executablePool = pool; }
#endif

        ScopeNode* ownerNode() const { return m_ownerNode; }

        void setGlobalData(JSGlobalData* globalData) { m_globalData = globalData; }

        void setThisRegister(int thisRegister) { m_thisRegister = thisRegister; }
        int thisRegister() const { return m_thisRegister; }

        void setNeedsFullScopeChain(bool needsFullScopeChain) { m_needsFullScopeChain = needsFullScopeChain; }
        bool needsFullScopeChain() const { return m_needsFullScopeChain; }
        void setUsesEval(bool usesEval) { m_usesEval = usesEval; }
        bool usesEval() const { return m_usesEval; }
        void setUsesArguments(bool usesArguments) { m_usesArguments = usesArguments; }
        bool usesArguments() const { return m_usesArguments; }

        CodeType codeType() const { return m_codeType; }

        SourceProvider* source() const { return m_source.get(); }
        unsigned sourceOffset() const { return m_sourceOffset; }

        void addGlobalResolveInstruction(unsigned globalResolveInstructions) { m_globalResolveInstructions.append(globalResolveInstructions); }

        size_t numberOfPropertyAccessInstructions() const { return m_propertyAccessInstructions.size(); }
        void addPropertyAccessInstruction(unsigned propertyAccessInstructions) { m_propertyAccessInstructions.append(StructureStubInfo(propertyAccessInstructions)); }
        StructureStubInfo& propertyAccessInstruction(int index) { return m_propertyAccessInstructions[index]; }

        size_t numberOfCallLinkInfos() const { return m_callLinkInfos.size(); }
        void addCallLinkInfo() { m_callLinkInfos.append(CallLinkInfo()); }
        CallLinkInfo& callLinkInfo(int index) { return m_callLinkInfos[index]; }

        size_t numberOfJumpTargets() const { return m_jumpTargets.size(); }
        void addJumpTarget(unsigned jumpTarget) { m_jumpTargets.append(jumpTarget); }
        unsigned jumpTarget(int index) const { return m_jumpTargets[index]; }
        unsigned lastJumpTarget() const { return m_jumpTargets.last(); }

        size_t numberOfExceptionHandlers() const { return m_exceptionHandlers.size(); }
        void addExceptionHandler(const HandlerInfo& hanler) { return m_exceptionHandlers.append(hanler); }
        HandlerInfo& exceptionHandler(int index) { return m_exceptionHandlers[index]; }

        void addExpressionInfo(const ExpressionRangeInfo& expressionInfo) { return m_expressionInfo.append(expressionInfo); }

        size_t numberOfLineInfos() const { return m_lineInfo.size(); }
        void addLineInfo(const LineInfo& lineInfo) { return m_lineInfo.append(lineInfo); }
        LineInfo& lastLineInfo() { return m_lineInfo.last(); }

#if ENABLE(JIT)
        HashMap<void*, unsigned>& jitReturnAddressVPCMap() { return m_jitReturnAddressVPCMap; }
#endif

        // Constant Pool

        size_t numberOfIdentifiers() const { return m_identifiers.size(); }
        void addIdentifier(const Identifier& i) { return m_identifiers.append(i); }
        Identifier& identifier(int index) { return m_identifiers[index]; }

        size_t numberOfConstantRegisters() const { return m_constantRegisters.size(); }
        void addConstantRegister(const Register& r) { return m_constantRegisters.append(r); }
        Register& constantRegister(int index) { return m_constantRegisters[index]; }

        unsigned addFunction(FuncDeclNode* n) { unsigned size = m_functions.size(); m_functions.append(n); return size; }
        FuncDeclNode* function(int index) const { return m_functions[index].get(); }

        unsigned addFunctionExpression(FuncExprNode* n) { unsigned size = m_functionExpressions.size(); m_functionExpressions.append(n); return size; }
        FuncExprNode* functionExpression(int index) const { return m_functionExpressions[index].get(); }

        unsigned addUnexpectedConstant(JSValue* v) { unsigned size = m_unexpectedConstants.size(); m_unexpectedConstants.append(v); return size; }
        JSValue* unexpectedConstant(int index) const { return m_unexpectedConstants[index]; }

        unsigned addRegExp(RegExp* r) { unsigned size = m_regexps.size(); m_regexps.append(r); return size; }
        RegExp* regexp(int index) const { return m_regexps[index].get(); }

        // Jump Tables

        size_t numberOfImmediateSwitchJumpTables() const { return m_immediateSwitchJumpTables.size(); }
        SimpleJumpTable& addImmediateSwitchJumpTable() { m_immediateSwitchJumpTables.append(SimpleJumpTable()); return m_immediateSwitchJumpTables.last(); }
        SimpleJumpTable& immediateSwitchJumpTable(int tableIndex) { return m_immediateSwitchJumpTables[tableIndex]; }

        size_t numberOfCharacterSwitchJumpTables() const { return m_characterSwitchJumpTables.size(); }
        SimpleJumpTable& addCharacterSwitchJumpTable() { m_characterSwitchJumpTables.append(SimpleJumpTable()); return m_characterSwitchJumpTables.last(); }
        SimpleJumpTable& characterSwitchJumpTable(int tableIndex) { return m_characterSwitchJumpTables[tableIndex]; }

        size_t numberOfStringSwitchJumpTables() const { return m_stringSwitchJumpTables.size(); }
        StringJumpTable& addStringSwitchJumpTable() { m_stringSwitchJumpTables.append(StringJumpTable()); return m_stringSwitchJumpTables.last(); }
        StringJumpTable& stringSwitchJumpTable(int tableIndex) { return m_stringSwitchJumpTables[tableIndex]; }


        SymbolTable& symbolTable() { return m_symbolTable; }
        EvalCodeCache& evalCodeCache() { return m_evalCodeCache; }

        void shrinkToFit();

        // FIXME: Make these remaining members private.

        int m_numCalleeRegisters;
        // NOTE: numConstants holds the number of constant registers allocated
        // by the code generator, not the number of constant registers used.
        // (Duplicate constants are uniqued during code generation, and spare
        // constant registers may be allocated.)
        int m_numConstants;
        int m_numVars;
        int m_numParameters;

    private:
#if !defined(NDEBUG) || ENABLE(OPCODE_SAMPLING)
        void dump(ExecState*, const Vector<Instruction>::const_iterator& begin, Vector<Instruction>::const_iterator&) const;
#endif

        ScopeNode* m_ownerNode;
        JSGlobalData* m_globalData;

        Vector<Instruction> m_instructions;
#if ENABLE(JIT)
        void* m_jitCode;
        RefPtr<ExecutablePool> m_executablePool;
#endif

        int m_thisRegister;

        bool m_needsFullScopeChain;
        bool m_usesEval;
        bool m_usesArguments;

        CodeType m_codeType;

        RefPtr<SourceProvider> m_source;
        unsigned m_sourceOffset;

        Vector<unsigned> m_globalResolveInstructions;
        Vector<StructureStubInfo> m_propertyAccessInstructions;
        Vector<CallLinkInfo> m_callLinkInfos;
        Vector<CallLinkInfo*> m_linkedCallerList;

        Vector<unsigned> m_jumpTargets;

        Vector<HandlerInfo> m_exceptionHandlers;
        Vector<ExpressionRangeInfo> m_expressionInfo;
        Vector<LineInfo> m_lineInfo;

#if ENABLE(JIT)
        HashMap<void*, unsigned> m_jitReturnAddressVPCMap;
#endif

        // Constant Pool
        Vector<Identifier> m_identifiers;
        Vector<Register> m_constantRegisters;
        Vector<RefPtr<FuncDeclNode> > m_functions;
        Vector<RefPtr<FuncExprNode> > m_functionExpressions;
        Vector<JSValue*> m_unexpectedConstants;
        Vector<RefPtr<RegExp> > m_regexps;

        // Jump Tables
        Vector<SimpleJumpTable> m_immediateSwitchJumpTables;
        Vector<SimpleJumpTable> m_characterSwitchJumpTables;
        Vector<StringJumpTable> m_stringSwitchJumpTables;

        SymbolTable m_symbolTable;

        EvalCodeCache m_evalCodeCache;
    };

    // Program code is not marked by any function, so we make the global object
    // responsible for marking it.

    class ProgramCodeBlock : public CodeBlock {
    public:
        ProgramCodeBlock(ScopeNode* ownerNode, CodeType codeType, JSGlobalObject* globalObject, PassRefPtr<SourceProvider> sourceProvider)
            : CodeBlock(ownerNode, codeType, sourceProvider, 0)
            , m_globalObject(globalObject)
        {
            m_globalObject->codeBlocks().add(this);
        }

        ~ProgramCodeBlock()
        {
            if (m_globalObject)
                m_globalObject->codeBlocks().remove(this);
        }

        void clearGlobalObject() { m_globalObject = 0; }

    private:
        JSGlobalObject* m_globalObject; // For program and eval nodes, the global object that marks the constant pool.
    };

    class EvalCodeBlock : public ProgramCodeBlock {
    public:
        EvalCodeBlock(ScopeNode* ownerNode, JSGlobalObject* globalObject, PassRefPtr<SourceProvider> sourceProvider)
            : ProgramCodeBlock(ownerNode, EvalCode, globalObject, sourceProvider)
        {
        }
    };

} // namespace JSC

#endif // CodeBlock_h
