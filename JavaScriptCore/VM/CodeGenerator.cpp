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

#include "config.h"
#include "CodeGenerator.h"

#include "BatchedTransitionOptimizer.h"
#include "JSFunction.h"
#include "Machine.h"
#include "ustring.h"

using namespace std;

namespace JSC {

/*
    The layout of a register frame looks like this:

    For

    function f(x, y) {
        var v1;
        function g() { }
        var v2;
        return (x) * (y);
    }

    assuming (x) and (y) generated temporaries t1 and t2, you would have

    ------------------------------------
    |  x |  y |  g | v2 | v1 | t1 | t2 | <-- value held
    ------------------------------------
    | -5 | -4 | -3 | -2 | -1 | +0 | +1 | <-- register index
    ------------------------------------
    | params->|<-locals      | temps->

    Because temporary registers are allocated in a stack-like fashion, we
    can reclaim them with a simple popping algorithm. The same goes for labels.
    (We never reclaim parameter or local registers, because parameters and
    locals are DontDelete.)

    The register layout before a function call looks like this:

    For

    function f(x, y)
    {
    }

    f(1);

    >                        <------------------------------
    <                        >  reserved: call frame  |  1 | <-- value held
    >         >snip<         <------------------------------
    <                        > +0 | +1 | +2 | +3 | +4 | +5 | <-- register index
    >                        <------------------------------
    | params->|<-locals      | temps->

    The call instruction fills in the "call frame" registers. It also pads
    missing arguments at the end of the call:

    >                        <-----------------------------------
    <                        >  reserved: call frame  |  1 |  ? | <-- value held ("?" stands for "undefined")
    >         >snip<         <-----------------------------------
    <                        > +0 | +1 | +2 | +3 | +4 | +5 | +6 | <-- register index
    >                        <-----------------------------------
    | params->|<-locals      | temps->

    After filling in missing arguments, the call instruction sets up the new
    stack frame to overlap the end of the old stack frame:

                             |---------------------------------->                        <
                             |  reserved: call frame  |  1 |  ? <                        > <-- value held ("?" stands for "undefined")
                             |---------------------------------->         >snip<         <
                             | -7 | -6 | -5 | -4 | -3 | -2 | -1 <                        > <-- register index
                             |---------------------------------->                        <
                             |                        | params->|<-locals       | temps->

    That way, arguments are "copied" into the callee's stack frame for free.

    If the caller supplies too many arguments, this trick doesn't work. The
    extra arguments protrude into space reserved for locals and temporaries.
    In that case, the call instruction makes a real copy of the call frame header,
    along with just the arguments expected by the callee, leaving the original
    call frame header and arguments behind. (The call instruction can't just discard
    extra arguments, because the "arguments" object may access them later.)
    This copying strategy ensures that all named values will be at the indices
    expected by the callee.
*/

#ifndef NDEBUG
bool CodeGenerator::s_dumpsGeneratedCode = false;
#endif

void CodeGenerator::setDumpsGeneratedCode(bool dumpsGeneratedCode)
{
#ifndef NDEBUG
    s_dumpsGeneratedCode = dumpsGeneratedCode;
#else
    UNUSED_PARAM(dumpsGeneratedCode);
#endif
}

void CodeGenerator::generate()
{
    m_codeBlock->thisRegister = m_thisRegister.index();

    m_scopeNode->emitCode(*this);

#ifndef NDEBUG
    if (s_dumpsGeneratedCode) {
        JSGlobalObject* globalObject = m_scopeChain->globalObject();
        m_codeBlock->dump(globalObject->globalExec());
    }
#endif

    m_scopeNode->children().shrinkCapacity(0);
    if (m_codeType != EvalCode) { // eval code needs to hang on to its declaration stacks to keep declaration info alive until Machine::execute time.
        m_scopeNode->varStack().shrinkCapacity(0);
        m_scopeNode->functionStack().shrinkCapacity(0);
    }
}

bool CodeGenerator::addVar(const Identifier& ident, bool isConstant, RegisterID*& r0)
{
    int index = m_calleeRegisters.size();
    SymbolTableEntry newEntry(index, isConstant ? ReadOnly : 0);
    pair<SymbolTable::iterator, bool> result = symbolTable().add(ident.ustring().rep(), newEntry);

    if (!result.second) {
        r0 = &registerFor(result.first->second.getIndex());
        return false;
    }

    ++m_codeBlock->numVars;
    r0 = newRegister();
    return true;
}

bool CodeGenerator::addGlobalVar(const Identifier& ident, bool isConstant, RegisterID*& r0)
{
    int index = m_nextGlobal;
    SymbolTableEntry newEntry(index, isConstant ? ReadOnly : 0);
    pair<SymbolTable::iterator, bool> result = symbolTable().add(ident.ustring().rep(), newEntry);

    if (!result.second)
        index = result.first->second.getIndex();
    else {
        --m_nextGlobal;
        m_globals.append(index + m_globalVarStorageOffset);
    }

    r0 = &registerFor(index);
    return result.second;
}

void CodeGenerator::allocateConstants(size_t count)
{
    m_codeBlock->numConstants = count;
    if (!count)
        return;
    
    m_nextConstant = m_calleeRegisters.size();

    for (size_t i = 0; i < count; ++i)
        newRegister();
    m_lastConstant = &m_calleeRegisters.last();
}

CodeGenerator::CodeGenerator(ProgramNode* programNode, const Debugger* debugger, const ScopeChain& scopeChain, SymbolTable* symbolTable, CodeBlock* codeBlock, VarStack& varStack, FunctionStack& functionStack)
    : m_shouldEmitDebugHooks(!!debugger)
    , m_shouldEmitProfileHooks(scopeChain.globalObject()->supportsProfiling())
    , m_scopeChain(&scopeChain)
    , m_symbolTable(symbolTable)
    , m_scopeNode(programNode)
    , m_codeBlock(codeBlock)
    , m_thisRegister(RegisterFile::ProgramCodeThisRegister)
    , m_finallyDepth(0)
    , m_dynamicScopeDepth(0)
    , m_codeType(GlobalCode)
    , m_nextGlobal(-1)
    , m_globalData(&scopeChain.globalObject()->globalExec()->globalData())
    , m_lastOpcodeID(op_end)
{
    if (m_shouldEmitDebugHooks)
        m_codeBlock->needsFullScopeChain = true;

    emitOpcode(op_enter);
    codeBlock->globalData = m_globalData;

    // FIXME: Move code that modifies the global object to Machine::execute.
    
    m_codeBlock->numParameters = 1; // Allocate space for "this"

    JSGlobalObject* globalObject = scopeChain.globalObject();
    ExecState* exec = globalObject->globalExec();
    RegisterFile* registerFile = &exec->globalData().machine->registerFile();
    
    // Shift register indexes in generated code to elide registers allocated by intermediate stack frames.
    m_globalVarStorageOffset = -RegisterFile::CallFrameHeaderSize - m_codeBlock->numParameters - registerFile->size();

    // Add previously defined symbols to bookkeeping.
    m_globals.resize(symbolTable->size());
    SymbolTable::iterator end = symbolTable->end();
    for (SymbolTable::iterator it = symbolTable->begin(); it != end; ++it)
        registerFor(it->second.getIndex()).setIndex(it->second.getIndex() + m_globalVarStorageOffset);
        
    BatchedTransitionOptimizer optimizer(globalObject);

    bool canOptimizeNewGlobals = symbolTable->size() + functionStack.size() + varStack.size() < registerFile->maxGlobals();
    if (canOptimizeNewGlobals) {
        // Shift new symbols so they get stored prior to existing symbols.
        m_nextGlobal -= symbolTable->size();

        for (size_t i = 0; i < functionStack.size(); ++i) {
            FuncDeclNode* funcDecl = functionStack[i].get();
            globalObject->removeDirect(funcDecl->m_ident); // Make sure our new function is not shadowed by an old property.
            emitNewFunction(addGlobalVar(funcDecl->m_ident, false), funcDecl);
        }

        Vector<RegisterID*, 32> newVars;
        for (size_t i = 0; i < varStack.size(); ++i)
            if (!globalObject->hasProperty(exec, varStack[i].first))
                newVars.append(addGlobalVar(varStack[i].first, varStack[i].second & DeclarationStacks::IsConstant));

        allocateConstants(programNode->neededConstants());

        for (size_t i = 0; i < newVars.size(); ++i)
            emitLoad(newVars[i], jsUndefined());
    } else {
        for (size_t i = 0; i < functionStack.size(); ++i) {
            FuncDeclNode* funcDecl = functionStack[i].get();
            globalObject->putWithAttributes(exec, funcDecl->m_ident, funcDecl->makeFunction(exec, scopeChain.node()), DontDelete);
        }
        for (size_t i = 0; i < varStack.size(); ++i) {
            if (globalObject->hasProperty(exec, varStack[i].first))
                continue;
            int attributes = DontDelete;
            if (varStack[i].second & DeclarationStacks::IsConstant)
                attributes |= ReadOnly;
            globalObject->putWithAttributes(exec, varStack[i].first, jsUndefined(), attributes);
        }

        allocateConstants(programNode->neededConstants());
    }
}

CodeGenerator::CodeGenerator(FunctionBodyNode* functionBody, const Debugger* debugger, const ScopeChain& scopeChain, SymbolTable* symbolTable, CodeBlock* codeBlock)
    : m_shouldEmitDebugHooks(!!debugger)
    , m_shouldEmitProfileHooks(scopeChain.globalObject()->supportsProfiling())
    , m_scopeChain(&scopeChain)
    , m_symbolTable(symbolTable)
    , m_scopeNode(functionBody)
    , m_codeBlock(codeBlock)
    , m_finallyDepth(0)
    , m_dynamicScopeDepth(0)
    , m_codeType(FunctionCode)
    , m_globalData(&scopeChain.globalObject()->globalExec()->globalData())
    , m_lastOpcodeID(op_end)
{
    if (m_shouldEmitDebugHooks)
        m_codeBlock->needsFullScopeChain = true;

    codeBlock->globalData = m_globalData;

    bool usesArguments = functionBody->usesArguments();
    codeBlock->usesArguments = usesArguments;
    if (usesArguments) {
        m_argumentsRegister.setIndex(RegisterFile::OptionalCalleeArguments);
        addVar(propertyNames().arguments, false);
    }

    if (m_codeBlock->needsFullScopeChain) {
        ++m_codeBlock->numVars;
        m_activationRegisterIndex = newRegister()->index();
        emitOpcode(op_enter_with_activation);
        instructions().append(m_activationRegisterIndex);
    } else
        emitOpcode(op_enter);

    if (usesArguments)
        emitOpcode(op_create_arguments);

    const Node::FunctionStack& functionStack = functionBody->functionStack();
    for (size_t i = 0; i < functionStack.size(); ++i) {
        FuncDeclNode* funcDecl = functionStack[i].get();
        const Identifier& ident = funcDecl->m_ident;
        m_functions.add(ident.ustring().rep());
        emitNewFunction(addVar(ident, false), funcDecl);
    }

    const Node::VarStack& varStack = functionBody->varStack();
    for (size_t i = 0; i < varStack.size(); ++i)
        addVar(varStack[i].first, varStack[i].second & DeclarationStacks::IsConstant);

    const Identifier* parameters = functionBody->parameters();
    size_t parameterCount = functionBody->parameterCount();
    m_nextParameter = -RegisterFile::CallFrameHeaderSize - parameterCount - 1;
    m_parameters.resize(1 + parameterCount); // reserve space for "this"

    // Add "this" as a parameter
    m_thisRegister.setIndex(m_nextParameter);
    ++m_nextParameter;
    ++m_codeBlock->numParameters;

    if (functionBody->usesThis()) {
        emitOpcode(op_convert_this);
        instructions().append(m_thisRegister.index());
    }
    
    for (size_t i = 0; i < parameterCount; ++i)
        addParameter(parameters[i]);

    allocateConstants(functionBody->neededConstants());
}

CodeGenerator::CodeGenerator(EvalNode* evalNode, const Debugger* debugger, const ScopeChain& scopeChain, SymbolTable* symbolTable, EvalCodeBlock* codeBlock)
    : m_shouldEmitDebugHooks(!!debugger)
    , m_shouldEmitProfileHooks(scopeChain.globalObject()->supportsProfiling())
    , m_scopeChain(&scopeChain)
    , m_symbolTable(symbolTable)
    , m_scopeNode(evalNode)
    , m_codeBlock(codeBlock)
    , m_thisRegister(RegisterFile::ProgramCodeThisRegister)
    , m_finallyDepth(0)
    , m_dynamicScopeDepth(0)
    , m_codeType(EvalCode)
    , m_globalData(&scopeChain.globalObject()->globalExec()->globalData())
    , m_lastOpcodeID(op_end)
{
    if (m_shouldEmitDebugHooks)
        m_codeBlock->needsFullScopeChain = true;

    emitOpcode(op_enter);
    codeBlock->globalData = m_globalData;
    m_codeBlock->numParameters = 1; // Allocate space for "this"

    allocateConstants(evalNode->neededConstants());
}

RegisterID* CodeGenerator::addParameter(const Identifier& ident)
{
    // Parameters overwrite var declarations, but not function declarations.
    RegisterID* result = 0;
    UString::Rep* rep = ident.ustring().rep();
    if (!m_functions.contains(rep)) {
        symbolTable().set(rep, m_nextParameter);
        RegisterID& parameter = registerFor(m_nextParameter);
        parameter.setIndex(m_nextParameter);
        result = &parameter;
    }

    // To maintain the calling convention, we have to allocate unique space for
    // each parameter, even if the parameter doesn't make it into the symbol table.
    ++m_nextParameter;
    ++m_codeBlock->numParameters;
    return result;
}

RegisterID* CodeGenerator::registerFor(const Identifier& ident)
{
    if (ident == propertyNames().thisIdentifier)
        return &m_thisRegister;

    if (!shouldOptimizeLocals())
        return 0;

    SymbolTableEntry entry = symbolTable().get(ident.ustring().rep());
    if (entry.isNull())
        return 0;

    return &registerFor(entry.getIndex());
}

RegisterID* CodeGenerator::constRegisterFor(const Identifier& ident)
{
    if (m_codeType == EvalCode)
        return 0;

    SymbolTableEntry entry = symbolTable().get(ident.ustring().rep());
    ASSERT(!entry.isNull());

    return &registerFor(entry.getIndex());
}

bool CodeGenerator::isLocal(const Identifier& ident)
{
    if (ident == propertyNames().thisIdentifier)
        return true;
    
    return shouldOptimizeLocals() && symbolTable().contains(ident.ustring().rep());
}

bool CodeGenerator::isLocalConstant(const Identifier& ident)
{
    return symbolTable().get(ident.ustring().rep()).isReadOnly();
}

RegisterID* CodeGenerator::newRegister()
{
    m_calleeRegisters.append(m_calleeRegisters.size());
    m_codeBlock->numCalleeRegisters = max<int>(m_codeBlock->numCalleeRegisters, m_calleeRegisters.size());
    return &m_calleeRegisters.last();
}

RegisterID* CodeGenerator::newTemporary()
{
    // Reclaim free register IDs.
    while (m_calleeRegisters.size() && !m_calleeRegisters.last().refCount())
        m_calleeRegisters.removeLast();
        
    RegisterID* result = newRegister();
    result->setTemporary();
    return result;
}

RegisterID* CodeGenerator::highestUsedRegister()
{
    size_t count = m_codeBlock->numCalleeRegisters;
    while (m_calleeRegisters.size() < count)
        newRegister();
    return &m_calleeRegisters.last();
}

PassRefPtr<LabelScope> CodeGenerator::newLabelScope(LabelScope::Type type, const Identifier* name)
{
    // Reclaim free label scopes.
    while (m_labelScopes.size() && !m_labelScopes.last().refCount())
        m_labelScopes.removeLast();

    // Allocate new label scope.
    LabelScope scope(type, name, scopeDepth(), newLabel(), type == LabelScope::Loop ? newLabel() : 0); // Only loops have continue targets.
    m_labelScopes.append(scope);
    return &m_labelScopes.last();
}

PassRefPtr<LabelID> CodeGenerator::newLabel()
{
    // Reclaim free label IDs.
    while (m_labels.size() && !m_labels.last().refCount())
        m_labels.removeLast();

    // Allocate new label ID.
    m_labels.append(m_codeBlock);
    return &m_labels.last();
}

PassRefPtr<LabelID> CodeGenerator::emitLabel(LabelID* l0)
{
    l0->setLocation(instructions().size());
    
    // This disables peephole optimizations when an instruction is a jump target
    m_lastOpcodeID = op_end;
    
    return l0;
}

void CodeGenerator::emitOpcode(OpcodeID opcodeID)
{
    instructions().append(globalData()->machine->getOpcode(opcodeID));
    m_lastOpcodeID = opcodeID;
}

void CodeGenerator::retrieveLastBinaryOp(int& dstIndex, int& src1Index, int& src2Index)
{
    ASSERT(instructions().size() >= 4);
    size_t size = instructions().size();
    dstIndex = instructions().at(size - 3).u.operand;
    src1Index = instructions().at(size - 2).u.operand;
    src2Index = instructions().at(size - 1).u.operand;
}

void CodeGenerator::retrieveLastUnaryOp(int& dstIndex, int& srcIndex)
{
    ASSERT(instructions().size() >= 3);
    size_t size = instructions().size();
    dstIndex = instructions().at(size - 2).u.operand;
    srcIndex = instructions().at(size - 1).u.operand;
}

void ALWAYS_INLINE CodeGenerator::rewindBinaryOp()
{
    ASSERT(instructions().size() >= 4);
    instructions().shrink(instructions().size() - 4);
}

void ALWAYS_INLINE CodeGenerator::rewindUnaryOp()
{
    ASSERT(instructions().size() >= 3);
    instructions().shrink(instructions().size() - 3);
}

PassRefPtr<LabelID> CodeGenerator::emitJump(LabelID* target)
{
    emitOpcode(target->isForwardLabel() ? op_jmp : op_loop);
    instructions().append(target->offsetFrom(instructions().size()));
    return target;
}

PassRefPtr<LabelID> CodeGenerator::emitJumpIfTrue(RegisterID* cond, LabelID* target)
{
    if (m_lastOpcodeID == op_less && !target->isForwardLabel()) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();
            emitOpcode(op_loop_if_less);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->offsetFrom(instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_lesseq && !target->isForwardLabel()) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();
            emitOpcode(op_loop_if_lesseq);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->offsetFrom(instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_eq_null && target->isForwardLabel()) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();
            emitOpcode(op_jeq_null);
            instructions().append(srcIndex);
            instructions().append(target->offsetFrom(instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_neq_null && target->isForwardLabel()) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();
            emitOpcode(op_jneq_null);
            instructions().append(srcIndex);
            instructions().append(target->offsetFrom(instructions().size()));
            return target;
        }
    }

    emitOpcode(target->isForwardLabel() ? op_jtrue : op_loop_if_true);
    instructions().append(cond->index());
    instructions().append(target->offsetFrom(instructions().size()));
    return target;
}

PassRefPtr<LabelID> CodeGenerator::emitJumpIfFalse(RegisterID* cond, LabelID* target)
{
    ASSERT(target->isForwardLabel());

    if (m_lastOpcodeID == op_less) {
        int dstIndex;
        int src1Index;
        int src2Index;

        retrieveLastBinaryOp(dstIndex, src1Index, src2Index);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindBinaryOp();
            emitOpcode(op_jnless);
            instructions().append(src1Index);
            instructions().append(src2Index);
            instructions().append(target->offsetFrom(instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_not) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();
            emitOpcode(op_jtrue);
            instructions().append(srcIndex);
            instructions().append(target->offsetFrom(instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_eq_null) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();
            emitOpcode(op_jneq_null);
            instructions().append(srcIndex);
            instructions().append(target->offsetFrom(instructions().size()));
            return target;
        }
    } else if (m_lastOpcodeID == op_neq_null) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (cond->index() == dstIndex && cond->isTemporary() && !cond->refCount()) {
            rewindUnaryOp();
            emitOpcode(op_jeq_null);
            instructions().append(srcIndex);
            instructions().append(target->offsetFrom(instructions().size()));
            return target;
        }
    }

    emitOpcode(op_jfalse);
    instructions().append(cond->index());
    instructions().append(target->offsetFrom(instructions().size()));
    return target;
}

unsigned CodeGenerator::addConstant(FuncDeclNode* n)
{
    // No need to explicitly unique function body nodes -- they're unique already.
    int index = m_codeBlock->functions.size();
    m_codeBlock->functions.append(n);
    return index;
}

unsigned CodeGenerator::addConstant(FuncExprNode* n)
{
    // No need to explicitly unique function expression nodes -- they're unique already.
    int index = m_codeBlock->functionExpressions.size();
    m_codeBlock->functionExpressions.append(n);
    return index;
}

unsigned CodeGenerator::addConstant(const Identifier& ident)
{
    UString::Rep* rep = ident.ustring().rep();
    pair<IdentifierMap::iterator, bool> result = m_identifierMap.add(rep, m_codeBlock->identifiers.size());
    if (result.second) // new entry
        m_codeBlock->identifiers.append(Identifier(m_globalData, rep));

    return result.first->second;
}

RegisterID* CodeGenerator::addConstant(JSValue* v)
{
    pair<JSValueMap::iterator, bool> result = m_jsValueMap.add(v, m_nextConstant);
    if (result.second) {
        RegisterID& constant = m_calleeRegisters[m_nextConstant];
        
        ++m_nextConstant;

        m_codeBlock->constantRegisters.append(v);
        return &constant;
    }

    return &registerFor(result.first->second);
}

unsigned CodeGenerator::addUnexpectedConstant(JSValue* v)
{
    int index = m_codeBlock->unexpectedConstants.size();
    m_codeBlock->unexpectedConstants.append(v);
    return index;
}

unsigned CodeGenerator::addRegExp(RegExp* r)
{
    int index = m_codeBlock->regexps.size();
    m_codeBlock->regexps.append(r);
    return index;
}

RegisterID* CodeGenerator::emitMove(RegisterID* dst, RegisterID* src)
{
    emitOpcode(op_mov);
    instructions().append(dst->index());
    instructions().append(src->index());
    return dst;
}

RegisterID* CodeGenerator::emitUnaryOp(OpcodeID opcode, RegisterID* dst, RegisterID* src, ResultType type)
{
    emitOpcode(opcode);
    instructions().append(dst->index());
    instructions().append(src->index());
    if (opcode == op_negate)
        instructions().append(type.toInt());
    return dst;
}

RegisterID* CodeGenerator::emitPreInc(RegisterID* srcDst)
{
    emitOpcode(op_pre_inc);
    instructions().append(srcDst->index());
    return srcDst;
}

RegisterID* CodeGenerator::emitPreDec(RegisterID* srcDst)
{
    emitOpcode(op_pre_dec);
    instructions().append(srcDst->index());
    return srcDst;
}

RegisterID* CodeGenerator::emitPostInc(RegisterID* dst, RegisterID* srcDst)
{
    emitOpcode(op_post_inc);
    instructions().append(dst->index());
    instructions().append(srcDst->index());
    return dst;
}

RegisterID* CodeGenerator::emitPostDec(RegisterID* dst, RegisterID* srcDst)
{
    emitOpcode(op_post_dec);
    instructions().append(dst->index());
    instructions().append(srcDst->index());
    return dst;
}

RegisterID* CodeGenerator::emitBinaryOp(OpcodeID opcode, RegisterID* dst, RegisterID* src1, RegisterID* src2, OperandTypes types)
{
    emitOpcode(opcode);
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());

    if (opcode == op_bitor || opcode == op_bitand || opcode == op_bitxor ||
        opcode == op_add || opcode == op_mul || opcode == op_sub) {
        instructions().append(types.toInt());
    }

    return dst;
}

RegisterID* CodeGenerator::emitEqualityOp(OpcodeID opcode, RegisterID* dst, RegisterID* src1, RegisterID* src2)
{
    if (m_lastOpcodeID == op_typeof) {
        int dstIndex;
        int srcIndex;

        retrieveLastUnaryOp(dstIndex, srcIndex);

        if (src1->index() == dstIndex
            && src1->isTemporary()
            && static_cast<unsigned>(src2->index()) < m_codeBlock->constantRegisters.size()
            && m_codeBlock->constantRegisters[src2->index()].jsValue(m_scopeChain->globalObject()->globalExec())->isString()) {
            const UString& value = asString(m_codeBlock->constantRegisters[src2->index()].jsValue(m_scopeChain->globalObject()->globalExec()))->value();
            if (value == "undefined") {
                rewindUnaryOp();
                emitOpcode(op_is_undefined);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "boolean") {
                rewindUnaryOp();
                emitOpcode(op_is_boolean);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "number") {
                rewindUnaryOp();
                emitOpcode(op_is_number);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "string") {
                rewindUnaryOp();
                emitOpcode(op_is_string);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "object") {
                rewindUnaryOp();
                emitOpcode(op_is_object);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
            if (value == "function") {
                rewindUnaryOp();
                emitOpcode(op_is_function);
                instructions().append(dst->index());
                instructions().append(srcIndex);
                return dst;
            }
        }
    }

    emitOpcode(opcode);
    instructions().append(dst->index());
    instructions().append(src1->index());
    instructions().append(src2->index());
    return dst;
}

RegisterID* CodeGenerator::emitLoad(RegisterID* dst, bool b)
{
    return emitLoad(dst, jsBoolean(b));
}

RegisterID* CodeGenerator::emitLoad(RegisterID* dst, double number)
{
    // FIXME: Our hash tables won't hold infinity, so we make a new JSNumberCell each time.
    // Later we can do the extra work to handle that like the other cases.
    if (number == HashTraits<double>::emptyValue() || HashTraits<double>::isDeletedValue(number))
        return emitLoad(dst, jsNumber(globalData(), number));
    JSValue*& valueInMap = m_numberMap.add(number, noValue()).first->second;
    if (!valueInMap)
        valueInMap = jsNumber(globalData(), number);
    return emitLoad(dst, valueInMap);
}

RegisterID* CodeGenerator::emitLoad(RegisterID* dst, const Identifier& identifier)
{
    JSString*& valueInMap = m_stringMap.add(identifier.ustring().rep(), 0).first->second;
    if (!valueInMap)
        valueInMap = jsOwnedString(globalData(), identifier.ustring());
    return emitLoad(dst, valueInMap);
}

RegisterID* CodeGenerator::emitLoad(RegisterID* dst, JSValue* v)
{
    RegisterID* constantID = addConstant(v);
    if (dst)
        return emitMove(dst, constantID);
    return constantID;
}

RegisterID* CodeGenerator::emitLoad(RegisterID* dst, JSCell* cell)
{
    JSValue* value = cell;
    return emitLoad(dst, value);
}

RegisterID* CodeGenerator::emitUnexpectedLoad(RegisterID* dst, bool b)
{
    emitOpcode(op_unexpected_load);
    instructions().append(dst->index());
    instructions().append(addUnexpectedConstant(jsBoolean(b)));
    return dst;
}

RegisterID* CodeGenerator::emitUnexpectedLoad(RegisterID* dst, double d)
{
    emitOpcode(op_unexpected_load);
    instructions().append(dst->index());
    instructions().append(addUnexpectedConstant(jsNumber(globalData(), d)));
    return dst;
}

bool CodeGenerator::findScopedProperty(const Identifier& property, int& index, size_t& stackDepth, bool forWriting, JSObject*& globalObject)
{
    // Cases where we cannot statically optimize the lookup.
    if (property == propertyNames().arguments || !canOptimizeNonLocals()) {
        stackDepth = 0;
        index = missingSymbolMarker();

        if (shouldOptimizeLocals() && m_codeType == GlobalCode) {
            ScopeChainIterator iter = m_scopeChain->begin();
            globalObject = *iter;
            ASSERT((++iter) == m_scopeChain->end());
        }
        return false;
    }

    size_t depth = 0;
    
    ScopeChainIterator iter = m_scopeChain->begin();
    ScopeChainIterator end = m_scopeChain->end();
    for (; iter != end; ++iter, ++depth) {
        JSObject* currentScope = *iter;
        if (!currentScope->isVariableObject())
            break;
        JSVariableObject* currentVariableObject = static_cast<JSVariableObject*>(currentScope);
        SymbolTableEntry entry = currentVariableObject->symbolTable().get(property.ustring().rep());

        // Found the property
        if (!entry.isNull()) {
            if (entry.isReadOnly() && forWriting) {
                stackDepth = 0;
                index = missingSymbolMarker();
                if (++iter == end)
                    globalObject = currentVariableObject;
                return false;
            }
            stackDepth = depth;
            index = entry.getIndex();
            if (++iter == end)
                globalObject = currentVariableObject;
            return true;
        }
        if (currentVariableObject->isDynamicScope())
            break;
    }

    // Can't locate the property but we're able to avoid a few lookups.
    stackDepth = depth;
    index = missingSymbolMarker();
    JSObject* scope = *iter;
    if (++iter == end)
        globalObject = scope;
    return true;
}

RegisterID* CodeGenerator::emitInstanceOf(RegisterID* dst, RegisterID* value, RegisterID* base, RegisterID* basePrototype)
{ 
    emitOpcode(op_instanceof);
    instructions().append(dst->index());
    instructions().append(value->index());
    instructions().append(base->index());
    instructions().append(basePrototype->index());
    return dst;
}

RegisterID* CodeGenerator::emitResolve(RegisterID* dst, const Identifier& property)
{
    size_t depth = 0;
    int index = 0;
    JSObject* globalObject = 0;
    if (!findScopedProperty(property, index, depth, false, globalObject) && !globalObject) {
        // We can't optimise at all :-(
        emitOpcode(op_resolve);
        instructions().append(dst->index());
        instructions().append(addConstant(property));
        return dst;
    }

    if (index != missingSymbolMarker()) {
        // Directly index the property lookup across multiple scopes.  Yay!
        return emitGetScopedVar(dst, depth, index, globalObject);
    }

    if (globalObject) {
        m_codeBlock->globalResolveInstructions.append(instructions().size());
        emitOpcode(op_resolve_global);
        instructions().append(dst->index());
        instructions().append(globalObject);
        instructions().append(addConstant(property));
        instructions().append(0);
        instructions().append(0);
        return dst;
    }

    // In this case we are at least able to drop a few scope chains from the
    // lookup chain, although we still need to hash from then on.
    emitOpcode(op_resolve_skip);
    instructions().append(dst->index());
    instructions().append(addConstant(property));
    instructions().append(depth);
    return dst;
}

RegisterID* CodeGenerator::emitGetScopedVar(RegisterID* dst, size_t depth, int index, JSValue* globalObject)
{
    if (globalObject) {
        emitOpcode(op_get_global_var);
        instructions().append(dst->index());
        instructions().append(asCell(globalObject));
        instructions().append(index);
        return dst;
    }

    emitOpcode(op_get_scoped_var);
    instructions().append(dst->index());
    instructions().append(index);
    instructions().append(depth);
    return dst;
}

RegisterID* CodeGenerator::emitPutScopedVar(size_t depth, int index, RegisterID* value, JSValue* globalObject)
{
    if (globalObject) {
        emitOpcode(op_put_global_var);
        instructions().append(asCell(globalObject));
        instructions().append(index);
        instructions().append(value->index());
        return value;
    }
    emitOpcode(op_put_scoped_var);
    instructions().append(index);
    instructions().append(depth);
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitResolveBase(RegisterID* dst, const Identifier& property)
{
    emitOpcode(op_resolve_base);
    instructions().append(dst->index());
    instructions().append(addConstant(property));
    return dst;
}

RegisterID* CodeGenerator::emitResolveWithBase(RegisterID* baseDst, RegisterID* propDst, const Identifier& property)
{
    emitOpcode(op_resolve_with_base);
    instructions().append(baseDst->index());
    instructions().append(propDst->index());
    instructions().append(addConstant(property));
    return baseDst;
}

RegisterID* CodeGenerator::emitResolveFunction(RegisterID* baseDst, RegisterID* funcDst, const Identifier& property)
{
    emitOpcode(op_resolve_func);
    instructions().append(baseDst->index());
    instructions().append(funcDst->index());
    instructions().append(addConstant(property));
    return baseDst;
}

RegisterID* CodeGenerator::emitGetById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    m_codeBlock->propertyAccessInstructions.append(instructions().size());

    emitOpcode(op_get_by_id);
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(addConstant(property));
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    return dst;
}

RegisterID* CodeGenerator::emitPutById(RegisterID* base, const Identifier& property, RegisterID* value)
{
    m_codeBlock->propertyAccessInstructions.append(instructions().size());

    emitOpcode(op_put_by_id);
    instructions().append(base->index());
    instructions().append(addConstant(property));
    instructions().append(value->index());
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    instructions().append(0);
    return value;
}

RegisterID* CodeGenerator::emitPutGetter(RegisterID* base, const Identifier& property, RegisterID* value)
{
    emitOpcode(op_put_getter);
    instructions().append(base->index());
    instructions().append(addConstant(property));
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitPutSetter(RegisterID* base, const Identifier& property, RegisterID* value)
{
    emitOpcode(op_put_setter);
    instructions().append(base->index());
    instructions().append(addConstant(property));
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitDeleteById(RegisterID* dst, RegisterID* base, const Identifier& property)
{
    emitOpcode(op_del_by_id);
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(addConstant(property));
    return dst;
}

RegisterID* CodeGenerator::emitGetByVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    emitOpcode(op_get_by_val);
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(property->index());
    return dst;
}

RegisterID* CodeGenerator::emitPutByVal(RegisterID* base, RegisterID* property, RegisterID* value)
{
    emitOpcode(op_put_by_val);
    instructions().append(base->index());
    instructions().append(property->index());
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitDeleteByVal(RegisterID* dst, RegisterID* base, RegisterID* property)
{
    emitOpcode(op_del_by_val);
    instructions().append(dst->index());
    instructions().append(base->index());
    instructions().append(property->index());
    return dst;
}

RegisterID* CodeGenerator::emitPutByIndex(RegisterID* base, unsigned index, RegisterID* value)
{
    emitOpcode(op_put_by_index);
    instructions().append(base->index());
    instructions().append(index);
    instructions().append(value->index());
    return value;
}

RegisterID* CodeGenerator::emitNewObject(RegisterID* dst)
{
    emitOpcode(op_new_object);
    instructions().append(dst->index());
    return dst;
}

RegisterID* CodeGenerator::emitNewArray(RegisterID* dst, ElementNode* elements)
{
    Vector<RefPtr<RegisterID>, 16> argv;
    for (ElementNode* n = elements; n; n = n->next()) {
        if (n->elision())
            break;
        argv.append(newTemporary());
        emitNode(argv.last().get(), n->value());
    }
    emitOpcode(op_new_array);
    instructions().append(dst->index());
    instructions().append(argv.size() ? argv[0]->index() : 0); // argv
    instructions().append(argv.size()); // argc
    return dst;
}

RegisterID* CodeGenerator::emitNewFunction(RegisterID* dst, FuncDeclNode* n)
{
    emitOpcode(op_new_func);
    instructions().append(dst->index());
    instructions().append(addConstant(n));
    return dst;
}

RegisterID* CodeGenerator::emitNewRegExp(RegisterID* dst, RegExp* regExp)
{
    emitOpcode(op_new_regexp);
    instructions().append(dst->index());
    instructions().append(addRegExp(regExp));
    return dst;
}


RegisterID* CodeGenerator::emitNewFunctionExpression(RegisterID* r0, FuncExprNode* n)
{
    emitOpcode(op_new_func_exp);
    instructions().append(r0->index());
    instructions().append(addConstant(n));
    return r0;
}

RegisterID* CodeGenerator::emitCall(RegisterID* dst, RegisterID* func, RegisterID* base, ArgumentsNode* argumentsNode, unsigned divot, unsigned startOffset, unsigned endOffset)
{
    return emitCall(op_call, dst, func, base, argumentsNode, divot, startOffset, endOffset);
}

RegisterID* CodeGenerator::emitCallEval(RegisterID* dst, RegisterID* func, RegisterID* base, ArgumentsNode* argumentsNode, unsigned divot, unsigned startOffset, unsigned endOffset)
{
    return emitCall(op_call_eval, dst, func, base, argumentsNode, divot, startOffset, endOffset);
}

RegisterID* CodeGenerator::emitCall(OpcodeID opcodeID, RegisterID* dst, RegisterID* func, RegisterID* base, ArgumentsNode* argumentsNode, unsigned divot, unsigned startOffset, unsigned endOffset)
{
    ASSERT(opcodeID == op_call || opcodeID == op_call_eval);
    ASSERT(func->refCount());
    ASSERT(!base || base->refCount());
    
    // Generate code for arguments.
    Vector<RefPtr<RegisterID>, 16> argv;
    argv.append(newTemporary()); // reserve space for "this"
    for (ArgumentListNode* n = argumentsNode->m_listNode.get(); n; n = n->m_next.get()) {
        argv.append(newTemporary());
        emitNode(argv.last().get(), n);
    }

    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, RegisterFile::CallFrameHeaderSize> callFrame;
    for (int i = 0; i < RegisterFile::CallFrameHeaderSize; ++i)
        callFrame.append(newTemporary());

    if (m_shouldEmitProfileHooks) {
        emitOpcode(op_profile_will_call);
        instructions().append(func->index());
    }

    emitExpressionInfo(divot, startOffset, endOffset);
    m_codeBlock->callLinkInfos.append(CallLinkInfo());
    emitOpcode(opcodeID);
    instructions().append(dst->index());
    instructions().append(func->index());
    instructions().append(base ? base->index() : missingThisObjectMarker()); // We encode the "this" value in the instruction stream, to avoid an explicit instruction for copying or loading it.
    instructions().append(argv[0]->index()); // argv
    instructions().append(argv.size()); // argc
    instructions().append(argv[0]->index() + argv.size() + RegisterFile::CallFrameHeaderSize); // registerOffset

    if (m_shouldEmitProfileHooks) {
        emitOpcode(op_profile_did_call);
        instructions().append(func->index());
    }

    return dst;
}

RegisterID* CodeGenerator::emitReturn(RegisterID* src)
{
    if (m_codeBlock->needsFullScopeChain) {
        emitOpcode(op_tear_off_activation);
        instructions().append(m_activationRegisterIndex);
    } else if (m_codeBlock->usesArguments && m_codeBlock->numParameters > 1)
        emitOpcode(op_tear_off_arguments);

    return emitUnaryNoDstOp(op_ret, src);
}

RegisterID* CodeGenerator::emitUnaryNoDstOp(OpcodeID opcode, RegisterID* src)
{
    emitOpcode(opcode);
    instructions().append(src->index());
    return src;
}

RegisterID* CodeGenerator::emitConstruct(RegisterID* dst, RegisterID* func, ArgumentsNode* argumentsNode, unsigned divot, unsigned startOffset, unsigned endOffset)
{
    ASSERT(func->refCount());

    RefPtr<RegisterID> funcProto = newTemporary();

    // Generate code for arguments.
    Vector<RefPtr<RegisterID>, 16> argv;
    argv.append(newTemporary()); // reserve space for "this"
    for (ArgumentListNode* n = argumentsNode ? argumentsNode->m_listNode.get() : 0; n; n = n->m_next.get()) {
        argv.append(newTemporary());
        emitNode(argv.last().get(), n);
    }

    if (m_shouldEmitProfileHooks) {
        emitOpcode(op_profile_will_call);
        instructions().append(func->index());
    }

    // Load prototype.
    emitExpressionInfo(divot, startOffset, endOffset);
    emitGetById(funcProto.get(), func, globalData()->propertyNames->prototype);

    // Reserve space for call frame.
    Vector<RefPtr<RegisterID>, RegisterFile::CallFrameHeaderSize> callFrame;
    for (int i = 0; i < RegisterFile::CallFrameHeaderSize; ++i)
        callFrame.append(newTemporary());

    emitExpressionInfo(divot, startOffset, endOffset);
    m_codeBlock->callLinkInfos.append(CallLinkInfo());
    emitOpcode(op_construct);
    instructions().append(dst->index());
    instructions().append(func->index());
    instructions().append(funcProto->index());
    instructions().append(argv[0]->index()); // argv
    instructions().append(argv.size()); // argc
    instructions().append(argv[0]->index() + argv.size() + RegisterFile::CallFrameHeaderSize); // registerOffset

    emitOpcode(op_construct_verify);
    instructions().append(dst->index());
    instructions().append(argv[0]->index());

    if (m_shouldEmitProfileHooks) {
        emitOpcode(op_profile_did_call);
        instructions().append(func->index());
    }

    return dst;
}

RegisterID* CodeGenerator::emitPushScope(RegisterID* scope)
{
    ControlFlowContext context;
    context.isFinallyBlock = false;
    m_scopeContextStack.append(context);
    m_dynamicScopeDepth++;

    return emitUnaryNoDstOp(op_push_scope, scope);
}

void CodeGenerator::emitPopScope()
{
    ASSERT(m_scopeContextStack.size());
    ASSERT(!m_scopeContextStack.last().isFinallyBlock);

    emitOpcode(op_pop_scope);

    m_scopeContextStack.removeLast();
    m_dynamicScopeDepth--;
}

void CodeGenerator::emitDebugHook(DebugHookID debugHookID, int firstLine, int lastLine)
{
    if (!m_shouldEmitDebugHooks)
        return;
    emitOpcode(op_debug);
    instructions().append(debugHookID);
    instructions().append(firstLine);
    instructions().append(lastLine);
}

void CodeGenerator::pushFinallyContext(LabelID* target, RegisterID* retAddrDst)
{
    ControlFlowContext scope;
    scope.isFinallyBlock = true;
    FinallyContext context = { target, retAddrDst };
    scope.finallyContext = context;
    m_scopeContextStack.append(scope);
    m_finallyDepth++;
}

void CodeGenerator::popFinallyContext()
{
    ASSERT(m_scopeContextStack.size());
    ASSERT(m_scopeContextStack.last().isFinallyBlock);
    ASSERT(m_finallyDepth > 0);
    m_scopeContextStack.removeLast();
    m_finallyDepth--;
}

LabelScope* CodeGenerator::breakTarget(const Identifier& name)
{
    // Reclaim free label scopes.
    while (m_labelScopes.size() && !m_labelScopes.last().refCount())
        m_labelScopes.removeLast();

    if (!m_labelScopes.size())
        return 0;

    // We special-case the following, which is a syntax error in Firefox:
    // label:
    //     break;
    if (name.isEmpty()) {
        for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
            LabelScope* scope = &m_labelScopes[i];
            if (scope->type() != LabelScope::NamedLabel) {
                ASSERT(scope->breakTarget());
                return scope;
            }
        }
        return 0;
    }

    for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
        LabelScope* scope = &m_labelScopes[i];
        if (scope->name() && *scope->name() == name) {
            ASSERT(scope->breakTarget());
            return scope;
        }
    }
    return 0;
}

LabelScope* CodeGenerator::continueTarget(const Identifier& name)
{
    // Reclaim free label scopes.
    while (m_labelScopes.size() && !m_labelScopes.last().refCount())
        m_labelScopes.removeLast();

    if (!m_labelScopes.size())
        return 0;

    if (name.isEmpty()) {
        for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
            LabelScope* scope = &m_labelScopes[i];
            if (scope->type() == LabelScope::Loop) {
                ASSERT(scope->continueTarget());
                return scope;
            }
        }
        return 0;
    }

    // Continue to the loop nested nearest to the label scope that matches
    // 'name'.
    LabelScope* result = 0;
    for (int i = m_labelScopes.size() - 1; i >= 0; --i) {
        LabelScope* scope = &m_labelScopes[i];
        if (scope->type() == LabelScope::Loop) {
            ASSERT(scope->continueTarget());
            result = scope;
        }
        if (scope->name() && *scope->name() == name)
            return result; // may be 0
    }
    return 0;
}

PassRefPtr<LabelID> CodeGenerator::emitComplexJumpScopes(LabelID* target, ControlFlowContext* topScope, ControlFlowContext* bottomScope)
{
    while (topScope > bottomScope) {
        // First we count the number of dynamic scopes we need to remove to get
        // to a finally block.
        int nNormalScopes = 0;
        while (topScope > bottomScope) {
            if (topScope->isFinallyBlock)
                break;
            ++nNormalScopes;
            --topScope;
        }

        if (nNormalScopes) {
            // We need to remove a number of dynamic scopes to get to the next
            // finally block
            emitOpcode(op_jmp_scopes);
            instructions().append(nNormalScopes);

            // If topScope == bottomScope then there isn't actually a finally block
            // left to emit, so make the jmp_scopes jump directly to the target label
            if (topScope == bottomScope) {
                instructions().append(target->offsetFrom(instructions().size()));
                return target;
            }

            // Otherwise we just use jmp_scopes to pop a group of scopes and go
            // to the next instruction
            RefPtr<LabelID> nextInsn = newLabel();
            instructions().append(nextInsn->offsetFrom(instructions().size()));
            emitLabel(nextInsn.get());
        }

        // To get here there must be at least one finally block present
        do {
            ASSERT(topScope->isFinallyBlock);
            emitJumpSubroutine(topScope->finallyContext.retAddrDst, topScope->finallyContext.finallyAddr);
            --topScope;
            if (!topScope->isFinallyBlock)
                break;
        } while (topScope > bottomScope);
    }
    return emitJump(target);
}

PassRefPtr<LabelID> CodeGenerator::emitJumpScopes(LabelID* target, int targetScopeDepth)
{
    ASSERT(scopeDepth() - targetScopeDepth >= 0);
    ASSERT(target->isForwardLabel());

    size_t scopeDelta = scopeDepth() - targetScopeDepth;
    ASSERT(scopeDelta <= m_scopeContextStack.size());
    if (!scopeDelta)
        return emitJump(target);

    if (m_finallyDepth)
        return emitComplexJumpScopes(target, &m_scopeContextStack.last(), &m_scopeContextStack.last() - scopeDelta);

    emitOpcode(op_jmp_scopes);
    instructions().append(scopeDelta);
    instructions().append(target->offsetFrom(instructions().size()));
    return target;
}

RegisterID* CodeGenerator::emitNextPropertyName(RegisterID* dst, RegisterID* iter, LabelID* target)
{
    emitOpcode(op_next_pname);
    instructions().append(dst->index());
    instructions().append(iter->index());
    instructions().append(target->offsetFrom(instructions().size()));
    return dst;
}

RegisterID* CodeGenerator::emitCatch(RegisterID* targetRegister, LabelID* start, LabelID* end)
{
    HandlerInfo info = { start->offsetFrom(0), end->offsetFrom(0), instructions().size(), m_dynamicScopeDepth, 0 };
    exceptionHandlers().append(info);
    emitOpcode(op_catch);
    instructions().append(targetRegister->index());
    return targetRegister;
}

RegisterID* CodeGenerator::emitNewError(RegisterID* dst, ErrorType type, JSValue* message)
{
    emitOpcode(op_new_error);
    instructions().append(dst->index());
    instructions().append(static_cast<int>(type));
    instructions().append(addUnexpectedConstant(message));
    return dst;
}

PassRefPtr<LabelID> CodeGenerator::emitJumpSubroutine(RegisterID* retAddrDst, LabelID* finally)
{
    emitOpcode(op_jsr);
    instructions().append(retAddrDst->index());
    instructions().append(finally->offsetFrom(instructions().size()));
    return finally;
}

void CodeGenerator::emitSubroutineReturn(RegisterID* retAddrSrc)
{
    emitOpcode(op_sret);
    instructions().append(retAddrSrc->index());
}

void CodeGenerator::emitPushNewScope(RegisterID* dst, Identifier& property, RegisterID* value)
{
    ControlFlowContext context;
    context.isFinallyBlock = false;
    m_scopeContextStack.append(context);
    m_dynamicScopeDepth++;
    
    emitOpcode(op_push_new_scope);
    instructions().append(dst->index());
    instructions().append(addConstant(property));
    instructions().append(value->index());
}

void CodeGenerator::beginSwitch(RegisterID* scrutineeRegister, SwitchInfo::SwitchType type)
{
    SwitchInfo info = { instructions().size(), type };
    switch (type) {
        case SwitchInfo::SwitchImmediate:
            emitOpcode(op_switch_imm);
            break;
        case SwitchInfo::SwitchCharacter:
            emitOpcode(op_switch_char);
            break;
        case SwitchInfo::SwitchString:
            emitOpcode(op_switch_string);
            break;
        default:
            ASSERT_NOT_REACHED();
    }

    instructions().append(0); // place holder for table index
    instructions().append(0); // place holder for default target    
    instructions().append(scrutineeRegister->index());
    m_switchContextStack.append(info);
}

static int32_t keyForImmediateSwitch(ExpressionNode* node, int32_t min, int32_t max)
{
    UNUSED_PARAM(max);
    ASSERT(node->isNumber());
    double value = static_cast<NumberNode*>(node)->value();
    ASSERT(JSImmediate::from(value));
    int32_t key = static_cast<int32_t>(value);
    ASSERT(key == value);
    ASSERT(key >= min);
    ASSERT(key <= max);
    return key - min;
}

static void prepareJumpTableForImmediateSwitch(SimpleJumpTable& jumpTable, int32_t switchAddress, uint32_t clauseCount, RefPtr<LabelID>* labels, ExpressionNode** nodes, int32_t min, int32_t max)
{
    jumpTable.min = min;
    jumpTable.branchOffsets.resize(max - min + 1);
    jumpTable.branchOffsets.fill(0);
    for (uint32_t i = 0; i < clauseCount; ++i) {
        // We're emitting this after the clause labels should have been fixed, so 
        // the labels should not be "forward" references
        ASSERT(!labels[i]->isForwardLabel());
        jumpTable.add(keyForImmediateSwitch(nodes[i], min, max), labels[i]->offsetFrom(switchAddress)); 
    }
}

static int32_t keyForCharacterSwitch(ExpressionNode* node, int32_t min, int32_t max)
{
    UNUSED_PARAM(max);
    ASSERT(node->isString());
    UString::Rep* clause = static_cast<StringNode*>(node)->value().ustring().rep();
    ASSERT(clause->size() == 1);
    
    int32_t key = clause->data()[0];
    ASSERT(key >= min);
    ASSERT(key <= max);
    return key - min;
}

static void prepareJumpTableForCharacterSwitch(SimpleJumpTable& jumpTable, int32_t switchAddress, uint32_t clauseCount, RefPtr<LabelID>* labels, ExpressionNode** nodes, int32_t min, int32_t max)
{
    jumpTable.min = min;
    jumpTable.branchOffsets.resize(max - min + 1);
    jumpTable.branchOffsets.fill(0);
    for (uint32_t i = 0; i < clauseCount; ++i) {
        // We're emitting this after the clause labels should have been fixed, so 
        // the labels should not be "forward" references
        ASSERT(!labels[i]->isForwardLabel());
        jumpTable.add(keyForCharacterSwitch(nodes[i], min, max), labels[i]->offsetFrom(switchAddress)); 
    }
}

static void prepareJumpTableForStringSwitch(StringJumpTable& jumpTable, int32_t switchAddress, uint32_t clauseCount, RefPtr<LabelID>* labels, ExpressionNode** nodes)
{
    for (uint32_t i = 0; i < clauseCount; ++i) {
        // We're emitting this after the clause labels should have been fixed, so 
        // the labels should not be "forward" references
        ASSERT(!labels[i]->isForwardLabel());
        
        ASSERT(nodes[i]->isString());
        UString::Rep* clause = static_cast<StringNode*>(nodes[i])->value().ustring().rep();
        OffsetLocation location;
        location.branchOffset = labels[i]->offsetFrom(switchAddress);
#if ENABLE(CTI)
        location.ctiOffset = 0;
#endif
        jumpTable.offsetTable.add(clause, location);
    }
}

void CodeGenerator::endSwitch(uint32_t clauseCount, RefPtr<LabelID>* labels, ExpressionNode** nodes, LabelID* defaultLabel, int32_t min, int32_t max)
{
    SwitchInfo switchInfo = m_switchContextStack.last();
    m_switchContextStack.removeLast();
    if (switchInfo.switchType == SwitchInfo::SwitchImmediate) {
        instructions()[switchInfo.opcodeOffset + 1] = m_codeBlock->immediateSwitchJumpTables.size();
        instructions()[switchInfo.opcodeOffset + 2] = defaultLabel->offsetFrom(switchInfo.opcodeOffset + 3);

        m_codeBlock->immediateSwitchJumpTables.append(SimpleJumpTable());
        SimpleJumpTable& jumpTable = m_codeBlock->immediateSwitchJumpTables.last();

        prepareJumpTableForImmediateSwitch(jumpTable, switchInfo.opcodeOffset + 3, clauseCount, labels, nodes, min, max);
    } else if (switchInfo.switchType == SwitchInfo::SwitchCharacter) {
        instructions()[switchInfo.opcodeOffset + 1] = m_codeBlock->characterSwitchJumpTables.size();
        instructions()[switchInfo.opcodeOffset + 2] = defaultLabel->offsetFrom(switchInfo.opcodeOffset + 3);
        
        m_codeBlock->characterSwitchJumpTables.append(SimpleJumpTable());
        SimpleJumpTable& jumpTable = m_codeBlock->characterSwitchJumpTables.last();

        prepareJumpTableForCharacterSwitch(jumpTable, switchInfo.opcodeOffset + 3, clauseCount, labels, nodes, min, max);
    } else {
        ASSERT(switchInfo.switchType == SwitchInfo::SwitchString);
        instructions()[switchInfo.opcodeOffset + 1] = m_codeBlock->stringSwitchJumpTables.size();
        instructions()[switchInfo.opcodeOffset + 2] = defaultLabel->offsetFrom(switchInfo.opcodeOffset + 3);

        m_codeBlock->stringSwitchJumpTables.append(StringJumpTable());
        StringJumpTable& jumpTable = m_codeBlock->stringSwitchJumpTables.last();

        prepareJumpTableForStringSwitch(jumpTable, switchInfo.opcodeOffset + 3, clauseCount, labels, nodes);
    }
}

} // namespace JSC
