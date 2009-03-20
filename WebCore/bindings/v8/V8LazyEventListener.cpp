/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8LazyEventListener.h"

#include "Frame.h"
#include "V8Binding.h"
#include "V8Proxy.h"

namespace WebCore {

V8LazyEventListener::V8LazyEventListener(Frame *frame, const String& code, const String& functionName)
    : V8AbstractEventListener(frame, true)
    , m_code(code)
    , m_functionName(functionName)
    , m_compiled(false)
    , m_wrappedFunctionCompiled(false)
{
}

V8LazyEventListener::~V8LazyEventListener()
{
    disposeListenerObject();

    // Dispose wrapped function
    if (!m_wrappedFunction.IsEmpty()) {
#ifndef NDEBUG
        V8Proxy::UnregisterGlobalHandle(this, m_wrappedFunction);
#endif
        m_wrappedFunction.Dispose();
        m_wrappedFunction.Clear();
    }
}

v8::Local<v8::Function> V8LazyEventListener::getListenerFunction()
{
    if (m_compiled) {
        ASSERT(m_listener.IsEmpty() || m_listener->IsFunction());
        return m_listener.IsEmpty() ? v8::Local<v8::Function>() : v8::Local<v8::Function>::New(v8::Persistent<v8::Function>::Cast(m_listener));
    }

    m_compiled = true;

    ASSERT(m_frame);

    {
        // Switch to the context of m_frame.
        v8::HandleScope handleScope;

        // Use the outer scope to hold context.
        v8::Handle<v8::Context> context = V8Proxy::GetContext(m_frame);
        // Bail out if we could not get the context.
        if (context.IsEmpty())
            return v8::Local<v8::Function>();

        v8::Context::Scope scope(context);

        // Wrap function around the event code.  The parenthesis around the function are needed so that evaluating the code yields
        // the function value.  Without the parenthesis the function value is thrown away.

        // Make it an anonymous function to avoid name conflict for cases like
        // <body onload='onload()'>
        // <script> function onload() { alert('hi'); } </script>.
        // Set function name to function object instead.
        // See issue 944690.
        //
        // The ECMAScript spec says (very obliquely) that the parameter to an event handler is named "evt".
        String code = "(function (evt) {\n";
        code.append(m_code);
        code.append("\n})");

        v8::Handle<v8::String> codeExternalString = v8ExternalString(code);
        v8::Handle<v8::Script> script = V8Proxy::CompileScript(codeExternalString, m_frame->document()->url(), m_lineNumber - 1);
        if (!script.IsEmpty()) {
            V8Proxy* proxy = V8Proxy::retrieve(m_frame);
            ASSERT(proxy);
            v8::Local<v8::Value> value = proxy->RunScript(script, false);
            if (!value.IsEmpty()) {
                ASSERT(value->IsFunction());
                v8::Local<v8::Function> listenerFunction = v8::Local<v8::Function>::Cast(value);
                listenerFunction->SetName(v8::String::New(FromWebCoreString(m_functionName), m_functionName.length()));

                m_listener = v8::Persistent<v8::Function>::New(listenerFunction);
#ifndef NDEBUG
                V8Proxy::RegisterGlobalHandle(EVENT_LISTENER, this, m_listener);
#endif
            }
        }
    }

    ASSERT(m_listener.IsEmpty() || m_listener->IsFunction());
    return m_listener.IsEmpty() ? v8::Local<v8::Function>() : v8::Local<v8::Function>::New(v8::Persistent<v8::Function>::Cast(m_listener));
}

v8::Local<v8::Value> V8LazyEventListener::callListenerFunction(v8::Handle<v8::Value> jsEvent, Event* event, bool isWindowEvent)
{
    v8::Local<v8::Function> handlerFunction = getWrappedListenerFunction();
    v8::Local<v8::Object> receiver = getReceiverObject(event, isWindowEvent);
    if (handlerFunction.IsEmpty() || receiver.IsEmpty())
        return v8::Local<v8::Value>();

    v8::Handle<v8::Value> parameters[1] = { jsEvent };

    V8Proxy* proxy = V8Proxy::retrieve(m_frame);
    return proxy->CallFunction(handlerFunction, receiver, 1, parameters);
}

v8::Local<v8::Function> V8LazyEventListener::getWrappedListenerFunction()
{
    if (m_wrappedFunctionCompiled) {
        ASSERT(m_wrappedFunction.IsEmpty() || m_wrappedFunction->IsFunction());
        return m_wrappedFunction.IsEmpty() ? v8::Local<v8::Function>() : v8::Local<v8::Function>::New(m_wrappedFunction);
    }

    m_wrappedFunctionCompiled = true;

    {
        // Switch to the context of m_frame.
        v8::HandleScope handleScope;

        // Use the outer scope to hold context.
        v8::Handle<v8::Context> context = V8Proxy::GetContext(m_frame);
        // Bail out if we cannot get the context.
        if (context.IsEmpty())
            return v8::Local<v8::Function>();

        v8::Context::Scope scope(context);

        // FIXME: cache the wrapper function.

        // Nodes other than the document object, when executing inline event handlers push document, form, and the target node on the scope chain.
        // We do this by using 'with' statement.
        // See chrome/fast/forms/form-action.html
        //     chrome/fast/forms/selected-index-value.html
        //     base/fast/overflow/onscroll-layer-self-destruct.html
        String code = "(function (evt) {\n" \
                      "  with (this.ownerDocument ? this.ownerDocument : {}) {\n" \
                      "    with (this.form ? this.form : {}) {\n" \
                      "      with (this) {\n" \
                      "        return (function(evt){\n";
        code.append(m_code);
        code.append(  "\n" \
                      "}).call(this, evt);\n" \
                      "      }\n" \
                      "    }\n" \
                      "  }\n" \
                      "})");
        v8::Handle<v8::String> codeExternalString = v8ExternalString(code);
        v8::Handle<v8::Script> script = V8Proxy::CompileScript(codeExternalString, m_frame->document()->url(), m_lineNumber - 4);
        if (!script.IsEmpty()) {
            V8Proxy* proxy = V8Proxy::retrieve(m_frame);
            ASSERT(proxy);
            v8::Local<v8::Value> value = proxy->RunScript(script, false);
            if (!value.IsEmpty()) {
                ASSERT(value->IsFunction());

                m_wrappedFunction = v8::Persistent<v8::Function>::New(v8::Local<v8::Function>::Cast(value));
#ifndef NDEBUG
                V8Proxy::RegisterGlobalHandle(EVENT_LISTENER, this, m_wrappedFunction);
#endif
                m_wrappedFunction->SetName(v8::String::New(fromWebCoreString(m_functionName), m_functionName.length()));
            }
        }
    }

    return v8::Local<v8::Function>::New(m_wrappedFunction);
}

} // namespace WebCore
