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

#ifndef DRTDevToolsAgent_h
#define DRTDevToolsAgent_h

#include "base/task.h" // FIXME: remove this
#include "public/WebDevToolsAgentClient.h"
#include <wtf/Noncopyable.h>

namespace WebKit {

class WebCString;
class WebDevToolsAgent;
class WebView;
struct WebDevToolsMessageData;

} // namespace WebKit

class DRTDevToolsCallArgs;
class DRTDevToolsClient;

class DRTDevToolsAgent : public WebKit::WebDevToolsAgentClient
                       , public Noncopyable {
public:
    DRTDevToolsAgent();
    virtual ~DRTDevToolsAgent() {}

    void setWebView(WebKit::WebView*);

    // WebDevToolsAgentClient implementation.
    virtual void sendMessageToFrontend(const WebKit::WebDevToolsMessageData&);
    virtual int hostIdentifier() { return m_routingID; }
    virtual void forceRepaint();
    virtual void runtimeFeatureStateChanged(const WebKit::WebString& feature, bool enabled);
    virtual WebKit::WebCString injectedScriptSource();
    virtual WebKit::WebCString injectedScriptDispatcherSource();
    virtual WebKit::WebCString debuggerScriptSource();

    void asyncCall(const DRTDevToolsCallArgs&);

    void attach(DRTDevToolsClient*);
    void detach(DRTDevToolsClient*);

    bool evaluateInWebInspector(long callID, const std::string& script);
    bool setTimelineProfilingEnabled(bool enable);

private:
    void call(const DRTDevToolsCallArgs&);
    static void dispatchMessageLoop();
    WebKit::WebDevToolsAgent* webDevToolsAgent();

    ScopedRunnableMethodFactory<DRTDevToolsAgent> m_callMethodFactory;
    DRTDevToolsClient* m_drtDevToolsClient;
    int m_routingID;
    WebKit::WebDevToolsAgent* m_webDevToolsAgent;
    WebKit::WebView* m_webView;
};

#endif // DRTDevToolsAgent_h
