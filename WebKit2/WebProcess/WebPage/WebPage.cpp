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

#include "WebPage.h"

#include "Arguments.h"
#include "DrawingArea.h"
#include "InjectedBundle.h"
#include "MessageID.h"
#include "WebBackForwardControllerClient.h"
#include "WebBackForwardListProxy.h"
#include "WebChromeClient.h"
#include "WebContextMenuClient.h"
#include "WebCoreArgumentCoders.h"
#include "WebDragClient.h"
#include "WebEditorClient.h"
#include "WebEvent.h"
#include "WebEventConversion.h"
#include "WebFrame.h"
#include "WebInspectorClient.h"
#include "WebPageMessageKinds.h"
#include "WebPageProxyMessageKinds.h"
#include "WebPreferencesStore.h"
#include "WebProcess.h"
#include <WebCore/BackForwardList.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/FrameView.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/Settings.h>
#include <WebCore/WindowsKeyboardCodes.h>
#include <runtime/JSLock.h>
#include <runtime/JSValue.h>

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace JSC;
using namespace WebCore;

namespace WebKit {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter webPageCounter("WebPage");
#endif

PassRefPtr<WebPage> WebPage::create(uint64_t pageID, const IntSize& viewSize, const WebPreferencesStore& store, const DrawingAreaBase::DrawingAreaInfo& drawingAreaInfo)
{
    return adoptRef(new WebPage(pageID, viewSize, store, drawingAreaInfo));
}

WebPage::WebPage(uint64_t pageID, const IntSize& viewSize, const WebPreferencesStore& store, const DrawingAreaBase::DrawingAreaInfo& drawingAreaInfo)
    : m_viewSize(viewSize)
    , m_drawingArea(DrawingArea::create(drawingAreaInfo.type, drawingAreaInfo.id, this))
    , m_pageID(pageID)
{
    ASSERT(m_pageID);

    Page::PageClients pageClients;
    pageClients.chromeClient = new WebChromeClient(this);
    pageClients.contextMenuClient = new WebContextMenuClient(this);
    pageClients.editorClient = new WebEditorClient(this);
    pageClients.dragClient = new WebDragClient(this);
    pageClients.inspectorClient = new WebInspectorClient(this);
    pageClients.backForwardControllerClient = new WebBackForwardControllerClient(this);
    m_page = new Page(pageClients);

    m_page->settings()->setJavaScriptEnabled(store.javaScriptEnabled);
    m_page->settings()->setLoadsImagesAutomatically(store.loadsImagesAutomatically);
    m_page->settings()->setPluginsEnabled(store.pluginsEnabled);
    m_page->settings()->setOfflineWebApplicationCacheEnabled(store.offlineWebApplicationCacheEnabled);
    m_page->settings()->setLocalStorageEnabled(store.localStorageEnabled);
    m_page->settings()->setMinimumFontSize(store.minimumFontSize);
    m_page->settings()->setMinimumLogicalFontSize(store.minimumLogicalFontSize);
    m_page->settings()->setDefaultFontSize(store.defaultFontSize);
    m_page->settings()->setDefaultFixedFontSize(store.defaultFixedFontSize);
    m_page->settings()->setStandardFontFamily(store.standardFontFamily);
    m_page->settings()->setCursiveFontFamily(store.cursiveFontFamily);
    m_page->settings()->setFantasyFontFamily(store.fantasyFontFamily);
    m_page->settings()->setFixedFontFamily(store.fixedFontFamily);
    m_page->settings()->setSansSerifFontFamily(store.sansSerifFontFamily);
    m_page->settings()->setSerifFontFamily(store.serifFontFamily);

    m_page->settings()->setJavaScriptCanOpenWindowsAutomatically(true);

    m_page->setGroupName("WebKit2Group");
    
    platformInitialize();

    m_mainFrame = WebFrame::createMainFrame(this);
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidCreateMainFrame, m_pageID, CoreIPC::In(m_mainFrame->frameID()));

    if (WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->didCreatePage(this);

#ifndef NDEBUG
    webPageCounter.increment();
#endif
}

WebPage::~WebPage()
{
    ASSERT(!m_page);
#ifndef NDEBUG
    webPageCounter.decrement();
#endif
}

void WebPage::initializeInjectedBundleEditorClient(WKBundlePageEditorClient* client)
{
    m_editorClient.initialize(client);
}

void WebPage::initializeInjectedBundleLoaderClient(WKBundlePageLoaderClient* client)
{
    m_loaderClient.initialize(client);
}

void WebPage::initializeInjectedBundleUIClient(WKBundlePageUIClient* client)
{
    m_uiClient.initialize(client);
}

String WebPage::renderTreeExternalRepresentation() const
{
    return externalRepresentation(m_mainFrame->coreFrame(), RenderAsTextBehaviorNormal);
}

void WebPage::executeEditingCommand(const String& commandName, const String& argument)
{
    m_mainFrame->coreFrame()->editor()->command(commandName).execute(argument);
}

bool WebPage::isEditingCommandEnabled(const String& commandName)
{
    return m_mainFrame->coreFrame()->editor()->command(commandName).isEnabled();
}

void WebPage::clearMainFrameName()
{
    mainFrame()->coreFrame()->tree()->clearName();
}

#if USE(ACCELERATED_COMPOSITING)
void WebPage::changeAcceleratedCompositingMode(WebCore::GraphicsLayer* layer)
{
    bool compositing = layer;
    
    // Tell the UI process that accelerated compositing changed. It may respond by changing
    // drawing area types.
    DrawingArea::DrawingAreaInfo newDrawingAreaInfo;
    WebProcess::shared().connection()->sendSync(WebPageProxyMessage::DidChangeAcceleratedCompositing,
                                                m_pageID, CoreIPC::In(compositing),
                                                CoreIPC::Out(newDrawingAreaInfo),
                                                CoreIPC::Connection::NoTimeout);
    
    if (newDrawingAreaInfo.type != drawingArea()->type()) {
        m_drawingArea = DrawingArea::create(newDrawingAreaInfo.type, newDrawingAreaInfo.id, this);
        m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), m_viewSize));
    }
}

void WebPage::enterAcceleratedCompositingMode(GraphicsLayer* layer)
{
    changeAcceleratedCompositingMode(layer);
    
#if USE(ACCELERATED_COMPOSITING)
    m_drawingArea->setRootCompositingLayer(layer);
#endif
}

void WebPage::exitAcceleratedCompositingMode()
{
    changeAcceleratedCompositingMode(0);
}
#endif

WebFrame* WebPage::webFrame(uint64_t frameID) const
{
    return m_frameMap.get(frameID);
}

void WebPage::addWebFrame(uint64_t frameID, WebFrame* frame)
{
    m_frameMap.set(frameID, frame);
}

void WebPage::removeWebFrame(uint64_t frameID)
{
    m_frameMap.remove(frameID);
}

void WebPage::close()
{
    if (WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->willDestroyPage(this);

    m_mainFrame->coreFrame()->loader()->detachFromParent();

    delete m_page;
    m_page = 0;

    WebProcess::shared().removeWebPage(m_pageID);
}

void WebPage::tryClose()
{
    if (!m_mainFrame->coreFrame()->loader()->shouldClose())
        return;

    sendClose();
}

void WebPage::sendClose()
{
    WebProcess::shared().connection()->send(WebPageProxyMessage::ClosePage, m_pageID, CoreIPC::In());
}

void WebPage::loadURL(const String& url)
{
    m_mainFrame->coreFrame()->loader()->load(ResourceRequest(KURL(KURL(), url)), false);
}

void WebPage::stopLoading()
{
    m_mainFrame->coreFrame()->loader()->stopForUserCancel();
}

void WebPage::reload(bool reloadFromOrigin)
{
    m_mainFrame->coreFrame()->loader()->reload(reloadFromOrigin);
}

void WebPage::goForward(uint64_t backForwardItemID)
{
    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    m_page->goToItem(item, FrameLoadTypeForward);
}

void WebPage::goBack(uint64_t backForwardItemID)
{
    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    m_page->goToItem(item, FrameLoadTypeBack);
}

void WebPage::goToBackForwardItem(uint64_t backForwardItemID)
{
    HistoryItem* item = WebBackForwardListProxy::itemForID(backForwardItemID);
    m_page->goToItem(item, FrameLoadTypeIndexedBackForward);
}

void WebPage::layoutIfNeeded()
{
    if (m_mainFrame->coreFrame()->view())
        m_mainFrame->coreFrame()->view()->layoutIfNeededRecursive();
}

void WebPage::setSize(const WebCore::IntSize& viewSize)
{
    if (m_viewSize == viewSize)
        return;

    Frame* frame = m_page->mainFrame();
    
    frame->view()->resize(viewSize);
    frame->view()->setNeedsLayout();
    m_drawingArea->setNeedsDisplay(IntRect(IntPoint(0, 0), viewSize));
    
    m_viewSize = viewSize;
}

void WebPage::drawRect(GraphicsContext& graphicsContext, const IntRect& rect)
{
    graphicsContext.save();
    graphicsContext.clip(rect);
    m_mainFrame->coreFrame()->view()->paint(&graphicsContext, rect);
    graphicsContext.restore();
}

// Events 

static const WebEvent* g_currentEvent = 0;

// FIXME: WebPage::currentEvent is used by the plug-in code to avoid having to convert from DOM events back to
// WebEvents. When we get the event handling sorted out, this should go away and the Widgets should get the correct
// platform events passed to the event handler code.
const WebEvent* WebPage::currentEvent()
{
    return g_currentEvent;
}

class CurrentEvent {
public:
    explicit CurrentEvent(const WebEvent& event)
        : m_previousCurrentEvent(g_currentEvent)
    {
        g_currentEvent = &event;
    }

    ~CurrentEvent()
    {
        g_currentEvent = m_previousCurrentEvent;
    }

private:
    const WebEvent* m_previousCurrentEvent;
};

void WebPage::mouseEvent(const WebMouseEvent& mouseEvent)
{
    CurrentEvent currentEvent(mouseEvent);

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveEvent, m_pageID, CoreIPC::In(static_cast<uint32_t>(mouseEvent.type())));

    if (!m_mainFrame->coreFrame()->view())
        return;

    PlatformMouseEvent platformMouseEvent = platform(mouseEvent);
    
    switch (platformMouseEvent.eventType()) {
        case WebCore::MouseEventPressed:
            m_mainFrame->coreFrame()->eventHandler()->handleMousePressEvent(platformMouseEvent);
            break;
        case WebCore::MouseEventReleased:
            m_mainFrame->coreFrame()->eventHandler()->handleMouseReleaseEvent(platformMouseEvent);
            break;
        case WebCore::MouseEventMoved:
            m_mainFrame->coreFrame()->eventHandler()->mouseMoved(platformMouseEvent);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

void WebPage::wheelEvent(const WebWheelEvent& wheelEvent)
{
    CurrentEvent currentEvent(wheelEvent);

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveEvent, m_pageID, CoreIPC::In(static_cast<uint32_t>(wheelEvent.type())));
    if (!m_mainFrame->coreFrame()->view())
        return;

    PlatformWheelEvent platformWheelEvent = platform(wheelEvent);
    m_mainFrame->coreFrame()->eventHandler()->handleWheelEvent(platformWheelEvent);
}


void WebPage::keyEvent(const WebKeyboardEvent& keyboardEvent)
{
    CurrentEvent currentEvent(keyboardEvent);

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveEvent, m_pageID, CoreIPC::In(static_cast<uint32_t>(keyboardEvent.type())));

    if (!m_mainFrame->coreFrame()->view())
        return;

    PlatformKeyboardEvent platformKeyboardEvent = platform(keyboardEvent);
    if (m_page->focusController()->focusedOrMainFrame()->eventHandler()->keyEvent(platformKeyboardEvent))
        return;

    performDefaultBehaviorForKeyEvent(keyboardEvent);
}

void WebPage::setActive(bool isActive)
{
    m_page->focusController()->setActive(isActive);
}

void WebPage::setFocused(bool isFocused)
{
    m_page->focusController()->setFocused(isFocused);
}

void WebPage::setIsInWindow(bool isInWindow)
{
    if (!isInWindow) {
        m_page->setCanStartMedia(false);
        m_page->willMoveOffscreen();
    } else {
        m_page->setCanStartMedia(true);
        m_page->didMoveOnscreen();
    }
}

void WebPage::didReceivePolicyDecision(WebFrame* frame, uint64_t listenerID, WebCore::PolicyAction policyAction)
{
    if (!frame)
        return;
    frame->didReceivePolicyDecision(listenerID, policyAction);
}

void WebPage::show()
{
    WebProcess::shared().connection()->send(WebPageProxyMessage::ShowPage, m_pageID, CoreIPC::In());
}

void WebPage::runJavaScriptInMainFrame(const WebCore::String& script, uint64_t callbackID)
{
    // NOTE: We need to be careful when running scripts that the objects we depend on don't
    // disappear during script execution.

    JSLock lock(SilenceAssertionsOnly);
    JSValue resultValue = m_mainFrame->coreFrame()->script()->executeScript(script, true).jsValue();
    String resultString = ustringToString(resultValue.toString(m_mainFrame->coreFrame()->script()->globalObject(mainThreadNormalWorld())->globalExec()));

    WebProcess::shared().connection()->send(WebPageProxyMessage::DidRunJavaScriptInMainFrame, m_pageID, CoreIPC::In(resultString, callbackID));
}

void WebPage::getRenderTreeExternalRepresentation(uint64_t callbackID)
{
    String resultString = renderTreeExternalRepresentation();
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidGetRenderTreeExternalRepresentation, m_pageID, CoreIPC::In(resultString, callbackID));
}

void WebPage::preferencesDidChange(const WebPreferencesStore& store)
{
    m_page->settings()->setJavaScriptEnabled(store.javaScriptEnabled);
    m_page->settings()->setLoadsImagesAutomatically(store.loadsImagesAutomatically);
    m_page->settings()->setOfflineWebApplicationCacheEnabled(store.offlineWebApplicationCacheEnabled);
    m_page->settings()->setLocalStorageEnabled(store.localStorageEnabled);
}

bool WebPage::handleEditingKeyboardEvent(KeyboardEvent* evt)
{
    Node* node = evt->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);

    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    if (!keyEvent)
        return false;

    Editor::Command command = frame->editor()->command(interpretKeyEvent(evt));

    if (keyEvent->type() == PlatformKeyboardEvent::RawKeyDown) {
        // WebKit doesn't have enough information about mode to decide how commands that just insert text if executed via Editor should be treated,
        // so we leave it upon WebCore to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        return !command.isTextInsertion() && command.execute(evt);
    }

     if (command.execute(evt))
        return true;

    // Don't insert null or control characters as they can result in unexpected behaviour
    if (evt->charCode() < ' ')
        return false;

    return frame->editor()->insertText(evt->keyEvent()->text(), evt);
}

static bool getScrollMapping(const WebKeyboardEvent& event, ScrollDirection& direction, ScrollGranularity& granularity)
{
    if (event.type() != WebEvent::KeyDown && event.type() != WebEvent::RawKeyDown)
        return false;

    switch (event.windowsVirtualKeyCode()) {
    case VK_SPACE:
        granularity = ScrollByPage;
        direction = event.shiftKey() ? ScrollUp : ScrollDown;
        break;
    case VK_LEFT:
        granularity = ScrollByLine;
        direction = ScrollLeft;
        break;
    case VK_RIGHT:
        granularity = ScrollByLine;
        direction = ScrollRight;
        break;
    case VK_UP:
        granularity = ScrollByLine;
        direction = ScrollUp;
        break;
    case VK_DOWN:
        granularity = ScrollByLine;
        direction = ScrollDown;
        break;
    case VK_HOME:
        granularity = ScrollByDocument;
        direction = ScrollUp;
        break;
    case VK_END:
        granularity = ScrollByDocument;
        direction = ScrollDown;
        break;
    case VK_PRIOR:
        granularity = ScrollByPage;
        direction = ScrollUp;
        break;
    case VK_NEXT:
        granularity = ScrollByPage;
        direction = ScrollDown;
        break;
    default:
        return false;
    }

    return true;
}

void WebPage::performDefaultBehaviorForKeyEvent(const WebKeyboardEvent& keyboardEvent)
{
    ScrollDirection direction;
    ScrollGranularity granularity;
    if (getScrollMapping(keyboardEvent, direction, granularity))
        m_page->focusController()->focusedOrMainFrame()->eventHandler()->scrollRecursively(direction, granularity);
}

void WebPage::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder& arguments)
{
    if (messageID.is<CoreIPC::MessageClassDrawingArea>()) {
        ASSERT(m_drawingArea);
        m_drawingArea->didReceiveMessage(connection, messageID, arguments);
        return;
    }

    switch (messageID.get<WebPageMessage::Kind>()) {
        case WebPageMessage::SetActive: {
            bool active;
            if (!arguments.decode(active))
                return;
         
            setActive(active);
            return;
        }
        case WebPageMessage::SetFocused: {
            bool focused;
            if (!arguments.decode(focused))
                return;
            
            setFocused(focused);
            return;
        }
        case WebPageMessage::SetIsInWindow: {
            bool isInWindow;
            if (!arguments.decode(isInWindow))
                return;
            
            setIsInWindow(isInWindow);
            return;
        }
        case WebPageMessage::MouseEvent: {
            WebMouseEvent event;
            if (!arguments.decode(event))
                return;
            mouseEvent(event);
            return;
        }
        case WebPageMessage::PreferencesDidChange: {
            WebPreferencesStore store;
            if (!arguments.decode(store))
                return;
            
            preferencesDidChange(store);
            return;
        }
        case WebPageMessage::WheelEvent: {
            WebWheelEvent event;
            if (!arguments.decode(event))
                return;

            wheelEvent(event);
            return;
        }
        case WebPageMessage::KeyEvent: {
            WebKeyboardEvent event;
            if (!arguments.decode(event))
                return;

            keyEvent(event);
            return;
        }
        case WebPageMessage::LoadURL: {
            String url;
            if (!arguments.decode(url))
                return;
            
            loadURL(url);
            return;
        }
        case WebPageMessage::StopLoading:
            stopLoading();
            break;
        case WebPageMessage::Reload: {
            bool reloadFromOrigin;
            if (!arguments.decode(CoreIPC::Out(reloadFromOrigin)))
                return;

            reload(reloadFromOrigin);
            return;
        }
        case WebPageMessage::GoForward: {
            uint64_t backForwardItemID;
            if (!arguments.decode(CoreIPC::Out(backForwardItemID)))
                return;
            goForward(backForwardItemID);
            return;
        }
        case WebPageMessage::GoBack: {
            uint64_t backForwardItemID;
            if (!arguments.decode(CoreIPC::Out(backForwardItemID)))
                return;
            goBack(backForwardItemID);
            return;
        }
       case WebPageMessage::GoToBackForwardItem: {
            uint64_t backForwardItemID;
            if (!arguments.decode(CoreIPC::Out(backForwardItemID)))
                return;
            goToBackForwardItem(backForwardItemID);
            return;
        }
        case WebPageMessage::DidReceivePolicyDecision: {
            uint64_t frameID;
            uint64_t listenerID;
            uint32_t policyAction;
            if (!arguments.decode(CoreIPC::Out(frameID, listenerID, policyAction)))
                return;
            didReceivePolicyDecision(webFrame(frameID), listenerID, (WebCore::PolicyAction)policyAction);
            return;
        }
        case WebPageMessage::RunJavaScriptInMainFrame: {
            String script;
            uint64_t callbackID;
            if (!arguments.decode(CoreIPC::Out(script, callbackID)))
                return;
            runJavaScriptInMainFrame(script, callbackID);
            return;
        }
        case WebPageMessage::GetRenderTreeExternalRepresentation: {
            uint64_t callbackID;
            if (!arguments.decode(callbackID))
                return;
            getRenderTreeExternalRepresentation(callbackID);
            return;
        }
        case WebPageMessage::Close: {
            close();
            return;
        }
        case WebPageMessage::TryClose: {
            tryClose();
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

} // namespace WebKit
