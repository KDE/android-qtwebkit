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

#include "WebContext.h"

#include "ImmutableArray.h"
#include "InjectedBundleMessageKinds.h"
#include "RunLoop.h"
#include "WKContextPrivate.h"
#include "WebContextMessageKinds.h"
#include "WebContextUserMessageCoders.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageNamespace.h"
#include "WebPreferences.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessManager.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/Language.h>
#include <WebCore/LinkHash.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/PassOwnArrayPtr.h>

#if ENABLE(WEB_PROCESS_SANDBOX)
#include <sandbox.h>
#endif

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

using namespace WebCore;

namespace WebKit {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter webContextCounter("WebContext");
#endif

WebContext* WebContext::sharedProcessContext()
{
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();
    static WebContext* context = adoptRef(new WebContext(ProcessModelSharedSecondaryProcess, String())).leakRef();
    return context;
}

WebContext* WebContext::sharedThreadContext()
{
    RunLoop::initializeMainRunLoop();
    static WebContext* context = adoptRef(new WebContext(ProcessModelSharedSecondaryThread, String())).leakRef();
    return context;
}

PassRefPtr<WebContext> WebContext::create(const String& injectedBundlePath)
{
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();
    return adoptRef(new WebContext(ProcessModelSecondaryProcess, injectedBundlePath));
}
    
WebContext::WebContext(ProcessModel processModel, const String& injectedBundlePath)
    : m_processModel(processModel)
    , m_injectedBundlePath(injectedBundlePath)
    , m_visitedLinkProvider(this)
    , m_cacheModel(CacheModelDocumentViewer)
#if PLATFORM(WIN)
    , m_shouldPaintNativeControls(true)
#endif
{
    addLanguageChangeObserver(this, languageChanged);

    m_preferences = WebPreferences::shared();
    m_preferences->addContext(this);

#ifndef NDEBUG
    webContextCounter.increment();
#endif
}

WebContext::~WebContext()
{
    ASSERT(m_pageNamespaces.isEmpty());
    m_preferences->removeContext(this);
    removeLanguageChangeObserver(this);
    
#ifndef NDEBUG
    webContextCounter.decrement();
#endif
}

void WebContext::initializeInjectedBundleClient(const WKContextInjectedBundleClient* client)
{
    m_injectedBundleClient.initialize(client);
}

void WebContext::initializeHistoryClient(const WKContextHistoryClient* client)
{
    m_historyClient.initialize(client);
    
    if (!hasValidProcess())
        return;
        
    m_process->send(Messages::WebProcess::SetShouldTrackVisitedLinks(m_historyClient.shouldTrackVisitedLinks()), 0);
}

void WebContext::languageChanged(void* context)
{
    static_cast<WebContext*>(context)->languageChanged();
}

void WebContext::languageChanged()
{
    if (!hasValidProcess())
        return;

    m_process->send(Messages::WebProcess::LanguageChanged(defaultLanguage()), 0);
}

void WebContext::ensureWebProcess()
{
    if (hasValidProcess())
        return;

    m_process = WebProcessManager::shared().getWebProcess(this);

    WebProcessCreationParameters parameters;

    parameters.applicationCacheDirectory = applicationCacheDirectory();

    if (!injectedBundlePath().isEmpty()) {
        parameters.injectedBundlePath = injectedBundlePath();

#if ENABLE(WEB_PROCESS_SANDBOX)
        char* sandboxBundleTokenUTF8 = 0;
        CString injectedBundlePathUTF8 = injectedBundlePath().utf8();
        sandbox_issue_extension(injectedBundlePathUTF8.data(), &sandboxBundleTokenUTF8);
        String sandboxBundleToken = String::fromUTF8(sandboxBundleTokenUTF8);
        if (sandboxBundleTokenUTF8)
            free(sandboxBundleTokenUTF8);

        parameters.injectedBundlePathToken = sandboxBundleToken;
#endif
    }

    parameters.shouldTrackVisitedLinks = m_historyClient.shouldTrackVisitedLinks();
    parameters.cacheModel = m_cacheModel;
    parameters.languageCode = defaultLanguage();
    parameters.applicationCacheDirectory = applicationCacheDirectory();

    copyToVector(m_schemesToRegisterAsEmptyDocument, parameters.urlSchemesRegistererdAsEmptyDocument);
    copyToVector(m_schemesToRegisterAsSecure, parameters.urlSchemesRegisteredAsSecure);
    copyToVector(m_schemesToSetDomainRelaxationForbiddenFor, parameters.urlSchemesForWhichDomainRelaxationIsForbidden);

    // Add any platform specific parameters
    platformInitializeWebProcess(parameters);

    m_process->send(Messages::WebProcess::InitializeWebProcess(parameters), 0);

    for (size_t i = 0; i != m_pendingMessagesToPostToInjectedBundle.size(); ++i) {
        pair<String, RefPtr<APIObject> >* message = &m_pendingMessagesToPostToInjectedBundle[i];
        m_process->send(InjectedBundleMessage::PostMessage, 0, CoreIPC::In(message->first, WebContextUserMessageEncoder(message->second.get())));
    }
    m_pendingMessagesToPostToInjectedBundle.clear();
}

void WebContext::processDidFinishLaunching(WebProcessProxy* process)
{
    // FIXME: Once we support multiple processes per context, this assertion won't hold.
    ASSERT(process == m_process);

    m_visitedLinkProvider.populateVisitedLinksIfNeeded();
}

WebPageProxy* WebContext::createWebPage(WebPageNamespace* pageNamespace)
{
    ensureWebProcess();
    return m_process->createWebPage(pageNamespace);
}

void WebContext::reviveIfNecessary()
{
    ensureWebProcess();
}

WebPageNamespace* WebContext::createPageNamespace()
{
    RefPtr<WebPageNamespace> pageNamespace = WebPageNamespace::create(this);
    m_pageNamespaces.add(pageNamespace.get());
    return pageNamespace.release().releaseRef();
}

void WebContext::pageNamespaceWasDestroyed(WebPageNamespace* pageNamespace)
{
    ASSERT(m_pageNamespaces.contains(pageNamespace));
    m_pageNamespaces.remove(pageNamespace);
}

void WebContext::setPreferences(WebPreferences* preferences)
{
    ASSERT(preferences);

    if (preferences == m_preferences)
        return;

    m_preferences->removeContext(this);
    m_preferences = preferences;
    m_preferences->addContext(this);

    // FIXME: Update all Pages/PageNamespace with the new WebPreferences.
}

WebPreferences* WebContext::preferences() const
{
    return m_preferences.get();
}

void WebContext::preferencesDidChange()
{
    if (!m_process)
        return;

    for (HashSet<WebPageNamespace*>::iterator it = m_pageNamespaces.begin(), end = m_pageNamespaces.end(); it != end; ++it) {
        WebPageNamespace* pageNamespace = *it;
        pageNamespace->preferencesDidChange();
    }
}

void WebContext::postMessageToInjectedBundle(const String& messageName, APIObject* messageBody)
{
    if (!hasValidProcess()) {
        m_pendingMessagesToPostToInjectedBundle.append(make_pair(messageName, messageBody));
        return;
    }

    // FIXME: We should consider returning false from this function if the messageBody cannot
    // be encoded.
    m_process->send(InjectedBundleMessage::PostMessage, 0, CoreIPC::In(messageName, WebContextUserMessageEncoder(messageBody)));
}

// InjectedBundle client

void WebContext::didReceiveMessageFromInjectedBundle(const String& messageName, APIObject* messageBody)
{
    m_injectedBundleClient.didReceiveMessageFromInjectedBundle(this, messageName, messageBody);
}

void WebContext::didReceiveSynchronousMessageFromInjectedBundle(const String& messageName, APIObject* messageBody, RefPtr<APIObject>& returnData)
{
    m_injectedBundleClient.didReceiveSynchronousMessageFromInjectedBundle(this, messageName, messageBody, returnData);
}

// HistoryClient

void WebContext::didNavigateWithNavigationData(WebFrameProxy* frame, const WebNavigationDataStore& store) 
{
    ASSERT(frame->page());
    m_historyClient.didNavigateWithNavigationData(this, frame->page(), store, frame);
}

void WebContext::didPerformClientRedirect(WebFrameProxy* frame, const String& sourceURLString, const String& destinationURLString)
{
    ASSERT(frame->page());
    m_historyClient.didPerformClientRedirect(this, frame->page(), sourceURLString, destinationURLString, frame);
}

void WebContext::didPerformServerRedirect(WebFrameProxy* frame, const String& sourceURLString, const String& destinationURLString)
{
    ASSERT(frame->page());
    m_historyClient.didPerformServerRedirect(this, frame->page(), sourceURLString, destinationURLString, frame);
}

void WebContext::didUpdateHistoryTitle(WebFrameProxy* frame, const String& title, const String& url)
{
    ASSERT(frame->page());
    m_historyClient.didUpdateHistoryTitle(this, frame->page(), title, url, frame);
}

void WebContext::populateVisitedLinks()
{
    m_historyClient.populateVisitedLinks(this);
}

void WebContext::getStatistics(WKContextStatistics* statistics)
{
    memset(statistics, 0, sizeof(WKContextStatistics));

    statistics->numberOfWKPageNamespaces = m_pageNamespaces.size();

    for (HashSet<WebPageNamespace*>::iterator it = m_pageNamespaces.begin(), end = m_pageNamespaces.end(); it != end; ++it)
        (*it)->getStatistics(statistics);
}

void WebContext::setAdditionalPluginsDirectory(const String& directory)
{
    Vector<String> directories;
    directories.append(directory);

    m_pluginInfoStore.setAdditionalPluginsDirectories(directories);
}

void WebContext::registerURLSchemeAsEmptyDocument(const String& urlScheme)
{
    m_schemesToRegisterAsEmptyDocument.add(urlScheme);

    if (!hasValidProcess())
        return;

    m_process->send(Messages::WebProcess::RegisterURLSchemeAsEmptyDocument(urlScheme), 0);
}

void WebContext::registerURLSchemeAsSecure(const String& urlScheme)
{
    m_schemesToRegisterAsSecure.add(urlScheme);

    if (!hasValidProcess())
        return;

    m_process->send(Messages::WebProcess::RegisterURLSchemeAsSecure(urlScheme), 0);
}

void WebContext::setDomainRelaxationForbiddenForURLScheme(const String& urlScheme)
{
    m_schemesToSetDomainRelaxationForbiddenFor.add(urlScheme);

    if (!hasValidProcess())
        return;

    m_process->send(Messages::WebProcess::SetDomainRelaxationForbiddenForURLScheme(urlScheme), 0);
}

void WebContext::addVisitedLink(const String& visitedURL)
{
    if (visitedURL.isEmpty())
        return;

    LinkHash linkHash = visitedLinkHash(visitedURL.characters(), visitedURL.length());
    addVisitedLink(linkHash);
}

void WebContext::addVisitedLink(LinkHash linkHash)
{
    m_visitedLinkProvider.addVisitedLink(linkHash);
}

void WebContext::setCacheModel(CacheModel cacheModel)
{
    m_cacheModel = cacheModel;

    if (!hasValidProcess())
        return;
    m_process->send(Messages::WebProcess::SetCacheModel(static_cast<uint32_t>(m_cacheModel)), 0);
}

void WebContext::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    switch (messageID.get<WebContextMessage::Kind>()) {
        case WebContextMessage::PostMessage: {
            String messageName;
            RefPtr<APIObject> messageBody;
            WebContextUserMessageDecoder messageDecoder(messageBody, this);
            if (!arguments->decode(CoreIPC::Out(messageName, messageDecoder)))
                return;

            didReceiveMessageFromInjectedBundle(messageName, messageBody.get());
            return;
        }
        case WebContextMessage::PostSynchronousMessage:
            ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
}

void WebContext::didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
    switch (messageID.get<WebContextMessage::Kind>()) {
        case WebContextMessage::PostSynchronousMessage: {
            // FIXME: We should probably encode something in the case that the arguments do not decode correctly.

            String messageName;
            RefPtr<APIObject> messageBody;
            WebContextUserMessageDecoder messageDecoder(messageBody, this);
            if (!arguments->decode(CoreIPC::Out(messageName, messageDecoder)))
                return;

            RefPtr<APIObject> returnData;
            didReceiveSynchronousMessageFromInjectedBundle(messageName, messageBody.get(), returnData);
            reply->encode(CoreIPC::In(WebContextUserMessageEncoder(returnData.get())));
            return;
        }
        case WebContextMessage::PostMessage:
            ASSERT_NOT_REACHED();
    }
}

} // namespace WebKit
