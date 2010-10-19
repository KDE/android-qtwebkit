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

#include "WebLoaderClient.h"

#include "WKAPICast.h"
#include <string.h>

using namespace WebCore;

namespace WebKit {

WebLoaderClient::WebLoaderClient()
{
    initialize(0);
}

void WebLoaderClient::initialize(const WKPageLoaderClient* client)
{
    if (client && !client->version)
        m_pageLoaderClient = *client;
    else 
        memset(&m_pageLoaderClient, 0, sizeof(m_pageLoaderClient));
}

void WebLoaderClient::didStartProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, APIObject* userData)
{
    if (!m_pageLoaderClient.didStartProvisionalLoadForFrame)
        return;

    m_pageLoaderClient.didStartProvisionalLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, APIObject* userData)
{
    if (!m_pageLoaderClient.didReceiveServerRedirectForProvisionalLoadForFrame)
        return;

    m_pageLoaderClient.didReceiveServerRedirectForProvisionalLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFailProvisionalLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame, const ResourceError& error, APIObject* userData)
{
    if (!m_pageLoaderClient.didFailProvisionalLoadWithErrorForFrame)
        return;

    m_pageLoaderClient.didFailProvisionalLoadWithErrorForFrame(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didCommitLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, APIObject* userData)
{
    if (!m_pageLoaderClient.didCommitLoadForFrame)
        return;

    m_pageLoaderClient.didCommitLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFinishDocumentLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, APIObject* userData)
{
    if (!m_pageLoaderClient.didFinishDocumentLoadForFrame)
        return;

    m_pageLoaderClient.didFinishDocumentLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFinishLoadForFrame(WebPageProxy* page, WebFrameProxy* frame, APIObject* userData)
{
    if (!m_pageLoaderClient.didFinishLoadForFrame)
        return;

    m_pageLoaderClient.didFinishLoadForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFailLoadWithErrorForFrame(WebPageProxy* page, WebFrameProxy* frame, const ResourceError& error, APIObject* userData)
{
    if (!m_pageLoaderClient.didFailLoadWithErrorForFrame)
        return;

    m_pageLoaderClient.didFailLoadWithErrorForFrame(toAPI(page), toAPI(frame), toAPI(error), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didReceiveTitleForFrame(WebPageProxy* page, const String& title, WebFrameProxy* frame, APIObject* userData)
{
    if (!m_pageLoaderClient.didReceiveTitleForFrame)
        return;

    m_pageLoaderClient.didReceiveTitleForFrame(toAPI(page), toAPI(title.impl()), toAPI(frame), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFirstLayoutForFrame(WebPageProxy* page, WebFrameProxy* frame, APIObject* userData)
{
    if (!m_pageLoaderClient.didFirstLayoutForFrame)
        return;

    m_pageLoaderClient.didFirstLayoutForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFirstVisuallyNonEmptyLayoutForFrame(WebPageProxy* page, WebFrameProxy* frame, APIObject* userData)
{
    if (!m_pageLoaderClient.didFirstVisuallyNonEmptyLayoutForFrame)
        return;

    m_pageLoaderClient.didFirstVisuallyNonEmptyLayoutForFrame(toAPI(page), toAPI(frame), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didRemoveFrameFromHierarchy(WebPageProxy* page, WebFrameProxy* frame, APIObject* userData)
{
    if (!m_pageLoaderClient.didRemoveFrameFromHierarchy)
        return;

    m_pageLoaderClient.didRemoveFrameFromHierarchy(toAPI(page), toAPI(frame), toAPI(userData), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didStartProgress(WebPageProxy* page)
{
    if (!m_pageLoaderClient.didStartProgress)
        return;

    m_pageLoaderClient.didStartProgress(toAPI(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didChangeProgress(WebPageProxy* page)
{
    if (!m_pageLoaderClient.didChangeProgress)
        return;

    m_pageLoaderClient.didChangeProgress(toAPI(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didFinishProgress(WebPageProxy* page)
{
    if (!m_pageLoaderClient.didFinishProgress)
        return;

    m_pageLoaderClient.didFinishProgress(toAPI(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didBecomeUnresponsive(WebPageProxy* page)
{
    if (!m_pageLoaderClient.didBecomeUnresponsive)
        return;

    m_pageLoaderClient.didBecomeUnresponsive(toAPI(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didBecomeResponsive(WebPageProxy* page)
{
    if (!m_pageLoaderClient.didBecomeResponsive)
        return;

    m_pageLoaderClient.didBecomeResponsive(toAPI(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::processDidExit(WebPageProxy* page)
{
    if (!m_pageLoaderClient.processDidExit)
        return;

    m_pageLoaderClient.processDidExit(toAPI(page), m_pageLoaderClient.clientInfo);
}

void WebLoaderClient::didChangeBackForwardList(WebPageProxy* page)
{
    if (!m_pageLoaderClient.didChangeBackForwardList)
        return;

    m_pageLoaderClient.didChangeBackForwardList(toAPI(page), m_pageLoaderClient.clientInfo);
}

} // namespace WebKit
