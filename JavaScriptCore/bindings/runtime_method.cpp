/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#include "context.h"
#include "internal.h"
#include "runtime_method.h"
#include "runtime_object.h"

using namespace Bindings;
using namespace KJS;

RuntimeMethodImp::RuntimeMethodImp(ExecState *exec, const Identifier &ident, Bindings::Method *m) : FunctionImp (exec, ident)
{
    method = m;
}

RuntimeMethodImp::~RuntimeMethodImp()
{
}

Value RuntimeMethodImp::get(ExecState *exec, const Identifier &propertyName) const
{
    // Find the arguments from the closest context.
    if (propertyName == argumentsPropertyName) {
        ContextImp *context = exec->_context;
        while (context) {
            if (context->function() == this)
                return static_cast<ActivationImp *>
                    (context->activationObject())->get(exec, propertyName);
            context = context->callingContext();
        }
        return Undefined();
    }
    
    // Compute length of parameters.
    if (propertyName == lengthPropertyName) {
        return Number(method->numParameters());
    }
    
    return FunctionImp::get(exec, propertyName);
}

bool RuntimeMethodImp::implementsCall() const
{
    return true;
}

Value RuntimeMethodImp::call(ExecState *exec, Object &thisObj, const List &args)
{
    if (method) {
        RuntimeObjectImp *imp = static_cast<RuntimeObjectImp*>(thisObj.imp());
        if (imp) {
            return imp->getInternalInstance()->invokeMethod(exec, method, args);
        }
    }
    
    return Undefined();
}

CodeType RuntimeMethodImp::codeType() const
{
    return FunctionCode;
}


Completion RuntimeMethodImp::execute(ExecState *exec)
{
    return Completion(Normal, Undefined());
}

