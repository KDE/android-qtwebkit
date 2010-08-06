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
#include "DRTDevToolsClient.h"

#include "DRTDevToolsAgent.h"
#include "DRTDevToolsCallArgs.h"

#include "public/WebDevToolsAgent.h"
#include "public/WebDevToolsFrontend.h"
#include "public/WebFrame.h"
#include "public/WebScriptSource.h"
#include "public/WebString.h"
#include "public/WebView.h"
#include "webkit/support/webkit_support.h"

using namespace WebKit;

DRTDevToolsClient::DRTDevToolsClient(DRTDevToolsAgent* agent, WebView* webView)
    : m_callMethodFactory(this)
    , m_drtDevToolsAgent(agent)
    , m_webView(webView)
{
    m_webDevToolsFrontend.set(WebDevToolsFrontend::create(m_webView,
                                                          this,
                                                          WebString::fromUTF8("en-US")));
    m_drtDevToolsAgent->attach(this);
}

DRTDevToolsClient::~DRTDevToolsClient()
{
    // There is a chance that the page will be destroyed at detach step of
    // m_drtDevToolsAgent and we should clean pending requests a bit earlier.
    m_callMethodFactory.RevokeAll();
    if (m_drtDevToolsAgent)
        m_drtDevToolsAgent->detach();
}

void DRTDevToolsClient::reset()
{
    m_callMethodFactory.RevokeAll();
}

void DRTDevToolsClient::sendFrontendLoaded() {
    if (m_drtDevToolsAgent)
        m_drtDevToolsAgent->frontendLoaded();
}

void DRTDevToolsClient::sendMessageToBackend(const WebString& data)
{
    if (m_drtDevToolsAgent)
        m_drtDevToolsAgent->asyncCall(DRTDevToolsCallArgs(data));
}

void DRTDevToolsClient::sendDebuggerCommandToAgent(const WebString& command)
{
    WebDevToolsAgent::executeDebuggerCommand(command, 1);
}

void DRTDevToolsClient::activateWindow()
{
    // Not implemented.
}

void DRTDevToolsClient::closeWindow()
{
    // Not implemented.
}

void DRTDevToolsClient::dockWindow()
{
    // Not implemented.
}

void DRTDevToolsClient::undockWindow()
{
    // Not implemented.
}

void DRTDevToolsClient::asyncCall(const DRTDevToolsCallArgs& args)
{
    webkit_support::PostTaskFromHere(
        m_callMethodFactory.NewRunnableMethod(&DRTDevToolsClient::call, args));
}

void DRTDevToolsClient::call(const DRTDevToolsCallArgs& args)
{
    m_webDevToolsFrontend->dispatchOnInspectorFrontend(args.m_data);
    if (DRTDevToolsCallArgs::callsCount() == 1)
        allMessagesProcessed();
}

void DRTDevToolsClient::allMessagesProcessed()
{
    m_webView->mainFrame()->executeScript(
        WebKit::WebScriptSource(WebString::fromUTF8(
            "if (window.WebInspector && WebInspector.queuesAreEmpty) WebInspector.queuesAreEmpty();")));
}
