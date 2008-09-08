/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSXMLHttpRequestUpload.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "JSDOMWindowCustom.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "XMLHttpRequest.h"
#include "XMLHttpRequestUpload.h"
#include <kjs/Error.h>

using namespace JSC;

namespace WebCore {

void JSXMLHttpRequestUpload::mark()
{
    Base::mark();

    if (XMLHttpRequest* xmlHttpRequest = m_impl->associatedXMLHttpRequest()) {
        DOMObject* wrapper = ScriptInterpreter::getDOMObject(xmlHttpRequest);
        if (wrapper && !wrapper->marked())
            wrapper->mark();
    }

    if (JSUnprotectedEventListener* onAbortListener = static_cast<JSUnprotectedEventListener*>(m_impl->onAbortListener()))
        onAbortListener->mark();

    if (JSUnprotectedEventListener* onErrorListener = static_cast<JSUnprotectedEventListener*>(m_impl->onErrorListener()))
        onErrorListener->mark();

    if (JSUnprotectedEventListener* onLoadListener = static_cast<JSUnprotectedEventListener*>(m_impl->onLoadListener()))
        onLoadListener->mark();

    if (JSUnprotectedEventListener* onLoadStartListener = static_cast<JSUnprotectedEventListener*>(m_impl->onLoadStartListener()))
        onLoadStartListener->mark();
    
    if (JSUnprotectedEventListener* onProgressListener = static_cast<JSUnprotectedEventListener*>(m_impl->onProgressListener()))
        onProgressListener->mark();
    
    typedef XMLHttpRequest::EventListenersMap EventListenersMap;
    typedef XMLHttpRequest::ListenerVector ListenerVector;
    EventListenersMap& eventListeners = m_impl->eventListeners();
    for (EventListenersMap::iterator mapIter = eventListeners.begin(); mapIter != eventListeners.end(); ++mapIter) {
        for (ListenerVector::iterator vecIter = mapIter->second.begin(); vecIter != mapIter->second.end(); ++vecIter) {
            JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(vecIter->get());
            listener->mark();
        }
    }
}

JSValue* JSXMLHttpRequestUpload::onabort(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onAbortListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequestUpload::setOnabort(ExecState* exec, JSValue* value)
{
    XMLHttpRequest* xmlHttpRequest = impl()->associatedXMLHttpRequest();
    if (!xmlHttpRequest)
        return;
    Document* document = xmlHttpRequest->document();
    if (!document)
        return;
    Frame* frame = document->frame();
    if (!frame)
        return;
    impl()->setOnAbortListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSXMLHttpRequestUpload::onerror(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onErrorListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequestUpload::setOnerror(ExecState* exec, JSValue* value)
{
    XMLHttpRequest* xmlHttpRequest = impl()->associatedXMLHttpRequest();
    if (!xmlHttpRequest)
        return;
    Document* document = xmlHttpRequest->document();
    if (!document)
        return;
    Frame* frame = document->frame();
    if (!frame)
        return;
    impl()->setOnErrorListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSXMLHttpRequestUpload::onload(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onLoadListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequestUpload::setOnload(ExecState* exec, JSValue* value)
{
    XMLHttpRequest* xmlHttpRequest = impl()->associatedXMLHttpRequest();
    if (!xmlHttpRequest)
        return;
    Document* document = xmlHttpRequest->document();
    if (!document)
        return;
    Frame* frame = document->frame();
    if (!frame)
        return;
    impl()->setOnLoadListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSXMLHttpRequestUpload::onloadstart(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onLoadStartListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequestUpload::setOnloadstart(ExecState* exec, JSValue* value)
{
    XMLHttpRequest* xmlHttpRequest = impl()->associatedXMLHttpRequest();
    if (!xmlHttpRequest)
        return;
    Document* document = xmlHttpRequest->document();
    if (!document)
        return;
    Frame* frame = document->frame();
    if (!frame)
        return;
    impl()->setOnLoadStartListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSXMLHttpRequestUpload::onprogress(ExecState*) const
{
    if (JSUnprotectedEventListener* listener = static_cast<JSUnprotectedEventListener*>(impl()->onProgressListener())) {
        if (JSObject* listenerObj = listener->listenerObj())
            return listenerObj;
    }
    return jsNull();
}

void JSXMLHttpRequestUpload::setOnprogress(ExecState* exec, JSValue* value)
{
    XMLHttpRequest* xmlHttpRequest = impl()->associatedXMLHttpRequest();
    if (!xmlHttpRequest)
        return;
    Document* document = xmlHttpRequest->document();
    if (!document)
        return;
    Frame* frame = document->frame();
    if (!frame)
        return;
    impl()->setOnProgressListener(toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, value, true));
}

JSValue* JSXMLHttpRequestUpload::addEventListener(ExecState* exec, const ArgList& args)
{
    XMLHttpRequest* xmlHttpRequest = impl()->associatedXMLHttpRequest();
    if (!xmlHttpRequest)
        return jsUndefined();
    Document* document = xmlHttpRequest->document();
    if (!document)
        return jsUndefined();
    Frame* frame = document->frame();
    if (!frame)
        return jsUndefined();
    RefPtr<JSUnprotectedEventListener> listener = toJSDOMWindow(frame)->findOrCreateJSUnprotectedEventListener(exec, args.at(exec, 1), true);
    if (!listener)
        return jsUndefined();
    impl()->addEventListener(args.at(exec, 0)->toString(exec), listener.release(), args.at(exec, 2)->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequestUpload::removeEventListener(ExecState* exec, const ArgList& args)
{
    XMLHttpRequest* xmlHttpRequest = impl()->associatedXMLHttpRequest();
    if (!xmlHttpRequest)
        return jsUndefined();
    Document* document = xmlHttpRequest->document();
    if (!document)
        return jsUndefined();
    Frame* frame = document->frame();
    if (!frame)
        return jsUndefined();
    JSUnprotectedEventListener* listener = toJSDOMWindow(frame)->findJSUnprotectedEventListener(exec, args.at(exec, 1), true);
    if (!listener)
        return jsUndefined();
    impl()->removeEventListener(args.at(exec, 0)->toString(exec), listener, args.at(exec, 2)->toBoolean(exec));
    return jsUndefined();
}

JSValue* JSXMLHttpRequestUpload::dispatchEvent(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    bool result = impl()->dispatchEvent(toEvent(args.at(exec, 0)), ec);
    setDOMException(exec, ec);
    return jsBoolean(result);    
}

} // namespace WebCore
