/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "DRTDevToolsAgent.h"

#include "DRTDevToolsCallArgs.h"
#include "DRTDevToolsClient.h"

#include "public/WebCString.h"
#include "public/WebDevToolsAgent.h"
#include "public/WebString.h"
#include "public/WebView.h"
#include "webkit/support/webkit_support.h"

using namespace WebKit;

DRTDevToolsAgent::DRTDevToolsAgent()
    : m_callMethodFactory(this)
    , m_drtDevToolsClient(0)
    , m_webView(0)
{
    static int devToolsAgentCounter = 0;

    m_routingID = ++devToolsAgentCounter;
    if (m_routingID == 1)
        WebDevToolsAgent::setMessageLoopDispatchHandler(&DRTDevToolsAgent::dispatchMessageLoop);
}

void DRTDevToolsAgent::reset()
{
    m_callMethodFactory.RevokeAll();
}

void DRTDevToolsAgent::setWebView(WebView* webView)
{
    m_webView = webView;
}

void DRTDevToolsAgent::sendMessageToInspectorFrontend(const WebKit::WebString& data)
{
    if (m_drtDevToolsClient)
         m_drtDevToolsClient->asyncCall(DRTDevToolsCallArgs(data));
}

void DRTDevToolsAgent::forceRepaint()
{
}

void DRTDevToolsAgent::runtimeFeatureStateChanged(const WebKit::WebString& feature, bool enabled)
{
    // FIXME: implement this.
}

WebCString DRTDevToolsAgent::injectedScriptSource()
{
    return webkit_support::GetDevToolsInjectedScriptSource();
}

WebCString DRTDevToolsAgent::injectedScriptDispatcherSource()
{
    return webkit_support::GetDevToolsInjectedScriptDispatcherSource();
}

WebCString DRTDevToolsAgent::debuggerScriptSource()
{
    return webkit_support::GetDevToolsDebuggerScriptSource();
}

void DRTDevToolsAgent::asyncCall(const DRTDevToolsCallArgs &args)
{
    webkit_support::PostTaskFromHere(
        m_callMethodFactory.NewRunnableMethod(&DRTDevToolsAgent::call, args));
}

void DRTDevToolsAgent::call(const DRTDevToolsCallArgs &args)
{
    WebDevToolsAgent* agent = webDevToolsAgent();
    if (agent)
        agent->dispatchOnInspectorBackend(args.m_data);
    if (DRTDevToolsCallArgs::callsCount() == 1 && m_drtDevToolsClient)
        m_drtDevToolsClient->allMessagesProcessed();
}

WebDevToolsAgent* DRTDevToolsAgent::webDevToolsAgent()
{
    if (!m_webView)
        return 0;
    return m_webView->devToolsAgent();
}

void DRTDevToolsAgent::attach(DRTDevToolsClient* client)
{
    ASSERT(!m_drtDevToolsClient);
    m_drtDevToolsClient = client;
    WebDevToolsAgent* agent = webDevToolsAgent();
    if (agent)
        agent->attach();
}

void DRTDevToolsAgent::detach()
{
    ASSERT(m_drtDevToolsClient);
    WebDevToolsAgent* agent = webDevToolsAgent();
    if (agent)
        agent->detach();
    m_drtDevToolsClient = 0;
}

void DRTDevToolsAgent::frontendLoaded() {
    WebDevToolsAgent* agent = webDevToolsAgent();
    if (agent)
        agent->frontendLoaded();
}

bool DRTDevToolsAgent::setTimelineProfilingEnabled(bool enabled)
{
    WebDevToolsAgent* agent = webDevToolsAgent();
    if (!agent)
        return false;
    agent->setTimelineProfilingEnabled(enabled);
    return true;
}

bool DRTDevToolsAgent::evaluateInWebInspector(long callID, const std::string& script)
{
    WebDevToolsAgent* agent = webDevToolsAgent();
    if (!agent)
        return false;
    agent->evaluateInWebInspector(callID, WebString::fromUTF8(script));
    return true;
}

// static method
void DRTDevToolsAgent::dispatchMessageLoop()
{
    webkit_support::DispatchMessageLoop();
}
