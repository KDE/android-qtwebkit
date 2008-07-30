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
#include "Machine.h"

#include "CodeBlock.h"
#include "DebuggerCallFrame.h"
#include "ExceptionHelpers.h"
#include "ExecState.h"
#include "GlobalEvalFunction.h"
#include "JSActivation.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSNotAnObject.h"
#include "JSPropertyNameIterator.h"
#include "JSString.h"
#include "ObjectPrototype.h"
#include "Parser.h"
#include "Profiler.h"
#include "RegExpObject.h"
#include "RegExpPrototype.h"
#include "Register.h"
#include "collector.h"
#include "debugger.h"
#include "operations.h"
#include "SamplingTool.h"
#include <stdio.h>

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if PLATFORM(WIN_OS)
#include <windows.h>
#endif

#if PLATFORM(QT)
#include <QDateTime>
#endif

using namespace std;

namespace KJS {

// Default number of ticks before a timeout check should be done.
static const int initialTickCountThreshold = 255;

// Preferred number of milliseconds between each timeout check
static const int preferredScriptCheckTimeInterval = 1000;

#if HAVE(COMPUTED_GOTO)
static void* op_throw_end_indirect;
static void* op_call_indirect;
#endif

// Returns the depth of the scope chain within a given call frame.
static int depth(ScopeChain& sc)
{
    int scopeDepth = 0;
    ScopeChainIterator iter = sc.begin();
    ScopeChainIterator end = sc.end();
    while (!(*iter)->isActivationObject()) {
        ++iter;
        if (iter == end)
            break;
        ++scopeDepth;
    }
    return scopeDepth;
}

static inline bool jsLess(ExecState* exec, JSValue* v1, JSValue* v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return JSImmediate::getTruncatedInt32(v1) < JSImmediate::getTruncatedInt32(v2);

    double n1;
    double n2;
    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(exec, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(exec, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 < n2;

    return static_cast<const JSString*>(p1)->value() < static_cast<const JSString*>(p2)->value();
}

static inline bool jsLessEq(ExecState* exec, JSValue* v1, JSValue* v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return JSImmediate::getTruncatedInt32(v1) <= JSImmediate::getTruncatedInt32(v2);

    double n1;
    double n2;
    JSValue* p1;
    JSValue* p2;
    bool wasNotString1 = v1->getPrimitiveNumber(exec, n1, p1);
    bool wasNotString2 = v2->getPrimitiveNumber(exec, n2, p2);

    if (wasNotString1 | wasNotString2)
        return n1 <= n2;

    return !(static_cast<const JSString*>(p2)->value() < static_cast<const JSString*>(p1)->value());
}

static JSValue* jsAddSlowCase(ExecState* exec, JSValue* v1, JSValue* v2)
{
    // exception for the Date exception in defaultValue()
    JSValue* p1 = v1->toPrimitive(exec, UnspecifiedType);
    JSValue* p2 = v2->toPrimitive(exec, UnspecifiedType);

    if (p1->isString() || p2->isString()) {
        UString value = p1->toString(exec) + p2->toString(exec);
        if (value.isNull())
            return throwOutOfMemoryError(exec);
        return jsString(exec, value);
    }

    return jsNumber(exec, p1->toNumber(exec) + p2->toNumber(exec));
}

// Fast-path choices here are based on frequency data from SunSpider:
//    <times> Add case: <t1> <t2>
//    ---------------------------
//    5626160 Add case: 3 3 (of these, 3637690 are for immediate values)
//    247412  Add case: 5 5
//    20900   Add case: 5 6
//    13962   Add case: 5 3
//    4000    Add case: 3 5

static inline JSValue* jsAdd(ExecState* exec, JSValue* v1, JSValue* v2)
{
    JSType t1 = v1->type();
    JSType t2 = v2->type();
    const unsigned bothTypes = (t1 << 3) | t2;

    if (bothTypes == ((NumberType << 3) | NumberType))
        return jsNumber(exec, v1->uncheckedGetNumber() + v2->uncheckedGetNumber());
    if (bothTypes == ((StringType << 3) | StringType)) {
        UString value = static_cast<JSString*>(v1)->value() + static_cast<JSString*>(v2)->value();
        if (value.isNull())
            return throwOutOfMemoryError(exec);
        return jsString(exec, value);
    }

    // All other cases are pretty uncommon
    return jsAddSlowCase(exec, v1, v2);
}

static JSValue* jsTypeStringForValue(ExecState* exec, JSValue* v)
{
    switch (v->type()) {
        case UndefinedType:
            return jsString(exec, "undefined");
        case NullType:
            return jsString(exec, "object");
        case BooleanType:
            return jsString(exec, "boolean");
        case NumberType:
            return jsString(exec, "number");
        case StringType:
            return jsString(exec, "string");
        default:
            if (v->isObject()) {
                // Return "undefined" for objects that should be treated
                // as null when doing comparisons.
                if (static_cast<JSObject*>(v)->masqueradeAsUndefined())
                    return jsString(exec, "undefined");
                CallData callData;
                if (static_cast<JSObject*>(v)->getCallData(callData) != CallTypeNone)
                    return jsString(exec, "function");
            }

            return jsString(exec, "object");
    }
}

static bool NEVER_INLINE resolve(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    Identifier& ident = codeBlock->identifiers[property];
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(exec, ident, slot)) {
            JSValue* result = slot.getValue(exec, ident);
            exceptionValue = exec->exception();
            if (exceptionValue)
                return false;
            r[dst] = result;
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(exec, ident, vPC, codeBlock);
    return false;
}

static bool NEVER_INLINE resolve_skip(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;
    int skip = (vPC + 3)->u.operand + codeBlock->needsFullScopeChain;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);
    while (skip--) {
        ++iter;
        ASSERT(iter != end);
    }
    Identifier& ident = codeBlock->identifiers[property];
    do {
        JSObject* o = *iter;
        PropertySlot slot(o);
        if (o->getPropertySlot(exec, ident, slot)) {
            JSValue* result = slot.getValue(exec, ident);
            exceptionValue = exec->exception();
            if (exceptionValue)
                return false;
            r[dst] = result;
            return true;
        }
    } while (++iter != end);
    exceptionValue = createUndefinedVariableError(exec, ident, vPC, codeBlock);
    return false;
}

static void NEVER_INLINE resolveBase(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock)
{
    int dst = (vPC + 1)->u.operand;
    int property = (vPC + 2)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator next = iter;
    ++next;
    ScopeChainIterator end = scopeChain->end();
    ASSERT(iter != end);

    PropertySlot slot;
    Identifier& ident = codeBlock->identifiers[property];
    JSObject* base;
    while (true) {
        base = *iter;
        if (next == end || base->getPropertySlot(exec, ident, slot)) {
            r[dst] = base;
            return;
        }
        iter = next;
        ++next;
    }
}

static bool NEVER_INLINE resolveBaseAndProperty(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
{
    int baseDst = (vPC + 1)->u.operand;
    int propDst = (vPC + 2)->u.operand;
    int property = (vPC + 3)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = codeBlock->identifiers[property];
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(exec, ident, slot)) {
            JSValue* result = slot.getValue(exec, ident);
            exceptionValue = exec->exception();
            if (exceptionValue)
                return false;
            r[propDst] = result;
            r[baseDst] = base;
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(exec, ident, vPC, codeBlock);
    return false;
}

static bool NEVER_INLINE resolveBaseAndFunc(ExecState* exec, Instruction* vPC, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue*& exceptionValue)
{
    int baseDst = (vPC + 1)->u.operand;
    int funcDst = (vPC + 2)->u.operand;
    int property = (vPC + 3)->u.operand;

    ScopeChainIterator iter = scopeChain->begin();
    ScopeChainIterator end = scopeChain->end();

    // FIXME: add scopeDepthIsZero optimization

    ASSERT(iter != end);

    Identifier& ident = codeBlock->identifiers[property];
    JSObject* base;
    do {
        base = *iter;
        PropertySlot slot(base);
        if (base->getPropertySlot(exec, ident, slot)) {            
            // ECMA 11.2.3 says that if we hit an activation the this value should be null.
            // However, section 10.2.3 says that in the case where the value provided
            // by the caller is null, the global object should be used. It also says
            // that the section does not apply to internal functions, but for simplicity
            // of implementation we use the global object anyway here. This guarantees
            // that in host objects you always get a valid object for this.
            // We also handle wrapper substitution for the global object at the same time.
            JSObject* thisObj = base->toThisObject(exec);
            JSValue* result = slot.getValue(exec, ident);
            exceptionValue = exec->exception();
            if (exceptionValue)
                return false;

            r[baseDst] = thisObj;
            r[funcDst] = result;
            return true;
        }
        ++iter;
    } while (iter != end);

    exceptionValue = createUndefinedVariableError(exec, ident, vPC, codeBlock);
    return false;
}

ALWAYS_INLINE void Machine::initializeCallFrame(Register* callFrame, CodeBlock* codeBlock, Instruction* vPC, ScopeChainNode* scopeChain, Register* r, int returnValueRegister, int argv, int argc, int calledAsConstructor, JSValue* function)
{
    callFrame[RegisterFile::CallerCodeBlock] = codeBlock;
    callFrame[RegisterFile::ReturnVPC] = vPC + 1;
    callFrame[RegisterFile::CallerScopeChain] = scopeChain;
    callFrame[RegisterFile::CallerRegisters] = r;
    callFrame[RegisterFile::ReturnValueRegister] = returnValueRegister;
    callFrame[RegisterFile::ArgumentStartRegister] = argv; // original argument vector (for the sake of the "arguments" object)
    callFrame[RegisterFile::ArgumentCount] = argc; // original argument count (for the sake of the "arguments" object)
    callFrame[RegisterFile::CalledAsConstructor] = calledAsConstructor;
    callFrame[RegisterFile::Callee] = function;
    callFrame[RegisterFile::OptionalCalleeActivation] = nullJSValue;
}

ALWAYS_INLINE Register* slideRegisterWindowForCall(ExecState* exec, CodeBlock* newCodeBlock, RegisterFile* registerFile, Register* registerBase, Register* r, int argv, int argc, JSValue*& exceptionValue)
{
    size_t registerOffset = argv + newCodeBlock->numLocals;
    size_t size = r - registerBase + registerOffset + newCodeBlock->numTemporaries;

    if (argc == newCodeBlock->numParameters) { // correct number of arguments
        if (!registerFile->grow(size)) {
            exceptionValue = createStackOverflowError(exec);
            return r;
        }
        r += registerOffset;
    } else if (argc < newCodeBlock->numParameters) { // too few arguments -- fill in the blanks
        if (!registerFile->grow(size)) {
            exceptionValue = createStackOverflowError(exec);
            return r;
        }
        r += registerOffset;

        int omittedArgCount = newCodeBlock->numParameters - argc;
        Register* endOfParams = r - newCodeBlock->numVars;
        for (Register* it = endOfParams - omittedArgCount; it != endOfParams; ++it)
            (*it) = jsUndefined();
    } else { // too many arguments -- copy return info and expected arguments, leaving the extra arguments behind
        int shift = argc + RegisterFile::CallFrameHeaderSize;
        registerOffset += shift;
        size += shift;

        if (!registerFile->grow(size)) {
            exceptionValue = createStackOverflowError(exec);
            return r;
        }
        r += registerOffset;

        Register* it = r - newCodeBlock->numLocals - RegisterFile::CallFrameHeaderSize - shift;
        Register* end = it + RegisterFile::CallFrameHeaderSize + newCodeBlock->numParameters;
        for ( ; it != end; ++it)
            *(it + shift) = *it;
    }
    
    // initialize local variable slots
    for (Register* it = r - newCodeBlock->numVars; it != r; ++it)
        (*it) = jsUndefined();

    return r;
}

ALWAYS_INLINE ScopeChainNode* scopeChainForCall(ExecState* exec, FunctionBodyNode* functionBodyNode, CodeBlock* newCodeBlock, ScopeChainNode* callDataScopeChain, Register* r)
{
    if (newCodeBlock->needsFullScopeChain) {
        JSActivation* activation = new (exec) JSActivation(functionBodyNode, r);
        r[RegisterFile::OptionalCalleeActivation - RegisterFile::CallFrameHeaderSize - newCodeBlock->numLocals] = activation;

        return callDataScopeChain->copy()->push(activation);
    }

    return callDataScopeChain;
}

static NEVER_INLINE bool isNotObject(ExecState* exec, bool forInstanceOf, CodeBlock* codeBlock, const Instruction* vPC, JSValue* value, JSValue*& exceptionData)
{
    if (value->isObject())
        return false;
    exceptionData = createInvalidParamError(exec, forInstanceOf ? "instanceof" : "in" , value, vPC, codeBlock);
    return true;
}

NEVER_INLINE JSValue* Machine::callEval(ExecState* exec, JSObject* thisObj, ScopeChainNode* scopeChain, RegisterFile* registerFile, Register* r, int argv, int argc, JSValue*& exceptionValue)
{
    if (argc < 2)
        return jsUndefined();

    JSValue* program = r[argv + 1].jsValue(exec);

    if (!program->isString())
        return program;

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(exec, scopeChain->globalObject()->evalFunction());

    int sourceId;
    int errLine;
    UString errMsg;
    RefPtr<EvalNode> evalNode = exec->parser()->parse<EvalNode>(exec, UString(), 1, UStringSourceProvider::create(static_cast<JSString*>(program)->value()), &sourceId, &errLine, &errMsg);

    if (!evalNode) {
        exceptionValue = Error::create(exec, SyntaxError, errMsg, errLine, sourceId, NULL);
        if (*profiler)
            (*profiler)->didExecute(exec, scopeChain->globalObject()->evalFunction());
        return 0;
    }

    JSValue* result = exec->globalData().machine->execute(evalNode.get(), exec, thisObj, r - registerFile->base() + argv + argc, scopeChain, &exceptionValue);

    if (*profiler)
        (*profiler)->didExecute(exec, scopeChain->globalObject()->evalFunction());

    return result;
}

Machine::Machine()
    : m_sampler(0)
    , m_reentryDepth(0)
    , m_timeoutTime(0)
    , m_timeAtLastCheckTimeout(0)
    , m_timeExecuting(0)
    , m_timeoutCheckCount(0)
    , m_ticksUntilNextTimeoutCheck(initialTickCountThreshold)
{
    privateExecute(InitializeAndReturn);
    
    // Bizarrely, calling fastMalloc here is faster than allocating space on the stack.
    void* storage = fastMalloc(sizeof(CollectorBlock));

    JSArray* jsArray = new (storage) JSArray(jsNull(), 0);
    m_jsArrayVptr = jsArray->vptr();
    static_cast<JSCell*>(jsArray)->~JSCell();

    JSString* jsString = new (storage) JSString("");
    m_jsStringVptr = jsString->vptr();
    static_cast<JSCell*>(jsString)->~JSCell();
    
    fastFree(storage);
}

void Machine::dumpCallFrame(const CodeBlock* codeBlock, ScopeChainNode* scopeChain, RegisterFile* registerFile, const Register* r)
{
    ScopeChain sc(scopeChain);
    JSGlobalObject* globalObject = sc.globalObject();
    codeBlock->dump(globalObject->globalExec());
    dumpRegisters(codeBlock, registerFile, r);
}

void Machine::dumpRegisters(const CodeBlock* codeBlock, RegisterFile* registerFile, const Register* r)
{
    printf("Register frame: \n\n");
    printf("----------------------------------------------------\n");
    printf("            use            |   address  |   value   \n");
    printf("----------------------------------------------------\n");

    const Register* it;
    const Register* end;

    if (codeBlock->codeType == GlobalCode) {
        it = registerFile->lastGlobal();
        end = it + registerFile->numGlobals();
        while (it != end) {
            printf("[global var]               | %10p | %10p \n", it, (*it).v());
            ++it;
        }
        printf("----------------------------------------------------\n");
    }
    
    it = r - codeBlock->numLocals - RegisterFile::CallFrameHeaderSize;
    printf("[CallerCodeBlock]          | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ReturnVPC]                | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[CallerScopeChain]         | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[CallerRegisterOffset]     | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ReturnValueRegister]      | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ArgumentStartRegister]    | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[ArgumentCount]            | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[CalledAsConstructor]      | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[Callee]                   | %10p | %10p \n", it, (*it).v()); ++it;
    printf("[OptionalCalleeActivation] | %10p | %10p \n", it, (*it).v()); ++it;
    printf("----------------------------------------------------\n");

    printf("[this]                     | %10p | %10p \n", it, (*it).v()); ++it;
    end = it + max(codeBlock->numParameters - 1, 0); // - 1 to skip "this"
    if (it != end) {
        do {
            printf("[param]                    | %10p | %10p \n", it, (*it).v());
            ++it;
        } while (it != end);
    }
    printf("----------------------------------------------------\n");

    if (codeBlock->codeType != GlobalCode) {
        end = it + codeBlock->numVars;
        if (it != end) {
            do {
                printf("[var]                      | %10p | %10p \n", it, (*it).v());
                ++it;
            } while (it != end);
        printf("----------------------------------------------------\n");
        }
    }

    end = it + codeBlock->numTemporaries;
    if (it != end) {
        do {
            printf("[temp]                     | %10p | %10p \n", it, (*it).v());
            ++it;
        } while (it != end);
    }
}

bool Machine::isOpcode(Opcode opcode)
{
#if HAVE(COMPUTED_GOTO)
    return opcode != HashTraits<Opcode>::emptyValue()
        && !HashTraits<Opcode>::isDeletedValue(opcode)
        && m_opcodeIDTable.contains(opcode);
#else
    return opcode >= 0 && opcode <= op_end;
#endif
}

NEVER_INLINE bool Machine::unwindCallFrame(ExecState* exec, JSValue* exceptionValue, const Instruction*& vPC, CodeBlock*& codeBlock, Register*& k, ScopeChainNode*& scopeChain, Register*& r)
{
    CodeBlock* oldCodeBlock = codeBlock;
    Register* callFrame = r - oldCodeBlock->numLocals - RegisterFile::CallFrameHeaderSize;

    if (Debugger* debugger = exec->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(exec->dynamicGlobalObject(), codeBlock, scopeChain, r, exceptionValue);
        if (callFrame[RegisterFile::Callee].jsValue(exec))
            debugger->returnEvent(debuggerCallFrame, codeBlock->ownerNode->sourceId(), codeBlock->ownerNode->lastLine());
        else
            debugger->didExecuteProgram(debuggerCallFrame, codeBlock->ownerNode->sourceId(), codeBlock->ownerNode->lastLine());
    }

    if (Profiler* profiler = *Profiler::enabledProfilerReference()) {
        if (callFrame[RegisterFile::Callee].jsValue(exec))
            profiler->didExecute(exec, static_cast<JSObject*>(callFrame[RegisterFile::Callee].jsValue(exec)));
        else
            profiler->didExecute(exec, codeBlock->ownerNode->sourceURL(), codeBlock->ownerNode->lineNo());
    }

    if (oldCodeBlock->needsFullScopeChain)
        scopeChain->deref();

    // If this call frame created an activation, tear it off.
    if (JSActivation* activation = static_cast<JSActivation*>(callFrame[RegisterFile::OptionalCalleeActivation].jsValue(exec))) {
        ASSERT(activation->isActivationObject());
        activation->copyRegisters();
    }
    
    codeBlock = callFrame[RegisterFile::CallerCodeBlock].codeBlock();
    if (!codeBlock)
        return false;

    k = codeBlock->registers.data();
    scopeChain = callFrame[RegisterFile::CallerScopeChain].scopeChain();
    r = callFrame[RegisterFile::CallerRegisters].r();
    exec->m_callFrame = r - oldCodeBlock->numLocals - RegisterFile::CallFrameHeaderSize;
    vPC = callFrame[RegisterFile::ReturnVPC].vPC();

    return true;
}

NEVER_INLINE Instruction* Machine::throwException(ExecState* exec, JSValue*& exceptionValue, const Instruction* vPC, CodeBlock*& codeBlock, Register*& k, ScopeChainNode*& scopeChain, Register*& r, bool explicitThrow)
{
    // Set up the exception object

    if (exceptionValue->isObject()) {
        JSObject* exception = static_cast<JSObject*>(exceptionValue);
        if (exception->isNotAnObjectErrorStub()) {
            exception = createNotAnObjectError(exec, static_cast<JSNotAnObjectErrorStub*>(exception), vPC, codeBlock);
            exceptionValue = exception;
        } else {
            if (!exception->hasProperty(exec, Identifier(exec, "line")) && 
                !exception->hasProperty(exec, Identifier(exec, "sourceId")) && 
                !exception->hasProperty(exec, Identifier(exec, "sourceURL")) && 
                !exception->hasProperty(exec, Identifier(exec, expressionBeginOffsetPropertyName)) && 
                !exception->hasProperty(exec, Identifier(exec, expressionCaretOffsetPropertyName)) && 
                !exception->hasProperty(exec, Identifier(exec, expressionEndOffsetPropertyName))) {
                if (explicitThrow) {
                    int startOffset = 0;
                    int endOffset = 0;
                    int divotPoint = 0;
                    int line = codeBlock->expressionRangeForVPC(vPC, divotPoint, startOffset, endOffset);
                    exception->putWithAttributes(exec, Identifier(exec, "line"), jsNumber(exec, line), ReadOnly | DontDelete);
                    
                    // We only hit this path for error messages and throw statements, which don't have a specific failure position
                    // So we just give the full range of the error/throw statement.
                    exception->putWithAttributes(exec, Identifier(exec, expressionBeginOffsetPropertyName), jsNumber(exec, divotPoint - startOffset), ReadOnly | DontDelete);
                    exception->putWithAttributes(exec, Identifier(exec, expressionEndOffsetPropertyName), jsNumber(exec, divotPoint + endOffset), ReadOnly | DontDelete);
                } else
                    exception->putWithAttributes(exec, Identifier(exec, "line"), jsNumber(exec, codeBlock->lineNumberForVPC(vPC)), ReadOnly | DontDelete);
                exception->putWithAttributes(exec, Identifier(exec, "sourceId"), jsNumber(exec, codeBlock->ownerNode->sourceId()), ReadOnly | DontDelete);
                exception->putWithAttributes(exec, Identifier(exec, "sourceURL"), jsOwnedString(exec, codeBlock->ownerNode->sourceURL()), ReadOnly | DontDelete);
            }
            
            if (exception->isWatchdogException()) {
                while (unwindCallFrame(exec, exceptionValue, vPC, codeBlock, k, scopeChain, r)) {
                    // Don't need handler checks or anything, we just want to unroll all the JS callframes possible.
                }
                return 0;
            }
        }
    }

    if (Debugger* debugger = exec->dynamicGlobalObject()->debugger()) {
        DebuggerCallFrame debuggerCallFrame(exec->dynamicGlobalObject(), codeBlock, scopeChain, r, exceptionValue);
        debugger->exception(debuggerCallFrame, codeBlock->ownerNode->sourceId(), codeBlock->lineNumberForVPC(vPC));
    }

    // Calculate an exception handler vPC, unwinding call frames as necessary.

    int scopeDepth;
    Instruction* handlerVPC;

    while (!codeBlock->getHandlerForVPC(vPC, handlerVPC, scopeDepth)) {
        if (!unwindCallFrame(exec, exceptionValue, vPC, codeBlock, k, scopeChain, r))
            return 0;
    }

    // Now unwind the scope chain within the exception handler's call frame.

    ScopeChain sc(scopeChain);
    int scopeDelta = depth(sc) - scopeDepth;
    ASSERT(scopeDelta >= 0);
    while (scopeDelta--)
        sc.pop();
    setScopeChain(exec, scopeChain, sc.node());

    return handlerVPC;
}

JSValue* Machine::execute(ProgramNode* programNode, ExecState* exec, ScopeChainNode* scopeChain, JSObject* thisObj, JSValue** exception)
{
    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(exec);
        return 0;
    }

    CodeBlock* codeBlock = &programNode->byteCode(scopeChain);

    size_t oldSize = m_registerFile.size();
    size_t newSize = oldSize + RegisterFile::CallFrameHeaderSize + codeBlock->numVars + codeBlock->numTemporaries;
    if (!m_registerFile.grow(newSize)) {
        *exception = createStackOverflowError(exec);
        return 0;
    }

    JSGlobalObject* lastGlobalObject = m_registerFile.globalObject();
    JSGlobalObject* globalObject = exec->dynamicGlobalObject();
    globalObject->copyGlobalsTo(m_registerFile);

    Register* callFrame = m_registerFile.base() + oldSize;

    // a 0 codeBlock indicates a built-in caller
    initializeCallFrame(callFrame, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    Register* r = callFrame + RegisterFile::CallFrameHeaderSize + codeBlock->numVars;
    r[codeBlock->thisRegister] = thisObj;

    if (codeBlock->needsFullScopeChain)
        scopeChain = scopeChain->copy();

    ExecState newExec(exec, &m_registerFile, scopeChain, 0);

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(exec, programNode->sourceURL(), programNode->lineNo());

    m_reentryDepth++;
    JSValue* result = privateExecute(Normal, &newExec, &m_registerFile, r, scopeChain, codeBlock, exception);
    m_reentryDepth--;

    MACHINE_SAMPLING_privateExecuteReturned();

    if (*profiler) {
        (*profiler)->didExecute(exec, programNode->sourceURL(), programNode->lineNo());
        if (!m_reentryDepth)
            (*profiler)->didFinishAllExecution(exec);
    }

    if (m_reentryDepth && lastGlobalObject && globalObject != lastGlobalObject)
        lastGlobalObject->copyGlobalsTo(m_registerFile);

    m_registerFile.shrink(oldSize);
    return result;
}

JSValue* Machine::execute(FunctionBodyNode* functionBodyNode, ExecState* exec, JSFunction* function, JSObject* thisObj, const ArgList& args, ScopeChainNode* scopeChain, JSValue** exception)
{
    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(exec);
        return 0;
    }

    int argv = RegisterFile::CallFrameHeaderSize;
    int argc = args.size() + 1; // implicit "this" parameter

    size_t oldSize = m_registerFile.size();
    if (!m_registerFile.grow(oldSize + RegisterFile::CallFrameHeaderSize + argc)) {
        *exception = createStackOverflowError(exec);
        return 0;
    }

    Register* callFrame = m_registerFile.base() + oldSize;

    // put args in place, including "this"
    Register* dst = callFrame + RegisterFile::CallFrameHeaderSize;
    (*dst) = thisObj;

    ArgList::const_iterator end = args.end();
    for (ArgList::const_iterator it = args.begin(); it != end; ++it)
        (*++dst) = *it;

    // a 0 codeBlock indicates a built-in caller
    initializeCallFrame(callFrame, 0, 0, 0, callFrame, 0, argv, argc, 0, function);

    CodeBlock* newCodeBlock = &functionBodyNode->byteCode(scopeChain);
    Register* r = slideRegisterWindowForCall(exec, newCodeBlock, &m_registerFile, m_registerFile.base(), callFrame, argv, argc, *exception);
    if (*exception) {
        m_registerFile.shrink(oldSize);
        return 0;
    }

    scopeChain = scopeChainForCall(exec, functionBodyNode, newCodeBlock, scopeChain, r);

    ExecState newExec(exec, &m_registerFile, scopeChain, callFrame);

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(exec, function);

    m_reentryDepth++;
    JSValue* result = privateExecute(Normal, &newExec, &m_registerFile, r, scopeChain, newCodeBlock, exception);
    m_reentryDepth--;

    MACHINE_SAMPLING_privateExecuteReturned();

    if (*profiler && !m_reentryDepth)
        (*profiler)->didFinishAllExecution(exec);

    m_registerFile.shrink(oldSize);
    return result;
}

JSValue* Machine::execute(EvalNode* evalNode, ExecState* exec, JSObject* thisObj, int registerOffset, ScopeChainNode* scopeChain, JSValue** exception)
{
    if (m_reentryDepth >= MaxReentryDepth) {
        *exception = createStackOverflowError(exec);
        return 0;
    }

    EvalCodeBlock* codeBlock = &evalNode->byteCode(scopeChain);

    JSVariableObject* variableObject;
    for (ScopeChainNode* node = scopeChain; ; node = node->next) {
        ASSERT(node);
        if (node->object->isVariableObject()) {
            variableObject = static_cast<JSVariableObject*>(node->object);
            break;
        }
    }

    const Node::VarStack& varStack = codeBlock->ownerNode->varStack();
    Node::VarStack::const_iterator varStackEnd = varStack.end();
    for (Node::VarStack::const_iterator it = varStack.begin(); it != varStackEnd; ++it) {
        const Identifier& ident = (*it).first;
        if (!variableObject->hasProperty(exec, ident))
            variableObject->put(exec, ident, jsUndefined());
    }

    const Node::FunctionStack& functionStack = codeBlock->ownerNode->functionStack();
    Node::FunctionStack::const_iterator functionStackEnd = functionStack.end();
    for (Node::FunctionStack::const_iterator it = functionStack.begin(); it != functionStackEnd; ++it)
        variableObject->put(exec, (*it)->m_ident, (*it)->makeFunction(exec, scopeChain));

    size_t oldSize = m_registerFile.size();
    size_t newSize = registerOffset + codeBlock->numVars + codeBlock->numTemporaries + RegisterFile::CallFrameHeaderSize;
    if (!m_registerFile.grow(newSize)) {
        *exception = createStackOverflowError(exec);
        return 0;
    }

    Register* callFrame = m_registerFile.base() + registerOffset;

    // a 0 codeBlock indicates a built-in caller
    initializeCallFrame(callFrame, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    Register* r = callFrame + RegisterFile::CallFrameHeaderSize + codeBlock->numVars;
    r[codeBlock->thisRegister] = thisObj;

    if (codeBlock->needsFullScopeChain)
        scopeChain = scopeChain->copy();

    ExecState newExec(exec, &m_registerFile, scopeChain, 0);

    Profiler** profiler = Profiler::enabledProfilerReference();
    if (*profiler)
        (*profiler)->willExecute(exec, evalNode->sourceURL(), evalNode->lineNo());

    m_reentryDepth++;
    JSValue* result = privateExecute(Normal, &newExec, &m_registerFile, r, scopeChain, codeBlock, exception);
    m_reentryDepth--;

    MACHINE_SAMPLING_privateExecuteReturned();

    if (*profiler) {
        (*profiler)->didExecute(exec, evalNode->sourceURL(), evalNode->lineNo());
        if (!m_reentryDepth)
            (*profiler)->didFinishAllExecution(exec);
    }

    m_registerFile.shrink(oldSize);
    return result;
}

ALWAYS_INLINE void Machine::setScopeChain(ExecState* exec, ScopeChainNode*& scopeChain, ScopeChainNode* newScopeChain)
{
    scopeChain = newScopeChain;
    exec->m_scopeChain = newScopeChain;
}

NEVER_INLINE void Machine::debug(ExecState* exec, const Instruction* vPC, const CodeBlock* codeBlock, ScopeChainNode* scopeChain, Register* r)
{
    int debugHookID = (++vPC)->u.operand;
    int firstLine = (++vPC)->u.operand;
    int lastLine = (++vPC)->u.operand;

    Debugger* debugger = exec->dynamicGlobalObject()->debugger();
    if (!debugger)
        return;

    DebuggerCallFrame debuggerCallFrame(exec->dynamicGlobalObject(), codeBlock, scopeChain, r, 0);

    switch((DebugHookID)debugHookID) {
    case DidEnterCallFrame: {
        debugger->callEvent(debuggerCallFrame, codeBlock->ownerNode->sourceId(), firstLine);
        return;
    }
    case WillLeaveCallFrame: {
        debugger->returnEvent(debuggerCallFrame, codeBlock->ownerNode->sourceId(), lastLine);
        return;
    }
    case WillExecuteStatement: {
        debugger->atStatement(debuggerCallFrame, codeBlock->ownerNode->sourceId(), firstLine);
        return;
    }
    case WillExecuteProgram: {
        debugger->willExecuteProgram(debuggerCallFrame, codeBlock->ownerNode->sourceId(), firstLine);
        return;
    }
    case DidExecuteProgram: {
        debugger->didExecuteProgram(debuggerCallFrame, codeBlock->ownerNode->sourceId(), lastLine);
        return;
    }
    case DidReachBreakpoint: {
        debugger->didReachBreakpoint(debuggerCallFrame, codeBlock->ownerNode->sourceId(), lastLine);
        return;
    }
    }
}

void Machine::resetTimeoutCheck()
{
    m_ticksUntilNextTimeoutCheck = initialTickCountThreshold;
    m_timeAtLastCheckTimeout = 0;
    m_timeExecuting = 0;
}

// Returns the current time in milliseconds
// It doesn't matter what "current time" is here, just as long as
// it's possible to measure the time difference correctly.
// In an ideal world this would just be getCurrentUTCTimeWithMicroseconds
// from DateMath.h, but unfortunately there's a slowdown if we use tha.
static inline unsigned getCurrentTime()
{
#if HAVE(SYS_TIME_H)
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#elif PLATFORM(QT)
    QDateTime t = QDateTime::currentDateTime();
    return t.toTime_t() * 1000 + t.time().msec();
#elif PLATFORM(WIN_OS)
    return timeGetTime();
#else
#error Platform does not have getCurrentTime function
#endif
}

// We have to return a JSValue here, gcc seems to produce worse code if 
// we attempt to return a bool
ALWAYS_INLINE JSValue* Machine::checkTimeout(JSGlobalObject* globalObject)
{
    unsigned currentTime = getCurrentTime();
    
    if (!m_timeAtLastCheckTimeout) {
        // Suspicious amount of looping in a script -- start timing it
        m_timeAtLastCheckTimeout = currentTime;
        return 0;
    }
    
    unsigned timeDiff = currentTime - m_timeAtLastCheckTimeout;
    
    if (timeDiff == 0)
        timeDiff = 1;
    
    m_timeExecuting += timeDiff;
    m_timeAtLastCheckTimeout = currentTime;
    
    // Adjust the tick threshold so we get the next checkTimeout call in the interval specified in 
    // preferredScriptCheckTimeInterval
    m_ticksUntilNextTimeoutCheck = static_cast<unsigned>((static_cast<float>(preferredScriptCheckTimeInterval) / timeDiff) * m_ticksUntilNextTimeoutCheck);
    // If the new threshold is 0 reset it to the default threshold. This can happen if the timeDiff is higher than the
    // preferred script check time interval.
    if (m_ticksUntilNextTimeoutCheck == 0)
        m_ticksUntilNextTimeoutCheck = initialTickCountThreshold;
    
    if (m_timeoutTime && m_timeExecuting > m_timeoutTime) {
        if (globalObject->shouldInterruptScript())
            return jsNull(); // Appeasing GCC, all we need is a non-null js value.
        
        resetTimeoutCheck();
    }
    
    return 0;
}

static int32_t offsetForStringSwitch(StringJumpTable& jumpTable, JSValue* scrutinee, int32_t defaultOffset) {
    StringJumpTable::const_iterator end = jumpTable.end();
    UString::Rep* value = static_cast<JSString*>(scrutinee)->value().rep();
    StringJumpTable::const_iterator loc = jumpTable.find(value);
    if (loc == end)
        return defaultOffset;
    return loc->second;
}

JSValue* Machine::privateExecute(ExecutionFlag flag, ExecState* exec, RegisterFile* registerFile, Register* r, ScopeChainNode* scopeChain, CodeBlock* codeBlock, JSValue** exception)
{
    // One-time initialization of our address tables. We have to put this code
    // here because our labels are only in scope inside this function.
    if (flag == InitializeAndReturn) {
        #if HAVE(COMPUTED_GOTO)
            #define ADD_OPCODE(id) m_opcodeTable[id] = &&id;
                FOR_EACH_OPCODE_ID(ADD_OPCODE);
            #undef ADD_OPCODE

            #define ADD_OPCODE_ID(id) m_opcodeIDTable.add(&&id, id);
                FOR_EACH_OPCODE_ID(ADD_OPCODE_ID);
            #undef ADD_OPCODE
            ASSERT(m_opcodeIDTable.size() == numOpcodeIDs);
            op_throw_end_indirect = &&op_throw_end;
            op_call_indirect = &&op_call;
        #endif // HAVE(COMPUTED_GOTO)
        return 0;
    }

    JSValue* exceptionValue = 0;
    Instruction* handlerVPC = 0;

    Register* registerBase = registerFile->base();
    Instruction* vPC = codeBlock->instructions.begin();
    Register* k = codeBlock->registers.data();
    Profiler** enabledProfilerReference = Profiler::enabledProfilerReference();
    unsigned tickCount = m_ticksUntilNextTimeoutCheck + 1;

#define VM_CHECK_EXCEPTION() \
     do { \
        if (UNLIKELY(exec->hadException())) { \
            exceptionValue = exec->exception(); \
            goto vm_throw; \
        } \
    } while (0)

#if DUMP_OPCODE_STATS
    OpcodeStats::resetLastInstruction();
#endif

#define CHECK_FOR_TIMEOUT() \
    if (!--tickCount) { \
        if ((exceptionValue = checkTimeout(exec->dynamicGlobalObject()))) \
            goto vm_throw; \
        tickCount = m_ticksUntilNextTimeoutCheck; \
    }
    
#if HAVE(COMPUTED_GOTO)
    #define NEXT_OPCODE MACHINE_SAMPLING_sample(codeBlock, vPC); goto *vPC->u.opcode
#if DUMP_OPCODE_STATS
    #define BEGIN_OPCODE(opcode) opcode: OpcodeStats::recordInstruction(opcode);
#else
    #define BEGIN_OPCODE(opcode) opcode:
#endif
    NEXT_OPCODE;
#else
    #define NEXT_OPCODE MACHINE_SAMPLING_sample(codeBlock, vPC); continue
#if DUMP_OPCODE_STATS
    #define BEGIN_OPCODE(opcode) case opcode: OpcodeStats::recordInstruction(opcode);
#else
    #define BEGIN_OPCODE(opcode) case opcode:
#endif
    while (1) // iterator loop begins
    switch (vPC->u.opcode)
#endif
    {
    BEGIN_OPCODE(op_load) {
        /* load dst(r) src(k)

           Copies constant src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = k[src];

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_object) {
        /* new_object dst(r)

           Constructs a new empty Object instance using the original
           constructor, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        r[dst] = constructEmptyObject(exec);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_array) {
        /* new_array dst(r) firstArg(r) argCount(n)

           Constructs a new Array instance using the original
           constructor, and puts the result in register dst.
           The array will contain argCount elements with values
           taken from registers starting at register firstArg.
        */
        int dst = (++vPC)->u.operand;
        int firstArg = (++vPC)->u.operand;
        int argCount = (++vPC)->u.operand;
        ArgList args(r + firstArg, argCount);
        r[dst] = constructArray(exec, args);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_regexp) {
        /* new_regexp dst(r) regExp(re)

           Constructs a new RegExp instance using the original
           constructor from regexp regExp, and puts the result in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        int regExp = (++vPC)->u.operand;
        r[dst] = new (exec) RegExpObject(scopeChain->globalObject()->regExpPrototype(), codeBlock->regexps[regExp]);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mov) {
        /* mov dst(r) src(r)

           Copies register src to register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = r[src];

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_eq) {
        /* eq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are equal,
           as with the ECMAScript '==' operator, and puts the result
           as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = jsBoolean(reinterpret_cast<intptr_t>(src1) == reinterpret_cast<intptr_t>(src2));
        else {
            JSValue* result = jsBoolean(equal(exec, src1, src2));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_neq) {
        /* neq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           equal, as with the ECMAScript '!=' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = jsBoolean(reinterpret_cast<intptr_t>(src1) != reinterpret_cast<intptr_t>(src2));
        else {
            JSValue* result = jsBoolean(!equal(exec, src1, src2));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_stricteq) {
        /* stricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are strictly
           equal, as with the ECMAScript '===' operator, and puts the
           result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = jsBoolean(reinterpret_cast<intptr_t>(src1) == reinterpret_cast<intptr_t>(src2));
        else
            r[dst] = jsBoolean(strictEqual(src1, src2));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_nstricteq) {
        /* nstricteq dst(r) src1(r) src2(r)

           Checks whether register src1 and register src2 are not
           strictly equal, as with the ECMAScript '!==' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = jsBoolean(reinterpret_cast<intptr_t>(src1) != reinterpret_cast<intptr_t>(src2));
        else
            r[dst] = jsBoolean(!strictEqual(src1, src2));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_less) {
        /* less dst(r) src1(r) src2(r)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and puts the result as
           a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* result = jsBoolean(jsLess(exec, src1, src2));
        VM_CHECK_EXCEPTION();
        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lesseq) {
        /* lesseq dst(r) src1(r) src2(r)

           Checks whether register src1 is less than or equal to
           register src2, as with the ECMAScript '<=' operator, and
           puts the result as a boolean in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* result = jsBoolean(jsLessEq(exec, src1, src2));
        VM_CHECK_EXCEPTION();
        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pre_inc) {
        /* pre_inc srcDst(r)

           Converts register srcDst to number, adds one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValue* v = r[srcDst].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(v))
            r[srcDst] = JSImmediate::incImmediateNumber(v);
        else {
            JSValue* result = jsNumber(exec, v->toNumber(exec) + 1);
            VM_CHECK_EXCEPTION();
            r[srcDst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pre_dec) {
        /* pre_dec srcDst(r)

           Converts register srcDst to number, subtracts one, and puts the result
           back in register srcDst.
        */
        int srcDst = (++vPC)->u.operand;
        JSValue* v = r[srcDst].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(v))
            r[srcDst] = JSImmediate::decImmediateNumber(v);
        else {
            JSValue* result = jsNumber(exec, v->toNumber(exec) - 1);
            VM_CHECK_EXCEPTION();
            r[srcDst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_post_inc) {
        /* post_inc dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number plus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValue* v = r[srcDst].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(v)) {
            r[dst] = v;
            r[srcDst] = JSImmediate::incImmediateNumber(v);
        } else {
            JSValue* number = r[srcDst].jsValue(exec)->toJSNumber(exec);
            VM_CHECK_EXCEPTION();
            r[dst] = number;
            r[srcDst] = jsNumber(exec, number->uncheckedGetNumber() + 1);
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_post_dec) {
        /* post_dec dst(r) srcDst(r)

           Converts register srcDst to number. The number itself is
           written to register dst, and the number minus one is written
           back to register srcDst.
        */
        int dst = (++vPC)->u.operand;
        int srcDst = (++vPC)->u.operand;
        JSValue* v = r[srcDst].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(v)) {
            r[dst] = v;
            r[srcDst] = JSImmediate::decImmediateNumber(v);
        } else {
            JSValue* number = r[srcDst].jsValue(exec)->toJSNumber(exec);
            VM_CHECK_EXCEPTION();
            r[dst] = number;
            r[srcDst] = jsNumber(exec, number->uncheckedGetNumber() - 1);
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_to_jsnumber) {
        /* to_jsnumber dst(r) src(r)

           Converts register src to number, and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValue* result = r[src].jsValue(exec)->toJSNumber(exec);
        VM_CHECK_EXCEPTION();

        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_negate) {
        /* negate dst(r) src(r)

           Converts register src to number, negates it, and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValue* result = jsNumber(exec, -r[src].jsValue(exec)->toNumber(exec));
        VM_CHECK_EXCEPTION();
        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_add) {
        /* add dst(r) src1(r) src2(r)

           Adds register src1 and register src2, and puts the result
           in register dst. (JS add may be string concatenation or
           numeric add, depending on the types of the operands.)
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(src1) && JSImmediate::canDoFastAdditiveOperations(src2))
            r[dst] = JSImmediate::addImmediateNumbers(src1, src2);
        else {
            JSValue* result = jsAdd(exec, src1, src2);
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mul) {
        /* mul dst(r) src1(r) src2(r)

           Multiplies register src1 and register src2 (converted to
           numbers), and puts the product in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* result = jsNumber(exec, src1->toNumber(exec) * src2->toNumber(exec));
        VM_CHECK_EXCEPTION();
        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_div) {
        /* div dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by the
           register divisor (converted to number), and puts the
           quotient in register dst.
        */
        int dst = (++vPC)->u.operand;
        int dividend = (++vPC)->u.operand;
        int divisor = (++vPC)->u.operand;
        JSValue* result = jsNumber(exec, r[dividend].jsValue(exec)->toNumber(exec) / r[divisor].jsValue(exec)->toNumber(exec));
        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_mod) {
        /* mod dst(r) dividend(r) divisor(r)

           Divides register dividend (converted to number) by
           register divisor (converted to number), and puts the
           remainder in register dst.
        */
        int dst = (++vPC)->u.operand;
        int dividend = (++vPC)->u.operand;
        int divisor = (++vPC)->u.operand;
        double d = r[dividend].jsValue(exec)->toNumber(exec);
        JSValue* result = jsNumber(exec, fmod(d, r[divisor].jsValue(exec)->toNumber(exec)));
        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_sub) {
        /* sub dst(r) src1(r) src2(r)

           Subtracts register src2 (converted to number) from register
           src1 (converted to number), and puts the difference in
           register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::canDoFastAdditiveOperations(src1) && JSImmediate::canDoFastAdditiveOperations(src2))
            r[dst] = JSImmediate::subImmediateNumbers(src1, src2);
        else {
            JSValue* result = jsNumber(exec, src1->toNumber(exec) - src2->toNumber(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_lshift) {
        /* lshift dst(r) val(r) shift(r)

           Performs left shift of register val (converted to int32) by
           register shift (converted to uint32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* val = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* shift = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(val, shift))
            r[dst] = jsNumber(exec, JSImmediate::getTruncatedInt32(val) << (JSImmediate::toTruncatedUInt32(shift) & 0x1f));
        else {
            JSValue* result = jsNumber(exec, (val->toInt32(exec)) << (shift->toUInt32(exec) & 0x1f));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_rshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs arithmetic right shift of register val (converted
           to int32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* val = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* shift = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(val, shift))
            r[dst] = JSImmediate::rightShiftImmediateNumbers(val, shift);
        else {
            JSValue* result = jsNumber(exec, (val->toInt32(exec)) >> (shift->toUInt32(exec) & 0x1f));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_urshift) {
        /* rshift dst(r) val(r) shift(r)

           Performs logical right shift of register val (converted
           to uint32) by register shift (converted to
           uint32), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* val = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* shift = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(val, shift) && !JSImmediate::isNegative(val))
            r[dst] = JSImmediate::rightShiftImmediateNumbers(val, shift);
        else {
            JSValue* result = jsNumber(exec, (val->toUInt32(exec)) >> (shift->toUInt32(exec) & 0x1f));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitand) {
        /* bitand dst(r) src1(r) src2(r)

           Computes bitwise AND of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = JSImmediate::andImmediateNumbers(src1, src2);
        else {
            JSValue* result = jsNumber(exec, src1->toInt32(exec) & src2->toInt32(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitxor) {
        /* bitxor dst(r) src1(r) src2(r)

           Computes bitwise XOR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the result
           in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = JSImmediate::xorImmediateNumbers(src1, src2);
        else {
            JSValue* result = jsNumber(exec, src1->toInt32(exec) ^ src2->toInt32(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitor) {
        /* bitor dst(r) src1(r) src2(r)

           Computes bitwise OR of register src1 (converted to int32)
           and register src2 (converted to int32), and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        if (JSImmediate::areBothImmediateNumbers(src1, src2))
            r[dst] = JSImmediate::orImmediateNumbers(src1, src2);
        else {
            JSValue* result = jsNumber(exec, src1->toInt32(exec) | src2->toInt32(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = result;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_bitnot) {
        /* bitnot dst(r) src(r)

           Computes bitwise NOT of register src1 (converted to int32),
           and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValue* result = jsNumber(exec, ~r[src].jsValue(exec)->toInt32(exec));
        VM_CHECK_EXCEPTION();
        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_not) {
        /* not dst(r) src1(r) src2(r)

           Computes logical NOT of register src1 (converted to
           boolean), and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        JSValue* result = jsBoolean(!r[src].jsValue(exec)->toBoolean(exec));
        VM_CHECK_EXCEPTION();
        r[dst] = result;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_instanceof) {
        /* instanceof dst(r) value(r) constructor(r)

           Tests whether register value is an instance of register
           constructor, and puts the boolean result in register dst.

           Raises an exception if register constructor is not an
           object.
        */
        int dst = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        JSValue* baseVal = r[base].jsValue(exec);

        if (isNotObject(exec, true, codeBlock, vPC, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = static_cast<JSObject*>(baseVal);
        r[dst] = jsBoolean(baseObj->implementsHasInstance() ? baseObj->hasInstance(exec, r[value].jsValue(exec)) : false);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_typeof) {
        /* typeof dst(r) src(r)

           Determines the type string for src according to ECMAScript
           rules, and puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int src = (++vPC)->u.operand;
        r[dst] = jsTypeStringForValue(exec, r[src].jsValue(exec));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_in) {
        /* in dst(r) property(r) base(r)

           Tests whether register base has a property named register
           property, and puts the boolean result in register dst.

           Raises an exception if register constructor is not an
           object.
        */
        int dst = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        JSValue* baseVal = r[base].jsValue(exec);
        if (isNotObject(exec, false, codeBlock, vPC, baseVal, exceptionValue))
            goto vm_throw;

        JSObject* baseObj = static_cast<JSObject*>(baseVal);

        JSValue* propName = r[property].jsValue(exec);

        uint32_t i;
        if (propName->getUInt32(i))
            r[dst] = jsBoolean(baseObj->hasProperty(exec, i));
        else {
            Identifier property(exec, propName->toString(exec));
            VM_CHECK_EXCEPTION();
            r[dst] = jsBoolean(baseObj->hasProperty(exec, property));
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve) {
        /* resolve dst(r) property(id)

           Looks up the property named by identifier property in the
           scope chain, and writes the resulting value to register
           dst. If the property is not found, raises an exception.
        */
        if (UNLIKELY(!resolve(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;

        vPC += 3;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_skip) {
        /* resolve_skip dst(r) property(id) skip(n)

         Looks up the property named by identifier property in the
         scope chain skipping the top 'skip' levels, and writes the resulting
         value to register dst. If the property is not found, raises an exception.
         */
        if (UNLIKELY(!resolve_skip(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;

        vPC += 4;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_scoped_var) {
        /* get_scoped_var dst(r) index(n) skip(n)

         Loads the contents of the index-th local from the scope skip nodes from
         the top of the scope chain, and places it in register dst
         */
        int dst = (++vPC)->u.operand;
        int index = (++vPC)->u.operand;
        int skip = (++vPC)->u.operand + codeBlock->needsFullScopeChain;

        ScopeChainIterator iter = scopeChain->begin();
        ScopeChainIterator end = scopeChain->end();
        ASSERT(iter != end);
        while (skip--) {
            ++iter;
            ASSERT(iter != end);
        }

        ASSERT((*iter)->isVariableObject());
        JSVariableObject* scope = static_cast<JSVariableObject*>(*iter);
        r[dst] = scope->registerAt(index);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_scoped_var) {
        /* put_scoped_var index(n) skip(n) value(r)

         */
        int index = (++vPC)->u.operand;
        int skip = (++vPC)->u.operand + codeBlock->needsFullScopeChain;
        int value = (++vPC)->u.operand;

        ScopeChainIterator iter = scopeChain->begin();
        ScopeChainIterator end = scopeChain->end();
        ASSERT(iter != end);
        while (skip--) {
            ++iter;
            ASSERT(iter != end);
        }

        ASSERT((*iter)->isVariableObject());
        JSVariableObject* scope = static_cast<JSVariableObject*>(*iter);
        scope->registerAt(index) = r[value].jsValue(exec);
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_base) {
        /* resolve_base dst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes it to
           register dst. If none is found, the outermost scope (which
           will be the global object) is stored in register dst.
        */
        resolveBase(exec, vPC, r, scopeChain, codeBlock);

        vPC += 3;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_with_base) {
        /* resolve_with_base baseDst(r) propDst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes it to
           register srcDst, and the retrieved property value to register
           propDst. If the property is not found, raises an exception.

           This is more efficient than doing resolve_base followed by
           resolve, or resolve_base followed by get_by_id, as it
           avoids duplicate hash lookups.
        */
        if (UNLIKELY(!resolveBaseAndProperty(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;

        vPC += 4;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_resolve_func) {
        /* resolve_func baseDst(r) funcDst(r) property(id)

           Searches the scope chain for an object containing
           identifier property, and if one is found, writes the
           appropriate object to use as "this" when calling its
           properties to register baseDst; and the retrieved property
           value to register propDst. If the property is not found,
           raises an exception.

           This differs from resolve_with_base, because the
           global this value will be substituted for activations or
           the global object, which is the right behavior for function
           calls but not for other property lookup.
        */
        if (UNLIKELY(!resolveBaseAndFunc(exec, vPC, r, scopeChain, codeBlock, exceptionValue)))
            goto vm_throw;

        vPC += 4;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_id) {
        /* get_by_id dst(r) base(r) property(id)

           Converts register base to Object, gets the property
           named by identifier property from the object, and puts the
           result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        Identifier& ident = codeBlock->identifiers[property];
        JSValue *result = r[base].jsValue(exec)->get(exec, ident);
        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_id) {
        /* put_by_id base(r) property(id) value(r)

           Sets register value on register base as the property named
           by identifier property. Base is converted to object first.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        Identifier& ident = codeBlock->identifiers[property];
        r[base].jsValue(exec)->put(exec, ident, r[value].jsValue(exec));

        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_del_by_id) {
        /* del_by_id dst(r) base(r) property(id)

           Converts register base to Object, deletes the property
           named by identifier property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = r[base].jsValue(exec)->toObject(exec);

        Identifier& ident = codeBlock->identifiers[property];
        JSValue* result = jsBoolean(baseObj->deleteProperty(exec, ident));
        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_by_val) {
        /* get_by_val dst(r) base(r) property(r)

           Converts register base to Object, gets the property named
           by register property from the object, and puts the result
           in register dst. property is nominally converted to string
           but numbers are treated more efficiently.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSValue* baseValue = r[base].jsValue(exec);
        JSValue* subscript = r[property].jsValue(exec);

        JSValue* result;
        unsigned i;

        bool isUInt32 = JSImmediate::getUInt32(subscript, i);
        if (LIKELY(isUInt32)) {
            if (isJSArray(baseValue)) {
                JSArray* jsArray = static_cast<JSArray*>(baseValue);
                if (jsArray->canGetIndex(i))
                    result = jsArray->getIndex(i);
                else
                    result = jsArray->JSArray::get(exec, i);
            } else if (isJSString(baseValue) && static_cast<JSString*>(baseValue)->canGetIndex(i))
                result = static_cast<JSString*>(baseValue)->getIndex(exec, i);
            else
                result = baseValue->get(exec, i);
        } else {
            Identifier property(exec, subscript->toString(exec));
            result = baseValue->get(exec, property);
        }

        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_val) {
        /* put_by_val base(r) property(r) value(r)

           Sets register value on register base as the property named
           by register property. Base is converted to object
           first. register property is nominally converted to string
           but numbers are treated more efficiently.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        JSValue* baseValue = r[base].jsValue(exec);
        JSValue* subscript = r[property].jsValue(exec);

        unsigned i;

        bool isUInt32 = JSImmediate::getUInt32(subscript, i);
        if (LIKELY(isUInt32)) {
            if (isJSArray(baseValue)) {
                JSArray* jsArray = static_cast<JSArray*>(baseValue);
                if (jsArray->canSetIndex(i))
                    jsArray->setIndex(i, r[value].jsValue(exec));
                else
                    jsArray->JSArray::put(exec, i, r[value].jsValue(exec));
            } else
                baseValue->put(exec, i, r[value].jsValue(exec));
        } else {
            Identifier property(exec, subscript->toString(exec));
            if (!exec->hadException()) // Don't put to an object if toString threw an exception.
                baseValue->put(exec, property, r[value].jsValue(exec));
        }

        VM_CHECK_EXCEPTION();
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_del_by_val) {
        /* del_by_val dst(r) base(r) property(r)

           Converts register base to Object, deletes the property
           named by register property from the object, and writes a
           boolean indicating success (if true) or failure (if false)
           to register dst.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;

        JSObject* baseObj = r[base].jsValue(exec)->toObject(exec); // may throw

        JSValue* subscript = r[property].jsValue(exec);
        JSValue* result;
        uint32_t i;
        if (subscript->getUInt32(i))
            result = jsBoolean(baseObj->deleteProperty(exec, i));
        else {
            VM_CHECK_EXCEPTION();
            Identifier property(exec, subscript->toString(exec));
            VM_CHECK_EXCEPTION();
            result = jsBoolean(baseObj->deleteProperty(exec, property));
        }

        VM_CHECK_EXCEPTION();
        r[dst] = result;
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_by_index) {
        /* put_by_index base(r) property(n) value(r)

           Sets register value on register base as the property named
           by the immediate number property. Base is converted to
           object first.

           Unlike many opcodes, this one does not write any output to
           the register file.

           This opcode is mainly used to initialize array literals.
        */
        int base = (++vPC)->u.operand;
        unsigned property = (++vPC)->u.operand;
        int value = (++vPC)->u.operand;

        r[base].jsValue(exec)->put(exec, property, r[value].jsValue(exec));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop) {
        /* loop target(offset)
         
           Jumps unconditionally to offset target from the current
           instruction.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
#if DUMP_OPCODE_STATS
        OpcodeStats::resetLastInstruction();
#endif
        int target = (++vPC)->u.operand;
        CHECK_FOR_TIMEOUT();
        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp) {
        /* jmp target(offset)

           Jumps unconditionally to offset target from the current
           instruction.
        */
#if DUMP_OPCODE_STATS
        OpcodeStats::resetLastInstruction();
#endif
        int target = (++vPC)->u.operand;

        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop_if_true) {
        /* loop_if_true cond(r) target(offset)
         
           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (r[cond].jsValue(exec)->toBoolean(exec)) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_OPCODE;
        }
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jtrue) {
        /* jtrue cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as true.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (r[cond].jsValue(exec)->toBoolean(exec)) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jfalse) {
        /* jfalse cond(r) target(offset)

           Jumps to offset target from the current instruction, if and
           only if register cond converts to boolean as false.
        */
        int cond = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        if (!r[cond].jsValue(exec)->toBoolean(exec)) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_loop_if_less) {
        /* loop_if_less src1(r) src2(r) target(offset)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and then jumps to offset
           target from the current instruction, if and only if the 
           result of the comparison is true.

           Additionally this loop instruction may terminate JS execution is
           the JS timeout is reached.
         */
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        int target = (++vPC)->u.operand;
        
        bool result = jsLess(exec, src1, src2);
        VM_CHECK_EXCEPTION();
        
        if (result) {
            vPC += target;
            CHECK_FOR_TIMEOUT();
            NEXT_OPCODE;
        }
        
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jless) {
        /* jless src1(r) src2(r) target(offset)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and then jumps to offset
           target from the current instruction, if and only if the 
           result of the comparison is true.
        */
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        int target = (++vPC)->u.operand;

        bool result = jsLess(exec, src1, src2);
        VM_CHECK_EXCEPTION();
        
        if (result) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jnless) {
        /* jnless src1(r) src2(r) target(offset)

           Checks whether register src1 is less than register src2, as
           with the ECMAScript '<' operator, and then jumps to offset
           target from the current instruction, if and only if the 
           result of the comparison is false.
        */
        JSValue* src1 = r[(++vPC)->u.operand].jsValue(exec);
        JSValue* src2 = r[(++vPC)->u.operand].jsValue(exec);
        int target = (++vPC)->u.operand;

        bool result = jsLess(exec, src1, src2);
        VM_CHECK_EXCEPTION();
        
        if (!result) {
            vPC += target;
            NEXT_OPCODE;
        }

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_switch_imm) {
        /* switch_imm tableIndex(n) defaultOffset(offset) scrutinee(r)

           Performs a range checked switch on the scrutinee value, using
           the tableIndex-th immediate switch jump table.  If the scrutinee value
           is an immediate number in the range covered by the referenced jump
           table, and the value at jumpTable[scrutinee value] is non-zero, then
           that value is used as the jump offset, otherwise defaultOffset is used.
         */
        int tableIndex = (++vPC)->u.operand;
        int defaultOffset = (++vPC)->u.operand;
        JSValue* scrutinee = r[(++vPC)->u.operand].jsValue(exec);
        if (!JSImmediate::isNumber(scrutinee))
            vPC += defaultOffset;
        else {
            int32_t value = JSImmediate::getTruncatedInt32(scrutinee);
            vPC += codeBlock->immediateSwitchJumpTables[tableIndex].offsetForValue(value, defaultOffset);
        }
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_switch_char) {
        /* switch_char tableIndex(n) defaultOffset(offset) scrutinee(r)

           Performs a range checked switch on the scrutinee value, using
           the tableIndex-th character switch jump table.  If the scrutinee value
           is a single character string in the range covered by the referenced jump
           table, and the value at jumpTable[scrutinee value] is non-zero, then
           that value is used as the jump offset, otherwise defaultOffset is used.
         */
        int tableIndex = (++vPC)->u.operand;
        int defaultOffset = (++vPC)->u.operand;
        JSValue* scrutinee = r[(++vPC)->u.operand].jsValue(exec);
        if (scrutinee->type() != StringType)
            vPC += defaultOffset;
        else {
            UString::Rep* value = static_cast<JSString*>(scrutinee)->value().rep();
            if (value->size() != 1)
                vPC += defaultOffset;
            else
                vPC += codeBlock->characterSwitchJumpTables[tableIndex].offsetForValue(value->data()[0], defaultOffset);
        }
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_switch_string) {
        /* switch_string tableIndex(n) defaultOffset(offset) scrutinee(r)

           Performs a sparse hashmap based switch on the value in the scrutinee
           register, using the tableIndex-th string switch jump table.  If the 
           scrutinee value is a string that exists as a key in the referenced 
           jump table, then the value associated with the string is used as the 
           jump offset, otherwise defaultOffset is used.
         */
        int tableIndex = (++vPC)->u.operand;
        int defaultOffset = (++vPC)->u.operand;
        JSValue* scrutinee = r[(++vPC)->u.operand].jsValue(exec);
        if (scrutinee->type() != StringType)
            vPC += defaultOffset;
        else 
            vPC += offsetForStringSwitch(codeBlock->stringSwitchJumpTables[tableIndex], scrutinee, defaultOffset);
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func) {
        /* new_func dst(r) func(f)

           Constructs a new Function instance from function func and
           the current scope chain using the original Function
           constructor, using the rules for function declarations, and
           puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;

        r[dst] = codeBlock->functions[func]->makeFunction(exec, scopeChain);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_func_exp) {
        /* new_func_exp dst(r) func(f)

           Constructs a new Function instance from function func and
           the current scope chain using the original Function
           constructor, using the rules for function expressions, and
           puts the result in register dst.
        */
        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;

        r[dst] = codeBlock->functionExpressions[func]->makeFunction(exec, scopeChain);

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_call_eval) {
        /* call_eval dst(r) func(r) thisVal(r) firstArg(r) argCount(n)

           Call a function named "eval" with no explicit "this" value
           (which may therefore be the eval operator). If register
           thisVal is the global object, and register func contains
           that global object's original global eval function, then
           perform the eval operator in local scope (interpreting
           the argument registers as for the "call"
           opcode). Otherwise, act exactly as the "call" opcode would.
         */

        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;
        int thisVal = (++vPC)->u.operand;
        int firstArg = (++vPC)->u.operand;
        int argCount = (++vPC)->u.operand;

        JSValue* funcVal = r[func].jsValue(exec);
        JSValue* baseVal = r[thisVal].jsValue(exec);

        if (baseVal == scopeChain->globalObject() && funcVal == scopeChain->globalObject()->evalFunction()) {
            JSObject* thisObject = static_cast<JSObject*>(r[codeBlock->thisRegister].jsValue(exec));
            JSValue* result = callEval(exec, thisObject, scopeChain, registerFile, r, firstArg, argCount, exceptionValue);
            if (exceptionValue)
                goto vm_throw;

            r[dst] = result;

            ++vPC;
            NEXT_OPCODE;
        }

        // We didn't find the blessed version of eval, so reset vPC and process
        // this instruction as a normal function call, supplying the proper 'this'
        // value.
        vPC -= 5;
        r[thisVal] = baseVal->toThisObject(exec);

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *op_call_indirect; // indirect goto -> op_call
#endif
        // fall through to op_call
    }
    BEGIN_OPCODE(op_call) {
        /* call dst(r) func(r) thisVal(r) firstArg(r) argCount(n)

           Perform a function call. Specifically, call register func
           with a "this" value of register thisVal, and put the result
           in register dst.

           The arguments start at register firstArg and go up to
           argCount, but the "this" value is considered an implicit
           first argument, so the argCount should be one greater than
           the number of explicit arguments passed, and the register
           after firstArg should contain the actual first
           argument. This opcode will copy from the thisVal register
           to the firstArg register, unless the register index of
           thisVal is the special missing this object marker, which is
           2^31-1; in that case, the global object will be used as the
           "this" value.

           If func is a native code function, then this opcode calls
           it and returns the value immediately. 

           But if it is a JS function, then the current scope chain
           and code block is set to the function's, and we slide the
           register window so that the arguments would form the first
           few local registers of the called function's register
           window. In addition, a call frame header is written
           immediately before the arguments; see the call frame
           documentation for an explanation of how many registers a
           call frame takes and what they contain. That many registers
           before the firstArg register will be overwritten by the
           call. In addition, any registers higher than firstArg +
           argCount may be overwritten. Once this setup is complete,
           execution continues from the called function's first
           argument, and does not return until a "ret" opcode is
           encountered.
         */

        int dst = (++vPC)->u.operand;
        int func = (++vPC)->u.operand;
        int thisVal = (++vPC)->u.operand;
        int firstArg = (++vPC)->u.operand;
        int argCount = (++vPC)->u.operand;

        JSValue* v = r[func].jsValue(exec);

        CallData callData;
        CallType callType = v->getCallData(callData);

        if (callType == CallTypeJS) {
            if (*enabledProfilerReference)
                (*enabledProfilerReference)->willExecute(exec, static_cast<JSObject*>(v));

            ScopeChainNode* callDataScopeChain = callData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = callData.js.functionBody;
            CodeBlock* newCodeBlock = &functionBodyNode->byteCode(callDataScopeChain);

            r[firstArg] = thisVal == missingThisObjectMarker() ? exec->globalThisValue() : r[thisVal].jsValue(exec);

            Register* callFrame = r + firstArg - RegisterFile::CallFrameHeaderSize;
            initializeCallFrame(callFrame, codeBlock, vPC, scopeChain, r, dst, firstArg, argCount, 0, v);
            exec->m_callFrame = callFrame;

            r = slideRegisterWindowForCall(exec, newCodeBlock, registerFile, registerBase, r, firstArg, argCount, exceptionValue);
            if (UNLIKELY(exceptionValue != 0))
                goto vm_throw;

            codeBlock = newCodeBlock;
            setScopeChain(exec, scopeChain, scopeChainForCall(exec, functionBodyNode, codeBlock, callDataScopeChain, r));
            k = codeBlock->registers.data();
            vPC = codeBlock->instructions.begin();

#if DUMP_OPCODE_STATS
            OpcodeStats::resetLastInstruction();
#endif
            
            NEXT_OPCODE;
        }

        if (callType == CallTypeHost) {
            if (*enabledProfilerReference)
                (*enabledProfilerReference)->willExecute(exec, static_cast<JSObject*>(v));

            JSValue* thisValue = thisVal == missingThisObjectMarker() ? exec->globalThisValue() : r[thisVal].jsValue(exec);
            ArgList args(r + firstArg + 1, argCount - 1);

            MACHINE_SAMPLING_callingHostFunction();

            JSValue* returnValue = callData.native.function(exec, static_cast<JSObject*>(v), thisValue, args);
            VM_CHECK_EXCEPTION();

            r[dst] = returnValue;

            if (*enabledProfilerReference)
                (*enabledProfilerReference)->didExecute(exec, static_cast<JSObject*>(v));

            ++vPC;
            NEXT_OPCODE;
        }

        ASSERT(callType == CallTypeNone);

        exceptionValue = createNotAFunctionError(exec, v, vPC, codeBlock);
        goto vm_throw;
    }
    BEGIN_OPCODE(op_ret) {
        /* ret result(r)
           
           Return register result as the return value of the current
           function call, writing it into the caller's expected return
           value register. In addition, unwind one call frame and
           restore the scope chain, code block instruction pointer and
           register base to those of the calling function.
        */
           
        int result = (++vPC)->u.operand;

        Register* callFrame = r - codeBlock->numLocals - RegisterFile::CallFrameHeaderSize;
        if (JSActivation* activation = static_cast<JSActivation*>(callFrame[RegisterFile::OptionalCalleeActivation].jsValue(exec))) {
            ASSERT(!codeBlock->needsFullScopeChain || scopeChain->object == activation);
            ASSERT(activation->isActivationObject());
            activation->copyRegisters();
        }

        if (*enabledProfilerReference)
            (*enabledProfilerReference)->didExecute(exec, static_cast<JSObject*>(callFrame[RegisterFile::Callee].jsValue(exec)));

        if (codeBlock->needsFullScopeChain)
            scopeChain->deref();

        JSValue* returnValue = r[result].jsValue(exec);
        if (callFrame[RegisterFile::CalledAsConstructor].i() && !returnValue->isObject()) {
            JSValue* thisObject = callFrame[RegisterFile::CallFrameHeaderSize].jsValue(exec);
            returnValue = thisObject;
        }

        codeBlock = callFrame[RegisterFile::CallerCodeBlock].codeBlock();
        if (!codeBlock)
            return returnValue;

        k = codeBlock->registers.data();
        vPC = callFrame[RegisterFile::ReturnVPC].vPC();
        setScopeChain(exec, scopeChain, callFrame[RegisterFile::CallerScopeChain].scopeChain());
        r = callFrame[RegisterFile::CallerRegisters].r();
        exec->m_callFrame = r - codeBlock->numLocals - RegisterFile::CallFrameHeaderSize;
        int dst = callFrame[RegisterFile::ReturnValueRegister].i();
        r[dst] = returnValue;

        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_construct) {
        /* construct dst(r) constr(r) firstArg(r) argCount(n)

           Invoke register "constr" as a constructor. For JS
           functions, the calling convention is exactly as for the
           "call" opcode, except that the "this" value is a newly
           created Object. For native constructors, a null "this"
           value is passed. In either case, the firstArg and argCount
           registers are interpreted as for the "call" opcode.
        */

        int dst = (++vPC)->u.operand;
        int constr = (++vPC)->u.operand;
        int firstArg = (++vPC)->u.operand;
        int argCount = (++vPC)->u.operand;

        JSValue* constrVal = r[constr].jsValue(exec);

        ConstructData constructData;
        ConstructType constructType = constrVal->getConstructData(constructData);

        // Removing this line of code causes a measurable regression on squirrelfish.
        JSObject* constructor = static_cast<JSObject*>(constrVal);

        if (constructType == ConstructTypeJS) {
            if (*enabledProfilerReference)
                (*enabledProfilerReference)->willExecute(exec, constructor);

            JSObject* prototype;
            JSValue* p = constructor->get(exec, exec->propertyNames().prototype);
            if (p->isObject())
                prototype = static_cast<JSObject*>(p);
            else
                prototype = scopeChain->globalObject()->objectPrototype();
            JSObject* newObject = new (exec) JSObject(prototype);

            ScopeChainNode* callDataScopeChain = constructData.js.scopeChain;
            FunctionBodyNode* functionBodyNode = constructData.js.functionBody;
            CodeBlock* newCodeBlock = &functionBodyNode->byteCode(callDataScopeChain);

            r[firstArg] = newObject; // "this" value

            Register* callFrame = r + firstArg - RegisterFile::CallFrameHeaderSize;
            initializeCallFrame(callFrame, codeBlock, vPC, scopeChain, r, dst, firstArg, argCount, 1, constructor);
            exec->m_callFrame = callFrame;

            r = slideRegisterWindowForCall(exec, newCodeBlock, registerFile, registerBase, r, firstArg, argCount, exceptionValue);
            if (exceptionValue)
                goto vm_throw;

            codeBlock = newCodeBlock;
            setScopeChain(exec, scopeChain, scopeChainForCall(exec, functionBodyNode, codeBlock, callDataScopeChain, r));
            k = codeBlock->registers.data();
            vPC = codeBlock->instructions.begin();

            NEXT_OPCODE;
        }

        if (constructType == ConstructTypeHost) {
            if (*enabledProfilerReference)
                (*enabledProfilerReference)->willExecute(exec, constructor);

            ArgList args(r + firstArg + 1, argCount - 1);

            MACHINE_SAMPLING_callingHostFunction();

            JSValue* returnValue = constructData.native.function(exec, constructor, args);

            VM_CHECK_EXCEPTION();
            r[dst] = returnValue;

            if (*enabledProfilerReference)
                (*enabledProfilerReference)->didExecute(exec, constructor);

            ++vPC;
            NEXT_OPCODE;
        }

        ASSERT(constructType == ConstructTypeNone);

        exceptionValue = createNotAConstructorError(exec, constrVal, vPC, codeBlock);
        goto vm_throw;
    }
    BEGIN_OPCODE(op_push_scope) {
        /* push_scope scope(r)

           Converts register scope to object, and pushes it onto the top
           of the current scope chain.
        */
        int scope = (++vPC)->u.operand;
        JSValue* v = r[scope].jsValue(exec);
        JSObject* o = v->toObject(exec);
        VM_CHECK_EXCEPTION();

        setScopeChain(exec, scopeChain, scopeChain->push(o));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_pop_scope) {
        /* pop_scope

           Removes the top item from the current scope chain.
        */
        setScopeChain(exec, scopeChain, scopeChain->pop());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_get_pnames) {
        /* get_pnames dst(r) base(r)

           Creates a property name list for register base and puts it
           in register dst. This is not a true JavaScript value, just
           a synthetic value used to keep the iteration state in a
           register.
        */
        int dst = (++vPC)->u.operand;
        int base = (++vPC)->u.operand;

        r[dst] = JSPropertyNameIterator::create(exec, r[base].jsValue(exec));
        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_next_pname) {
        /* next_pname dst(r) iter(r) target(offset)

           Tries to copies the next name from property name list in
           register iter. If there are names left, then copies one to
           register dst, and jumps to offset target. If there are none
           left, invalidates the iterator and continues to the next
           instruction.
        */
        int dst = (++vPC)->u.operand;
        int iter = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;

        JSPropertyNameIterator* it = r[iter].jsPropertyNameIterator();
        if (JSValue* temp = it->next(exec)) {
            CHECK_FOR_TIMEOUT();
            r[dst] = temp;
            vPC += target;
            NEXT_OPCODE;
        }
        it->invalidate();

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jmp_scopes) {
        /* jmp_scopes count(n) target(offset)

           Removes the a number of items from the current scope chain
           specified by immediate number count, then jumps to offset
           target.
        */
        int count = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;

        ScopeChainNode* tmp = scopeChain;
        while (count--)
            tmp = tmp->pop();
        setScopeChain(exec, scopeChain, tmp);

        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_catch) {
        /* catch ex(r)

           Retrieves the VMs current exception and puts it in register
           ex. This is only valid after an exception has been raised,
           and usually forms the beginning of an exception handler.
        */
        ASSERT(exceptionValue);
        ASSERT(!exec->hadException());
        int ex = (++vPC)->u.operand;
        r[ex] = exceptionValue;
        exceptionValue = 0;

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_throw) {
        /* throw ex(r)

           Throws register ex as an exception. This involves three
           steps: first, it is set as the current exception in the
           VM's internal state, then the stack is unwound until an
           exception handler or a native code boundary is found, and
           then control resumes at the exception handler if any or
           else the script returns control to the nearest native caller.
        */

        int ex = (++vPC)->u.operand;
        exceptionValue = r[ex].jsValue(exec);

        handlerVPC = throwException(exec, exceptionValue, vPC, codeBlock, k, scopeChain, r, true);
        if (!handlerVPC) {
            *exception = exceptionValue;
            return jsNull();
        }

#if HAVE(COMPUTED_GOTO)
        // Hack around gcc performance quirk by performing an indirect goto
        // in order to set the vPC -- attempting to do so directly results in a
        // significant regression.
        goto *op_throw_end_indirect; // indirect goto -> op_throw_end
    }
    op_throw_end: {
#endif

        vPC = handlerVPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_new_error) {
        /* new_error dst(r) type(n) message(k)

           Constructs a new Error instance using the original
           constructor, using immediate number n as the type and
           constant message as the message string. The result is
           written to register dst.
        */
        int dst = (++vPC)->u.operand;
        int type = (++vPC)->u.operand;
        int message = (++vPC)->u.operand;

        r[dst] = Error::create(exec, (ErrorType)type, k[message].jsValue(exec)->toString(exec), codeBlock->lineNumberForVPC(vPC), codeBlock->ownerNode->sourceId(), codeBlock->ownerNode->sourceURL());

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_end) {
        /* end result(r)
           
           Return register result as the value of a global or eval
           program. Return control to the calling native code.
        */

        if (codeBlock->needsFullScopeChain) {
            ASSERT(scopeChain->refCount > 1);
            scopeChain->deref();
        }
        int result = (++vPC)->u.operand;
        return r[result].jsValue(exec);
    }
    BEGIN_OPCODE(op_put_getter) {
        /* put_getter base(r) property(id) function(r)

           Sets register function on register base as the getter named
           by identifier property. Base and function are assumed to be
           objects as this op should only be used for getters defined
           in object literal form.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int function = (++vPC)->u.operand;

        ASSERT(r[base].jsValue(exec)->isObject());
        JSObject* baseObj = static_cast<JSObject*>(r[base].jsValue(exec));
        Identifier& ident = codeBlock->identifiers[property];
        ASSERT(r[function].jsValue(exec)->isObject());
        baseObj->defineGetter(exec, ident, static_cast<JSObject*>(r[function].jsValue(exec)));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_put_setter) {
        /* put_setter base(r) property(id) function(r)

           Sets register function on register base as the setter named
           by identifier property. Base and function are assumed to be
           objects as this op should only be used for setters defined
           in object literal form.

           Unlike many opcodes, this one does not write any output to
           the register file.
        */
        int base = (++vPC)->u.operand;
        int property = (++vPC)->u.operand;
        int function = (++vPC)->u.operand;

        ASSERT(r[base].jsValue(exec)->isObject());
        JSObject* baseObj = static_cast<JSObject*>(r[base].jsValue(exec));
        Identifier& ident = codeBlock->identifiers[property];
        ASSERT(r[function].jsValue(exec)->isObject());
        baseObj->defineSetter(exec, ident, static_cast<JSObject*>(r[function].jsValue(exec)));

        ++vPC;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_jsr) {
        /* jsr retAddrDst(r) target(offset)

           Places the address of the next instruction into the retAddrDst
           register and jumps to offset target from the current instruction.
        */
        int retAddrDst = (++vPC)->u.operand;
        int target = (++vPC)->u.operand;
        r[retAddrDst] = vPC + 1;

        vPC += target;
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_sret) {
        /* sret retAddrSrc(r)

         Jumps to the address stored in the retAddrSrc register. This
         differs from op_jmp because the target address is stored in a
         register, not as an immediate.
        */
        int retAddrSrc = (++vPC)->u.operand;
        vPC = r[retAddrSrc].vPC();
        NEXT_OPCODE;
    }
    BEGIN_OPCODE(op_debug) {
        /* debug debugHookID(n) firstLine(n) lastLine(n)

         Notifies the debugger of the current state of execution. This opcode
         is only generated while the debugger is attached.
        */

        debug(exec, vPC, codeBlock, scopeChain, r);

        vPC += 4;
        NEXT_OPCODE;
    }
    vm_throw: {
        exec->clearException();
        if (!tickCount) {
            // The exceptionValue is a lie! (GCC produces bad code for reasons I 
            // cannot fathom if we don't assign to the exceptionValue before branching)
            exceptionValue = createInterruptedExecutionException(exec);
        }
        handlerVPC = throwException(exec, exceptionValue, vPC, codeBlock, k, scopeChain, r, false);
        if (!handlerVPC) {
            *exception = exceptionValue;
            return jsNull();
        }
        vPC = handlerVPC;
        NEXT_OPCODE;
    }
    }
    #undef NEXT_OPCODE
    #undef BEGIN_OPCODE
    #undef VM_CHECK_EXCEPTION
}

JSValue* Machine::retrieveArguments(ExecState* exec, JSFunction* function) const
{
    Register* callFrame = this->callFrame(exec, function);
    if (!callFrame)
        return jsNull();

    JSActivation* activation = static_cast<JSActivation*>(callFrame[RegisterFile::OptionalCalleeActivation].jsValue(exec));
    if (!activation) {
        CodeBlock* codeBlock = &function->m_body->generatedByteCode();
        activation = new (exec) JSActivation(function->m_body, callFrame + RegisterFile::CallFrameHeaderSize + codeBlock->numLocals);
        callFrame[RegisterFile::OptionalCalleeActivation] = activation;
    }

    return activation->get(exec, exec->propertyNames().arguments);
}

JSValue* Machine::retrieveCaller(ExecState* exec, JSFunction* function) const
{
    Register* callFrame = this->callFrame(exec, function);
    if (!callFrame)
        return jsNull();

    CodeBlock* callerCodeBlock = callFrame[RegisterFile::CallerCodeBlock].codeBlock();
    if (!callerCodeBlock)
        return jsNull();

    Register* callerCallFrame = callFrame[RegisterFile::CallerRegisters].r() - callerCodeBlock->numLocals - RegisterFile::CallFrameHeaderSize;
    if (JSValue* caller = callerCallFrame[RegisterFile::Callee].jsValue(exec))
        return caller;

    return jsNull();
}

Register* Machine::callFrame(ExecState* exec, JSFunction* function) const
{
    Register* callFrame = exec->m_callFrame;

    while (1) {
        while (!callFrame) {
            exec = exec->m_prev;
            if (!exec)
                return 0;
            callFrame = exec->m_callFrame;
        }

        if (callFrame[RegisterFile::Callee].jsValue(exec) == function)
            return callFrame;

        CodeBlock* callerCodeBlock = callFrame[RegisterFile::CallerCodeBlock].codeBlock();
        if (!callerCodeBlock) {
            callFrame = 0;
            continue;
        }
        
        callFrame = callFrame[RegisterFile::CallerRegisters].r() - callerCodeBlock->numLocals - RegisterFile::CallFrameHeaderSize;
    }
}

void Machine::getArgumentsData(Register* callFrame, JSFunction*& function, Register*& argv, int& argc)
{
    function = static_cast<JSFunction*>(callFrame[RegisterFile::Callee].getJSValue());
    ASSERT(function->inherits(&JSFunction::info));

    argv = callFrame[RegisterFile::CallerRegisters].r() + callFrame[RegisterFile::ArgumentStartRegister].i() + 1; //  + 1 to skip "this"
    argc = callFrame[RegisterFile::ArgumentCount].i() - 1; // - 1 to skip "this"
}

} // namespace KJS
