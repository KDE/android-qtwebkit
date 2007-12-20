// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef ExecState_H
#define ExecState_H

#include "LabelStack.h"
#include "LocalStorage.h"
#include "scope_chain.h"
#include "types.h"

namespace KJS  {

    enum CodeType {
        GlobalCode,
        EvalCode,
        FunctionCode,
    };
    
    class ActivationImp;
    class CommonIdentifiers;
    class FunctionImp;
    class GlobalFuncImp;
    class Interpreter;
    class JSGlobalObject;
    class JSVariableObject;
    class ScopeChain;
    class ScopeNode;
    struct LocalStorageEntry;
    
    /**
     * Represents the current state of script execution. This is
     * passed as the first argument to most functions.
     */
    class ExecState {
        friend class Interpreter;
        friend class FunctionImp;
        friend class GlobalFuncImp;
    public:
        /**
         * Returns the global object that was in scope when the current script started executing.
         */
        JSGlobalObject* dynamicGlobalObject() const { return m_globalObject; }
        
        /**
         * Returns the global object that was in scope when the current body of code was defined.
         */
        JSGlobalObject* lexicalGlobalObject() const;
                
        void setException(JSValue* e) { m_exception = e; }
        void clearException() { m_exception = 0; }
        JSValue* exception() const { return m_exception; }
        JSValue** exceptionSlot() { return &m_exception; }
        bool hadException() const { return !!m_exception; }
        
        const ScopeChain& scopeChain() const { return m_scopeChain; }
        
        JSVariableObject* variableObject() const { return m_variableObject; }
        void setVariableObject(JSVariableObject* v) { m_variableObject = v; }
        
        JSObject* thisValue() const { return m_thisVal; }
        
        ExecState* callingExecState() { return m_callingExec; }
        ExecState* savedExec() { return m_savedExec; }
        
        ActivationImp* activationObject() { return m_activation; }
        CodeType codeType() { return m_codeType; }
        ScopeNode* scopeNode() { return m_scopeNode; }
        FunctionImp* function() const { return m_function; }
        const List* arguments() const { return m_arguments; }
        
        void pushScope(JSObject* s) { m_scopeChain.push(s); }
        void popScope() { m_scopeChain.pop(); }
        LabelStack* seenLabels() { return &ls; }
        
        void pushIteration() { m_iterationDepth++; }
        void popIteration() { m_iterationDepth--; }
        bool inIteration() const { return (m_iterationDepth > 0); }
        
        void pushSwitch() { m_switchDepth++; }
        void popSwitch() { m_switchDepth--; }
        bool inSwitch() const { return (m_switchDepth > 0); }

        void mark();
        
        // This is a workaround to avoid accessing the global variables for these identifiers in
        // important property lookup functions, to avoid taking PIC branches in Mach-O binaries
        const CommonIdentifiers& propertyNames() const { return *m_propertyNames; }

        LocalStorage& localStorage() { return *m_localStorage; }

        // These are only valid right after calling execute().
        ComplType completionType() const { return m_completionType; }
        const Identifier& breakOrContinueTarget() const
        {
            ASSERT(m_completionType == Break || m_completionType == Continue);
            return *m_breakOrContinueTarget;
        }

        // Only for use in the implementation of execute().
        void setCompletionType(ComplType type)
        {
            ASSERT(type != Break);
            ASSERT(type != Continue);
            m_completionType = type;
        }
        JSValue* setNormalCompletion()
        {
            ASSERT(!hadException());
            m_completionType = Normal;
            return 0;
        }
        JSValue* setNormalCompletion(JSValue* value)
        {
            ASSERT(!hadException());
            m_completionType = Normal;
            return value;
        }
        JSValue* setBreakCompletion(const Identifier* target)
        {
            ASSERT(!hadException());
            m_completionType = Break;
            m_breakOrContinueTarget = target;
            return 0;
        }
        JSValue* setContinueCompletion(const Identifier* target)
        {
            ASSERT(!hadException());
            m_completionType = Continue;
            m_breakOrContinueTarget = target;
            return 0;
        }
        JSValue* setReturnValueCompletion(JSValue* returnValue)
        {
            ASSERT(!hadException());
            ASSERT(returnValue);
            m_completionType = ReturnValue;
            return returnValue;
        }
        JSValue* setThrowCompletion(JSValue* exception)
        {
            ASSERT(!hadException());
            ASSERT(exception);
            m_completionType = Throw;
            return exception;
        }
        JSValue* setInterruptedCompletion()
        {
            ASSERT(!hadException());
            m_completionType = Interrupted;
            return 0;
        }

    public:
        ExecState(JSGlobalObject* glob, JSObject* thisV,
                  ScopeNode* scopeNode, CodeType type = GlobalCode,
                  ExecState* callingExecState = 0, ExecState* currentExec = 0, 
                  FunctionImp* function = 0, const List* args = 0);
        ~ExecState();

    private:
        // ExecStates are always stack-allocated, and the garbage collector
        // marks the stack, so we don't need to protect the objects below from GC.

        JSGlobalObject* m_globalObject;
        JSValue* m_exception;
        CommonIdentifiers* m_propertyNames;

        ExecState* m_callingExec;
        ExecState* m_savedExec;
        ScopeNode* m_scopeNode;
        
        FunctionImp* m_function;
        const List* m_arguments;
        ActivationImp* m_activation;
        LocalStorage* m_localStorage;

        ScopeChain m_scopeChain;
        JSVariableObject* m_variableObject;
        JSObject* m_thisVal;
        
        LabelStack ls;
        int m_iterationDepth;
        int m_switchDepth;
        CodeType m_codeType;

        ComplType m_completionType;
        const Identifier* m_breakOrContinueTarget;
    };

} // namespace KJS

#endif // ExecState_H
