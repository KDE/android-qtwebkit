/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebFrameLoaderClient.h"

#include "CFDictionaryPropertyBag.h"
#include "MarshallingHelpers.h"
#include "WebCachedPagePlatformData.h"
#include "WebChromeClient.h"
#include "WebDocumentLoader.h"
#include "WebError.h"
#include "WebFrame.h"
#include "WebHistory.h"
#include "WebMutableURLRequest.h"
#include "WebNotificationCenter.h"
#include "WebScriptDebugServer.h"
#include "WebURLAuthenticationChallenge.h"
#include "WebURLResponse.h"
#include "WebView.h"
#pragma warning(push, 0)
#include <WebCore/DocumentLoader.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/Page.h>
#include <WebCore/PluginPackage.h>
#include <WebCore/PluginView.h>
#include <WebCore/RenderPart.h>
#pragma warning(pop)

using namespace WebCore;
using namespace HTMLNames;

WebView* kit(Page* page)
{
    return page ? static_cast<WebChromeClient*>(page->chrome()->client())->webView() : 0;
}

static WebDataSource* getWebDataSource(DocumentLoader* loader)
{
    return loader ? static_cast<WebDocumentLoader*>(loader)->dataSource() : 0;
}

WebFrameLoaderClient::WebFrameLoaderClient(WebFrame* webFrame)
    : m_webFrame(webFrame)
    , m_pluginView(0) 
    , m_hasSentResponseToPlugin(false) 
{
    ASSERT_ARG(webFrame, webFrame);
}

WebFrameLoaderClient::~WebFrameLoaderClient()
{
}

bool WebFrameLoaderClient::hasWebView() const
{
    return m_webFrame->webView();
}

bool WebFrameLoaderClient::hasFrameView() const
{
    return core(m_webFrame)->view();
}

void WebFrameLoaderClient::forceLayout()
{
    core(m_webFrame)->forceLayout(true);
}

void WebFrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebMutableURLRequest> webURLRequest(AdoptCOM, WebMutableURLRequest::createInstance(request));
    resourceLoadDelegate->identifierForInitialRequest(webView, webURLRequest.get(), getWebDataSource(loader), identifier);
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge& challenge)
{
    ASSERT(challenge.sourceHandle());

    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (SUCCEEDED(webView->resourceLoadDelegate(&resourceLoadDelegate))) {
        COMPtr<WebURLAuthenticationChallenge> webChallenge(AdoptCOM, WebURLAuthenticationChallenge::createInstance(challenge));
        if (SUCCEEDED(resourceLoadDelegate->didReceiveAuthenticationChallenge(webView, identifier, webChallenge.get(), getWebDataSource(loader))))
            return;
    }

    // If the ResourceLoadDelegate doesn't exist or fails to handle the call, we tell the ResourceHandle
    // to continue without credential - this is the best approximation of Mac behavior
    challenge.sourceHandle()->receivedRequestToContinueWithoutCredential(challenge);
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(DocumentLoader* loader, unsigned long identifier, const AuthenticationChallenge& challenge)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebURLAuthenticationChallenge> webChallenge(AdoptCOM, WebURLAuthenticationChallenge::createInstance(challenge));
    resourceLoadDelegate->didCancelAuthenticationChallenge(webView, identifier, webChallenge.get(), getWebDataSource(loader));
}

void WebFrameLoaderClient::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebMutableURLRequest> webURLRequest(AdoptCOM, WebMutableURLRequest::createInstance(request));
    COMPtr<WebURLResponse> webURLRedirectResponse(AdoptCOM, WebURLResponse::createInstance(redirectResponse));

    COMPtr<IWebURLRequest> newWebURLRequest;
    if (FAILED(resourceLoadDelegate->willSendRequest(webView, identifier, webURLRequest.get(), webURLRedirectResponse.get(), getWebDataSource(loader), &newWebURLRequest)))
        return;

    if (webURLRequest == newWebURLRequest)
        return;

    COMPtr<WebMutableURLRequest> newWebURLRequestImpl(Query, newWebURLRequest);
    if (!newWebURLRequestImpl)
        return;

    request = newWebURLRequestImpl->resourceRequest();
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& response)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebURLResponse> webURLResponse(AdoptCOM, WebURLResponse::createInstance(response));
    resourceLoadDelegate->didReceiveResponse(webView, identifier, webURLResponse.get(), getWebDataSource(loader));
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader* loader, unsigned long identifier, int length)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    resourceLoadDelegate->didReceiveContentLength(webView, identifier, length, getWebDataSource(loader));
}

void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    resourceLoadDelegate->didFinishLoadingFromDataSource(webView, identifier, getWebDataSource(loader));
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return;

    COMPtr<WebError> webError(AdoptCOM, WebError::createInstance(error));
    resourceLoadDelegate->didFailLoadingWithError(webView, identifier, webError.get(), getWebDataSource(loader));
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didHandleOnloadEventsForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveServerRedirectForProvisionalLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didCancelClientRedirectForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const KURL& url, double delay, double fireDate)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willPerformClientRedirectToURL(webView, BString(url.string()), delay, MarshallingHelpers::CFAbsoluteTimeToDATE(fireDate), m_webFrame);
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didChangeLocationWithinPageForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchWillClose()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willCloseFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReceiveIcon()
{
    m_webFrame->webView()->dispatchDidReceiveIconFromWebFrame(m_webFrame);
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didStartProvisionalLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveTitle(webView, BString(title), m_webFrame);
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didCommitLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didFinishDocumentLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didFinishLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didFirstLayoutInFrame(webView, m_webFrame);
}

Frame* WebFrameLoaderClient::dispatchCreatePage()
{
    WebView* webView = m_webFrame->webView();

    COMPtr<IWebUIDelegate> ui;
    if (FAILED(webView->uiDelegate(&ui)))
        return 0;

    COMPtr<IWebView> newWebView;
    if (FAILED(ui->createWebViewWithRequest(webView, 0, &newWebView)))
        return 0;

    COMPtr<IWebFrame> mainFrame;
    if (FAILED(newWebView->mainFrame(&mainFrame)))
        return 0;

    COMPtr<WebFrame> mainFrameImpl(Query, mainFrame);
    return core(mainFrameImpl.get());
}

void WebFrameLoaderClient::dispatchShow()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(webView->uiDelegate(&ui)))
        ui->webViewShow(webView);
}

void WebFrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader* loader)
{
    if (WebScriptDebugServer::listenerCount() <= 0)
        return;

    Frame* coreFrame = core(m_webFrame);
    if (!coreFrame)
        return;

    WebScriptDebugServer::sharedWebScriptDebugServer()->didLoadMainResourceForDataSource(kit(coreFrame->page()), getWebDataSource(loader));
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError& error)
{
    if (!m_pluginView)
        return;

    if (m_pluginView->status() == PluginStatusLoadedSuccessfully)
        m_pluginView->didFail(error);
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}

void WebFrameLoaderClient::postProgressStartedNotification()
{
    static BSTR progressStartedName = SysAllocString(WebViewProgressStartedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressStartedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::postProgressEstimateChangedNotification()
{
    static BSTR progressEstimateChangedName = SysAllocString(WebViewProgressEstimateChangedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressEstimateChangedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::postProgressFinishedNotification()
{
    static BSTR progressFinishedName = SysAllocString(WebViewProgressFinishedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressFinishedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    // FIXME: This should probably go through the data source.
    const String& textEncoding = loader->response().textEncodingName();

    if (!m_pluginView)
        receivedData(data, length, textEncoding);

    if (!m_pluginView || m_pluginView->status() != PluginStatusLoadedSuccessfully)
        return;

    if (!m_hasSentResponseToPlugin) {
        m_pluginView->didReceiveResponse(core(m_webFrame)->loader()->documentLoader()->response());
        // didReceiveResponse sets up a new stream to the plug-in. on a full-page plug-in, a failure in
        // setting up this stream can cause the main document load to be cancelled, setting m_pluginView
        // to null
        if (!m_pluginView)
            return;
        m_hasSentResponseToPlugin = true;
    }
    m_pluginView->didReceiveData(data, length);
}

void WebFrameLoaderClient::receivedData(const char* data, int length, const String& textEncoding)
{
    Frame* coreFrame = core(m_webFrame);
    if (!coreFrame)
        return;

    // Set the encoding. This only needs to be done once, but it's harmless to do it again later.
    String encoding = coreFrame->loader()->documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncoding;
    coreFrame->loader()->setEncoding(encoding, userChosen);

    coreFrame->loader()->addData(data, length);
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* loader)
{
    // Telling the frame we received some data and passing 0 as the data is our
    // way to get work done that is normally done when the first bit of data is
    // received, even for the case of a document with no data (like about:blank)
    if (!m_pluginView) {
        committedLoad(loader, 0, 0);
        return;
    }

    if (m_pluginView->status() == PluginStatusLoadedSuccessfully)
        m_pluginView->didFinishLoading();
    m_pluginView = 0;
    m_hasSentResponseToPlugin = false;
}

void WebFrameLoaderClient::updateGlobalHistory(const KURL& url)
{
    COMPtr<WebHistory> history = webHistory();
    if (!history)
        return;
    history->addItemForURL(BString(url.string()), 0);                 
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem*) const
{
    return true;
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<WebDocumentLoader> loader = new WebDocumentLoader(request, substituteData);

    COMPtr<WebDataSource> dataSource(AdoptCOM, WebDataSource::createInstance(loader.get()));

    loader->setDataSource(dataSource.get());
    return loader.release();
}

void WebFrameLoaderClient::setTitle(const String& title, const KURL& url)
{
    BOOL privateBrowsingEnabled = FALSE;
    COMPtr<IWebPreferences> preferences;
    if (SUCCEEDED(m_webFrame->webView()->preferences(&preferences)))
        preferences->privateBrowsingEnabled(&privateBrowsingEnabled);
    if (privateBrowsingEnabled)
        return;

    // update title in global history
    COMPtr<WebHistory> history = webHistory();
    if (!history)
        return;

    COMPtr<IWebHistoryItem> item;
    if (FAILED(history->itemForURL(BString(url.string()), &item)))
        return;

    COMPtr<IWebHistoryItemPrivate> itemPrivate(Query, item);
    if (!itemPrivate)
        return;

    itemPrivate->setTitle(BString(title));
}

void WebFrameLoaderClient::savePlatformDataToCachedPage(CachedPage* cachedPage)
{
    Frame* coreFrame = core(m_webFrame);
    if (!coreFrame)
        return;

    ASSERT(coreFrame->loader()->documentLoader() == cachedPage->documentLoader());

    WebCachedPagePlatformData* webPlatformData = new WebCachedPagePlatformData(static_cast<IWebDataSource*>(getWebDataSource(coreFrame->loader()->documentLoader())));
    cachedPage->setCachedPagePlatformData(webPlatformData);
}

void WebFrameLoaderClient::transitionToCommittedForNewPage()
{
    Frame* frame = core(m_webFrame);
    ASSERT(frame);

    Page* page = frame->page();
    ASSERT(page);

    bool isMainFrame = frame == page->mainFrame();

    if (isMainFrame && frame->view())
        frame->view()->detachFromWindow();

    frame->setView(0);

    WebView* webView = m_webFrame->webView();

    FrameView* frameView;
    if (isMainFrame) {
        RECT rect;
        webView->frameRect(&rect);
        frameView = new FrameView(frame, IntRect(rect).size());
    } else
        frameView = new FrameView(frame);

    frame->setView(frameView);
    frameView->deref(); // FrameViews are created with a ref count of 1. Release this ref since we've assigned it to frame.

    HWND viewWindow;
    if (SUCCEEDED(webView->viewWindow(reinterpret_cast<OLE_HANDLE*>(&viewWindow))))
        frameView->setContainingWindow(viewWindow);

    if (isMainFrame)
        frameView->attachToWindow();

    if (frame->ownerRenderer())
        frame->ownerRenderer()->setWidget(frameView);

    if (HTMLFrameOwnerElement* owner = frame->ownerElement())
        frame->view()->setScrollbarsMode(owner->scrollingMode());
}

bool WebFrameLoaderClient::canCachePage() const
{
    return true;
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                            const String& referrer, bool /*allowsScrolling*/, int /*marginWidth*/, int /*marginHeight*/)
{
    RefPtr<Frame> result = createFrame(url, name, ownerElement, referrer);
    if (!result)
        return 0;

    // Propagate the marginwidth/height and scrolling modes to the view.
    if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag)) {
        HTMLFrameElement* frameElt = static_cast<HTMLFrameElement*>(ownerElement);
        if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
            result->view()->setScrollbarsMode(ScrollbarAlwaysOff);
        int marginWidth = frameElt->getMarginWidth();
        int marginHeight = frameElt->getMarginHeight();
        if (marginWidth != -1)
            result->view()->setMarginWidth(marginWidth);
        if (marginHeight != -1)
            result->view()->setMarginHeight(marginHeight);
    }

    return result.release();
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const KURL& URL, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer)
{
    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    COMPtr<WebFrame> webFrame(AdoptCOM, WebFrame::createInstance());

    webFrame->initWithWebFrameView(0, m_webFrame->webView(), coreFrame->page(), ownerElement);

    RefPtr<Frame> childFrame(adoptRef(core(webFrame.get()))); // We have to adopt, because Frames start out with a refcount of 1.
    ASSERT(childFrame);

    coreFrame->tree()->appendChild(childFrame);
    childFrame->tree()->setName(name);
    childFrame->init();

    loadURLIntoChild(URL, referrer, webFrame.get());

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();
}

void WebFrameLoaderClient::loadURLIntoChild(const KURL& originalURL, const String& referrer, WebFrame* childFrame)
{
    ASSERT(childFrame);
    ASSERT(core(childFrame));

    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    HistoryItem* parentItem = coreFrame->loader()->currentHistoryItem();
    FrameLoadType loadType = coreFrame->loader()->loadType();
    FrameLoadType childLoadType = FrameLoadTypeRedirectWithLockedHistory;

    KURL url = originalURL;

    // If we're moving in the backforward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    // Reload will maintain the frame contents, LoadSame will not.
    if (parentItem && parentItem->children().size() &&
        (isBackForwardLoadType(loadType)
         || loadType == FrameLoadTypeReload
         || loadType == FrameLoadTypeReloadAllowingStaleData))
    {
        if (HistoryItem* childItem = parentItem->childItemWithName(core(childFrame)->tree()->name())) {
            // Use the original URL to ensure we get all the side-effects, such as
            // onLoad handlers, of any redirects that happened. An example of where
            // this is needed is Radar 3213556.
            url = childItem->originalURL();
            // These behaviors implied by these loadTypes should apply to the child frames
            childLoadType = loadType;

            if (isBackForwardLoadType(loadType))
                // For back/forward, remember this item so we can traverse any child items as child frames load
                core(childFrame)->loader()->setProvisionalHistoryItem(childItem);
            else
                // For reload, just reinstall the current item, since a new child frame was created but we won't be creating a new BF item
                core(childFrame)->loader()->setCurrentHistoryItem(childItem);
        }
    }

    // FIXME: Handle loading WebArchives here

    core(childFrame)->loader()->load(url, referrer, childLoadType, String(), 0, 0);
}

Widget* WebFrameLoaderClient::createPlugin(const IntSize& pluginSize, Element* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    Frame* frame = core(m_webFrame);
    PluginView* pluginView = PluginView::create(frame, pluginSize, element, url, paramNames, paramValues, mimeType, loadManually);

    if (pluginView->status() == PluginStatusLoadedSuccessfully)
        return pluginView;

    COMPtr<IWebResourceLoadDelegate> resourceLoadDelegate;

    WebView* webView = m_webFrame->webView();
    if (FAILED(webView->resourceLoadDelegate(&resourceLoadDelegate)))
        return pluginView;

    RetainPtr<CFMutableDictionaryRef> userInfo(AdoptCF, CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    unsigned count = (unsigned)paramNames.size();
    for (unsigned i = 0; i < count; i++) {
        if (paramNames[i] == "pluginspage") {
            static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorPlugInPageURLStringKey);
            RetainPtr<CFStringRef> str(AdoptCF, paramValues[i].createCFString());
            CFDictionarySetValue(userInfo.get(), key, str.get());
            break;
        }
    }

    if (!mimeType.isNull()) {
        static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorMIMETypeKey);

        RetainPtr<CFStringRef> str(AdoptCF, mimeType.createCFString());
        CFDictionarySetValue(userInfo.get(), key, str.get());
    }

    String pluginName;
    if (pluginView->plugin())
        pluginName = pluginView->plugin()->name();
    if (!pluginName.isNull()) {
        static CFStringRef key = MarshallingHelpers::LPCOLESTRToCFStringRef(WebKitErrorPlugInNameKey);
        RetainPtr<CFStringRef> str(AdoptCF, pluginName.createCFString());
        CFDictionarySetValue(userInfo.get(), key, str.get());
    }

    COMPtr<CFDictionaryPropertyBag> userInfoBag(AdoptCOM, CFDictionaryPropertyBag::createInstance());
    userInfoBag->setDictionary(userInfo.get());
 
    int errorCode = 0;
    switch (pluginView->status()) {
        case PluginStatusCanNotFindPlugin:
            errorCode = WebKitErrorCannotFindPlugIn;
            break;
        case PluginStatusCanNotLoadPlugin:
            errorCode = WebKitErrorCannotLoadPlugIn;
            break;
        default:
            ASSERT_NOT_REACHED();
    }

    ResourceError resourceError(String(WebKitErrorDomain), errorCode, url.string(), String());
    COMPtr<IWebError> error(AdoptCOM, WebError::createInstance(resourceError, userInfoBag.get()));
     
    resourceLoadDelegate->plugInFailedWithError(webView, error.get(), getWebDataSource(frame->loader()->documentLoader()));

    return pluginView;
}

void WebFrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    // Ideally, this function shouldn't be necessary, see <rdar://problem/4852889>

    m_pluginView = static_cast<PluginView*>(pluginWidget);
}

COMPtr<WebHistory> WebFrameLoaderClient::webHistory() const
{
    if (m_webFrame != m_webFrame->webView()->topLevelFrame())
        return 0;

    IWebHistoryPrivate* historyInternal = WebHistory::optionalSharedHistoryInternal(); // does not add a ref
    if (!historyInternal)
        return 0;

    return COMPtr<WebHistory>(Query, historyInternal);
}
