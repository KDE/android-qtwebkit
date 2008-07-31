// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSContextRef.h"

#include "APICast.h"
#include "InitializeThreading.h"
#include "JSCallbackObject.h"
#include "JSClassRef.h"
#include "JSGlobalObject.h"
#include "JSObject.h"
#include <wtf/Platform.h>

using namespace KJS;

JSContextGroupRef JSContextGroupCreate()
{
    return toRef(JSGlobalData::create().releaseRef());
}

JSContextGroupRef JSContextGroupRetain(JSContextGroupRef group)
{
    toJS(group)->ref();
    return group;
}

void JSContextGroupRelease(JSContextGroupRef group)
{
    toJS(group)->deref();
}

JSGlobalContextRef JSGlobalContextCreate(JSClassRef globalObjectClass)
{
    return JSGlobalContextCreateInGroup(toRef(JSGlobalData::create().get()), globalObjectClass);
}

JSGlobalContextRef JSGlobalContextCreateInGroup(JSContextGroupRef group, JSClassRef globalObjectClass)
{
    initializeThreading();

    JSGlobalData* globalData = toJS(group);

    if (!globalObjectClass) {
        JSGlobalObject* globalObject = new (globalData) JSGlobalObject;
        return JSGlobalContextRetain(toGlobalRef(globalObject->globalExec()));
    }

    JSGlobalObject* globalObject = new (globalData) JSCallbackObject<JSGlobalObject>(globalObjectClass);
    ExecState* exec = globalObject->globalExec();
    JSValue* prototype = globalObjectClass->prototype(exec);
    if (!prototype)
        prototype = jsNull();
    globalObject->reset(prototype);
    return JSGlobalContextRetain(toGlobalRef(exec));
}

JSGlobalContextRef JSGlobalContextRetain(JSGlobalContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap->registerThread();

    gcProtect(exec->dynamicGlobalObject());
    return ctx;
}

void JSGlobalContextRelease(JSGlobalContextRef ctx)
{
    ExecState* exec = toJS(ctx);

    gcUnprotect(exec->dynamicGlobalObject());

    JSGlobalData& globalData = exec->globalData();
    if (globalData.refCount() == 1) {
        // The last reference was released, this is our last chance to collect.
        Heap* heap = globalData.heap;

        ASSERT(!heap->protectedObjectCount());
        ASSERT(!heap->isBusy());

        // Heap::destroy() will delete JSGlobalObject, which will in turn delete JSGlobalData, which will
        // delete the heap, which would cause a crash if allowed.
        globalData.heap = 0;

        delete heap;
    }
}

JSObjectRef JSContextGetGlobalObject(JSContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    exec->globalData().heap->registerThread();

    // It is necessary to call toThisObject to get the wrapper object when used with WebCore.
    return toRef(exec->dynamicGlobalObject()->toThisObject(exec));
}

JSContextGroupRef JSContextGetGroup(JSContextRef ctx)
{
    ExecState* exec = toJS(ctx);
    return toRef(&exec->globalData());
}
