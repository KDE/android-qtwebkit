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

#include "WebFrameLoaderClient.h"

#define DISABLE_NOT_IMPLEMENTED_WARNINGS 1
#include "NotImplemented.h"

#include "InjectedBundleUserMessageCoders.h"
#include "PlatformCertificateInfo.h"
#include "PluginView.h"
#include "StringPairVector.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebEvent.h"
#include "WebFrame.h"
#include "WebFrameNetworkingContext.h"
#include "WebNavigationDataStore.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessageKinds.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSObject.h>
#include <WebCore/Chrome.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FormState.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/Page.h>
#include <WebCore/PluginData.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ResourceError.h>
#include <WebCore/UIEventWithKeyState.h>
#include <WebCore/Widget.h>
#include <WebCore/WindowFeatures.h>

using namespace WebCore;

namespace WebKit {

WebFrameLoaderClient::WebFrameLoaderClient(WebFrame* frame)
    : m_frame(frame)
    , m_hasSentResponseToPluginView(false)
{
}

WebFrameLoaderClient::~WebFrameLoaderClient()
{
}
    
void WebFrameLoaderClient::frameLoaderDestroyed()
{
    m_frame->invalidate();

    // Balances explicit ref() in WebFrame::createMainFrame and WebFrame::createSubframe.
    m_frame->deref();
}

bool WebFrameLoaderClient::hasHTMLView() const
{
    return true;
}

bool WebFrameLoaderClient::hasWebView() const
{
    return m_frame->page();
}

void WebFrameLoaderClient::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::forceLayout()
{
    notImplemented();
}

void WebFrameLoaderClient::forceLayoutForNonHTML()
{
    notImplemented();
}

void WebFrameLoaderClient::setCopiesOnScroll()
{
    notImplemented();
}

void WebFrameLoaderClient::detachedFromParent2()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didRemoveFrameFromHierarchy(webPage, m_frame, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidRemoveFrameFromHierarchy(m_frame->frameID(), InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());

}

void WebFrameLoaderClient::detachedFromParent3()
{
    notImplemented();
}

void WebFrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&)
{
    notImplemented();
}


void WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse)
{
    notImplemented();
}

bool WebFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*, unsigned long identifier)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&)    
{
    notImplemented();
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool WebFrameLoaderClient::canAuthenticateAgainstProtectionSpace(DocumentLoader*, unsigned long identifier, const ProtectionSpace&)
{
    notImplemented();
    return false;
}
#endif

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&)
{
    notImplemented();
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length)
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::dispatchDidLoadResourceByXMLHttpRequest(unsigned long identifier, const String&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didHandleOnloadEventsForFrame(webPage, m_frame);
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    DocumentLoader* provisionalLoader = m_frame->coreFrame()->loader()->provisionalDocumentLoader();
    const String& url = provisionalLoader->url().string();
    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didReceiveServerRedirectForProvisionalLoadForFrame(webPage, m_frame, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidReceiveServerRedirectForProvisionalLoadForFrame(m_frame->frameID(), url, InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didChangeLocationWithinPageForFrame(webPage, m_frame);
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const KURL& url, double interval, double fireDate)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().willPerformClientRedirectForFrame(webPage, m_frame, url.string(), interval, fireDate);
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didChangeLocationWithinPageForFrame(webPage, m_frame);
}

void WebFrameLoaderClient::dispatchDidPushStateWithinPage()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReplaceStateWithinPage()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidPopStateWithinPage()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchWillClose()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidReceiveIcon()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->findController().hideFindUI();
    
    DocumentLoader* provisionalLoader = m_frame->coreFrame()->loader()->provisionalDocumentLoader();
    const String& url = provisionalLoader->url().string();
    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didStartProvisionalLoadForFrame(webPage, m_frame, userData);


    bool loadingSubstituteDataForUnreachableURL = !provisionalLoader->unreachableURL().isNull();

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidStartProvisionalLoadForFrame(m_frame->frameID(), url, loadingSubstituteDataForUnreachableURL, InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didReceiveTitleForFrame(webPage, title, m_frame, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidReceiveTitleForFrame(m_frame->frameID(), title, InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());
}

void WebFrameLoaderClient::dispatchDidChangeIcons()
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    const ResourceResponse& response = m_frame->coreFrame()->loader()->documentLoader()->response();
    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didCommitLoadForFrame(webPage, m_frame, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidCommitLoadForFrame(m_frame->frameID(), response.mimeType(), PlatformCertificateInfo(response), InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError& error)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFailProvisionalLoadWithErrorForFrame(webPage, m_frame, error, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidFailProvisionalLoadForFrame(m_frame->frameID(), error, InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());
    
    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFailLoad(m_frame, error.isCancellation());
}

void WebFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFailLoadWithErrorForFrame(webPage, m_frame, error, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidFailLoadForFrame(m_frame->frameID(), error, InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFailLoad(m_frame, error.isCancellation());
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFinishDocumentLoadForFrame(webPage, m_frame, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidFinishDocumentLoadForFrame(m_frame->frameID(), InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFinishLoadForFrame(webPage, m_frame, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidFinishLoadForFrame(m_frame->frameID(), InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());

    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFinishLoad(m_frame);
}

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFirstLayoutForFrame(webPage, m_frame, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidFirstLayoutForFrame(m_frame->frameID(), InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());
}

void WebFrameLoaderClient::dispatchDidFirstVisuallyNonEmptyLayout()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    RefPtr<APIObject> userData;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didFirstVisuallyNonEmptyLayoutForFrame(webPage, m_frame, userData);

    // Notify the UIProcess.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidFirstVisuallyNonEmptyLayoutForFrame(m_frame->frameID(), InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());
}

Frame* WebFrameLoaderClient::dispatchCreatePage()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return 0;

    // Just call through to the chrome client.
    Page* newPage = webPage->corePage()->chrome()->createWindow(m_frame->coreFrame(), FrameLoadRequest(), WindowFeatures());
    if (!newPage)
        return 0;
    
    return newPage->mainFrame();
}

void WebFrameLoaderClient::dispatchShow()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->show();
}

static uint32_t modifiersForNavigationAction(const NavigationAction& navigationAction)
{
    uint32_t modifiers = 0;
    if (const UIEventWithKeyState* keyStateEvent = findEventWithKeyState(const_cast<Event*>(navigationAction.event()))) {
        if (keyStateEvent->shiftKey())
            modifiers |= WebEvent::ShiftKey;
        if (keyStateEvent->ctrlKey())
            modifiers |= WebEvent::ControlKey;
        if (keyStateEvent->altKey())
            modifiers |= WebEvent::AltKey;
        if (keyStateEvent->metaKey())
            modifiers |= WebEvent::MetaKey;
    }

    return modifiers;
}

static const MouseEvent* findMouseEvent(const Event* event)
{
    for (const Event* e = event; e; e = e->underlyingEvent()) {
        if (e->isMouseEvent())
            return static_cast<const MouseEvent*>(e);
    }
    return 0;
}

static int32_t mouseButtonForNavigationAction(const NavigationAction& navigationAction)
{
    const MouseEvent* mouseEvent = findMouseEvent(navigationAction.event());
    if (!mouseEvent)
        return -1;

    if (!mouseEvent->buttonDown())
        return -1;

    return mouseEvent->button();
}


void WebFrameLoaderClient::dispatchDecidePolicyForMIMEType(FramePolicyFunction function, const String& MIMEType, const ResourceRequest& request)
{
    if (m_frame->coreFrame()->loader()->documentLoader()->url().isEmpty() && request.url() == blankURL()) {
        // WebKit2 loads initial about:blank documents synchronously, without consulting the policy delegate
        ASSERT(m_frame->coreFrame()->loader()->stateMachine()->committingFirstRealLoad());
        (m_frame->coreFrame()->loader()->policyChecker()->*function)(PolicyUse);
        return;
    }
    
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    uint64_t listenerID = m_frame->setUpPolicyListener(function);
    const String& url = request.url().string(); // FIXME: Pass entire request.

    WebProcess::shared().connection()->send(Messages::WebPageProxy::DecidePolicyForMIMEType(m_frame->frameID(), MIMEType, url, listenerID), webPage->pageID());
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction function, const NavigationAction& navigationAction, const ResourceRequest& request, PassRefPtr<FormState>, const String& frameName)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    uint64_t listenerID = m_frame->setUpPolicyListener(function);

    // FIXME: Pass more than just the navigation action type.
    // FIXME: Pass the frame name.
    const String& url = request.url().string(); // FIXME: Pass entire request.

    uint32_t navigationType = static_cast<uint32_t>(navigationAction.type());
    uint32_t modifiers = modifiersForNavigationAction(navigationAction);
    int32_t mouseButton = mouseButtonForNavigationAction(navigationAction);

    WebProcess::shared().connection()->send(Messages::WebPageProxy::DecidePolicyForNewWindowAction(m_frame->frameID(), navigationType, modifiers, mouseButton, url, listenerID), webPage->pageID());
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(FramePolicyFunction function, const NavigationAction& navigationAction, const ResourceRequest& request, PassRefPtr<FormState>)
{
    if (m_frame->coreFrame()->loader()->documentLoader()->url().isEmpty() && request.url() == blankURL()) {
        // WebKit2 loads initial about:blank documents synchronously, without consulting the policy delegate
        ASSERT(m_frame->coreFrame()->loader()->stateMachine()->committingFirstRealLoad());
        (m_frame->coreFrame()->loader()->policyChecker()->*function)(PolicyUse);
        return;
    }

    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    uint64_t listenerID = m_frame->setUpPolicyListener(function);

    // FIXME: Pass more than just the navigation action type.
    const String& url = request.url().string(); // FIXME: Pass entire request.

    uint32_t navigationType = static_cast<uint32_t>(navigationAction.type());
    uint32_t modifiers = modifiersForNavigationAction(navigationAction);
    int32_t mouseButton = mouseButtonForNavigationAction(navigationAction);

    WebProcess::shared().connection()->send(Messages::WebPageProxy::DecidePolicyForNavigationAction(m_frame->frameID(), navigationType, modifiers, mouseButton, url, listenerID), webPage->pageID());
}

void WebFrameLoaderClient::cancelPolicyCheck()
{
    m_frame->invalidatePolicyListener();
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchWillSubmitForm(FramePolicyFunction function, PassRefPtr<FormState> prpFormState)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // FIXME: Pass more of the form state.
    RefPtr<FormState> formState = prpFormState;
    
    HTMLFormElement* form = formState->form();
    WebFrame* sourceFrame = static_cast<WebFrameLoaderClient*>(formState->sourceFrame()->loader()->client())->webFrame();    
    const Vector<std::pair<String, String> >& values = formState->textFieldValues();

    RefPtr<APIObject> userData;
    webPage->injectedBundleFormClient().willSubmitForm(webPage, form, m_frame, sourceFrame, values, userData);


    uint64_t listenerID = m_frame->setUpPolicyListener(function);
    StringPairVector valuesVector(values);

    WebProcess::shared().connection()->send(Messages::WebPageProxy::WillSubmitForm(m_frame->frameID(), sourceFrame->frameID(), valuesVector, listenerID, InjectedBundleUserMessageEncoder(userData.get())), webPage->pageID());
}

void WebFrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError& error)
{
    if (!m_pluginView)
        return;
    
    m_pluginView->manualLoadDidFail(error);
    m_pluginView = 0;
    m_hasSentResponseToPluginView = false;
}

void WebFrameLoaderClient::willChangeEstimatedProgress()
{
    notImplemented();
}

void WebFrameLoaderClient::didChangeEstimatedProgress()
{
    notImplemented();
}

void WebFrameLoaderClient::postProgressStartedNotification()
{
    if (WebPage* webPage = m_frame->page())
        WebProcess::shared().connection()->send(Messages::WebPageProxy::DidStartProgress(), webPage->pageID());
}

void WebFrameLoaderClient::postProgressEstimateChangedNotification()
{
    if (WebPage* webPage = m_frame->page()) {
        double progress = webPage->corePage()->progress()->estimatedProgress();
        WebProcess::shared().connection()->send(Messages::WebPageProxy::DidChangeProgress(progress), webPage->pageID());

    }
}

void WebFrameLoaderClient::postProgressFinishedNotification()
{
    if (WebPage* webPage = m_frame->page())
        WebProcess::shared().connection()->send(Messages::WebPageProxy::DidFinishProgress(), webPage->pageID());
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool)
{
    notImplemented();
}

void WebFrameLoaderClient::startDownload(const ResourceRequest&)
{
    notImplemented();
}

void WebFrameLoaderClient::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::didChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    if (!m_pluginView)
        loader->commitData(data, length);

    // Calling commitData did not create the plug-in view.
    if (!m_pluginView)
        return;

    if (!m_hasSentResponseToPluginView) {
        m_pluginView->manualLoadDidReceiveResponse(m_frame->coreFrame()->loader()->documentLoader()->response());
        // manualLoadDidReceiveResponse sets up a new stream to the plug-in. on a full-page plug-in, a failure in
        // setting up this stream can cause the main document load to be cancelled, setting m_pluginView
        // to null
        if (!m_pluginView)
            return;
        m_hasSentResponseToPluginView = true;
    }
    m_pluginView->manualLoadDidReceiveData(data, length);
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    if (!m_pluginView) {
        committedLoad(loader, 0, 0);
        return;
    }

    m_pluginView->manualLoadDidFinishLoading();
    m_pluginView = 0;
    m_hasSentResponseToPluginView = false;
}

void WebFrameLoaderClient::updateGlobalHistory()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    DocumentLoader* loader = m_frame->coreFrame()->loader()->documentLoader();

    WebNavigationDataStore data;
    data.url = loader->urlForHistory().string();
    data.title = loader->title();

    WebProcess::shared().connection()->send(WebProcessProxyMessage::DidNavigateWithNavigationData,
                                            0,
                                            CoreIPC::In(webPage->pageID(), data, m_frame->frameID()));
}

void WebFrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    DocumentLoader* loader = m_frame->coreFrame()->loader()->documentLoader();
    ASSERT(loader->unreachableURL().isEmpty());

    // Client redirect
    if (!loader->clientRedirectSourceForHistory().isNull()) {
        WebProcess::shared().connection()->send(WebProcessProxyMessage::DidPerformClientRedirect,
                                                0,
                                                CoreIPC::In(webPage->pageID(),
                                                            loader->clientRedirectSourceForHistory(), 
                                                            loader->clientRedirectDestinationForHistory(),
                                                            m_frame->frameID()));
    }

    // Server redirect
    if (!loader->serverRedirectSourceForHistory().isNull()) {
        WebProcess::shared().connection()->send(WebProcessProxyMessage::DidPerformServerRedirect,
                                                0,
                                                CoreIPC::In(webPage->pageID(),
                                                            loader->serverRedirectSourceForHistory(),
                                                            loader->serverRedirectDestinationForHistory(),
                                                            m_frame->frameID()));
    }
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem*) const
{
    notImplemented();
    return true;
}

void WebFrameLoaderClient::dispatchDidAddBackForwardItem(HistoryItem*) const
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidRemoveBackForwardItem(HistoryItem*) const
{
    notImplemented();
}

void WebFrameLoaderClient::dispatchDidChangeBackForwardIndex() const
{
    notImplemented();
}

void WebFrameLoaderClient::didDisplayInsecureContent()
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didDisplayInsecureContentForFrame(webPage, m_frame);
}

void WebFrameLoaderClient::didRunInsecureContent(SecurityOrigin*)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    // Notify the bundle client.
    webPage->injectedBundleLoaderClient().didRunInsecureContentForFrame(webPage, m_frame);
}

ResourceError WebFrameLoaderClient::cancelledError(const ResourceRequest& request)
{
    return WebKit::cancelledError(request);
}

ResourceError WebFrameLoaderClient::blockedError(const ResourceRequest& request)
{
    return WebKit::blockedError(request);
}

ResourceError WebFrameLoaderClient::cannotShowURLError(const ResourceRequest& request)
{
    return WebKit::cannotShowURLError(request);
}

ResourceError WebFrameLoaderClient::interruptForPolicyChangeError(const ResourceRequest& request)
{
    return WebKit::interruptForPolicyChangeError(request);
}

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse& response)
{
    return WebKit::cannotShowMIMETypeError(response);
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const ResourceResponse& response)
{
    return WebKit::fileDoesNotExistError(response);
}

ResourceError WebFrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse&)
{
    notImplemented();
    return ResourceError();
}

bool WebFrameLoaderClient::shouldFallBack(const ResourceError&)
{
    notImplemented();
    return false;
}

bool WebFrameLoaderClient::canHandleRequest(const ResourceRequest&) const
{
    notImplemented();
    return true;
}

bool WebFrameLoaderClient::canShowMIMEType(const String& MIMEType) const
{
    notImplemented();
    return true;
}

bool WebFrameLoaderClient::canShowMIMETypeAsHTML(const String& MIMEType) const
{
    return true;
}

bool WebFrameLoaderClient::representationExistsForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return false;
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(const String& URLScheme) const
{
    notImplemented();
    return String();
}

void WebFrameLoaderClient::frameLoadCompleted()
{
    notImplemented();
}

void WebFrameLoaderClient::saveViewStateToItem(HistoryItem*)
{
    notImplemented();
}

void WebFrameLoaderClient::restoreViewState()
{
    notImplemented();
}

void WebFrameLoaderClient::provisionalLoadStarted()
{
    notImplemented();
}

void WebFrameLoaderClient::didFinishLoad()
{
    // If we have a load listener, notify it.
    if (WebFrame::LoadListener* loadListener = m_frame->loadListener())
        loadListener->didFinishLoad(m_frame);
}

void WebFrameLoaderClient::prepareForDataSourceReplacement()
{
    notImplemented();
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& data)
{
    return DocumentLoader::create(request, data);
}

void WebFrameLoaderClient::setTitle(const String& title, const KURL& url)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    WebProcess::shared().connection()->send(WebProcessProxyMessage::DidUpdateHistoryTitle, 0, CoreIPC::In(webPage->pageID(), title, url.string(), m_frame->frameID()));
}

String WebFrameLoaderClient::userAgent(const KURL&)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return String();

    return webPage->userAgent();
}

void WebFrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame*)
{
    notImplemented();
}

void WebFrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
    notImplemented();
}

void WebFrameLoaderClient::transitionToCommittedForNewPage()
{
    m_frame->coreFrame()->createView(m_frame->page()->size(), Color::white, false, IntSize(), false);
}

bool WebFrameLoaderClient::canCachePage() const
{
    notImplemented();
    return false;
}

void WebFrameLoaderClient::download(ResourceHandle*, const ResourceRequest&, const ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                                    const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    WebPage* webPage = m_frame->page();

    RefPtr<WebFrame> subframe = WebFrame::createSubframe(webPage, name, ownerElement);

    // Notify the UI process that subframe has been added.
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidCreateSubFrame(subframe->frameID()), webPage->pageID());

    Frame* coreSubframe = subframe->coreFrame();

     // The creation of the frame may have run arbitrary JavaScript that removed it from the page already.
    m_frame->coreFrame()->loader()->loadURLIntoChildFrame(url, referrer, coreSubframe);

    // The frame's onload handler may have removed it from the document.
    if (!coreSubframe->tree()->parent())
        return 0;

    return coreSubframe;
}

void WebFrameLoaderClient::didTransferChildFrameToNewDocument(Page*)
{
    notImplemented();
}

PassRefPtr<Widget> WebFrameLoaderClient::createPlugin(const IntSize&, HTMLPlugInElement* pluginElement, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    ASSERT(paramNames.size() == paramValues.size());
    
    WebPage* webPage = m_frame->page();
    ASSERT(webPage);
    
    Plugin::Parameters parameters;
    parameters.url = url;
    parameters.names = paramNames;
    parameters.values = paramValues;
    parameters.mimeType = mimeType;
    parameters.loadManually = loadManually;

    // <rdar://problem/8440903>: AppleConnect has a bug where it does not
    // understand the parameter names specified in the <object> element that
    // embeds its plug-in. This hack works around the issue by converting the
    // parameter names to lowercase before passing them to the plug-in.
    // FIXME: This workaround should be dependent on site-specific quirks being
    // enabled. This requires adding this setting to WebKit2's WebPreferences
    // implementation. See <https://bugs.webkit.org/show_bug.cgi?id=46076>.
    if (equalIgnoringCase(mimeType, "application/x-snkp")) {
        for (size_t i = 0; i < paramNames.size(); ++i)
            parameters.names[i] = paramNames[i].lower();
    }

    RefPtr<Plugin> plugin = webPage->createPlugin(parameters);
    if (!plugin)
        return 0;
    
    return PluginView::create(pluginElement, plugin.release(), parameters);
}

void WebFrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    ASSERT(pluginWidget);
    
    m_pluginView = static_cast<PluginView*>(pluginWidget);
}

PassRefPtr<Widget> WebFrameLoaderClient::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL& baseURL, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

ObjectContentType WebFrameLoaderClient::objectContentType(const KURL& url, const String& mimeTypeIn)
{
    // FIXME: This should be merged with WebCore::FrameLoader::defaultObjectContentType when the plugin code
    // is consolidated.

    String mimeType = mimeTypeIn;
    if (mimeType.isEmpty())
        mimeType = MIMETypeRegistry::getMIMETypeForExtension(url.path().substring(url.path().reverseFind('.') + 1));

    if (mimeType.isEmpty())
        return ObjectContentFrame;

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return WebCore::ObjectContentImage;

    if (WebPage* webPage = m_frame->page()) {
        if (PluginData* pluginData = webPage->corePage()->pluginData()) {
            if (pluginData->supportsMimeType(mimeType))
                return ObjectContentNetscapePlugin;
        }
    }

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentFrame;

    return ObjectContentNone;
}

String WebFrameLoaderClient::overrideMediaType() const
{
    notImplemented();
    return String();
}

void WebFrameLoaderClient::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    WebPage* webPage = m_frame->page();
    if (!webPage)
        return;

    webPage->injectedBundleLoaderClient().didClearWindowObjectForFrame(webPage, m_frame, world);
}

void WebFrameLoaderClient::documentElementAvailable()
{
    notImplemented();
}

void WebFrameLoaderClient::didPerformFirstNavigation() const
{
    notImplemented();
}

void WebFrameLoaderClient::registerForIconNotification(bool listen)
{
    notImplemented();
}

#if PLATFORM(MAC)
#if ENABLE(MAC_JAVA_BRIDGE)
jobject WebFrameLoaderClient::javaApplet(NSView*) { return 0; }
#endif
NSCachedURLResponse* WebFrameLoaderClient::willCacheResponse(DocumentLoader*, unsigned long identifier, NSCachedURLResponse*) const
{
    notImplemented();
    return 0;
}

#endif
#if USE(CFNETWORK)
bool WebFrameLoaderClient::shouldCacheResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&, const unsigned char* data, unsigned long long length)
{
    notImplemented();
    return false;
}

#endif

bool WebFrameLoaderClient::shouldUsePluginDocument(const String& /*mimeType*/) const
{
    notImplemented();
    return false;
}

PassRefPtr<FrameNetworkingContext> WebFrameLoaderClient::createNetworkingContext()
{
    return WebFrameNetworkingContext::create(m_frame->coreFrame());
}

} // namespace WebKit
