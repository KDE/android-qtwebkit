/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007 Apple Inc.
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

#include "config.h"
#include "interpreter.h"

#include "ExecState.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "Machine.h"
#include "Parser.h"
#include "completion.h"
#include "Debugger.h"
#include <stdio.h>

#if !PLATFORM(WIN_OS)
#include <unistd.h>
#endif

namespace JSC {

Completion Interpreter::checkSyntax(ExecState* exec, const SourceCode& source)
{
    JSLock lock(exec);

    int errLine;
    UString errMsg;

    RefPtr<ProgramNode> progNode = exec->globalData().parser->parse<ProgramNode>(exec, exec->dynamicGlobalObject()->debugger(), source, &errLine, &errMsg);
    if (!progNode)
        return Completion(Throw, Error::create(exec, SyntaxError, errMsg, errLine, source.provider()->asID(), source.provider()->url()));
    return Completion(Normal);
}

Completion Interpreter::evaluate(ExecState* exec, ScopeChain& scopeChain, const SourceCode& source, JSValue* thisValue)
{
    JSLock lock(exec);
    
    int errLine;
    UString errMsg;
    RefPtr<ProgramNode> programNode = exec->globalData().parser->parse<ProgramNode>(exec, exec->dynamicGlobalObject()->debugger(), source, &errLine, &errMsg);

    if (!programNode)
        return Completion(Throw, Error::create(exec, SyntaxError, errMsg, errLine, source.provider()->asID(), source.provider()->url()));

    JSObject* thisObj = (!thisValue || thisValue->isUndefinedOrNull()) ? exec->dynamicGlobalObject() : thisValue->toObject(exec);

    JSValue* exception = noValue();
    JSValue* result = exec->machine()->execute(programNode.get(), exec, scopeChain.node(), thisObj, &exception);

    if (exception) {
        if (exception->isObject() && asObject(exception)->isWatchdogException())
            return Completion(Interrupted, result);
        return Completion(Throw, exception);
    }
    return Completion(Normal, result);
}

} // namespace JSC
