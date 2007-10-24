/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DebuggerClient_H
#define DebuggerClient_H

#include "BaseDelegate.h"

#include <string>
#include <WebCore/COMPtr.h>
#include <wtf/OwnPtr.h>

class DebuggerDocument;
class ServerConnection;
struct IWebView;
struct IWebFrame;

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;
typedef const struct OpaqueJSValue* JSValueRef;

class DebuggerClient : public BaseDelegate {
public:
    DebuggerClient();
    explicit DebuggerClient(const std::wstring& serverName);

    void initWithServerName(const std::wstring& serverName);
    bool webViewLoaded() const { return m_webViewLoaded; }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(
        /* [in] */ REFIID riid,
        /* [retval][out] */ void** ppvObject);

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    // IWebFrameLoadDelegate
    HRESULT STDMETHODCALLTYPE didFinishLoadForFrame( 
        /* [in] */ IWebView*,
        /* [in] */ IWebFrame*);

    HRESULT STDMETHODCALLTYPE windowScriptObjectAvailable( 
        /* [in] */ IWebView*,
        /* [in] */ JSContextRef,
        /* [in] */ JSObjectRef);

    HRESULT STDMETHODCALLTYPE didReceiveTitle( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR,
        /* [in] */ IWebFrame*);

    // IWebUIDelegate
    HRESULT STDMETHODCALLTYPE runJavaScriptAlertPanelWithMessage( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR);

    HRESULT STDMETHODCALLTYPE createWebViewWithRequest( 
        /* [in] */ IWebView*,
        /* [in] */ IWebURLRequest*,
        /* [retval][out] */ IWebView**);

    // IWebNotificationObserver
    HRESULT STDMETHODCALLTYPE onNotify(
        /* [in] */ IWebNotification*);

private:
    bool m_webViewLoaded;

    OwnPtr<DebuggerDocument> m_debuggerDocument;
};

#endif //DebuggerClient_H
