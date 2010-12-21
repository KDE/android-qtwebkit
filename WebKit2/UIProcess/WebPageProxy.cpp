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

#include "WebPageProxy.h"

#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "DataReference.h"
#include "DrawingAreaProxy.h"
#include "FindIndicator.h"
#include "MessageID.h"
#include "NativeWebKeyboardEvent.h"
#include "PageClient.h"
#include "StringPairVector.h"
#include "WKContextPrivate.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebCertificateInfo.h"
#include "WebContext.h"
#include "WebContextMenuProxy.h"
#include "WebContextUserMessageCoders.h"
#include "WebCoreArgumentCoders.h"
#include "WebData.h"
#include "WebEditCommandProxy.h"
#include "WebEvent.h"
#include "WebFormSubmissionListenerProxy.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebOpenPanelResultListenerProxy.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroup.h"
#include "WebPageGroupData.h"
#include "WebPageMessages.h"
#include "WebPopupItem.h"
#include "WebPopupMenuProxy.h"
#include "WebPreferences.h"
#include "WebProcessManager.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include "WebProtectionSpace.h"
#include "WebSecurityOrigin.h"
#include "WebURLRequest.h"
#include <WebCore/FloatRect.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/WindowFeatures.h>
#include <stdio.h>

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

// This controls what strategy we use for mouse wheel coalesing.
#define MERGE_WHEEL_EVENTS 0

using namespace WebCore;

namespace WebKit {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter webPageProxyCounter("WebPageProxy");
#endif

PassRefPtr<WebPageProxy> WebPageProxy::create(WebContext* context, WebPageGroup* pageGroup, uint64_t pageID)
{
    return adoptRef(new WebPageProxy(context, pageGroup, pageID));
}

WebPageProxy::WebPageProxy(WebContext* context, WebPageGroup* pageGroup, uint64_t pageID)
    : m_pageClient(0)
    , m_context(context)
    , m_pageGroup(pageGroup)
    , m_mainFrame(0)
    , m_userAgent(standardUserAgent())
    , m_estimatedProgress(0.0)
    , m_isInWindow(false)
    , m_backForwardList(WebBackForwardList::create(this))
    , m_textZoomFactor(1)
    , m_pageZoomFactor(1)
    , m_viewScaleFactor(1)
    , m_drawsBackground(true)
    , m_drawsTransparentBackground(false)
    , m_isValid(true)
    , m_isClosed(false)
    , m_inDecidePolicyForMIMEType(false)
    , m_syncMimeTypePolicyActionIsValid(false)
    , m_syncMimeTypePolicyAction(PolicyUse)
    , m_syncMimeTypePolicyDownloadID(0)
    , m_processingWheelEvent(false)
    , m_pageID(pageID)
    , m_mainFrameHasCustomRepresentation(false)
{
#ifndef NDEBUG
    webPageProxyCounter.increment();
#endif

    WebContext::statistics().wkPageCount++;

    m_pageGroup->addPage(this);
}

WebPageProxy::~WebPageProxy()
{
    WebContext::statistics().wkPageCount--;

    m_pageGroup->removePage(this);

#ifndef NDEBUG
    webPageProxyCounter.decrement();
#endif
}

WebProcessProxy* WebPageProxy::process() const
{
    return m_context->process();
}

bool WebPageProxy::isValid()
{
    // A page that has been explicitly closed is never valid.
    if (m_isClosed)
        return false;

    return m_isValid;
}

void WebPageProxy::setPageClient(PageClient* pageClient)
{
    m_pageClient = pageClient;
}

void WebPageProxy::setDrawingArea(PassOwnPtr<DrawingAreaProxy> drawingArea)
{
    if (drawingArea == m_drawingArea)
        return;

    m_drawingArea = drawingArea;
}

void WebPageProxy::initializeLoaderClient(const WKPageLoaderClient* loadClient)
{
    m_loaderClient.initialize(loadClient);
}

void WebPageProxy::initializePolicyClient(const WKPagePolicyClient* policyClient)
{
    m_policyClient.initialize(policyClient);
}

void WebPageProxy::initializeFormClient(const WKPageFormClient* formClient)
{
    m_formClient.initialize(formClient);
}

void WebPageProxy::initializeResourceLoadClient(const WKPageResourceLoadClient* client)
{
    m_resourceLoadClient.initialize(client);
}

void WebPageProxy::initializeUIClient(const WKPageUIClient* client)
{
    m_uiClient.initialize(client);
}

void WebPageProxy::initializeFindClient(const WKPageFindClient* client)
{
    m_findClient.initialize(client);
}

void WebPageProxy::initializeContextMenuClient(const WKPageContextMenuClient* client)
{
    m_contextMenuClient.initialize(client);
}

void WebPageProxy::relaunch()
{
    m_isValid = true;
    context()->relaunchProcessIfNecessary();
    process()->addExistingWebPage(this, m_pageID);

    m_pageClient->didRelaunchProcess();
}

void WebPageProxy::initializeWebPage(const IntSize& size)
{
    if (!isValid()) {
        relaunch();
        return;
    }

    ASSERT(m_drawingArea);
    process()->send(Messages::WebProcess::CreateWebPage(m_pageID, creationParameters(size)), 0);
}

void WebPageProxy::reinitializeWebPage(const WebCore::IntSize& size)
{
    if (!isValid())
        return;

    ASSERT(m_drawingArea);
    process()->send(Messages::WebProcess::CreateWebPage(m_pageID, creationParameters(size)), 0);
}

void WebPageProxy::close()
{
    if (!isValid())
        return;

    m_isClosed = true;

    process()->disconnectFramesFromPage(this);
    m_mainFrame = 0;

#if ENABLE(INSPECTOR)
    if (m_inspector) {
        m_inspector->invalidate();
        m_inspector = 0;
    }
#endif

    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = 0;
    }

    m_toolTip = String();

    invalidateCallbackMap(m_contentsAsStringCallbacks);
    invalidateCallbackMap(m_frameSourceCallbacks);
    invalidateCallbackMap(m_renderTreeExternalRepresentationCallbacks);
    invalidateCallbackMap(m_scriptReturnValueCallbacks);

    Vector<WebEditCommandProxy*> editCommandVector;
    copyToVector(m_editCommandSet, editCommandVector);
    m_editCommandSet.clear();
    for (size_t i = 0, size = editCommandVector.size(); i < size; ++i)
        editCommandVector[i]->invalidate();

    m_activePopupMenu = 0;

    m_estimatedProgress = 0.0;
    
    m_loaderClient.initialize(0);
    m_policyClient.initialize(0);
    m_uiClient.initialize(0);

    m_drawingArea.clear();

    process()->send(Messages::WebPage::Close(), m_pageID);
    process()->removeWebPage(m_pageID);
}

bool WebPageProxy::tryClose()
{
    if (!isValid())
        return true;

    process()->send(Messages::WebPage::TryClose(), m_pageID);
    return false;
}

static void initializeSandboxExtensionHandle(const KURL& url, SandboxExtension::Handle& sandboxExtensionHandle)
{
    if (!url.isLocalFile())
        return;

    SandboxExtension::createHandle("/", SandboxExtension::ReadOnly, sandboxExtensionHandle);
}

void WebPageProxy::loadURL(const String& url)
{
    if (!isValid())
        relaunch();

    SandboxExtension::Handle sandboxExtensionHandle;
    initializeSandboxExtensionHandle(KURL(KURL(), url), sandboxExtensionHandle);
    process()->send(Messages::WebPage::LoadURL(url, sandboxExtensionHandle), m_pageID);
}

void WebPageProxy::loadURLRequest(WebURLRequest* urlRequest)
{
    if (!isValid())
        relaunch();

    SandboxExtension::Handle sandboxExtensionHandle;
    initializeSandboxExtensionHandle(urlRequest->resourceRequest().url(), sandboxExtensionHandle);
    process()->send(Messages::WebPage::LoadURLRequest(urlRequest->resourceRequest(), sandboxExtensionHandle), m_pageID);
}

void WebPageProxy::loadHTMLString(const String& htmlString, const String& baseURL)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::LoadHTMLString(htmlString, baseURL), m_pageID);
}

void WebPageProxy::loadAlternateHTMLString(const String& htmlString, const String& baseURL, const String& unreachableURL)
{
    if (!isValid())
        return;

    if (!m_mainFrame)
        return;

    m_mainFrame->setUnreachableURL(unreachableURL);
    process()->send(Messages::WebPage::LoadAlternateHTMLString(htmlString, baseURL, unreachableURL), m_pageID);
}

void WebPageProxy::loadPlainTextString(const String& string)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::LoadPlainTextString(string), m_pageID);
}

void WebPageProxy::stopLoading()
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::StopLoading(), m_pageID);
}

void WebPageProxy::reload(bool reloadFromOrigin)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::Reload(reloadFromOrigin), m_pageID);
}

void WebPageProxy::goForward()
{
    if (!isValid())
        return;

    if (!canGoForward())
        return;

    process()->send(Messages::WebPage::GoForward(m_backForwardList->forwardItem()->itemID()), m_pageID);
}

bool WebPageProxy::canGoForward() const
{
    return m_backForwardList->forwardItem();
}

void WebPageProxy::goBack()
{
    if (!isValid())
        return;

    if (!canGoBack())
        return;

    process()->send(Messages::WebPage::GoBack(m_backForwardList->backItem()->itemID()), m_pageID);
}

bool WebPageProxy::canGoBack() const
{
    return m_backForwardList->backItem();
}

void WebPageProxy::goToBackForwardItem(WebBackForwardListItem* item)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::GoToBackForwardItem(item->itemID()), m_pageID);
}

void WebPageProxy::didChangeBackForwardList()
{
    m_loaderClient.didChangeBackForwardList(this);
}

    
bool WebPageProxy::canShowMIMEType(const String& mimeType) const
{
    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return true;

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return true;
    
    String newMimeType = mimeType;
    PluginInfoStore::Plugin plugin = context()->pluginInfoStore()->findPlugin(newMimeType, KURL());
    if (!plugin.path.isNull())
        return true;

    return false;
}

void WebPageProxy::setDrawsBackground(bool drawsBackground)
{
    if (m_drawsBackground == drawsBackground)
        return;

    m_drawsBackground = drawsBackground;

    if (isValid())
        process()->send(Messages::WebPage::SetDrawsBackground(drawsBackground), m_pageID);
}

void WebPageProxy::setDrawsTransparentBackground(bool drawsTransparentBackground)
{
    if (m_drawsTransparentBackground == drawsTransparentBackground)
        return;

    m_drawsTransparentBackground = drawsTransparentBackground;

    if (isValid())
        process()->send(Messages::WebPage::SetDrawsTransparentBackground(drawsTransparentBackground), m_pageID);
}

void WebPageProxy::setFocused(bool isFocused)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetFocused(isFocused), m_pageID);
}

void WebPageProxy::setInitialFocus(bool forward)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetInitialFocus(forward), m_pageID);
}

void WebPageProxy::setActive(bool active)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetActive(active), m_pageID);
}

void WebPageProxy::setWindowResizerSize(const IntSize& windowResizerSize)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetWindowResizerSize(windowResizerSize), m_pageID);
}

void WebPageProxy::validateMenuItem(const String& commandName)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::ValidateMenuItem(commandName), m_pageID);
}
    
void WebPageProxy::executeEditCommand(const String& commandName)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::ExecuteEditCommand(commandName), m_pageID);
}
    
void WebPageProxy::setIsInWindow(bool isInWindow)
{
    if (m_isInWindow == isInWindow)
        return;
    
    m_isInWindow = isInWindow;
    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetIsInWindow(isInWindow), m_pageID);
}

#if PLATFORM(MAC)
void WebPageProxy::updateWindowIsVisible(bool windowIsVisible)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetWindowIsVisible(windowIsVisible), m_pageID);
}

void WebPageProxy::windowAndViewFramesChanged(const IntRect& windowFrameInScreenCoordinates, const IntRect& viewFrameInWindowCoordinates)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::WindowAndViewFramesChanged(windowFrameInScreenCoordinates, viewFrameInWindowCoordinates), m_pageID);
}

void WebPageProxy::getMarkedRange(uint64_t& location, uint64_t& length)
{
    process()->sendSync(Messages::WebPage::GetMarkedRange(), Messages::WebPage::GetMarkedRange::Reply(location, length), m_pageID);
}
    
uint64_t WebPageProxy::characterIndexForPoint(const IntPoint point)
{
    uint64_t result;
    process()->sendSync(Messages::WebPage::CharacterIndexForPoint(point), Messages::WebPage::CharacterIndexForPoint::Reply(result), m_pageID);
    return result;
}

WebCore::IntRect WebPageProxy::firstRectForCharacterRange(uint64_t location, uint64_t length)
{
    IntRect resultRect;
    process()->sendSync(Messages::WebPage::FirstRectForCharacterRange(location, length), Messages::WebPage::FirstRectForCharacterRange::Reply(resultRect), m_pageID);
    return resultRect;
}
#elif PLATFORM(WIN)
WebCore::IntRect WebPageProxy::firstRectForCharacterInSelectedRange(int characterPosition)
{
    IntRect resultRect;
    process()->sendSync(Messages::WebPage::FirstRectForCharacterInSelectedRange(characterPosition), Messages::WebPage::FirstRectForCharacterInSelectedRange::Reply(resultRect), m_pageID);
    return resultRect;
}

String WebPageProxy::getSelectedText()
{
    String text;
    process()->sendSync(Messages::WebPage::GetSelectedText(), Messages::WebPage::GetSelectedText::Reply(text), m_pageID);
    return text;
}
#endif

#if ENABLE(TILED_BACKING_STORE)
void WebPageProxy::setActualVisibleContentRect(const IntRect& rect)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::SetActualVisibleContentRect(rect), m_pageID);
}
#endif

void WebPageProxy::handleMouseEvent(const WebMouseEvent& event)
{
    if (!isValid())
        return;

    // NOTE: This does not start the responsiveness timer because mouse move should not indicate interaction.
    if (event.type() != WebEvent::MouseMove)
        process()->responsivenessTimer()->start();
    process()->send(Messages::WebPage::MouseEvent(event), m_pageID);
}

static PassOwnPtr<WebWheelEvent> coalesceWheelEvents(WebWheelEvent* oldNextWheelEvent, const WebWheelEvent& newWheelEvent)
{
#if MERGE_WHEEL_EVENTS
    // Merge model: Combine wheel event deltas (and wheel ticks) into a single wheel event.
    if (!oldNextWheelEvent)
        return adoptPtr(new WebWheelEvent(newWheelEvent));

    if (oldNextWheelEvent->position() != newWheelEvent.position() || oldNextWheelEvent->modifiers() != newWheelEvent.modifiers() || oldNextWheelEvent->granularity() != newWheelEvent.granularity())
        return adoptPtr(new WebWheelEvent(newWheelEvent));

    FloatSize mergedDelta = oldNextWheelEvent->delta() + newWheelEvent.delta();
    FloatSize mergedWheelTicks = oldNextWheelEvent->wheelTicks() + newWheelEvent.wheelTicks();

    return adoptPtr(new WebWheelEvent(WebEvent::Wheel, newWheelEvent.position(), newWheelEvent.globalPosition(), mergedDelta, mergedWheelTicks, newWheelEvent.granularity(), newWheelEvent.modifiers(), newWheelEvent.timestamp()));
#else
    // Simple model: Just keep the last event, dropping all interim events.
    return adoptPtr(new WebWheelEvent(newWheelEvent));
#endif
}

void WebPageProxy::handleWheelEvent(const WebWheelEvent& event)
{
    if (!isValid())
        return;

    if (m_processingWheelEvent) {
        m_nextWheelEvent = coalesceWheelEvents(m_nextWheelEvent.get(), event);
        return;
    }

    process()->responsivenessTimer()->start();
    process()->send(Messages::WebPage::WheelEvent(event), m_pageID);
    m_processingWheelEvent = true;
}

void WebPageProxy::handleKeyboardEvent(const NativeWebKeyboardEvent& event)
{
    if (!isValid())
        return;

    m_keyEventQueue.append(event);

    process()->responsivenessTimer()->start();
    process()->send(Messages::WebPage::KeyEvent(event), m_pageID);
}

#if ENABLE(TOUCH_EVENTS)
void WebPageProxy::handleTouchEvent(const WebTouchEvent& event)
{
    if (!isValid())
        return;
    process()->send(Messages::WebPage::TouchEvent(event), m_pageID); 
}
#endif

void WebPageProxy::receivedPolicyDecision(PolicyAction action, WebFrameProxy* frame, uint64_t listenerID)
{
    if (!isValid())
        return;

    uint64_t downloadID = 0;
    if (action == PolicyDownload) {
        // Create a download proxy.
        downloadID = context()->createDownloadProxy();
    }

    // If we received a policy decision while in decidePolicyForMIMEType the decision will 
    // be sent back to the web process by decidePolicyForMIMEType. 
    if (m_inDecidePolicyForMIMEType) {
        m_syncMimeTypePolicyActionIsValid = true;
        m_syncMimeTypePolicyAction = action;
        m_syncMimeTypePolicyDownloadID = downloadID;
        return;
    }

    process()->send(Messages::WebPage::DidReceivePolicyDecision(frame->frameID(), listenerID, action, downloadID), m_pageID);
}

String WebPageProxy::pageTitle() const
{
    // Return the null string if there is no main frame (e.g. nothing has been loaded in the page yet, WebProcess has
    // crashed, page has been closed).
    if (!m_mainFrame)
        return String();

    return m_mainFrame->title();
}

void WebPageProxy::setUserAgent(const String& userAgent)
{
    if (m_userAgent == userAgent)
        return;
    m_userAgent = userAgent;

    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetUserAgent(m_userAgent), m_pageID);
}

void WebPageProxy::setApplicationNameForUserAgent(const String& applicationName)
{
    if (m_applicationNameForUserAgent == applicationName)
        return;

    m_applicationNameForUserAgent = applicationName;
    if (!m_customUserAgent.isEmpty())
        return;

    setUserAgent(standardUserAgent(m_applicationNameForUserAgent));
}

void WebPageProxy::setCustomUserAgent(const String& customUserAgent)
{
    if (m_customUserAgent == customUserAgent)
        return;

    m_customUserAgent = customUserAgent;

    if (m_customUserAgent.isEmpty()) {
        setUserAgent(standardUserAgent(m_applicationNameForUserAgent));
        return;
    }

    setUserAgent(m_customUserAgent);
}

bool WebPageProxy::supportsTextEncoding() const
{
    return !m_mainFrameHasCustomRepresentation && m_mainFrame && !m_mainFrame->isDisplayingStandaloneImageDocument();
}

void WebPageProxy::setCustomTextEncodingName(const String& encodingName)
{
    if (m_customTextEncodingName == encodingName)
        return;
    m_customTextEncodingName = encodingName;

    if (!isValid())
        return;
    process()->send(Messages::WebPage::SetCustomTextEncodingName(encodingName), m_pageID);
}

void WebPageProxy::terminateProcess()
{
    if (!isValid())
        return;

    process()->terminate();
}

#if !PLATFORM(CF) || defined(BUILDING_QT__)
PassRefPtr<WebData> WebPageProxy::sessionStateData(WebPageProxySessionStateFilterCallback, void* context) const
{
    // FIXME: Return session state data for saving Page state.
    return 0;
}

void WebPageProxy::restoreFromSessionStateData(WebData*)
{
    // FIXME: Restore the Page from the passed in session state data.
}
#endif

void WebPageProxy::setTextZoomFactor(double zoomFactor)
{
    if (!isValid())
        return;

    if (m_textZoomFactor == zoomFactor)
        return;

    m_textZoomFactor = zoomFactor;
    process()->send(Messages::WebPage::SetTextZoomFactor(m_textZoomFactor), m_pageID); 
}

void WebPageProxy::setPageZoomFactor(double zoomFactor)
{
    if (!isValid())
        return;

    if (m_pageZoomFactor == zoomFactor)
        return;

    m_pageZoomFactor = zoomFactor;
    process()->send(Messages::WebPage::SetPageZoomFactor(m_pageZoomFactor), m_pageID); 
}

void WebPageProxy::setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor)
{
    if (!isValid())
        return;

    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;
    process()->send(Messages::WebPage::SetPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor), m_pageID); 
}

void WebPageProxy::scaleWebView(double scale, const IntPoint& origin)
{
    if (!isValid())
        return;

    if (m_viewScaleFactor == scale)
        return;

    m_viewScaleFactor = scale;
    process()->send(Messages::WebPage::ScaleWebView(scale, origin), m_pageID);
}

void WebPageProxy::findString(const String& string, FindOptions options, unsigned maxMatchCount)
{
    process()->send(Messages::WebPage::FindString(string, options, maxMatchCount), m_pageID);
}

void WebPageProxy::hideFindUI()
{
    process()->send(Messages::WebPage::HideFindUI(), m_pageID);
}

void WebPageProxy::countStringMatches(const String& string, FindOptions options, unsigned maxMatchCount)
{
    process()->send(Messages::WebPage::CountStringMatches(string, options, maxMatchCount), m_pageID);
}
    
void WebPageProxy::runJavaScriptInMainFrame(const String& script, PassRefPtr<ScriptReturnValueCallback> prpCallback)
{
    RefPtr<ScriptReturnValueCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_scriptReturnValueCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::RunJavaScriptInMainFrame(script, callbackID), m_pageID);
}

void WebPageProxy::getRenderTreeExternalRepresentation(PassRefPtr<RenderTreeExternalRepresentationCallback> prpCallback)
{
    RefPtr<RenderTreeExternalRepresentationCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_renderTreeExternalRepresentationCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetRenderTreeExternalRepresentation(callbackID), m_pageID);
}

void WebPageProxy::getSourceForFrame(WebFrameProxy* frame, PassRefPtr<FrameSourceCallback> prpCallback)
{
    RefPtr<FrameSourceCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_frameSourceCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetSourceForFrame(frame->frameID(), callbackID), m_pageID);
}

void WebPageProxy::getContentsAsString(PassRefPtr<ContentsAsStringCallback> prpCallback)
{
    RefPtr<ContentsAsStringCallback> callback = prpCallback;
    uint64_t callbackID = callback->callbackID();
    m_contentsAsStringCallbacks.set(callbackID, callback.get());
    process()->send(Messages::WebPage::GetContentsAsString(callbackID), m_pageID);
}

void WebPageProxy::preferencesDidChange()
{
    if (!isValid())
        return;

    // FIXME: It probably makes more sense to send individual preference changes.
    // However, WebKitTestRunner depends on getting a preference change notification
    // even if nothing changed in UI process, so that overrides get removed.
    process()->send(Messages::WebPage::PreferencesDidChange(pageGroup()->preferences()->store()), m_pageID);
}

#if ENABLE(TILED_BACKING_STORE)
void WebPageProxy::setResizesToContentsUsingLayoutSize(const WebCore::IntSize& targetLayoutSize)
{
    process()->send(Messages::WebPage::SetResizesToContentsUsingLayoutSize(targetLayoutSize), m_pageID);
}
#endif

void WebPageProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    if (messageID.is<CoreIPC::MessageClassDrawingAreaProxy>()) {
        m_drawingArea->didReceiveMessage(connection, messageID, arguments);
        return;
    }

#if ENABLE(INSPECTOR)
    if (messageID.is<CoreIPC::MessageClassWebInspectorProxy>()) {
        if (WebInspectorProxy* inspector = this->inspector())
            inspector->didReceiveWebInspectorProxyMessage(connection, messageID, arguments);
        return;
    }
#endif

    didReceiveWebPageProxyMessage(connection, messageID, arguments);
}

void WebPageProxy::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
    if (messageID.is<CoreIPC::MessageClassDrawingAreaProxy>()) {
        m_drawingArea->didReceiveSyncMessage(connection, messageID, arguments, reply);
        return;
    }

#if ENABLE(INSPECTOR)
    if (messageID.is<CoreIPC::MessageClassWebInspectorProxy>()) {
        if (WebInspectorProxy* inspector = this->inspector())
            inspector->didReceiveSyncWebInspectorProxyMessage(connection, messageID, arguments, reply);
        return;
    }
#endif

    // FIXME: Do something with reply.
    didReceiveSyncWebPageProxyMessage(connection, messageID, arguments, reply);
}

#if PLATFORM(MAC)
void WebPageProxy::interpretKeyEvent(uint32_t type, Vector<KeypressCommand>& commandsList, uint32_t selectionStart, uint32_t selectionEnd, Vector<CompositionUnderline>& underlines)
{
    m_pageClient->interceptKeyEvent(m_keyEventQueue.first(), commandsList, selectionStart, selectionEnd, underlines);
}
#endif

void WebPageProxy::didCreateMainFrame(uint64_t frameID)
{
    ASSERT(!m_mainFrame);

    m_mainFrame = WebFrameProxy::create(this, frameID);
    process()->frameCreated(frameID, m_mainFrame.get());
}

void WebPageProxy::didCreateSubFrame(uint64_t frameID)
{
    ASSERT(m_mainFrame);
    process()->frameCreated(frameID, WebFrameProxy::create(this, frameID).get());
}

void WebPageProxy::didStartProgress()
{
    m_estimatedProgress = 0.0;
    
    m_loaderClient.didStartProgress(this);
}

void WebPageProxy::didChangeProgress(double value)
{
    m_estimatedProgress = value;
    
    m_loaderClient.didChangeProgress(this);
}

void WebPageProxy::didFinishProgress()
{
    m_estimatedProgress = 1.0;

    m_loaderClient.didFinishProgress(this);
}

void WebPageProxy::didStartProvisionalLoadForFrame(uint64_t frameID, const String& url, bool loadingSubstituteDataForUnreachableURL, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    if (!loadingSubstituteDataForUnreachableURL)
        frame->setUnreachableURL(String());

    frame->didStartProvisionalLoad(url);
    m_loaderClient.didStartProvisionalLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(uint64_t frameID, const String& url, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    frame->didReceiveServerRedirectForProvisionalLoad(url);
    m_loaderClient.didReceiveServerRedirectForProvisionalLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didFailProvisionalLoadForFrame(uint64_t frameID, const ResourceError& error, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    frame->didFailProvisionalLoad();
    m_loaderClient.didFailProvisionalLoadWithErrorForFrame(this, frame, error, userData.get());
}

void WebPageProxy::didCommitLoadForFrame(uint64_t frameID, const String& mimeType, bool frameHasCustomRepresentation, const PlatformCertificateInfo& certificateInfo, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    frame->setMIMEType(mimeType);
    frame->setIsFrameSet(false);
    frame->setCertificateInfo(WebCertificateInfo::create(certificateInfo));
    frame->didCommitLoad();

    if (frame->isMainFrame()) {
        m_mainFrameHasCustomRepresentation = frameHasCustomRepresentation;
        m_pageClient->didCommitLoadForMainFrame(frameHasCustomRepresentation);
    }

    m_loaderClient.didCommitLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didFinishDocumentLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    m_loaderClient.didFinishDocumentLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didFinishLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    frame->didFinishLoad();
    m_loaderClient.didFinishLoadForFrame(this, frame, userData.get());
}

void WebPageProxy::didFailLoadForFrame(uint64_t frameID, const ResourceError& error, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    frame->didFailLoad();

    m_loaderClient.didFailLoadWithErrorForFrame(this, frame, error, userData.get());
}

void WebPageProxy::didSameDocumentNavigationForFrame(uint64_t frameID, uint32_t opaqueSameDocumentNavigationType, const String& url, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    frame->didSameDocumentNavigation(url);

    m_loaderClient.didSameDocumentNavigationForFrame(this, frame, static_cast<SameDocumentNavigationType>(opaqueSameDocumentNavigationType), userData.get());
}

void WebPageProxy::didReceiveTitleForFrame(uint64_t frameID, const String& title, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    frame->didChangeTitle(title);
    
    m_loaderClient.didReceiveTitleForFrame(this, title, frame, userData.get());
}

void WebPageProxy::didFirstLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    m_loaderClient.didFirstLayoutForFrame(this, frame, userData.get());
}

void WebPageProxy::didFirstVisuallyNonEmptyLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    m_loaderClient.didFirstVisuallyNonEmptyLayoutForFrame(this, frame, userData.get());
}

void WebPageProxy::didRemoveFrameFromHierarchy(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    m_loaderClient.didRemoveFrameFromHierarchy(this, frame, userData.get());
}

void WebPageProxy::didDisplayInsecureContentForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    m_loaderClient.didDisplayInsecureContentForFrame(this, frame, userData.get());
}

void WebPageProxy::didRunInsecureContentForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);

    m_loaderClient.didRunInsecureContentForFrame(this, frame, userData.get());
}

void WebPageProxy::frameDidBecomeFrameSet(uint64_t frameID, bool value)
{
    WebFrameProxy* frame = process()->webFrame(frameID);

    frame->setIsFrameSet(value);
}

// PolicyClient

void WebPageProxy::decidePolicyForNavigationAction(uint64_t frameID, uint32_t opaqueNavigationType, uint32_t opaqueModifiers, int32_t opaqueMouseButton, const String& url, uint64_t listenerID)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    NavigationType navigationType = static_cast<NavigationType>(opaqueNavigationType);
    WebEvent::Modifiers modifiers = static_cast<WebEvent::Modifiers>(opaqueModifiers);
    WebMouseEvent::Button mouseButton = static_cast<WebMouseEvent::Button>(opaqueMouseButton);
    
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!m_policyClient.decidePolicyForNavigationAction(this, navigationType, modifiers, mouseButton, url, frame, listener.get()))
        listener->use();
}

void WebPageProxy::decidePolicyForNewWindowAction(uint64_t frameID, uint32_t opaqueNavigationType, uint32_t opaqueModifiers, int32_t opaqueMouseButton, const String& url, uint64_t listenerID)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    NavigationType navigationType = static_cast<NavigationType>(opaqueNavigationType);
    WebEvent::Modifiers modifiers = static_cast<WebEvent::Modifiers>(opaqueModifiers);
    WebMouseEvent::Button mouseButton = static_cast<WebMouseEvent::Button>(opaqueMouseButton);

    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);
    if (!m_policyClient.decidePolicyForNewWindowAction(this, navigationType, modifiers, mouseButton, url, frame, listener.get()))
        listener->use();
}

void WebPageProxy::decidePolicyForMIMEType(uint64_t frameID, const String& MIMEType, const String& url, uint64_t listenerID, bool& receivedPolicyAction, uint64_t& policyAction, uint64_t& downloadID)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    RefPtr<WebFramePolicyListenerProxy> listener = frame->setUpPolicyListenerProxy(listenerID);

    ASSERT(!m_inDecidePolicyForMIMEType);

    m_inDecidePolicyForMIMEType = true;
    m_syncMimeTypePolicyActionIsValid = false;

    if (!m_policyClient.decidePolicyForMIMEType(this, MIMEType, url, frame, listener.get()))
        listener->use();

    m_inDecidePolicyForMIMEType = false;

    // Check if we received a policy decision already. If we did, we can just pass it back.
    if (m_syncMimeTypePolicyActionIsValid) {
        receivedPolicyAction = true;
        policyAction = m_syncMimeTypePolicyAction;
        downloadID = m_syncMimeTypePolicyDownloadID;
    }
}

// FormClient

void WebPageProxy::willSubmitForm(uint64_t frameID, uint64_t sourceFrameID, const StringPairVector& textFieldValues, uint64_t listenerID, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebFrameProxy* frame = process()->webFrame(frameID);
    WebFrameProxy* sourceFrame = process()->webFrame(sourceFrameID);

    RefPtr<WebFormSubmissionListenerProxy> listener = frame->setUpFormSubmissionListenerProxy(listenerID);
    if (!m_formClient.willSubmitForm(this, frame, sourceFrame, textFieldValues.stringPairVector(), userData.get(), listener.get()))
        listener->continueSubmission();
}

// ResourceLoad Client

void WebPageProxy::didInitiateLoadForResource(uint64_t frameID, uint64_t resourceIdentifier, const ResourceRequest& request)
{
    WebFrameProxy* frame = process()->webFrame(frameID);

    m_resourceLoadClient.didInitiateLoadForResource(this, frame, resourceIdentifier, request);
}

void WebPageProxy::didSendRequestForResource(uint64_t frameID, uint64_t resourceIdentifier, const ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    WebFrameProxy* frame = process()->webFrame(frameID);

    m_resourceLoadClient.didSendRequestForResource(this, frame, resourceIdentifier, request, redirectResponse);
}

void WebPageProxy::didReceiveResponseForResource(uint64_t frameID, uint64_t resourceIdentifier, const ResourceResponse& response)
{
    WebFrameProxy* frame = process()->webFrame(frameID);

    m_resourceLoadClient.didReceiveResponseForResource(this, frame, resourceIdentifier, response);
}

void WebPageProxy::didReceiveContentLengthForResource(uint64_t frameID, uint64_t resourceIdentifier, uint64_t contentLength)
{
    WebFrameProxy* frame = process()->webFrame(frameID);

    m_resourceLoadClient.didReceiveContentLengthForResource(this, frame, resourceIdentifier, contentLength);
}

void WebPageProxy::didFinishLoadForResource(uint64_t frameID, uint64_t resourceIdentifier)
{
    WebFrameProxy* frame = process()->webFrame(frameID);

    m_resourceLoadClient.didFinishLoadForResource(this, frame, resourceIdentifier);
}

void WebPageProxy::didFailLoadForResource(uint64_t frameID, uint64_t resourceIdentifier, const ResourceError& error)
{
    WebFrameProxy* frame = process()->webFrame(frameID);

    m_resourceLoadClient.didFailLoadForResource(this, frame, resourceIdentifier, error);
}


// UIClient

void WebPageProxy::createNewPage(const WindowFeatures& windowFeatures, uint32_t opaqueModifiers, int32_t opaqueMouseButton, uint64_t& newPageID, WebPageCreationParameters& newPageParameters)
{
    RefPtr<WebPageProxy> newPage = m_uiClient.createNewPage(this, windowFeatures, static_cast<WebEvent::Modifiers>(opaqueModifiers), static_cast<WebMouseEvent::Button>(opaqueMouseButton));
    if (newPage) {
        // FIXME: Pass the real size.
        newPageID = newPage->pageID();
        newPageParameters = newPage->creationParameters(IntSize(100, 100));
    } else
        newPageID = 0;
}
    
void WebPageProxy::showPage()
{
    m_uiClient.showPage(this);
}

void WebPageProxy::closePage()
{
    m_uiClient.close(this);
}

void WebPageProxy::runJavaScriptAlert(uint64_t frameID, const String& message)
{
    m_uiClient.runJavaScriptAlert(this, message, process()->webFrame(frameID));
}

void WebPageProxy::runJavaScriptConfirm(uint64_t frameID, const String& message, bool& result)
{
    result = m_uiClient.runJavaScriptConfirm(this, message, process()->webFrame(frameID));
}

void WebPageProxy::runJavaScriptPrompt(uint64_t frameID, const String& message, const String& defaultValue, String& result)
{
    result = m_uiClient.runJavaScriptPrompt(this, message, defaultValue, process()->webFrame(frameID));
}

void WebPageProxy::setStatusText(const String& text)
{
    m_uiClient.setStatusText(this, text);
}

void WebPageProxy::mouseDidMoveOverElement(uint32_t opaqueModifiers, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    WebEvent::Modifiers modifiers = static_cast<WebEvent::Modifiers>(opaqueModifiers);

    m_uiClient.mouseDidMoveOverElement(this, modifiers, userData.get());
}

void WebPageProxy::missingPluginButtonClicked(const String& mimeType, const String& url)
{
    m_uiClient.missingPluginButtonClicked(this, mimeType, url);
}

void WebPageProxy::setToolbarsAreVisible(bool toolbarsAreVisible)
{
    m_uiClient.setToolbarsAreVisible(this, toolbarsAreVisible);
}

void WebPageProxy::getToolbarsAreVisible(bool& toolbarsAreVisible)
{
    toolbarsAreVisible = m_uiClient.toolbarsAreVisible(this);
}

void WebPageProxy::setMenuBarIsVisible(bool menuBarIsVisible)
{
    m_uiClient.setMenuBarIsVisible(this, menuBarIsVisible);
}

void WebPageProxy::getMenuBarIsVisible(bool& menuBarIsVisible)
{
    menuBarIsVisible = m_uiClient.menuBarIsVisible(this);
}

void WebPageProxy::setStatusBarIsVisible(bool statusBarIsVisible)
{
    m_uiClient.setStatusBarIsVisible(this, statusBarIsVisible);
}

void WebPageProxy::getStatusBarIsVisible(bool& statusBarIsVisible)
{
    statusBarIsVisible = m_uiClient.statusBarIsVisible(this);
}

void WebPageProxy::setIsResizable(bool isResizable)
{
    m_uiClient.setIsResizable(this, isResizable);
}

void WebPageProxy::getIsResizable(bool& isResizable)
{
    isResizable = m_uiClient.isResizable(this);
}

void WebPageProxy::setWindowFrame(const FloatRect& newWindowFrame)
{
    m_uiClient.setWindowFrame(this, m_pageClient->convertToDeviceSpace(newWindowFrame));
}

void WebPageProxy::getWindowFrame(FloatRect& newWindowFrame)
{
    newWindowFrame = m_pageClient->convertToUserSpace(m_uiClient.windowFrame(this));
}

void WebPageProxy::canRunBeforeUnloadConfirmPanel(bool& canRun)
{
    canRun = m_uiClient.canRunBeforeUnloadConfirmPanel();
}

void WebPageProxy::runBeforeUnloadConfirmPanel(const String& message, uint64_t frameID, bool& shouldClose)
{
    shouldClose = m_uiClient.runBeforeUnloadConfirmPanel(this, message, process()->webFrame(frameID));
}

#if ENABLE(TILED_BACKING_STORE)
void WebPageProxy::pageDidRequestScroll(const IntSize& delta)
{
    m_pageClient->pageDidRequestScroll(delta);
}
#endif

void WebPageProxy::didChangeViewportData(const ViewportArguments& args)
{
    m_pageClient->setViewportArguments(args);
}

void WebPageProxy::pageDidScroll()
{
    m_uiClient.pageDidScroll(this);
}

void WebPageProxy::runOpenPanel(uint64_t frameID, const WebOpenPanelParameters::Data& data)
{
    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = 0;
    }

    m_openPanelResultListener = WebOpenPanelResultListenerProxy::create(this);

    if (!m_uiClient.runOpenPanel(this, process()->webFrame(frameID), data, m_openPanelResultListener.get()))
        didCancelForOpenPanel();
}

#if PLATFORM(QT)
void WebPageProxy::didChangeContentsSize(const WebCore::IntSize& size)
{
    m_pageClient->didChangeContentsSize(size);
}

void WebPageProxy::didFindZoomableArea(const WebCore::IntRect& area)
{
    m_pageClient->didFindZoomableArea(area);
}

void WebPageProxy::findZoomableAreaForPoint(const WebCore::IntPoint& point)
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::FindZoomableAreaForPoint(point), m_pageID);
}
#endif

void WebPageProxy::didDraw()
{
    m_uiClient.didDraw(this);
}

// Inspector

#if ENABLE(INSPECTOR)

WebInspectorProxy* WebPageProxy::inspector()
{
    if (isClosed() || !isValid())
        return 0;
    if (!m_inspector)
        m_inspector = WebInspectorProxy::create(this);
    return m_inspector.get();
}

#endif

// BackForwardList

void WebPageProxy::backForwardAddItem(uint64_t itemID)
{
    m_backForwardList->addItem(process()->webBackForwardItem(itemID));
}

void WebPageProxy::backForwardGoToItem(uint64_t itemID)
{
    m_backForwardList->goToItem(process()->webBackForwardItem(itemID));
}

void WebPageProxy::backForwardItemAtIndex(int32_t index, uint64_t& itemID)
{
    WebBackForwardListItem* item = m_backForwardList->itemAtIndex(index);
    itemID = item ? item->itemID() : 0;
}

void WebPageProxy::backForwardBackListCount(int32_t& count)
{
    count = m_backForwardList->backListCount();
}

void WebPageProxy::backForwardForwardListCount(int32_t& count)
{
    count = m_backForwardList->forwardListCount();
}

#if PLATFORM(MAC)
// Selection change support
void WebPageProxy::didSelectionChange(bool isNone, bool isContentEditable, bool isPasswordField, bool hasComposition, uint64_t location, uint64_t length)
{
    m_pageClient->selectionChanged(isNone, isContentEditable, isPasswordField, hasComposition, location, length);
}

// Complex text input support for plug-ins.
void WebPageProxy::sendComplexTextInputToPlugin(uint64_t pluginComplexTextInputIdentifier, const String& textInput)
{
    if (!isValid())
        return;
    
    process()->send(Messages::WebPage::SendComplexTextInputToPlugin(pluginComplexTextInputIdentifier, textInput), m_pageID);
}
    
#else    
void WebPageProxy::didChangeSelection(bool isNone, bool isContentEditable, bool isPasswordField, bool hasComposition)
{
    m_pageClient->selectionChanged(isNone, isContentEditable, isPasswordField, hasComposition);
}
#endif

#if PLATFORM(WIN)
void WebPageProxy::didChangeCompositionSelection(bool hasComposition)
{
    m_pageClient->compositionSelectionChanged(hasComposition);
}

void WebPageProxy::confirmComposition(const String& compositionString)
{
    process()->send(Messages::WebPage::ConfirmComposition(compositionString), m_pageID);
}

void WebPageProxy::setComposition(const String& compositionString, Vector<WebCore::CompositionUnderline>& underlines, int cursorPosition)
{
    process()->send(Messages::WebPage::SetComposition(compositionString, underlines, cursorPosition), m_pageID);
}
#endif
    
// Undo management

void WebPageProxy::registerEditCommandForUndo(uint64_t commandID, uint32_t editAction)
{
    registerEditCommand(WebEditCommandProxy::create(commandID, static_cast<EditAction>(editAction), this), Undo);
}

void WebPageProxy::clearAllEditCommands()
{
    m_pageClient->clearAllEditCommands();
}

void WebPageProxy::didCountStringMatches(const String& string, uint32_t matchCount)
{
    m_findClient.didCountStringMatches(this, string, matchCount);
}

void WebPageProxy::setFindIndicator(const FloatRect& selectionRect, const Vector<FloatRect>& textRects, const SharedMemory::Handle& contentImageHandle, bool fadeOut)
{
    RefPtr<FindIndicator> findIndicator = FindIndicator::create(selectionRect, textRects, contentImageHandle);
    m_pageClient->setFindIndicator(findIndicator.release(), fadeOut);
}

void WebPageProxy::didFindString(const String& string, uint32_t matchCount)
{
    m_findClient.didFindString(this, string, matchCount);
}

void WebPageProxy::didFailToFindString(const String& string)
{
    m_findClient.didFailToFindString(this, string);
}

void WebPageProxy::valueChangedForPopupMenu(WebPopupMenuProxy*, int32_t newSelectedIndex)
{
    process()->send(Messages::WebPage::DidChangeSelectedIndexForActivePopupMenu(newSelectedIndex), m_pageID);
}

void WebPageProxy::setTextFromItemForPopupMenu(WebPopupMenuProxy*, int32_t index)
{
    process()->send(Messages::WebPage::SetTextForActivePopupMenu(index), m_pageID);
}

void WebPageProxy::showPopupMenu(const IntRect& rect, const Vector<WebPopupItem>& items, int32_t selectedIndex, const PlatformPopupMenuData& data)
{
    if (m_activePopupMenu)
        m_activePopupMenu->hidePopupMenu();
    else
        m_activePopupMenu = m_pageClient->createPopupMenuProxy(this);

    m_activePopupMenu->showPopupMenu(rect, items, data, selectedIndex);
    m_activePopupMenu = 0;
}

void WebPageProxy::hidePopupMenu()
{
    if (!m_activePopupMenu)
        return;

    m_activePopupMenu->hidePopupMenu();
    m_activePopupMenu = 0;
}

void WebPageProxy::showContextMenu(const WebCore::IntPoint& menuLocation, const Vector<WebContextMenuItemData>& proposedItems, CoreIPC::ArgumentDecoder* arguments)
{
    RefPtr<APIObject> userData;
    WebContextUserMessageDecoder messageDecoder(userData, context());
    if (!arguments->decode(messageDecoder))
        return;

    if (m_activeContextMenu)
        m_activeContextMenu->hideContextMenu();
    else
        m_activeContextMenu = m_pageClient->createContextMenuProxy(this);

    // Give the PageContextMenuClient one last swipe at changing the menu.
    Vector<WebContextMenuItemData> items;
        
    if (!m_contextMenuClient.getContextMenuFromProposedMenu(this, proposedItems, items, userData.get())) {
        m_activeContextMenu->showContextMenu(menuLocation, proposedItems);
        return;
    }
    
    if (items.size())
        m_activeContextMenu->showContextMenu(menuLocation, items);
}

void WebPageProxy::contextMenuItemSelected(const WebContextMenuItemData& item)
{
    // Application custom items don't need to round-trip through to WebCore in the WebProcess.
    if (item.action() >= ContextMenuItemBaseApplicationTag) {
        m_contextMenuClient.customContextMenuItemSelected(this, item);
        return;
    }
    
    process()->send(Messages::WebPage::DidSelectItemFromActiveContextMenu(item), m_pageID);
}

void WebPageProxy::didChooseFilesForOpenPanel(const Vector<String>& fileURLs)
{
    if (!isValid())
        return;

    // FIXME: This also needs to send a sandbox extension for these paths.
    process()->send(Messages::WebPage::DidChooseFilesForOpenPanel(fileURLs), m_pageID);

    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = 0;
}

void WebPageProxy::didCancelForOpenPanel()
{
    if (!isValid())
        return;

    process()->send(Messages::WebPage::DidCancelForOpenPanel(), m_pageID);
    
    m_openPanelResultListener->invalidate();
    m_openPanelResultListener = 0;
}

void WebPageProxy::unmarkAllMisspellings()
{
    process()->send(Messages::WebPage::UnmarkAllMisspellings(), m_pageID);
}

void WebPageProxy::unmarkAllBadGrammar()
{
    process()->send(Messages::WebPage::UnmarkAllBadGrammar(), m_pageID);
}

void WebPageProxy::registerEditCommand(PassRefPtr<WebEditCommandProxy> commandProxy, UndoOrRedo undoOrRedo)
{
    m_pageClient->registerEditCommand(commandProxy, undoOrRedo);
}

void WebPageProxy::addEditCommand(WebEditCommandProxy* command)
{
    m_editCommandSet.add(command);
}

void WebPageProxy::removeEditCommand(WebEditCommandProxy* command)
{
    m_editCommandSet.remove(command);

    if (!isValid())
        return;
    process()->send(Messages::WebPage::DidRemoveEditCommand(command->commandID()), m_pageID);
}

// Other

void WebPageProxy::takeFocus(bool direction)
{
    m_pageClient->takeFocus(direction);
}

void WebPageProxy::setToolTip(const String& toolTip)
{
    String oldToolTip = m_toolTip;
    m_toolTip = toolTip;
    m_pageClient->toolTipChanged(oldToolTip, m_toolTip);
}

void WebPageProxy::setCursor(const WebCore::Cursor& cursor)
{
    m_pageClient->setCursor(cursor);
}

void WebPageProxy::didValidateMenuItem(const String& commandName, bool isEnabled, int32_t state)
{
    m_pageClient->setEditCommandState(commandName, isEnabled, state);
}

void WebPageProxy::didReceiveEvent(uint32_t opaqueType, bool handled)
{
    WebEvent::Type type = static_cast<WebEvent::Type>(opaqueType);

    switch (type) {
    case WebEvent::MouseMove:
        break;

    case WebEvent::MouseDown:
    case WebEvent::MouseUp:
    case WebEvent::Wheel:
    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char:
        process()->responsivenessTimer()->stop();
        break;
    }

    switch (type) {
    case WebEvent::MouseMove:
    case WebEvent::MouseDown:
    case WebEvent::MouseUp:
        break;

    case WebEvent::Wheel: {
        m_processingWheelEvent = false;
        if (m_nextWheelEvent) {
            handleWheelEvent(*m_nextWheelEvent);
            m_nextWheelEvent.clear();
        }
        break;
    }

    case WebEvent::KeyDown:
    case WebEvent::KeyUp:
    case WebEvent::RawKeyDown:
    case WebEvent::Char: {
        NativeWebKeyboardEvent event = m_keyEventQueue.first();
        ASSERT(type == event.type());
        m_keyEventQueue.removeFirst();

        if (handled)
            break;

        m_pageClient->didNotHandleKeyEvent(event);
        m_uiClient.didNotHandleKeyEvent(this, event);
        break;
    }
    }
}

void WebPageProxy::didGetContentsAsString(const String& resultString, uint64_t callbackID)
{
    RefPtr<ContentsAsStringCallback> callback = m_contentsAsStringCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(resultString.impl());
}

void WebPageProxy::didRunJavaScriptInMainFrame(const String& resultString, uint64_t callbackID)
{
    RefPtr<ScriptReturnValueCallback> callback = m_scriptReturnValueCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(resultString.impl());
}

void WebPageProxy::didGetRenderTreeExternalRepresentation(const String& resultString, uint64_t callbackID)
{
    RefPtr<RenderTreeExternalRepresentationCallback> callback = m_renderTreeExternalRepresentationCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(resultString.impl());
}

void WebPageProxy::didGetSourceForFrame(const String& resultString, uint64_t callbackID)
{
    RefPtr<FrameSourceCallback> callback = m_frameSourceCallbacks.take(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(resultString.impl());
}

void WebPageProxy::focusedFrameChanged(uint64_t frameID)
{
    m_focusedFrame = frameID ? process()->webFrame(frameID) : 0;
}

#if USE(ACCELERATED_COMPOSITING)
void WebPageProxy::didChangeAcceleratedCompositing(bool compositing, DrawingAreaInfo& drawingAreaInfo)
{
    if (compositing)
        didEnterAcceleratedCompositing();
    else
        didLeaveAcceleratedCompositing();

    drawingAreaInfo = drawingArea()->info();
}
#endif

void WebPageProxy::processDidBecomeUnresponsive()
{
    m_loaderClient.processDidBecomeUnresponsive(this);
}

void WebPageProxy::processDidBecomeResponsive()
{
    m_loaderClient.processDidBecomeResponsive(this);
}

void WebPageProxy::processDidCrash()
{
    ASSERT(m_pageClient);

    m_isValid = false;

    if (m_mainFrame)
        m_urlAtProcessExit = m_mainFrame->url();

    m_mainFrame = 0;

#if ENABLE(INSPECTOR)
    if (m_inspector) {
        m_inspector->invalidate();
        m_inspector = 0;
    }
#endif

    if (m_openPanelResultListener) {
        m_openPanelResultListener->invalidate();
        m_openPanelResultListener = 0;
    }

    m_toolTip = String();

    invalidateCallbackMap(m_contentsAsStringCallbacks);
    invalidateCallbackMap(m_frameSourceCallbacks);
    invalidateCallbackMap(m_renderTreeExternalRepresentationCallbacks);
    invalidateCallbackMap(m_scriptReturnValueCallbacks);

    Vector<WebEditCommandProxy*> editCommandVector;
    copyToVector(m_editCommandSet, editCommandVector);
    m_editCommandSet.clear();
    for (size_t i = 0, size = editCommandVector.size(); i < size; ++i)
        editCommandVector[i]->invalidate();
    m_pageClient->clearAllEditCommands();

    m_activePopupMenu = 0;

    m_estimatedProgress = 0.0;

    m_pageClient->processDidCrash();
    m_loaderClient.processDidCrash(this);
}

WebPageCreationParameters WebPageProxy::creationParameters(const IntSize& size) const
{
    WebPageCreationParameters parameters;
    parameters.viewSize = size;
    parameters.drawingAreaInfo = m_drawingArea->info();
    parameters.store = m_pageGroup->preferences()->store();
    parameters.pageGroupData = m_pageGroup->data();
    parameters.drawsBackground = m_drawsBackground;
    parameters.drawsTransparentBackground = m_drawsTransparentBackground;
    parameters.userAgent = userAgent();

#if PLATFORM(WIN)
    parameters.nativeWindow = m_pageClient->nativeWindow();
#endif

    return parameters;
}

#if USE(ACCELERATED_COMPOSITING)
void WebPageProxy::didEnterAcceleratedCompositing()
{
    m_pageClient->pageDidEnterAcceleratedCompositing();
}

void WebPageProxy::didLeaveAcceleratedCompositing()
{
    m_pageClient->pageDidLeaveAcceleratedCompositing();
}
#endif // USE(ACCELERATED_COMPOSITING)

void WebPageProxy::backForwardClear()
{
    m_backForwardList->clear();
}

void WebPageProxy::canAuthenticateAgainstProtectionSpaceInFrame(uint64_t frameID, const WebCore::ProtectionSpace& coreProtectionSpace, bool& canAuthenticate)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    RefPtr<WebProtectionSpace> protectionSpace = WebProtectionSpace::create(coreProtectionSpace);
    
    canAuthenticate = m_loaderClient.canAuthenticateAgainstProtectionSpaceInFrame(this, frame, protectionSpace.get());
}

void WebPageProxy::didReceiveAuthenticationChallenge(uint64_t frameID, const WebCore::AuthenticationChallenge& coreChallenge, uint64_t challengeID)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    RefPtr<AuthenticationChallengeProxy> authenticationChallenge = AuthenticationChallengeProxy::create(coreChallenge, challengeID, this);
    
    m_loaderClient.didReceiveAuthenticationChallengeInFrame(this, frame, authenticationChallenge.get());
}

void WebPageProxy::exceededDatabaseQuota(uint64_t frameID, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentUsage, uint64_t expectedUsage, uint64_t& newQuota)
{
    WebFrameProxy* frame = process()->webFrame(frameID);
    RefPtr<WebSecurityOrigin> origin = WebSecurityOrigin::create(originIdentifier);

    newQuota = m_uiClient.exceededDatabaseQuota(this, frame, origin.get(), databaseName, displayName, currentQuota, currentUsage, expectedUsage);
}

void WebPageProxy::didFinishLoadingDataForCustomRepresentation(const CoreIPC::DataReference& dataReference)
{
    m_pageClient->didFinishLoadingDataForCustomRepresentation(dataReference);
}

#if PLATFORM(MAC)
void WebPageProxy::setComplexTextInputEnabled(uint64_t pluginComplexTextInputIdentifier, bool complexTextInputEnabled)
{
    m_pageClient->setComplexTextInputEnabled(pluginComplexTextInputIdentifier, complexTextInputEnabled);
}
#endif

} // namespace WebKit
