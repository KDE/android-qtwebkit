/*
 *  Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 *  Copyright (C) 2007, 2008 Holger Hans Peter Freyther
 *  Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>
 *  Copyright (C) 2008 Collabora Ltd.  All rights reserved.
 *  Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "FrameLoaderClientGtk.h"

#include "Color.h"
#include "DocumentLoader.h"
#include "FormState.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "FrameTree.h"
#include "HTMLAppletElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSDOMWindow.h"
#include "Language.h"
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "PluginDatabase.h"
#include "RenderPart.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "CString.h"
#include "ProgressTracker.h"
#include "JSDOMBinding.h"
#include "ScriptController.h"
#include "webkitwebview.h"
#include "webkitnetworkrequest.h"
#include "webkitwebframe.h"
#include "webkitwebnavigationaction.h"
#include "webkitwebpolicydecision.h"
#include "webkitprivate.h"

#include <JavaScriptCore/APICast.h>
#include <stdio.h>
#if PLATFORM(UNIX)
#include <sys/utsname.h>
#endif

using namespace WebCore;

namespace WebKit {

FrameLoaderClient::FrameLoaderClient(WebKitWebFrame* frame)
    : m_frame(frame)
    , m_userAgent("")
    , m_policyDecision(0)
    , m_pluginView(0)
    , m_hasSentResponseToPlugin(false)
{
    ASSERT(m_frame);
}

FrameLoaderClient::~FrameLoaderClient()
{
    if (m_policyDecision)
        g_object_unref(m_policyDecision);
}

static String agentPlatform()
{
#ifdef GDK_WINDOWING_X11
    return "X11";
#elif defined(GDK_WINDOWING_WIN32)
    return "Windows";
#elif defined(GDK_WINDOWING_QUARTZ)
    return "Macintosh";
#elif defined(GDK_WINDOWING_DIRECTFB)
    return "DirectFB";
#else
    notImplemented();
    return "Unknown";
#endif
}

static String agentOS()
{
#if PLATFORM(DARWIN)
#if PLATFORM(X86)
    return "Intel Mac OS X";
#else
    return "PPC Mac OS X";
#endif
#elif PLATFORM(UNIX)
    struct utsname name;
    if (uname(&name) != -1)
        return String::format("%s %s", name.sysname, name.machine);
    else
        return "Unknown";
#elif PLATFORM(WIN_OS)
    // FIXME: Compute the Windows version
    return "Windows";
#else
    notImplemented();
    return "Unknown";
#endif
}

static String composeUserAgent()
{
    // This is a liberal interpretation of http://www.mozilla.org/build/revised-user-agent-strings.html
    // See also http://developer.apple.com/internet/safari/faq.html#anchor2

    String ua;

    // Product
    ua += "Mozilla/5.0";

    // Comment
    ua += " (";
    ua += agentPlatform(); // Platform
    ua += "; U; "; // Security
    ua += agentOS(); // OS-or-CPU
    ua += "; ";
    ua += defaultLanguage(); // Localization information
    ua += ") ";

    // WebKit Product
    // FIXME: The WebKit version is hardcoded
    static const String webKitVersion = "528.5+";
    ua += "AppleWebKit/" + webKitVersion;
    ua += " (KHTML, like Gecko, ";
    // We mention Safari since many broken sites check for it (OmniWeb does this too)
    // We re-use the WebKit version, though it doesn't seem to matter much in practice
    ua += "Safari/" + webKitVersion;
    ua += ") ";

    // Vendor Product
    ua += g_get_prgname();

    return ua;
}

String FrameLoaderClient::userAgent(const KURL&)
{
    if (m_userAgent.isEmpty())
        m_userAgent = composeUserAgent();

    return m_userAgent;
}

WTF::PassRefPtr<WebCore::DocumentLoader> FrameLoaderClient::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
    return DocumentLoader::create(request, substituteData);
}

void FrameLoaderClient::dispatchWillSubmitForm(FramePolicyFunction policyFunction, PassRefPtr<FormState>)
{
    // FIXME: This is surely too simple
    ASSERT(policyFunction);
    if (!policyFunction)
        return;
    (core(m_frame)->loader()->*policyFunction)(PolicyUse);
}


void FrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    if (!m_pluginView) {
        ASSERT(loader->frame());
        // Setting the encoding on the frame loader is our way to get work done that is normally done
        // when the first bit of data is received, even for the case of a document with no data (like about:blank).
        String encoding = loader->overrideEncoding();
        bool userChosen = !encoding.isNull();
        if (!userChosen)
            encoding = loader->response().textEncodingName();

        FrameLoader* frameLoader = loader->frameLoader();
        frameLoader->setEncoding(encoding, userChosen);
        if (data)
            frameLoader->addData(data, length);
    }

    if (m_pluginView) {
        if (!m_hasSentResponseToPlugin) {
            m_pluginView->didReceiveResponse(loader->response());
            m_hasSentResponseToPlugin = true;
        }
        m_pluginView->didReceiveData(data, length);
    }
}

bool
FrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*, unsigned long  identifier)
{
    notImplemented();
    return false;
}

void FrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long  identifier, const AuthenticationChallenge&)
{
    notImplemented();
}

void FrameLoaderClient::dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&)
{
    notImplemented();
}

void FrameLoaderClient::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&)
{
    notImplemented();
}

void FrameLoaderClient::postProgressStartedNotification()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    g_signal_emit_by_name(webView, "load-started", m_frame);
}

void FrameLoaderClient::postProgressEstimateChangedNotification()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    Page* corePage = core(webView);

    g_signal_emit_by_name(webView, "load-progress-changed", lround(corePage->progress()->estimatedProgress()*100));
}

void FrameLoaderClient::postProgressFinishedNotification()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    WebKitWebViewPrivate* privateData = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);

    // We can get a stopLoad() from dispose when the object is being
    // destroyed, don't emit the signal in that case.
    if (!privateData->disposing)
        g_signal_emit_by_name(webView, "load-finished", m_frame);
}

void FrameLoaderClient::frameLoaderDestroyed()
{
    webkit_web_frame_core_frame_gone(m_frame);
    g_object_unref(m_frame);
    m_frame = 0;
    delete this;
}

void FrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse& response)
{
    m_response = response;
}

void FrameLoaderClient::dispatchDecidePolicyForMIMEType(FramePolicyFunction policyFunction, const String& mimeType, const ResourceRequest& resourceRequest)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;

    if (resourceRequest.isNull()) {
        (core(m_frame)->loader()->*policyFunction)(PolicyIgnore);
        return;
    }

    WebKitWebView* page = getViewFromFrame(m_frame);
    WebKitNetworkRequest* request = webkit_network_request_new(resourceRequest.url().string().utf8().data());

    WebKitWebPolicyDecision* policyDecision = webkit_web_policy_decision_new(m_frame, policyFunction);
    if (m_policyDecision)
        g_object_unref(m_policyDecision);
    m_policyDecision = policyDecision;

    gboolean isHandled = false;
    g_signal_emit_by_name(page, "mime-type-policy-decision-requested", m_frame, request, mimeType.utf8().data(), policyDecision, &isHandled);

    g_object_unref(request);

    if (isHandled)
        return;

    if (canShowMIMEType(mimeType))
        webkit_web_policy_decision_use (policyDecision);
    else
        webkit_web_policy_decision_ignore (policyDecision);
}

static WebKitWebNavigationAction* getNavigationAction(const NavigationAction& action)
{
    gint button = -1;

    const Event* event = action.event();
    if (event && event->isMouseEvent()) {
        const MouseEvent* mouseEvent = static_cast<const MouseEvent*>(event);
        // DOM button values are 0, 1 and 2 for left, middle and right buttons.
        // GTK+ uses 1, 2 and 3, so let's add 1 to remain consistent.
        button = mouseEvent->button() + 1;
    }

    gint modifierFlags = 0;
    UIEventWithKeyState* keyStateEvent = findEventWithKeyState(const_cast<Event*>(event));
    if (keyStateEvent) {
        if (keyStateEvent->shiftKey())
            modifierFlags |= GDK_SHIFT_MASK;
        if (keyStateEvent->ctrlKey())
            modifierFlags |= GDK_CONTROL_MASK;
        if (keyStateEvent->altKey())
            modifierFlags |= GDK_MOD1_MASK;
        if (keyStateEvent->metaKey())
            modifierFlags |= GDK_MOD2_MASK;
    }

    return WEBKIT_WEB_NAVIGATION_ACTION(g_object_new(WEBKIT_TYPE_WEB_NAVIGATION_ACTION,
                                                     "reason", kit(action.type()),
                                                     "original-uri", action.url().string().utf8().data(),
                                                     "button", button,
                                                     "modifier-state", modifierFlags,
                                                     NULL));
}

void FrameLoaderClient::dispatchDecidePolicyForNewWindowAction(FramePolicyFunction policyFunction, const NavigationAction& action, const ResourceRequest& resourceRequest, PassRefPtr<FormState>, const String& s)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;

    if (resourceRequest.isNull()) {
        (core(m_frame)->loader()->*policyFunction)(PolicyIgnore);
        return;
    }

    WebKitWebPolicyDecision* policyDecision = webkit_web_policy_decision_new(m_frame, policyFunction);

    if (m_policyDecision)
        g_object_unref(m_policyDecision);
    m_policyDecision = policyDecision;

    WebKitWebView* webView = getViewFromFrame(m_frame);
    WebKitNetworkRequest* request = webkit_network_request_new(resourceRequest.url().string().utf8().data());
    WebKitWebNavigationAction* navigationAction = getNavigationAction(action);
    gboolean isHandled = false;

    g_signal_emit_by_name(webView, "new-window-policy-decision-requested", m_frame, request, navigationAction, policyDecision, &isHandled);

    g_object_unref(navigationAction);
    g_object_unref(request);

    // FIXME: I think Qt version marshals this to another thread so when we
    // have multi-threaded download, we might need to do the same
    if (!isHandled)
        (core(m_frame)->loader()->*policyFunction)(PolicyUse);
}

void FrameLoaderClient::dispatchDecidePolicyForNavigationAction(FramePolicyFunction policyFunction, const NavigationAction& action, const ResourceRequest& resourceRequest, PassRefPtr<FormState>)
{
    ASSERT(policyFunction);
    if (!policyFunction)
        return;

    if (resourceRequest.isNull()) {
        (core(m_frame)->loader()->*policyFunction)(PolicyIgnore);
        return;
    }

    WebKitWebView* webView = getViewFromFrame(m_frame);
    WebKitNetworkRequest* request = webkit_network_request_new(resourceRequest.url().string().utf8().data());
    WebKitNavigationResponse response;
    /*
     * We still support the deprecated navigation-requested signal, if the
     * application doesn't ignore the navigation then the new signal is
     * emitted.
     * navigation-policy-decision-requested must be emitted after
     * navigation-requested as the policy decision can be async.
     */
    g_signal_emit_by_name(webView, "navigation-requested", m_frame, request, &response);

    if (response == WEBKIT_NAVIGATION_RESPONSE_IGNORE) {
        (core(m_frame)->loader()->*policyFunction)(PolicyIgnore);
        g_object_unref(request);
        return;
    }

    WebKitWebPolicyDecision* policyDecision = webkit_web_policy_decision_new(m_frame, policyFunction);
    if (m_policyDecision)
        g_object_unref(m_policyDecision);
    m_policyDecision = policyDecision;

    WebKitWebNavigationAction* navigationAction = getNavigationAction(action);
    gboolean isHandled = false;
    g_signal_emit_by_name(webView, "navigation-policy-decision-requested", m_frame, request, navigationAction, policyDecision, &isHandled);

    g_object_unref(navigationAction);
    g_object_unref(request);

    // FIXME Implement default behavior when we can query the backend what protocols it supports
    if (!isHandled)
        webkit_web_policy_decision_use(m_policyDecision);
}

Widget* FrameLoaderClient::createPlugin(const IntSize& pluginSize, HTMLPlugInElement* element, const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually)
{
    PluginView* pluginView = PluginView::create(core(m_frame), pluginSize, element, url, paramNames, paramValues, mimeType, loadManually);

    if (pluginView->status() == PluginStatusLoadedSuccessfully)
        return pluginView;

    return 0;
}

PassRefPtr<Frame> FrameLoaderClient::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                                                 const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight)
{
    Frame* coreFrame = core(webFrame());

    ASSERT(core(getViewFromFrame(webFrame())) == coreFrame->page());

    RefPtr<Frame> childFrame = webkit_web_frame_init_with_web_view(getViewFromFrame(webFrame()), ownerElement);

    coreFrame->tree()->appendChild(childFrame);

    childFrame->tree()->setName(name);
    childFrame->init();
    childFrame->loader()->loadURL(url, referrer, String(), false, FrameLoadTypeRedirectWithLockedBackForwardList, 0, 0);

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();
}

void FrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget)
{
    ASSERT(!m_pluginView);
    m_pluginView = static_cast<PluginView*>(pluginWidget);
    m_hasSentResponseToPlugin = false;
}

Widget* FrameLoaderClient::createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL& baseURL,
                                                  const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    notImplemented();
    return 0;
}

ObjectContentType FrameLoaderClient::objectContentType(const KURL& url, const String& mimeType)
{
    String type = mimeType;
    // We don't use MIMETypeRegistry::getMIMETypeForPath() because it returns "application/octet-stream" upon failure
    if (type.isEmpty())
        type = MIMETypeRegistry::getMIMETypeForExtension(url.path().substring(url.path().reverseFind('.') + 1));

    if (type.isEmpty())
        return WebCore::ObjectContentFrame;

    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return WebCore::ObjectContentImage;

    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return WebCore::ObjectContentNetscapePlugin;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return WebCore::ObjectContentFrame;

    return WebCore::ObjectContentNone;
}

String FrameLoaderClient::overrideMediaType() const
{
    notImplemented();
    return String();
}

void FrameLoaderClient::windowObjectCleared()
{
    // Is this obsolete now?
    g_signal_emit_by_name(m_frame, "cleared");

    Frame* coreFrame = core(webFrame());
    ASSERT(coreFrame);

    Settings* settings = coreFrame->settings();
    if (!settings || !settings->isJavaScriptEnabled())
        return;

    // TODO: Consider using g_signal_has_handler_pending() to avoid the overhead
    // when there are no handlers.
    JSGlobalContextRef context = toGlobalRef(coreFrame->script()->globalObject()->globalExec());
    JSObjectRef windowObject = toRef(coreFrame->script()->globalObject());
    ASSERT(windowObject);

    WebKitWebView* webView = getViewFromFrame(m_frame);
    g_signal_emit_by_name(webView, "window-object-cleared", m_frame, context, windowObject);

    // TODO: Re-attach debug clients if present.
    // The Win port has an example of how we might do this.
}

void FrameLoaderClient::documentElementAvailable()
{
}

void FrameLoaderClient::didPerformFirstNavigation() const
{
}

void FrameLoaderClient::registerForIconNotification(bool)
{
    notImplemented();
}

void FrameLoaderClient::setMainFrameDocumentReady(bool)
{
    // this is only interesting once we provide an external API for the DOM
}

bool FrameLoaderClient::hasWebView() const
{
    notImplemented();
    return true;
}

void FrameLoaderClient::dispatchDidFinishLoad()
{
    g_signal_emit_by_name(m_frame, "load-done", true);
}

void FrameLoaderClient::frameLoadCompleted()
{
    notImplemented();
}

void FrameLoaderClient::saveViewStateToItem(HistoryItem*)
{
    notImplemented();
}

void FrameLoaderClient::restoreViewState()
{
    notImplemented();
}

bool FrameLoaderClient::shouldGoToHistoryItem(HistoryItem* item) const
{
    // FIXME: This is a very simple implementation. More sophisticated
    // implementation would delegate the decision to a PolicyDelegate.
    // See mac implementation for example.
    return item != 0;
}

void FrameLoaderClient::makeRepresentation(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClient::forceLayout()
{
    FrameView* view = core(m_frame)->view();
    if (view)
        view->forceLayout(true);
}

void FrameLoaderClient::forceLayoutForNonHTML()
{
    notImplemented();
}

void FrameLoaderClient::setCopiesOnScroll()
{
    notImplemented();
}

void FrameLoaderClient::detachedFromParent2()
{
    notImplemented();
}

void FrameLoaderClient::detachedFromParent3()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidCancelClientRedirect()
{
    notImplemented();
}

void FrameLoaderClient::dispatchWillPerformClientRedirect(const KURL&, double, double)
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebKitWebFramePrivate* priv = m_frame->priv;
    g_free(priv->uri);
    priv->uri = g_strdup(core(m_frame)->loader()->url().prettyURL().utf8().data());
    g_object_notify(G_OBJECT(m_frame), "uri");
}

void FrameLoaderClient::dispatchWillClose()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidReceiveIcon()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);

    g_signal_emit_by_name(webView, "icon-loaded", m_frame);
}

void FrameLoaderClient::dispatchDidStartProvisionalLoad()
{
}

void FrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    WebKitWebFramePrivate* priv = m_frame->priv;
    g_free(priv->title);
    priv->title = g_strdup(title.utf8().data());

    g_signal_emit_by_name(m_frame, "title-changed", priv->title);
    g_object_notify(G_OBJECT(m_frame), "title");

    WebKitWebView* webView = getViewFromFrame(m_frame);
    if (m_frame == webkit_web_view_get_main_frame(webView)) {
        g_signal_emit_by_name(webView, "title-changed", m_frame, title.utf8().data());
        g_object_notify(G_OBJECT(webView), "title");
    }
}

void FrameLoaderClient::dispatchDidCommitLoad()
{
    /* Update the URI once first data has been received.
     * This means the URI is valid and successfully identify the page that's going to be loaded.
     */
    g_object_freeze_notify(G_OBJECT(m_frame));

    WebKitWebFramePrivate* priv = m_frame->priv;
    g_free(priv->uri);
    priv->uri = g_strdup(core(m_frame)->loader()->url().prettyURL().utf8().data());
    g_free(priv->title);
    priv->title = NULL;
    g_object_notify(G_OBJECT(m_frame), "uri");
    g_object_notify(G_OBJECT(m_frame), "title");

    g_signal_emit_by_name(m_frame, "load-committed");

    WebKitWebView* webView = getViewFromFrame(m_frame);
    if (m_frame == webkit_web_view_get_main_frame(webView)) {
        g_object_freeze_notify(G_OBJECT(webView));
        g_object_notify(G_OBJECT(webView), "uri");
        g_object_notify(G_OBJECT(webView), "title");
        g_object_thaw_notify(G_OBJECT(webView));
        g_signal_emit_by_name(webView, "load-committed", m_frame);
    }

    g_object_thaw_notify(G_OBJECT(m_frame));
}

void FrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidFirstLayout()
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidFirstVisuallyNonEmptyLayout()
{
    notImplemented();
}

void FrameLoaderClient::dispatchShow()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    webkit_web_view_notify_ready(webView);
}

void FrameLoaderClient::cancelPolicyCheck()
{
    //FIXME Add support for more than one policy decision at once
    if (m_policyDecision)
        webkit_web_policy_decision_cancel(m_policyDecision);
}

void FrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClient::revertToProvisionalState(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClient::willChangeTitle(DocumentLoader*)
{
    notImplemented();
}

void FrameLoaderClient::didChangeTitle(DocumentLoader *l)
{
    setTitle(l->title(), l->url());
}

bool FrameLoaderClient::canHandleRequest(const ResourceRequest&) const
{
    notImplemented();
    return true;
}

bool FrameLoaderClient::canShowMIMEType(const String& type) const
{
    return MIMETypeRegistry::isSupportedImageMIMEType(type) || MIMETypeRegistry::isSupportedNonImageMIMEType(type) ||
        PluginDatabase::installedPlugins()->isMIMETypeRegistered(type);
}

bool FrameLoaderClient::representationExistsForURLScheme(const String&) const
{
    notImplemented();
    return false;
}

String FrameLoaderClient::generatedMIMETypeForURLScheme(const String&) const
{
    notImplemented();
    return String();
}

void FrameLoaderClient::finishedLoading(DocumentLoader* documentLoader)
{
    if (!m_pluginView)
        committedLoad(documentLoader, 0, 0);
    else {
        m_pluginView->didFinishLoading();
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}


void FrameLoaderClient::provisionalLoadStarted()
{
    notImplemented();
}

void FrameLoaderClient::didFinishLoad() {
    notImplemented();
}

void FrameLoaderClient::prepareForDataSourceReplacement() { notImplemented(); }

void FrameLoaderClient::setTitle(const String& title, const KURL& url)
{
    WebKitWebFramePrivate* frameData = WEBKIT_WEB_FRAME_GET_PRIVATE(m_frame);
    g_free(frameData->title);
    frameData->title = g_strdup(title.utf8().data());
}

void FrameLoaderClient::dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived)
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier)
{
    notImplemented();
}

void FrameLoaderClient::dispatchDidFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&)
{
    notImplemented();
}

bool FrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length)
{
    notImplemented();
    return false;
}

void FrameLoaderClient::dispatchDidFailProvisionalLoad(const ResourceError&)
{
    g_signal_emit_by_name(m_frame, "load-done", false);
}

void FrameLoaderClient::dispatchDidFailLoad(const ResourceError&)
{
    g_signal_emit_by_name(m_frame, "load-done", false);
}

void FrameLoaderClient::download(ResourceHandle* handle, const ResourceRequest& request, const ResourceRequest&, const ResourceResponse&)
{
    // FIXME: We could reuse the same handle here, but when I tried
    // implementing that the main load would fail and stop, so I have
    // simplified this case for now.
    handle->cancel();
    startDownload(request);
}

ResourceError FrameLoaderClient::cancelledError(const ResourceRequest&)
{
    notImplemented();
    ResourceError error("", 0, "", "");
    error.setIsCancellation(true);
    return error;
}

ResourceError FrameLoaderClient::blockedError(const ResourceRequest&)
{
    notImplemented();
    return ResourceError("", 0, "", "");
}

ResourceError FrameLoaderClient::cannotShowURLError(const ResourceRequest&)
{
    notImplemented();
    return ResourceError("", 0, "", "");
}

ResourceError FrameLoaderClient::interruptForPolicyChangeError(const ResourceRequest&)
{
    notImplemented();
    return ResourceError("", 0, "", "");
}

ResourceError FrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse&)
{
    notImplemented();
    return ResourceError("", 0, "", "");
}

ResourceError FrameLoaderClient::fileDoesNotExistError(const ResourceResponse&)
{
    notImplemented();
    return ResourceError("", 0, "", "");
}

ResourceError FrameLoaderClient::pluginWillHandleLoadError(const ResourceResponse&)
{
    notImplemented();
    return ResourceError("", 0, "", "");
}

bool FrameLoaderClient::shouldFallBack(const ResourceError&)
{
    notImplemented();
    return false;
}

bool FrameLoaderClient::canCachePage() const
{
    return true;
}

Frame* FrameLoaderClient::dispatchCreatePage()
{
    WebKitWebView* webView = getViewFromFrame(m_frame);
    WebKitWebView* newWebView = 0;

    g_signal_emit_by_name(webView, "create-web-view", m_frame, &newWebView);

    if (!newWebView)
        return 0;

    WebKitWebViewPrivate* privateData = WEBKIT_WEB_VIEW_GET_PRIVATE(newWebView);
    return core(privateData->mainFrame);
}

void FrameLoaderClient::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClient::setMainDocumentError(DocumentLoader*, const ResourceError& error)
{
    if (m_pluginView) {
        m_pluginView->didFail(error);
        m_pluginView = 0;
        m_hasSentResponseToPlugin = false;
    }
}

void FrameLoaderClient::startDownload(const ResourceRequest& request)
{
    WebKitNetworkRequest* networkRequest = webkit_network_request_new(request.url().string().utf8().data());
    WebKitDownload* download = webkit_download_new(networkRequest);
    g_object_unref(networkRequest);

    WebKitWebView* view = getViewFromFrame(m_frame);
    gboolean handled;
    g_signal_emit_by_name(view, "download-requested", download, &handled);

    if (!handled) {
        webkit_download_cancel(download);
        g_object_unref(download);
        return;
    }

    webkit_download_start(download);
}

void FrameLoaderClient::updateGlobalHistory()
{
    notImplemented();
}

void FrameLoaderClient::updateGlobalHistoryRedirectLinks()
{
    notImplemented();
}

void FrameLoaderClient::savePlatformDataToCachedFrame(CachedFrame*)
{
}

void FrameLoaderClient::transitionToCommittedFromCachedFrame(CachedFrame*)
{
}

void FrameLoaderClient::transitionToCommittedForNewPage()
{
    WebKitWebView* containingWindow = getViewFromFrame(m_frame);
    IntSize size = IntSize(GTK_WIDGET(containingWindow)->allocation.width,
                           GTK_WIDGET(containingWindow)->allocation.height);
    bool transparent = webkit_web_view_get_transparent(containingWindow);
    Color backgroundColor = transparent ? WebCore::Color::transparent : WebCore::Color::white;
    Frame* frame = core(m_frame);
    ASSERT(frame);

    frame->createView(size, backgroundColor, transparent, IntSize(), false);

    // We need to do further manipulation on the FrameView if it was the mainFrame
    if (frame != frame->page()->mainFrame())
        return;

    WebKitWebViewPrivate* priv = WEBKIT_WEB_VIEW_GET_PRIVATE(containingWindow);
    frame->view()->setGtkAdjustments(priv->horizontalAdjustment, priv->verticalAdjustment);
}

}
