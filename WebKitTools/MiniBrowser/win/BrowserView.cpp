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

#include "StdAfx.h"
#include "BrowserView.h"

#include <WebKit2/WKURLCF.h>

BrowserView::BrowserView()
    : m_webView(0)
{
}

void BrowserView::create(RECT webViewRect, HWND parentWindow)
{
    assert(!m_webView);

    WKContextRef context = WKContextCreateWithProcessModel(kWKProcessModelSecondaryProcess);
    WKPageNamespaceRef pageNamespace = WKPageNamespaceCreate(context);

    m_webView = WKViewCreate(webViewRect, pageNamespace, parentWindow);
}

void BrowserView::setFrame(RECT rect)
{
    HWND webViewWindow = WKViewGetWindow(m_webView);
    ::SetWindowPos(webViewWindow, 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

void BrowserView::goToURL(const std::wstring& urlString)
{
    CFStringRef string = CFStringCreateWithCharacters(0, (const UniChar*)urlString.data(), urlString.size());
    CFURLRef cfURL = CFURLCreateWithString(0, string, 0);
    CFRelease(string);

    WKURLRef url = WKURLCreateWithCFURL(cfURL);
    CFRelease(cfURL); 

    WKPageRef page = WKViewGetPage(m_webView);
    WKPageLoadURL(page, url);
    WKURLRelease(url);
}
