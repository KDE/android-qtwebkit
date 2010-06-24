/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "InjectedBundle.h"

#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessProxyMessageKinds.h"

using namespace WebCore;

namespace WebKit {

InjectedBundle::InjectedBundle(const WebCore::String& path)
    : m_path(path)
    , m_platformBundle(0)
{
    initializeClient(0);
}

InjectedBundle::~InjectedBundle()
{
}

void InjectedBundle::initializeClient(WKBundleClient* client)
{
    if (client && !client->version)
        m_client = *client;
    else
        memset(&m_client, 0, sizeof(m_client));
}

void InjectedBundle::postMessage(StringImpl* message)
{
    WebProcess::shared().connection()->send(WebProcessProxyMessage::PostMessage, 0, CoreIPC::In(String(message)));
}

void InjectedBundle::didCreatePage(WebPage* page)
{
    if (m_client.didCreatePage)
        m_client.didCreatePage(toRef(this), toRef(page), m_client.clientInfo);
}

void InjectedBundle::willDestroyPage(WebPage* page)
{
    if (m_client.willDestroyPage)
        m_client.willDestroyPage(toRef(this), toRef(page), m_client.clientInfo);
}

void InjectedBundle::didRecieveMessage(const WebCore::String& message)
{
    if (m_client.didRecieveMessage)
        m_client.didRecieveMessage(toRef(this), toRef(message.impl()), m_client.clientInfo);
}

} // namespace WebKit
