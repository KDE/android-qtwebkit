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
    RefPtr<WebPage> page = adoptRef(new WebPage(pageID, viewSize, store, drawingAreaInfo));

    if (WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->didCreatePage(page.get());

    return page.release();
}

WebPage::WebPage(uint64_t pageID, const IntSize& viewSize, const WebPreferencesStore& store, const DrawingAreaBase::DrawingAreaInfo& drawingAreaInfo)
    : m_viewSize(viewSize)
    , m_drawingArea(DrawingArea::create(drawingAreaInfo.type, drawingAreaInfo.id, this))
    , m_isInRedo(false)
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
    m_page = adoptPtr(new Page(pageClients));

    m_page->settings()->setJavaScriptEnabled(store.javaScriptEnabled);
    m_page->settings()->setLoadsImagesAutomatically(store.loadsImagesAutomatically);
    m_page->settings()->setPluginsEnabled(store.pluginsEnabled);
    m_page->settings()->setOfflineWebApplicationCacheEnabled(store.offlineWebApplicationCacheEnabled);
    m_page->settings()->setLocalStorageEnabled(store.localStorageEnabled);
    m_page->settings()->setXSSAuditorEnabled(store.xssAuditorEnabled);
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
    m_page->settings()->setMinDOMTimerInterval(0.004);

    m_page->setGroupName("WebKit2Group");
    
    platformInitialize();

    m_mainFrame = WebFrame::createMainFrame(this);
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidCreateMainFrame, m_pageID, CoreIPC::In(m_mainFrame->frameID()));

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

void WebPage::initializeInjectedBundleFormClient(WKBundlePageFormClient* client)
{
    m_formClient.initialize(client);
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

void WebPage::close()
{
    if (WebProcess::shared().injectedBundle())
        WebProcess::shared().injectedBundle()->willDestroyPage(this);

    m_mainFrame->coreFrame()->loader()->detachFromParent();

    m_page.clear();

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
    loadURLRequest(ResourceRequest(KURL(KURL(), url)));
}

void WebPage::loadURLRequest(const ResourceRequest& request)
{
    m_mainFrame->coreFrame()->loader()->load(request, false);
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
        m_mainFrame->coreFrame()->view()->updateLayoutAndStyleIfNeededRecursive();
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

double WebPage::textZoomFactor() const
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return 1;
    return frame->textZoomFactor();
}

void WebPage::setTextZoomFactor(double zoomFactor)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    frame->setTextZoomFactor(static_cast<float>(zoomFactor));
}

double WebPage::pageZoomFactor() const
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return 1;
    return frame->pageZoomFactor();
}

void WebPage::setPageZoomFactor(double zoomFactor)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    frame->setPageZoomFactor(static_cast<float>(zoomFactor));
}

void WebPage::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    Frame* frame = m_mainFrame->coreFrame();
    if (!frame)
        return;
    return frame->setPageAndTextZoomFactors(static_cast<float>(pageZoomFactor), static_cast<float>(textZoomFactor));
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

    bool handled = performDefaultBehaviorForKeyEvent(keyboardEvent);
    // FIXME: Communicate back to the UI process that the event was handled.
    (void)handled;
}

void WebPage::selectAll()
{
    if (m_page->focusController()->focusedOrMainFrame())
        m_page->focusController()->focusedOrMainFrame()->selection()->selectAll();
}

void WebPage::copy()
{
    if (m_page->focusController()->focusedOrMainFrame())
        m_page->focusController()->focusedOrMainFrame()->editor()->copy();
}

void WebPage::cut()
{
    if (m_page->focusController()->focusedOrMainFrame())
        m_page->focusController()->focusedOrMainFrame()->editor()->cut();
}

void WebPage::paste()
{
    if (m_page->focusController()->focusedOrMainFrame())
        m_page->focusController()->focusedOrMainFrame()->editor()->paste();
}    
    
#if ENABLE(TOUCH_EVENTS)
void WebPage::touchEvent(const WebTouchEvent& touchEvent)
{
    CurrentEvent currentEvent(touchEvent);
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidReceiveEvent, m_pageID, CoreIPC::In(static_cast<uint32_t>(touchEvent.type())));
            
    if (!m_mainFrame->coreFrame()->view())
        return;

    PlatformTouchEvent platformTouchEvent = platform(touchEvent);
    m_mainFrame->coreFrame()->eventHandler()->handleTouchEvent(platformTouchEvent);
}
#endif

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

void WebPage::setCustomUserAgent(const String& customUserAgent)
{
    m_customUserAgent = customUserAgent;
}

String WebPage::userAgent() const
{
    if (!m_customUserAgent.isNull())
        return m_customUserAgent;

    // FIXME: This should be based on an application name.
    return "Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_6; en-us) AppleWebKit/531.4 (KHTML, like Gecko) Version/4.0.3 Safari/531.4";
}

IntRect WebPage::windowResizerRect() const
{
    // FIXME: This function should conditionally return a null IntRect for circumstances when
    // you don't always want to show a resizer rect (i.e. you never want to show one on windows
    // and you don't want to show one in Safari when the status bar is visible).

    // FIXME: This should be either platform specific or based off the width of the scrollbar. 
    static const int windowResizerSize = 15;

    IntSize frameViewSize;
    if (Frame* coreFrame = m_mainFrame->coreFrame()) {
        if (FrameView* view = coreFrame->view())
            frameViewSize = view->size();
    }

    return IntRect(frameViewSize.width() - windowResizerSize, frameViewSize.height() - windowResizerSize, 
                   windowResizerSize, windowResizerSize);
}

void WebPage::runJavaScriptInMainFrame(const WTF::String& script, uint64_t callbackID)
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

void WebPage::getSourceForFrame(WebFrame* frame, uint64_t callbackID)
{
    String resultString;
    if (frame)
       resultString = frame->source();
    WebProcess::shared().connection()->send(WebPageProxyMessage::DidGetSourceForFrame, m_pageID, CoreIPC::In(resultString, callbackID));
}

void WebPage::preferencesDidChange(const WebPreferencesStore& store)
{
    WebPreferencesStore::removeTestRunnerOverrides();

    m_page->settings()->setJavaScriptEnabled(store.javaScriptEnabled);
    m_page->settings()->setLoadsImagesAutomatically(store.loadsImagesAutomatically);
    m_page->settings()->setOfflineWebApplicationCacheEnabled(store.offlineWebApplicationCacheEnabled);
    m_page->settings()->setLocalStorageEnabled(store.localStorageEnabled);
    m_page->settings()->setXSSAuditorEnabled(store.xssAuditorEnabled);

    platformPreferencesDidChange(store);
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

WebEditCommand* WebPage::webEditCommand(uint64_t commandID)
{
    return m_editCommandMap.get(commandID).get();
}

void WebPage::addWebEditCommand(uint64_t commandID, WebEditCommand* command)
{
    m_editCommandMap.set(commandID, command);
}

void WebPage::removeWebEditCommand(uint64_t commandID)
{
    m_editCommandMap.remove(commandID);
}

void WebPage::unapplyEditCommand(uint64_t commandID)
{
    WebEditCommand* command = webEditCommand(commandID);
    if (!command)
        return;

    command->command()->unapply();
}

void WebPage::reapplyEditCommand(uint64_t commandID)
{
    WebEditCommand* command = webEditCommand(commandID);
    if (!command)
        return;

    m_isInRedo = true;
    command->command()->reapply();
    m_isInRedo = false;
}

void WebPage::didRemoveEditCommand(uint64_t commandID)
{
    removeWebEditCommand(commandID);
}

void WebPage::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassDrawingArea>()) {
        ASSERT(m_drawingArea);
        m_drawingArea->didReceiveMessage(connection, messageID, arguments);
        return;
    }

    switch (messageID.get<WebPageMessage::Kind>()) {
        case WebPageMessage::SetActive: {
            bool active;
            if (!arguments->decode(active))
                return;
         
            setActive(active);
            return;
        }
        case WebPageMessage::SetFocused: {
            bool focused;
            if (!arguments->decode(focused))
                return;
            
            setFocused(focused);
            return;
        }
        case WebPageMessage::SetIsInWindow: {
            bool isInWindow;
            if (!arguments->decode(isInWindow))
                return;
            
            setIsInWindow(isInWindow);
            return;
        }
        case WebPageMessage::MouseEvent: {
            WebMouseEvent event;
            if (!arguments->decode(event))
                return;
            mouseEvent(event);
            return;
        }
        case WebPageMessage::PreferencesDidChange: {
            WebPreferencesStore store;
            if (!arguments->decode(store))
                return;
            
            preferencesDidChange(store);
            return;
        }
        case WebPageMessage::WheelEvent: {
            WebWheelEvent event;
            if (!arguments->decode(event))
                return;

            wheelEvent(event);
            return;
        }
        case WebPageMessage::KeyEvent: {
            WebKeyboardEvent event;
            if (!arguments->decode(event))
                return;

            keyEvent(event);
            return;
        }
        case WebPageMessage::SelectAll: {
            selectAll();
            return;
        }
        case WebPageMessage::Copy: {
            copy();
            return;
        }
        case WebPageMessage::Cut: {
            cut();
            return;
        }
        case WebPageMessage::Paste: {
            paste();
            return;
        }            
#if ENABLE(TOUCH_EVENTS)
        case WebPageMessage::TouchEvent: {
            WebTouchEvent event;
            if (!arguments->decode(event))
                return;
            touchEvent(event);
        }
#endif
        case WebPageMessage::LoadURL: {
            String url;
            if (!arguments->decode(url))
                return;
            
            loadURL(url);
            return;
        }
        case WebPageMessage::LoadURLRequest: {
            ResourceRequest request;
            if (!arguments->decode(request))
                return;
            
            loadURLRequest(request);
            return;
        }
        case WebPageMessage::StopLoading:
            stopLoading();
            return;
        case WebPageMessage::Reload: {
            bool reloadFromOrigin;
            if (!arguments->decode(CoreIPC::Out(reloadFromOrigin)))
                return;

            reload(reloadFromOrigin);
            return;
        }
        case WebPageMessage::GoForward: {
            uint64_t backForwardItemID;
            if (!arguments->decode(CoreIPC::Out(backForwardItemID)))
                return;
            goForward(backForwardItemID);
            return;
        }
        case WebPageMessage::GoBack: {
            uint64_t backForwardItemID;
            if (!arguments->decode(CoreIPC::Out(backForwardItemID)))
                return;
            goBack(backForwardItemID);
            return;
        }
       case WebPageMessage::GoToBackForwardItem: {
            uint64_t backForwardItemID;
            if (!arguments->decode(CoreIPC::Out(backForwardItemID)))
                return;
            goToBackForwardItem(backForwardItemID);
            return;
        }
        case WebPageMessage::DidReceivePolicyDecision: {
            uint64_t frameID;
            uint64_t listenerID;
            uint32_t policyAction;
            if (!arguments->decode(CoreIPC::Out(frameID, listenerID, policyAction)))
                return;
            didReceivePolicyDecision(WebProcess::shared().webFrame(frameID), listenerID, (WebCore::PolicyAction)policyAction);
            return;
        }
        case WebPageMessage::RunJavaScriptInMainFrame: {
            String script;
            uint64_t callbackID;
            if (!arguments->decode(CoreIPC::Out(script, callbackID)))
                return;
            runJavaScriptInMainFrame(script, callbackID);
            return;
        }
        case WebPageMessage::GetRenderTreeExternalRepresentation: {
            uint64_t callbackID;
            if (!arguments->decode(callbackID))
                return;
            getRenderTreeExternalRepresentation(callbackID);
            return;
        }
        case WebPageMessage::GetSourceForFrame: {
            uint64_t frameID;
            uint64_t callbackID;
            if (!arguments->decode(CoreIPC::Out(frameID, callbackID)))
                return;
            getSourceForFrame(WebProcess::shared().webFrame(frameID), callbackID);
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
        case WebPageMessage::SetCustomUserAgent: {
            String customUserAgent;
            if (!arguments->decode(CoreIPC::Out(customUserAgent)))
                return;
            setCustomUserAgent(customUserAgent);
            return;
        }
        case WebPageMessage::UnapplyEditCommand: {
            uint64_t commandID;
            if (!arguments->decode(CoreIPC::Out(commandID)))
                return;
            unapplyEditCommand(commandID);
            return;
        }
        case WebPageMessage::ReapplyEditCommand: {
            uint64_t commandID;
            if (!arguments->decode(CoreIPC::Out(commandID)))
                return;
            reapplyEditCommand(commandID);
            return;
        }
        case WebPageMessage::DidRemoveEditCommand: {
            uint64_t commandID;
            if (!arguments->decode(CoreIPC::Out(commandID)))
                return;
            didRemoveEditCommand(commandID);
            return;
        }
        case WebPageMessage::SetPageZoomFactor: {
            double zoomFactor;
            if (!arguments->decode(CoreIPC::Out(zoomFactor)))
                return;
            setPageZoomFactor(zoomFactor);
            return;
        }
        case WebPageMessage::SetTextZoomFactor: {
            double zoomFactor;
            if (!arguments->decode(CoreIPC::Out(zoomFactor)))
                return;
            setTextZoomFactor(zoomFactor);
            return;
        }
        case WebPageMessage::SetPageAndTextZoomFactors: {
            double pageZoomFactor;
            double textZoomFactor;
            if (!arguments->decode(CoreIPC::Out(pageZoomFactor, textZoomFactor)))
                return;
            setPageAndTextZoomFactors(pageZoomFactor, textZoomFactor);
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

} // namespace WebKit
