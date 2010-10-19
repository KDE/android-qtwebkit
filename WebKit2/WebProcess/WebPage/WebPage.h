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
#include "FindController.h"
#include "InjectedBundlePageEditorClient.h"
#include "InjectedBundlePageFormClient.h"
#include "InjectedBundlePageLoaderClient.h"
#include "InjectedBundlePageUIClient.h"
#include "Plugin.h"
#include "WebEditCommand.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntRect.h>
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
    class ResourceRequest;
    class SharedBuffer;
}

namespace WebKit {

class DrawingArea;
class PageOverlay;
class PluginView;
class WebEvent;
class WebFrame;
class WebKeyboardEvent;
class WebMouseEvent;
class WebPopupMenu;
class WebWheelEvent;
#if ENABLE(TOUCH_EVENTS)
class WebTouchEvent;
#endif
struct WebPageCreationParameters;
struct WebPreferencesStore;

class WebPage : public APIObject {
public:
    static const Type APIType = TypeBundlePage;

    static PassRefPtr<WebPage> create(uint64_t pageID, const WebPageCreationParameters&);
    ~WebPage();

    void close();

    WebCore::Page* corePage() const { return m_page.get(); }
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
    WebCore::IntRect windowResizerRect() const;

    WebEditCommand* webEditCommand(uint64_t);
    void addWebEditCommand(uint64_t, WebEditCommand*);
    void removeWebEditCommand(uint64_t);
    bool isInRedo() const { return m_isInRedo; }

    void setActivePopupMenu(WebPopupMenu*);

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
    PassRefPtr<Plugin> createPlugin(const Plugin::Parameters&);

    String renderTreeExternalRepresentation() const;
    void executeEditingCommand(const String& commandName, const String& argument);
    bool isEditingCommandEnabled(const String& commandName);
    void clearMainFrameName();
    void sendClose();

    double textZoomFactor() const;
    void setTextZoomFactor(double);
    double pageZoomFactor() const;
    void setPageZoomFactor(double);
    void setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor);

    void stopLoading();

#if USE(ACCELERATED_COMPOSITING)
    void changeAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void exitAcceleratedCompositingMode();
#endif

#if PLATFORM(MAC)
    void addPluginView(PluginView*);
    void removePluginView(PluginView*);

    bool windowIsVisible() const { return m_windowIsVisible; }
    const WebCore::IntRect& windowFrame() const { return m_windowFrame; }
    bool windowIsFocused() const;
#elif PLATFORM(WIN)
    HWND nativeWindow() const { return m_nativeWindow; }
#endif

    void installPageOverlay(PassOwnPtr<PageOverlay>);
    void uninstallPageOverlay();

    static const WebEvent* currentEvent();

    FindController& findController() { return m_findController; }

private:
    WebPage(uint64_t pageID, const WebPageCreationParameters&);

    virtual Type type() const { return APIType; }

    void platformInitialize();

    void didReceiveWebPageMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    static const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
    bool performDefaultBehaviorForKeyEvent(const WebKeyboardEvent&);

    String sourceForFrame(WebFrame*);

    void loadData(PassRefPtr<WebCore::SharedBuffer>, const String& MIMEType, const String& encodingName, const WebCore::KURL& baseURL, const WebCore::KURL& failingURL);

    // Actions
    void tryClose();
    void loadURL(const String&);
    void loadURLRequest(const WebCore::ResourceRequest&);
    void loadHTMLString(const String& htmlString, const String& baseURL);
    void loadAlternateHTMLString(const String& htmlString, const String& baseURL, const String& unreachableURL);
    void loadPlainTextString(const String&);
    void reload(bool reloadFromOrigin);
    void goForward(uint64_t);
    void goBack(uint64_t);
    void goToBackForwardItem(uint64_t);
    void setActive(bool);
    void setFocused(bool);
    void setWindowResizerSize(const WebCore::IntSize&);
    void setIsInWindow(bool);
    void mouseEvent(const WebMouseEvent&);
    void wheelEvent(const WebWheelEvent&);
    void keyEvent(const WebKeyboardEvent&);
    void validateMenuItem(const String&);
    void executeEditCommand(const String&);
#if ENABLE(TOUCH_EVENTS)
    void touchEvent(const WebTouchEvent&);
#endif
    void runJavaScriptInMainFrame(const String&, uint64_t callbackID);
    void getRenderTreeExternalRepresentation(uint64_t callbackID);
    void getSourceForFrame(uint64_t frameID, uint64_t callbackID);

    void preferencesDidChange(const WebPreferencesStore&);
    void platformPreferencesDidChange(const WebPreferencesStore&);
    void updatePreferences(const WebPreferencesStore&);

    void didReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction);
    void setCustomUserAgent(const String&);

#if PLATFORM(MAC)
    void setWindowIsVisible(bool windowIsVisible);
    void setWindowFrame(const WebCore::IntRect&);
#endif

    void unapplyEditCommand(uint64_t commandID);
    void reapplyEditCommand(uint64_t commandID);
    void didRemoveEditCommand(uint64_t commandID);

    void findString(const String&, uint32_t findDirection, uint32_t findOptions, uint32_t maxMatchCount);
    void hideFindUI();
    void countStringMatches(const String&, bool caseInsensitive, uint32_t maxMatchCount);

    void didChangeSelectedIndexForActivePopupMenu(int32_t newIndex);

    OwnPtr<WebCore::Page> m_page;
    RefPtr<WebFrame> m_mainFrame;

    String m_customUserAgent;

    WebCore::IntSize m_viewSize;
    RefPtr<DrawingArea> m_drawingArea;

    bool m_isInRedo;
    bool m_isClosed;

#if PLATFORM(MAC)
    // Whether the containing window is visible or not.
    bool m_windowIsVisible;

    // The frame of the containing window.
    WebCore::IntRect m_windowFrame;

    // All plug-in views on this web page.
    HashSet<PluginView*> m_pluginViews;
#elif PLATFORM(WIN)
    // Our view's window (in the UI process).
    HWND m_nativeWindow;
#endif
    
    HashMap<uint64_t, RefPtr<WebEditCommand> > m_editCommandMap;

    WebCore::IntSize m_windowResizerSize;

    InjectedBundlePageEditorClient m_editorClient;
    InjectedBundlePageFormClient m_formClient;
    InjectedBundlePageLoaderClient m_loaderClient;
    InjectedBundlePageUIClient m_uiClient;

    FindController m_findController;
    OwnPtr<PageOverlay> m_pageOverlay;

    RefPtr<WebPopupMenu> m_activePopupMenu;

    uint64_t m_pageID;
};

} // namespace WebKit

#endif // WebPage_h
