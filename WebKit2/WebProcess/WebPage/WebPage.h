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

#ifndef WebPage_h
#define WebPage_h

#include "APIObject.h"
#include "DrawingArea.h"
#include "InjectedBundlePageEditorClient.h"
#include "InjectedBundlePageFormClient.h"
#include "InjectedBundlePageLoaderClient.h"
#include "InjectedBundlePageUIClient.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntRect.h>
#include <WebCore/ZoomMode.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>
#if ENABLE(TOUCH_EVENTS)
#include <WebCore/PlatformTouchEvent.h>
#endif

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class GraphicsContext;
    class KeyboardEvent;
    class Page;
}

namespace WebKit {

class DrawingArea;
class WebEvent;
class WebFrame;
class WebKeyboardEvent;
class WebMouseEvent;
class WebWheelEvent;
#if ENABLE(TOUCH_EVENTS)
class WebTouchEvent;
#endif
struct WebPreferencesStore;

class WebPage : public APIObject {
public:
    static const Type APIType = TypeBundlePage;

    static PassRefPtr<WebPage> create(uint64_t pageID, const WebCore::IntSize& viewSize, const WebPreferencesStore&, const DrawingAreaBase::DrawingAreaInfo&);
    ~WebPage();

    void close();

    WebCore::Page* corePage() const { return m_page; }
    uint64_t pageID() const { return m_pageID; }

    void setSize(const WebCore::IntSize&);
    const WebCore::IntSize& size() const { return m_viewSize; }

    DrawingArea* drawingArea() const { return m_drawingArea.get(); }

    // -- Called by the DrawingArea.
    // FIXME: We could genericize these into a DrawingArea client interface. Would that be beneficial?
    void drawRect(WebCore::GraphicsContext&, const WebCore::IntRect&);
    void layoutIfNeeded();

    // -- Called from WebCore clients.
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);
    void show();
    String userAgent() const;

    // -- Called from WebProcess.
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    // -- InjectedBundle methods
    void initializeInjectedBundleEditorClient(WKBundlePageEditorClient*);
    void initializeInjectedBundleFormClient(WKBundlePageFormClient*);
    void initializeInjectedBundleLoaderClient(WKBundlePageLoaderClient*);
    void initializeInjectedBundleUIClient(WKBundlePageUIClient*);

    InjectedBundlePageEditorClient& injectedBundleEditorClient() { return m_editorClient; }
    InjectedBundlePageFormClient& injectedBundleFormClient() { return m_formClient; }
    InjectedBundlePageLoaderClient& injectedBundleLoaderClient() { return m_loaderClient; }
    InjectedBundlePageUIClient& injectedBundleUIClient() { return m_uiClient; }

    WebFrame* mainFrame() const { return m_mainFrame.get(); }

    String renderTreeExternalRepresentation() const;
    void executeEditingCommand(const WTF::String& commandName, const WTF::String& argument);
    bool isEditingCommandEnabled(const WTF::String& commandName);
    void clearMainFrameName();
    void sendClose();

    float zoomFactor() const;
    void setZoomFactor(float);
    void setZoomMode(WebCore::ZoomMode);

    void stopLoading();

#if USE(ACCELERATED_COMPOSITING)
    void changeAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void exitAcceleratedCompositingMode();
#endif

    static const WebEvent* currentEvent();

private:
    WebPage(uint64_t pageID, const WebCore::IntSize& viewSize, const WebPreferencesStore&, const DrawingAreaBase::DrawingAreaInfo&);

    virtual Type type() const { return APIType; }

    void platformInitialize();
    static const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
    bool performDefaultBehaviorForKeyEvent(const WebKeyboardEvent&);

    // Actions
    void tryClose();
    void loadURL(const WTF::String&);
    void reload(bool reloadFromOrigin);
    void goForward(uint64_t);
    void goBack(uint64_t);
    void goToBackForwardItem(uint64_t);
    void setActive(bool);
    void setFocused(bool);
    void setIsInWindow(bool);
    void mouseEvent(const WebMouseEvent&);
    void wheelEvent(const WebWheelEvent&);
    void keyEvent(const WebKeyboardEvent&);
#if ENABLE(TOUCH_EVENTS)
    void touchEvent(const WebTouchEvent&);
#endif
    void runJavaScriptInMainFrame(const WTF::String&, uint64_t callbackID);
    void getRenderTreeExternalRepresentation(uint64_t callbackID);
    void preferencesDidChange(const WebPreferencesStore&);
    void platformPreferencesDidChange(const WebPreferencesStore&);
    void didReceivePolicyDecision(WebFrame*, uint64_t listenerID, WebCore::PolicyAction policyAction);
    void setCustomUserAgent(const WTF::String&);

    WebCore::Page* m_page;
    RefPtr<WebFrame> m_mainFrame;

    String m_customUserAgent;

    WebCore::IntSize m_viewSize;
    RefPtr<DrawingArea> m_drawingArea;

    InjectedBundlePageEditorClient m_editorClient;
    InjectedBundlePageFormClient m_formClient;
    InjectedBundlePageLoaderClient m_loaderClient;
    InjectedBundlePageUIClient m_uiClient;

    uint64_t m_pageID;
};

} // namespace WebKit

#endif // WebPage_h
