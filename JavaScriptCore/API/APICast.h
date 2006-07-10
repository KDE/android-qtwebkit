// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef APICast_h
#define APICast_h

#include "JSValueRef.h"
#include "ustring.h"

namespace KJS {
    class ExecState;
    class JSValue;
    class JSObject;
    class ReferenceList;
}

/* Opaque typing convenience methods */

inline KJS::ExecState* toJS(JSContextRef c)
{
    return reinterpret_cast<KJS::ExecState*>(c);
}

inline KJS::JSValue* toJS(JSValueRef v)
{
    return reinterpret_cast<KJS::JSValue*>(v);
}

inline KJS::UString::Rep* toJS(JSInternalStringRef b)
{
    return reinterpret_cast<KJS::UString::Rep*>(b);
}

inline KJS::JSObject* toJS(JSObjectRef o)
{
    return reinterpret_cast<KJS::JSObject*>(o);
}

inline KJS::ReferenceList* toJS(JSPropertyListRef l)
{
    return reinterpret_cast<KJS::ReferenceList*>(l);
}

inline JSValueRef toRef(KJS::JSValue* v)
{
    return reinterpret_cast<JSValueRef>(v);
}

inline JSInternalStringRef toRef(KJS::UString::Rep* s)
{
    return reinterpret_cast<JSInternalStringRef>(s);
}

inline JSObjectRef toRef(KJS::JSObject* o)
{
    return reinterpret_cast<JSObjectRef>(o);
}

inline JSObjectRef toRef(const KJS::JSObject* o)
{
    return reinterpret_cast<JSObjectRef>(const_cast<KJS::JSObject*>(o));
}

inline JSPropertyListRef toRef(KJS::ReferenceList* l)
{
    return reinterpret_cast<JSPropertyListRef>(l);
}

inline JSContextRef toRef(KJS::ExecState* e)
{
    return reinterpret_cast<JSContextRef>(e);
}

#endif // APICast_h
