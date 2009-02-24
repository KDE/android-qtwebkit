/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "FunctionPrototype.h"

#include "Arguments.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSString.h"
#include "Interpreter.h"
#include "Lexer.h"
#include "PrototypeFunction.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(FunctionPrototype);

static JSValuePtr functionProtoFuncToString(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr functionProtoFuncApply(ExecState*, JSObject*, JSValuePtr, const ArgList&);
static JSValuePtr functionProtoFuncCall(ExecState*, JSObject*, JSValuePtr, const ArgList&);

FunctionPrototype::FunctionPrototype(ExecState* exec, PassRefPtr<Structure> structure)
    : InternalFunction(&exec->globalData(), structure, exec->propertyNames().nullIdentifier)
{
    putDirectWithoutTransition(exec->propertyNames().length, jsNumber(exec, 0), DontDelete | ReadOnly | DontEnum);
}

void FunctionPrototype::addFunctionProperties(ExecState* exec, Structure* prototypeFunctionStructure)
{
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 0, exec->propertyNames().toString, functionProtoFuncToString), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 2, exec->propertyNames().apply, functionProtoFuncApply), DontEnum);
    putDirectFunctionWithoutTransition(exec, new (exec) PrototypeFunction(exec, prototypeFunctionStructure, 1, exec->propertyNames().call, functionProtoFuncCall), DontEnum);
}

static JSValuePtr callFunctionPrototype(ExecState*, JSObject*, JSValuePtr, const ArgList&)
{
    return jsUndefined();
}

// ECMA 15.3.4
CallType FunctionPrototype::getCallData(CallData& callData)
{
    callData.native.function = callFunctionPrototype;
    return CallTypeHost;
}

// Functions

// Compatibility hack for the Optimost JavaScript library. (See <rdar://problem/6595040>.)
static inline void insertSemicolonIfNeeded(UString& functionBody)
{
    ASSERT(functionBody[0] == '{');
    ASSERT(functionBody[functionBody.size() - 1] == '}');

    for (size_t i = functionBody.size() - 2; i > 0; --i) {
        UChar ch = functionBody[i];
        if (!Lexer::isWhiteSpace(ch) && !Lexer::isLineTerminator(ch)) {
            if (ch != ';' && ch != '}')
                functionBody = functionBody.substr(0, i + 1) + ";" + functionBody.substr(i + 1, functionBody.size() - (i + 1));
            return;
        }
    }
}

JSValuePtr functionProtoFuncToString(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList&)
{
    if (thisValue.isObject(&JSFunction::info)) {
        JSFunction* function = asFunction(thisValue);
        UString functionBody = function->body()->toSourceString();
        insertSemicolonIfNeeded(functionBody);
        return jsString(exec, "function " + function->name(&exec->globalData()) + "(" + function->body()->paramString() + ") " + functionBody);
    }

    if (thisValue.isObject(&InternalFunction::info)) {
        InternalFunction* function = asInternalFunction(thisValue);
        return jsString(exec, "function " + function->name(&exec->globalData()) + "() {\n    [native code]\n}");
    }

    return throwError(exec, TypeError);
}

JSValuePtr functionProtoFuncApply(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    CallData callData;
    CallType callType = thisValue.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    JSValuePtr array = args.at(exec, 1);

    ArgList applyArgs;
    if (!array.isUndefinedOrNull()) {
        if (!array.isObject())
            return throwError(exec, TypeError);
        if (asObject(array)->classInfo() == &Arguments::info)
            asArguments(array)->fillArgList(exec, applyArgs);
        else if (isJSArray(&exec->globalData(), array))
            asArray(array)->fillArgList(exec, applyArgs);
        else if (asObject(array)->inherits(&JSArray::info)) {
            unsigned length = asArray(array)->get(exec, exec->propertyNames().length).toUInt32(exec);
            for (unsigned i = 0; i < length; ++i)
                applyArgs.append(asArray(array)->get(exec, i));
        } else
            return throwError(exec, TypeError);
    }

    return call(exec, thisValue, callType, callData, args.at(exec, 0), applyArgs);
}

JSValuePtr functionProtoFuncCall(ExecState* exec, JSObject*, JSValuePtr thisValue, const ArgList& args)
{
    CallData callData;
    CallType callType = thisValue.getCallData(callData);
    if (callType == CallTypeNone)
        return throwError(exec, TypeError);

    ArgList callArgs;
    args.getSlice(1, callArgs);
    return call(exec, thisValue, callType, callData, args.at(exec, 0), callArgs);
}

} // namespace JSC
