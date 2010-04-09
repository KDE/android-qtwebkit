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

#include "WKPage.h"

#include "WKAPICast.h"
#include "WebPageProxy.h"

#if __BLOCKS__
#include <Block.h>
#endif

using namespace WebKit;

WKPageNamespaceRef WKPageGetPageNamespace(WKPageRef pageRef)
{
    return toRef(toWK(pageRef)->pageNamespace());
}

void WKPageLoadURL(WKPageRef pageRef, WKURLRef URLRef)
{
    toWK(pageRef)->loadURL(toWK(URLRef)->url());
}

void WKPageStopLoading(WKPageRef pageRef)
{
    toWK(pageRef)->stopLoading();
}

void WKPageReload(WKPageRef pageRef)
{
    toWK(pageRef)->reload();
}

bool WKPageTryClose(WKPageRef pageRef)
{
    return toWK(pageRef)->tryClose();
}

void WKPageClose(WKPageRef pageRef)
{
    toWK(pageRef)->close();
}

bool WKPageIsClosed(WKPageRef pageRef)
{
    return toWK(pageRef)->isClosed();
}

void WKPageGoForward(WKPageRef pageRef)
{
    toWK(pageRef)->goForward();
}

bool WKPageCanGoForward(WKPageRef pageRef)
{
    return toWK(pageRef)->canGoForward();
}

void WKPageGoBack(WKPageRef pageRef)
{
    toWK(pageRef)->goBack();
}

bool WKPageCanGoBack(WKPageRef pageRef)
{
    return toWK(pageRef)->canGoBack();
}

WKStringRef WKPageGetTitle(WKPageRef pageRef)
{
    return toRef(toWK(pageRef)->pageTitle().impl());
}

WKFrameRef WKPageGetMainFrame(WKPageRef pageRef)
{
    return toRef(toWK(pageRef)->mainFrame());
}

void WKPageTerminate(WKPageRef pageRef)
{
    toWK(pageRef)->terminateProcess();
}

void WKPageSetPageLoaderClient(WKPageRef pageRef, WKPageLoaderClient* wkClient)
{
    if (wkClient && !wkClient->version)
        toWK(pageRef)->initializeLoaderClient(wkClient);
}

void WKPageSetPagePolicyClient(WKPageRef pageRef, WKPagePolicyClient * wkClient)
{
    if (wkClient && !wkClient->version)
        toWK(pageRef)->initializePolicyClient(wkClient);
}

void WKPageSetPageUIClient(WKPageRef pageRef, WKPageUIClient * wkClient)
{
   if (wkClient && !wkClient->version)
        toWK(pageRef)->initializeUIClient(wkClient);
}

#if __BLOCKS__
static void callBlockAndRelease(void* context, WKStringRef resultValue)
{
    void (^block)(WKStringRef) = (void (^ReturnValueBlockType)(WKStringRef))context;
    block(resultValue);
    Block_release(block);
}

static void disposeBlock(void* context)
{
    void (^block)(WKStringRef) = (void (^ReturnValueBlockType)(WKStringRef))context;
    Block_release(block);
}

void WKPageRunJavaScriptInMainFrame(WKPageRef pageRef, WKStringRef scriptRef, void (^returnValueBlock)(WKStringRef))
{
    WKPageRunJavaScriptInMainFrame_f(pageRef, scriptRef, Block_copy(returnValueBlock), callBlockAndRelease, disposeBlock);
}
#endif

void WKPageRunJavaScriptInMainFrame_f(WKPageRef pageRef, WKStringRef scriptRef, void* context, void (*returnValueCallback)(void*, WKStringRef), void (*disposeContextCallback)(void*))
{
    toWK(pageRef)->runJavaScriptInMainFrame(toWK(scriptRef), ScriptReturnValueCallback::create(context, returnValueCallback, disposeContextCallback));
}

WKPageRef WKPageRetain(WKPageRef pageRef)
{
    toWK(pageRef)->ref();
    return pageRef;
}

void WKPageRelease(WKPageRef pageRef)
{
    toWK(pageRef)->deref();
}
