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

#ifndef WebPageProxy_h
#define WebPageProxy_h

#include "APIObject.h"
#include "DrawingAreaProxy.h"
#include "GenericCallback.h"
#include "SharedMemory.h"
#include "WKBase.h"
#include "WebPageContextMenuClient.h"
#include "WebContextMenuItemData.h"
#include "WebEvent.h"
#include "WebFindClient.h"
#include "WebFindOptions.h"
#include "WebFormClient.h"
#include "WebFrameProxy.h"
#include "WebHistoryClient.h"
#include "WebInspectorProxy.h"
#include "WebLoaderClient.h"
#include "WebPolicyClient.h"
#include "WebPopupMenuProxy.h"
#include "WebUIClient.h"
#include <WebCore/EditAction.h>
#include <WebCore/Editor.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/KeyboardEvent.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class AuthenticationChallenge;
    class Cursor;
    class FloatRect;
    class IntSize;
    class ProtectionSpace;
    struct ViewportArguments;
    struct WindowFeatures;
}

namespace WebKit {

class DrawingAreaProxy;
class NativeWebKeyboardEvent;
class PageClient;
class PlatformCertificateInfo;
class StringPairVector;
class WebOpenPanelResultListenerProxy;
class WebBackForwardList;
class WebBackForwardListItem;
class WebContextMenuProxy;
class WebData;
class WebEditCommandProxy;
class WebKeyboardEvent;
class WebMouseEvent;
class WebPageGroup;
class WebPopupMenuProxy;
class WebProcessProxy;
class WebURLRequest;
class WebWheelEvent;
struct PlatformPopupMenuData;
struct WebPageCreationParameters;
struct WebPopupItem;

typedef GenericCallback<WKStringRef, StringImpl*> FrameSourceCallback;
typedef GenericCallback<WKStringRef, StringImpl*> RenderTreeExternalRepresentationCallback;
typedef GenericCallback<WKStringRef, StringImpl*> ScriptReturnValueCallback;
typedef GenericCallback<WKStringRef, StringImpl*> ContentsAsStringCallback;

class WebPageProxy : public APIObject, public WebPopupMenuProxy::Client {
public:
    static const Type APIType = TypePage;

    static PassRefPtr<WebPageProxy> create(WebContext*, WebPageGroup*, uint64_t pageID);

    virtual ~WebPageProxy();

    uint64_t pageID() const { return m_pageID; }

    WebFrameProxy* mainFrame() const { return m_mainFrame.get(); }
    WebFrameProxy* focusedFrame() const { return m_focusedFrame.get(); }

    DrawingAreaProxy* drawingArea() { return m_drawingArea.get(); }
    void setDrawingArea(PassOwnPtr<DrawingAreaProxy>);

    WebBackForwardList* backForwardList() { return m_backForwardList.get(); }

#if ENABLE(INSPECTOR)
    WebInspectorProxy* inspector();
#endif

    void setPageClient(PageClient*);
    void initializeContextMenuClient(const WKPageContextMenuClient*);
    void initializeFindClient(const WKPageFindClient*);
    void initializeFormClient(const WKPageFormClient*);
    void initializeLoaderClient(const WKPageLoaderClient*);
    void initializePolicyClient(const WKPagePolicyClient*);
    void initializeUIClient(const WKPageUIClient*);
    void relaunch();

    void initializeWebPage(const WebCore::IntSize&);
    void reinitializeWebPage(const WebCore::IntSize&);

    void close();
    bool tryClose();
    bool isClosed() const { return m_isClosed; }

    void loadURL(const String&);
    void loadURLRequest(WebURLRequest*);
    void loadHTMLString(const String& htmlString, const String& baseURL);
    void loadAlternateHTMLString(const String& htmlString, const String& baseURL, const String& unreachableURL);
    void loadPlainTextString(const String& string);

    void stopLoading();
    void reload(bool reloadFromOrigin);

    void goForward();
    bool canGoForward() const;
    void goBack();
    bool canGoBack() const;

    void goToBackForwardItem(WebBackForwardListItem*);
    void didChangeBackForwardList();

    bool canShowMIMEType(const String& mimeType) const;

    bool drawsBackground() const { return m_drawsBackground; }
    void setDrawsBackground(bool);

    bool drawsTransparentBackground() const { return m_drawsTransparentBackground; }
    void setDrawsTransparentBackground(bool);

    void setFocused(bool);
    void setActive(bool);
    void setIsInWindow(bool);
    void setWindowResizerSize(const WebCore::IntSize&);

    void executeEditCommand(const String& commandName);
    void validateMenuItem(const String& commandName);

// These are only used on Mac currently.
#if PLATFORM(MAC)
    void updateWindowIsVisible(bool windowIsVisible);
    void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates);
    void getMarkedRange(uint64_t& location, uint64_t& length);
    uint64_t characterIndexForPoint(const WebCore::IntPoint);
    WebCore::IntRect firstRectForCharacterRange(uint64_t, uint64_t);
    void didSelectionChange(bool, bool, bool, bool, uint64_t, uint64_t);
    void sendComplexTextInputToPlugin(uint64_t pluginComplexTextInputIdentifier, const String& textInput);
#else
    void didChangeSelection(bool, bool, bool, bool);
#endif
#if PLATFORM(WIN)
    void didChangeCompositionSelection(bool);
    void confirmComposition(const String&);
    void setComposition(const String&, Vector<WebCore::CompositionUnderline>&, int);
    WebCore::IntRect firstRectForCharacterInSelectedRange(int);
    String getSelectedText();
#endif
#if ENABLE(TILED_BACKING_STORE)
    void setActualVisibleContentRect(const WebCore::IntRect& rect);
#endif

    void handleMouseEvent(const WebMouseEvent&);
    void handleWheelEvent(const WebWheelEvent&);
    void handleKeyboardEvent(const NativeWebKeyboardEvent&);
#if ENABLE(TOUCH_EVENTS)
    void handleTouchEvent(const WebTouchEvent&);
#endif

    String pageTitle() const;
    const String& toolTip() const { return m_toolTip; }

    void setUserAgent(const String&);
    const String& userAgent() const { return m_userAgent; }
    void setApplicationNameForUserAgent(const String&);
    const String& applicationNameForUserAgent() const { return m_applicationNameForUserAgent; }
    void setCustomUserAgent(const String&);
    const String& customUserAgent() const { return m_customUserAgent; }

    bool supportsTextEncoding() const;
    void setCustomTextEncodingName(const String&);
    String customTextEncodingName() const { return m_customTextEncodingName; }

    double estimatedProgress() const { return m_estimatedProgress; }

    void terminateProcess();

    typedef bool (*WebPageProxySessionStateFilterCallback)(WKPageRef, WKStringRef type, WKTypeRef object, void* context);
    PassRefPtr<WebData> sessionStateData(WebPageProxySessionStateFilterCallback, void* context) const;
    void restoreFromSessionStateData(WebData*);

    double textZoomFactor() const { return m_textZoomFactor; }
    void setTextZoomFactor(double);
    double pageZoomFactor() const { return m_pageZoomFactor; }
    void setPageZoomFactor(double);
    void setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor);

    void scaleWebView(double scale, const WebCore::IntPoint& origin);
    double viewScaleFactor() const { return m_viewScaleFactor; }

    // Find.
    void findString(const String&, FindOptions, unsigned maxMatchCount);
    void hideFindUI();
    void countStringMatches(const String&, FindOptions, unsigned maxMatchCount);

    void runJavaScriptInMainFrame(const String&, PassRefPtr<ScriptReturnValueCallback>);
    void getRenderTreeExternalRepresentation(PassRefPtr<RenderTreeExternalRepresentationCallback>);
    void getSourceForFrame(WebFrameProxy*, PassRefPtr<FrameSourceCallback>);
    void getContentsAsString(PassRefPtr<ContentsAsStringCallback>);

    void receivedPolicyDecision(WebCore::PolicyAction, WebFrameProxy*, uint64_t listenerID);

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

    void processDidBecomeUnresponsive();
    void processDidBecomeResponsive();
    void processDidCrash();

#if USE(ACCELERATED_COMPOSITING)
    void didEnterAcceleratedCompositing();
    void didLeaveAcceleratedCompositing();
#endif

    void didDraw();

    enum UndoOrRedo { Undo, Redo };
    void addEditCommand(WebEditCommandProxy*);
    void removeEditCommand(WebEditCommandProxy*);
    void registerEditCommand(PassRefPtr<WebEditCommandProxy>, UndoOrRedo);

    WebProcessProxy* process() const;
    WebContext* context() const { return m_context.get(); }

    WebPageGroup* pageGroup() const { return m_pageGroup.get(); }

    bool isValid();

    // REMOVE: For demo purposes only.
    const String& urlAtProcessExit() const { return m_urlAtProcessExit; }

    void preferencesDidChange();

#if ENABLE(TILED_BACKING_STORE)
    void setResizesToContentsUsingLayoutSize(const WebCore::IntSize&);
#endif

    // Called by the WebContextMenuProxy.
    void contextMenuItemSelected(const WebContextMenuItemData&);

    // Called by the WebOpenPanelResultListenerProxy.
    void didChooseFilesForOpenPanel(const Vector<String>&);
    void didCancelForOpenPanel();

    WebPageCreationParameters creationParameters(const WebCore::IntSize&) const;

#if PLATFORM(QT)
    void findZoomableAreaForPoint(const WebCore::IntPoint&);
#endif

private:
    WebPageProxy(WebContext*, WebPageGroup*, uint64_t pageID);

    virtual Type type() const { return APIType; }

    // WebPopupMenuProxy::Client
    virtual void valueChangedForPopupMenu(WebPopupMenuProxy*, int32_t newSelectedIndex);
    virtual void setTextFromItemForPopupMenu(WebPopupMenuProxy*, int32_t index);

    // Implemented in generated WebPageProxyMessageReceiver.cpp
    void didReceiveWebPageProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncWebPageProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

    void didCreateMainFrame(uint64_t frameID);
    void didCreateSubFrame(uint64_t frameID);

    void didStartProvisionalLoadForFrame(uint64_t frameID, const String&, bool loadingSubstituteDataForUnreachableURL, CoreIPC::ArgumentDecoder*);
    void didReceiveServerRedirectForProvisionalLoadForFrame(uint64_t frameID, const String&, CoreIPC::ArgumentDecoder*);
    void didFailProvisionalLoadForFrame(uint64_t frameID, const WebCore::ResourceError&, CoreIPC::ArgumentDecoder*);
    void didCommitLoadForFrame(uint64_t frameID, const String& mimeType, bool frameHasCustomRepresentation, const PlatformCertificateInfo&, CoreIPC::ArgumentDecoder*);
    void didFinishDocumentLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didFinishLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didFailLoadForFrame(uint64_t frameID, const WebCore::ResourceError&, CoreIPC::ArgumentDecoder*);
    void didSameDocumentNavigationForFrame(uint64_t frameID, uint32_t sameDocumentNavigationType, const String&, CoreIPC::ArgumentDecoder*);
    void didReceiveTitleForFrame(uint64_t frameID, const String&, CoreIPC::ArgumentDecoder*);
    void didFirstLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didFirstVisuallyNonEmptyLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didRemoveFrameFromHierarchy(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didDisplayInsecureContentForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didRunInsecureContentForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void frameDidBecomeFrameSet(uint64_t frameID, bool);
    void didStartProgress();
    void didChangeProgress(double);
    void didFinishProgress();
    
    void decidePolicyForNavigationAction(uint64_t frameID, uint32_t navigationType, uint32_t modifiers, int32_t mouseButton, const String& url, uint64_t listenerID);
    void decidePolicyForNewWindowAction(uint64_t frameID, uint32_t navigationType, uint32_t modifiers, int32_t mouseButton, const String& url, uint64_t listenerID);
    void decidePolicyForMIMEType(uint64_t frameID, const String& MIMEType, const String& url, uint64_t listenerID, bool& receivedPolicyAction, uint64_t& policyAction, uint64_t& downloadID);

    void willSubmitForm(uint64_t frameID, uint64_t sourceFrameID, const StringPairVector& textFieldValues, uint64_t listenerID, CoreIPC::ArgumentDecoder*);

    // UI client
    void createNewPage(const WebCore::WindowFeatures&, uint32_t modifiers, int32_t mouseButton, uint64_t& newPageID, WebPageCreationParameters&);
    void showPage();
    void closePage();
    void runJavaScriptAlert(uint64_t frameID, const String&);
    void runJavaScriptConfirm(uint64_t frameID, const String&, bool& result);
    void runJavaScriptPrompt(uint64_t frameID, const String&, const String&, String& result);
    void setStatusText(const String&);
    void mouseDidMoveOverElement(uint32_t modifiers, CoreIPC::ArgumentDecoder*);
    void setToolbarsAreVisible(bool toolbarsAreVisible);
    void getToolbarsAreVisible(bool& toolbarsAreVisible);
    void setMenuBarIsVisible(bool menuBarIsVisible);
    void getMenuBarIsVisible(bool& menuBarIsVisible);
    void setStatusBarIsVisible(bool statusBarIsVisible);
    void getStatusBarIsVisible(bool& statusBarIsVisible);
    void setIsResizable(bool isResizable);
    void getIsResizable(bool& isResizable);
    void setWindowFrame(const WebCore::FloatRect&);
    void getWindowFrame(WebCore::FloatRect&);
    void canRunBeforeUnloadConfirmPanel(bool& canRun);
    void runBeforeUnloadConfirmPanel(const String& message, uint64_t frameID, bool& shouldClose);
    void didChangeViewportData(const WebCore::ViewportArguments&);
    void pageDidScroll();
    void runOpenPanel(uint64_t frameID, const WebOpenPanelParameters::Data&);

#if ENABLE(TILED_BACKING_STORE)
    void pageDidRequestScroll(const WebCore::IntSize&);
#endif
    void exceededDatabaseQuota(uint64_t frameID, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentUsage, uint64_t expectedUsage, uint64_t& newQuota);
#if PLATFORM(QT)
    void didChangeContentsSize(const WebCore::IntSize&);
    void didFindZoomableArea(const WebCore::IntRect&);
#endif

    // Back/Forward list management
    void backForwardAddItem(uint64_t itemID);
    void backForwardGoToItem(uint64_t itemID);
    void backForwardItemAtIndex(int32_t index, uint64_t& itemID);
    void backForwardBackListCount(int32_t& count);
    void backForwardForwardListCount(int32_t& count);
    void backForwardClear();

    // Undo management
    void registerEditCommandForUndo(uint64_t commandID, uint32_t editAction);
    void clearAllEditCommands();

#if PLATFORM(MAC)
    // Keyboard handling
    void interpretKeyEvent(uint32_t eventType, Vector<WebCore::KeypressCommand>&, uint32_t selectionStart, uint32_t selectionEnd, Vector<WebCore::CompositionUnderline>& underlines);
#endif
    
    // Find.
    void didCountStringMatches(const String&, uint32_t matchCount);
    void setFindIndicator(const WebCore::FloatRect& selectionRect, const Vector<WebCore::FloatRect>& textRects, const SharedMemory::Handle& contentImageHandle, bool fadeOut);
    void didFindString(const String&, uint32_t matchCount);
    void didFailToFindString(const String&);

    // Popup Menu.
    void showPopupMenu(const WebCore::IntRect& rect, const Vector<WebPopupItem>& items, int32_t selectedIndex, const PlatformPopupMenuData&);
    void hidePopupMenu();

    // Context Menu.
    void showContextMenu(const WebCore::IntPoint&, const Vector<WebContextMenuItemData>&, CoreIPC::ArgumentDecoder*);

    void takeFocus(bool direction);
    void setToolTip(const String&);
    void setCursor(const WebCore::Cursor&);
    void didValidateMenuItem(const String& commandName, bool isEnabled, int32_t state);

    void didReceiveEvent(uint32_t opaqueType, bool handled);

    void didGetContentsAsString(const String&, uint64_t);
    void didRunJavaScriptInMainFrame(const String&, uint64_t);
    void didGetRenderTreeExternalRepresentation(const String&, uint64_t);
    void didGetSourceForFrame(const String&, uint64_t);

    void focusedFrameChanged(uint64_t frameID);

#if USE(ACCELERATED_COMPOSITING)
    void didChangeAcceleratedCompositing(bool compositing, DrawingAreaInfo&);
#endif

    void canAuthenticateAgainstProtectionSpaceInFrame(uint64_t frameID, const WebCore::ProtectionSpace&, bool& canAuthenticate);
    void didReceiveAuthenticationChallenge(uint64_t frameID, const WebCore::AuthenticationChallenge&, uint64_t challengeID);

    void didFinishLoadingDataForCustomRepresentation(const CoreIPC::DataReference& data);

#if PLATFORM(MAC)
    void setComplexTextInputEnabled(uint64_t pluginComplexTextInputIdentifier, bool complexTextInputEnabled);
#endif

    static String standardUserAgent(const String& applicationName = String());

    PageClient* m_pageClient;
    WebLoaderClient m_loaderClient;
    WebPolicyClient m_policyClient;
    WebFormClient m_formClient;
    WebUIClient m_uiClient;
    WebFindClient m_findClient;
    WebPageContextMenuClient m_contextMenuClient;

    OwnPtr<DrawingAreaProxy> m_drawingArea;
    RefPtr<WebContext> m_context;
    RefPtr<WebPageGroup> m_pageGroup;
    RefPtr<WebFrameProxy> m_mainFrame;
    RefPtr<WebFrameProxy> m_focusedFrame;

    String m_userAgent;
    String m_applicationNameForUserAgent;
    String m_customUserAgent;
    String m_customTextEncodingName;

#if ENABLE(INSPECTOR)
    RefPtr<WebInspectorProxy> m_inspector;
#endif

    HashMap<uint64_t, RefPtr<ContentsAsStringCallback> > m_contentsAsStringCallbacks;
    HashMap<uint64_t, RefPtr<FrameSourceCallback> > m_frameSourceCallbacks;
    HashMap<uint64_t, RefPtr<RenderTreeExternalRepresentationCallback> > m_renderTreeExternalRepresentationCallbacks;
    HashMap<uint64_t, RefPtr<ScriptReturnValueCallback> > m_scriptReturnValueCallbacks;

    HashSet<WebEditCommandProxy*> m_editCommandSet;

    RefPtr<WebPopupMenuProxy> m_activePopupMenu;
    RefPtr<WebContextMenuProxy> m_activeContextMenu;
    RefPtr<WebOpenPanelResultListenerProxy> m_openPanelResultListener;

    double m_estimatedProgress;

    // Whether the web page is contained in a top-level window.
    bool m_isInWindow;

    bool m_canGoBack;
    bool m_canGoForward;
    RefPtr<WebBackForwardList> m_backForwardList;

    String m_toolTip;

    // REMOVE: For demo purposes only.
    String m_urlAtProcessExit;

    double m_textZoomFactor;
    double m_pageZoomFactor;
    double m_viewScaleFactor;

    bool m_drawsBackground;
    bool m_drawsTransparentBackground;

    // If the process backing the web page is alive and kicking.
    bool m_isValid;

    // Whether WebPageProxy::close() has been called on this page.
    bool m_isClosed;

    bool m_inDecidePolicyForMIMEType;
    bool m_syncMimeTypePolicyActionIsValid;
    WebCore::PolicyAction m_syncMimeTypePolicyAction;
    uint64_t m_syncMimeTypePolicyDownloadID;

    Deque<NativeWebKeyboardEvent> m_keyEventQueue;
    bool m_processingWheelEvent;
    OwnPtr<WebWheelEvent> m_nextWheelEvent;

    uint64_t m_pageID;

    bool m_mainFrameHasCustomRepresentation;
};

} // namespace WebKit

#endif // WebPageProxy_h
