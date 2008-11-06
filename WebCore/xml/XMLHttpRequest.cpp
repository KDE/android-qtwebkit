/*
 *  Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005-2007 Alexey Proskuryakov <ap@webkit.org>
 *  Copyright (C) 2007, 2008 Julien Chaffraix <jchaffraix@webkit.org>
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
#include "XMLHttpRequest.h"

#include "CString.h"
#include "Console.h"
#include "DOMImplementation.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "File.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTTPParsers.h"
#include "InspectorController.h"
#include "JSDOMBinding.h"
#include "JSDOMWindow.h"
#include "KURL.h"
#include "KURLHash.h"
#include "Page.h"
#include "Settings.h"
#include "SubresourceLoader.h"
#include "SystemTime.h"
#include "TextResourceDecoder.h"
#include "XMLHttpRequestException.h"
#include "XMLHttpRequestProgressEvent.h"
#include "XMLHttpRequestUpload.h"
#include "markup.h"
#include <runtime/JSLock.h>

namespace WebCore {

struct PreflightResultCacheItem {
    PreflightResultCacheItem(unsigned expiryDelta, bool credentials, HashSet<String>* methods, HashSet<String, CaseFoldingHash>* headers)
        : m_absoluteExpiryTime(currentTime() + expiryDelta)
        , m_credentials(credentials)
        , m_methods(methods)
        , m_headers(headers)
    {
    }

    // FIXME: A better solution to holding onto the absolute expiration time might be
    // to start a timer for the expiration delta, that removes this from the cache when
    // it fires.
    double m_absoluteExpiryTime;
    bool m_credentials;
    OwnPtr<HashSet<String> > m_methods;
    OwnPtr<HashSet<String, CaseFoldingHash> > m_headers;
};

typedef HashMap<std::pair<String, KURL>, PreflightResultCacheItem*> PreflightResultCache;

static PreflightResultCache& preflightResultCache()
{
    static PreflightResultCache cache;
    return cache;
}

static void appendPreflightResultCacheEntry(String origin, KURL url, unsigned expiryDelta, 
                                            bool credentials, HashSet<String>* methods, HashSet<String, CaseFoldingHash>* headers)
{
    ASSERT(!preflightResultCache().contains(std::make_pair(origin, url)));

    PreflightResultCacheItem* item = new PreflightResultCacheItem(expiryDelta, credentials, methods, headers);
    preflightResultCache().set(std::make_pair(origin, url), item);
}

static bool isSafeRequestHeader(const String& name)
{
    static HashSet<String, CaseFoldingHash> forbiddenHeaders;
    static String proxyString("proxy-");
    static String secString("sec-");
    
    if (forbiddenHeaders.isEmpty()) {
        forbiddenHeaders.add("accept-charset");
        forbiddenHeaders.add("accept-encoding");
        forbiddenHeaders.add("connection");
        forbiddenHeaders.add("content-length");
        forbiddenHeaders.add("content-transfer-encoding");
        forbiddenHeaders.add("date");
        forbiddenHeaders.add("expect");
        forbiddenHeaders.add("host");
        forbiddenHeaders.add("keep-alive");
        forbiddenHeaders.add("referer");
        forbiddenHeaders.add("te");
        forbiddenHeaders.add("trailer");
        forbiddenHeaders.add("transfer-encoding");
        forbiddenHeaders.add("upgrade");
        forbiddenHeaders.add("via");
    }
    
    return !forbiddenHeaders.contains(name) && !name.startsWith(proxyString, false) &&
           !name.startsWith(secString, false);
}

static bool isOnAccessControlSimpleRequestHeaderWhitelist(const String& name)
{
    return equalIgnoringCase(name, "accept") || equalIgnoringCase(name, "accept-language") || equalIgnoringCase(name, "content-type");
}

static bool isOnAccessControlResponseHeaderWhitelist(const String& name)
{
    static HashSet<String, CaseFoldingHash> allowedHeaders;
    if (allowedHeaders.isEmpty()) {
        allowedHeaders.add("cache-control");
        allowedHeaders.add("content-language");
        allowedHeaders.add("content-type");
        allowedHeaders.add("expires");
        allowedHeaders.add("last-modified");
        allowedHeaders.add("pragma");
    }

    return allowedHeaders.contains(name);
}

// Determines if a string is a valid token, as defined by
// "token" in section 2.2 of RFC 2616.
static bool isValidToken(const String& name)
{
    unsigned length = name.length();
    for (unsigned i = 0; i < length; i++) {
        UChar c = name[i];
        
        if (c >= 127 || c <= 32)
            return false;
        
        if (c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
            c == ',' || c == ';' || c == ':' || c == '\\' || c == '\"' ||
            c == '/' || c == '[' || c == ']' || c == '?' || c == '=' ||
            c == '{' || c == '}')
            return false;
    }
    
    return true;
}
    
static bool isValidHeaderValue(const String& name)
{
    // FIXME: This should really match name against 
    // field-value in section 4.2 of RFC 2616.
        
    return !name.contains('\r') && !name.contains('\n');
}

XMLHttpRequest::XMLHttpRequest(Document* doc)
    : ActiveDOMObject(doc, this)
    , m_async(true)
    , m_includeCredentials(false)
    , m_state(UNSENT)
    , m_identifier(std::numeric_limits<unsigned long>::max())
    , m_responseText("")
    , m_createdDocument(false)
    , m_error(false)
    , m_uploadComplete(false)
    , m_sameOriginRequest(true)
    , m_inPreflight(false)
    , m_receivedLength(0)
    , m_lastSendLineNumber(0)
{
    ASSERT(document());
}

XMLHttpRequest::~XMLHttpRequest()
{
    if (m_upload)
        m_upload->disconnectXMLHttpRequest();
}

Document* XMLHttpRequest::document() const
{
    ASSERT(scriptExecutionContext()->isDocument());
    return static_cast<Document*>(scriptExecutionContext());
}

XMLHttpRequest::State XMLHttpRequest::readyState() const
{
    return m_state;
}

const JSC::UString& XMLHttpRequest::responseText() const
{
    return m_responseText;
}

Document* XMLHttpRequest::responseXML() const
{
    if (m_state != DONE)
        return 0;

    if (!m_createdDocument) {
        if (m_response.isHTTP() && !responseIsXML()) {
            // The W3C spec requires this.
            m_responseXML = 0;
        } else {
            m_responseXML = document()->implementation()->createDocument(0);
            m_responseXML->open();
            m_responseXML->setURL(m_url);
            // FIXME: set Last-Modified and cookies (currently, those are only available for HTMLDocuments).
            m_responseXML->write(String(m_responseText));
            m_responseXML->finishParsing();
            m_responseXML->close();
            
            if (!m_responseXML->wellFormed())
                m_responseXML = 0;
        }
        m_createdDocument = true;
    }

    return m_responseXML.get();
}

XMLHttpRequestUpload* XMLHttpRequest::upload()
{
    if (!m_upload)
        m_upload = XMLHttpRequestUpload::create(this);
    return m_upload.get();
}

void XMLHttpRequest::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end()) {
        ListenerVector listeners;
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    } else {
        ListenerVector& listeners = iter->second;
        for (ListenerVector::iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter)
            if (*listenerIter == eventListener)
                return;
        
        listeners.append(eventListener);
        m_eventListeners.add(eventType, listeners);
    }
}

void XMLHttpRequest::removeEventListener(const AtomicString& eventType, EventListener* eventListener, bool)
{
    EventListenersMap::iterator iter = m_eventListeners.find(eventType);
    if (iter == m_eventListeners.end())
        return;

    ListenerVector& listeners = iter->second;
    for (ListenerVector::const_iterator listenerIter = listeners.begin(); listenerIter != listeners.end(); ++listenerIter)
        if (*listenerIter == eventListener) {
            listeners.remove(listenerIter - listeners.begin());
            return;
        }
}

bool XMLHttpRequest::dispatchEvent(PassRefPtr<Event> evt, ExceptionCode& ec)
{
    // FIXME: check for other error conditions enumerated in the spec.
    if (evt->type().isEmpty()) {
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return true;
    }

    ListenerVector listenersCopy = m_eventListeners.get(evt->type());
    for (ListenerVector::const_iterator listenerIter = listenersCopy.begin(); listenerIter != listenersCopy.end(); ++listenerIter) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        listenerIter->get()->handleEvent(evt.get(), false);
    }

    return !evt->defaultPrevented();
}

void XMLHttpRequest::changeState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        callReadyStateChangeListener();
    }
}

void XMLHttpRequest::callReadyStateChangeListener()
{
    if (!document() || !document()->frame())
        return;

    dispatchReadyStateChangeEvent();

    if (m_state == DONE)
        dispatchLoadEvent();
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, ExceptionCode& ec)
{
    internalAbort();
    State previousState = m_state;
    m_state = UNSENT;
    m_error = false;

    m_uploadComplete = false;

    // clear stuff from possible previous load
    clearResponse();
    clearRequest();

    ASSERT(m_state == UNSENT);

    if (!isValidToken(method)) {
        ec = SYNTAX_ERR;
        return;
    }
    
    // Method names are case sensitive. But since Firefox uppercases method names it knows, we'll do the same.
    String methodUpper(method.upper());
    
    if (methodUpper == "TRACE" || methodUpper == "TRACK" || methodUpper == "CONNECT") {
        ec = SECURITY_ERR;
        return;
    }

    m_url = url;

    if (methodUpper == "COPY" || methodUpper == "DELETE" || methodUpper == "GET" || methodUpper == "HEAD"
        || methodUpper == "INDEX" || methodUpper == "LOCK" || methodUpper == "M-POST" || methodUpper == "MKCOL" || methodUpper == "MOVE"
        || methodUpper == "OPTIONS" || methodUpper == "POST" || methodUpper == "PROPFIND" || methodUpper == "PROPPATCH" || methodUpper == "PUT" 
        || methodUpper == "UNLOCK")
        m_method = methodUpper;
    else
        m_method = method;

    m_async = async;

    ASSERT(!m_loader);

    // Check previous state to avoid dispatching readyState event
    // when calling open several times in a row.
    if (previousState != OPENED)
        changeState(OPENED);
    else
        m_state = OPENED;
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, ExceptionCode& ec)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user);
    
    open(method, urlWithCredentials, async, ec);
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, const String& password, ExceptionCode& ec)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user);
    urlWithCredentials.setPass(password);
    
    open(method, urlWithCredentials, async, ec);
}

bool XMLHttpRequest::initSend(ExceptionCode& ec)
{
    if (!document())
        return false;

    if (m_state != OPENED || m_loader) {
        ec = INVALID_STATE_ERR;
        return false;
    }

    m_error = false;
    return true;
}

void XMLHttpRequest::send(ExceptionCode& ec)
{
    send(String(), ec);
}

void XMLHttpRequest::send(Document* document, ExceptionCode& ec)
{
    ASSERT(document);

    if (!initSend(ec))
        return;

    if (m_method != "GET" && m_method != "HEAD" && (m_url.protocolIs("http") || m_url.protocolIs("https"))) {
        String contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
#if ENABLE(DASHBOARD_SUPPORT)
            Settings* settings = document->settings();
            if (settings && settings->usesDashboardBackwardCompatibilityMode())
                setRequestHeaderInternal("Content-Type", "application/x-www-form-urlencoded");
            else
#endif
                // FIXME: this should include the charset used for encoding.
                setRequestHeaderInternal("Content-Type", "application/xml");
        }

        // FIXME: According to XMLHttpRequest Level 2, this should use the Document.innerHTML algorithm
        // from the HTML5 specification to serialize the document.
        String body = createMarkup(document);

        // FIXME: this should use value of document.inputEncoding to determine the encoding to use.
        TextEncoding encoding = UTF8Encoding();
        m_requestEntityBody = FormData::create(encoding.encode(body.characters(), body.length(), EntitiesForUnencodables));
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    createRequest(ec);
}

void XMLHttpRequest::send(const String& body, ExceptionCode& ec)
{
    if (!initSend(ec))
        return;

    if (!body.isNull() && m_method != "GET" && m_method != "HEAD" && (m_url.protocolIs("http") || m_url.protocolIs("https"))) {
        String contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
#if ENABLE(DASHBOARD_SUPPORT)
            Settings* settings = document()->settings();
            if (settings && settings->usesDashboardBackwardCompatibilityMode())
                setRequestHeaderInternal("Content-Type", "application/x-www-form-urlencoded");
            else
#endif
                setRequestHeaderInternal("Content-Type", "application/xml");
        }

        m_requestEntityBody = FormData::create(UTF8Encoding().encode(body.characters(), body.length(), EntitiesForUnencodables));
        if (m_upload)
            m_requestEntityBody->setAlwaysStream(true);
    }

    createRequest(ec);
}

void XMLHttpRequest::send(File* body, ExceptionCode& ec)
{
    if (!initSend(ec))
        return;

    if (m_method != "GET" && m_method != "HEAD" && (m_url.protocolIs("http") || m_url.protocolIs("https"))) {
        // FIXME: Should we set a Content-Type if one is not set.
        // FIXME: add support for uploading bundles.
        m_requestEntityBody = FormData::create();
        m_requestEntityBody->appendFile(body->path(), false);
    }

    createRequest(ec);
}

void XMLHttpRequest::createRequest(ExceptionCode& ec)
{
    if (m_async) {
        dispatchLoadStartEvent();
        if (m_requestEntityBody && m_upload)
            m_upload->dispatchLoadStartEvent();
    }

    m_sameOriginRequest = document()->securityOrigin()->canRequest(m_url);

    if (!m_sameOriginRequest) {
        makeCrossSiteAccessRequest(ec);
        return;
    }

    makeSameOriginRequest(ec);
}

void XMLHttpRequest::makeSameOriginRequest(ExceptionCode& ec)
{
    ASSERT(m_sameOriginRequest);

    ResourceRequest request(m_url);
    request.setHTTPMethod(m_method);

    if (m_requestEntityBody) {
        ASSERT(m_method != "GET");
        request.setHTTPBody(m_requestEntityBody.release());
    }

    if (m_requestHeaders.size() > 0)
        request.addHTTPHeaderFields(m_requestHeaders);

    if (m_async)
        loadRequestAsynchronously(request);
    else
        loadRequestSynchronously(request, ec);
}

bool XMLHttpRequest::isSimpleCrossSiteAccessRequest() const
{
    if (m_method != "GET" && m_method != "POST")
        return false;

    HTTPHeaderMap::const_iterator end = m_requestHeaders.end();
    for (HTTPHeaderMap::const_iterator it = m_requestHeaders.begin(); it != end; ++it) {
        if (!isOnAccessControlSimpleRequestHeaderWhitelist(it->first))
            return false;
    }

    return true;
}

void XMLHttpRequest::makeCrossSiteAccessRequest(ExceptionCode& ec)
{
    ASSERT(!m_sameOriginRequest);

    if (isSimpleCrossSiteAccessRequest())
        makeSimpleCrossSiteAccessRequest(ec);
    else
        makeCrossSiteAccessRequestWithPreflight(ec);
}

void XMLHttpRequest::makeSimpleCrossSiteAccessRequest(ExceptionCode& ec)
{
    ASSERT(isSimpleCrossSiteAccessRequest());

    KURL url = m_url;
    url.setUser(String());
    url.setPass(String());
 
    ResourceRequest request(url);
    request.setHTTPMethod(m_method);
    request.setAllowHTTPCookies(m_includeCredentials);
    request.setHTTPOrigin(document()->securityOrigin()->toString());

    if (m_requestHeaders.size() > 0)
        request.addHTTPHeaderFields(m_requestHeaders);

    if (m_async)
        loadRequestAsynchronously(request);
    else
        loadRequestSynchronously(request, ec);
}

static bool canSkipPrelight(PreflightResultCache::iterator cacheIt, bool includeCredentials, const String& method, const HTTPHeaderMap& requestHeaders)
{
    PreflightResultCacheItem* item = cacheIt->second;
    if (item->m_absoluteExpiryTime < currentTime())
        return false;
    if (includeCredentials && !item->m_credentials)
        return false;
    if (!item->m_methods->contains(method) && method != "GET" && method != "POST")
        return false;
    HTTPHeaderMap::const_iterator end = requestHeaders.end();
    for (HTTPHeaderMap::const_iterator it = requestHeaders.begin(); it != end; ++it) {
        if (!item->m_headers->contains(it->first) && !isOnAccessControlSimpleRequestHeaderWhitelist(it->first))
            return false;
    }

    return true;
}

void XMLHttpRequest::makeCrossSiteAccessRequestWithPreflight(ExceptionCode& ec)
{
    String origin = document()->securityOrigin()->toString();
    KURL url = m_url;
    url.setUser(String());
    url.setPass(String());

    bool skipPreflight = false;

    PreflightResultCache::iterator cacheIt = preflightResultCache().find(std::make_pair(origin, url));
    if (cacheIt != preflightResultCache().end()) {
        skipPreflight = canSkipPrelight(cacheIt, m_includeCredentials, m_method, m_requestHeaders);
        if (!skipPreflight) {
            delete cacheIt->second;
            preflightResultCache().remove(cacheIt);
        }
    }

    if (!skipPreflight) {
        m_inPreflight = true;
        ResourceRequest preflightRequest(url);
        preflightRequest.setHTTPMethod("OPTIONS");
        preflightRequest.setHTTPHeaderField("Origin", origin);
        preflightRequest.setHTTPHeaderField("Access-Control-Request-Method", m_method);

        if (m_requestHeaders.size() > 0) {
            Vector<UChar> headerBuffer;
            HTTPHeaderMap::const_iterator it = m_requestHeaders.begin();
            append(headerBuffer, it->first);
            ++it;

            HTTPHeaderMap::const_iterator end = m_requestHeaders.end();
            for (; it != end; ++it) {
                headerBuffer.append(',');
                headerBuffer.append(' ');
                append(headerBuffer, it->first);
            }

            preflightRequest.setHTTPHeaderField("Access-Control-Request-Headers", String::adopt(headerBuffer));
            preflightRequest.addHTTPHeaderFields(m_requestHeaders);
        }

        if (m_async) {
            loadRequestAsynchronously(preflightRequest);
            return;
        }

        loadRequestSynchronously(preflightRequest, ec);
        m_inPreflight = false;

        if (ec)
            return;
    }

    // Send the actual request.
    ResourceRequest request(url);
    request.setHTTPMethod(m_method);
    request.setAllowHTTPCookies(m_includeCredentials);
    request.setHTTPHeaderField("Origin", origin);

    if (m_requestHeaders.size() > 0)
        request.addHTTPHeaderFields(m_requestHeaders);

    if (m_requestEntityBody) {
        ASSERT(m_method != "GET");
        request.setHTTPBody(m_requestEntityBody.release());
    }

    if (m_async) {
        loadRequestAsynchronously(request);
        return;
    }

    loadRequestSynchronously(request, ec);
}

void XMLHttpRequest::handleAsynchronousPreflightResult()
{
    ASSERT(m_inPreflight);
    ASSERT(m_async);

    m_inPreflight = false;

    KURL url = m_url;
    url.setUser(String());
    url.setPass(String());

    ResourceRequest request(url);
    request.setHTTPMethod(m_method);
    request.setAllowHTTPCookies(m_includeCredentials);
    request.setHTTPOrigin(document()->securityOrigin()->toString());

    if (m_requestHeaders.size() > 0)
        request.addHTTPHeaderFields(m_requestHeaders);

    if (m_requestEntityBody) {
        ASSERT(m_method != "GET");
        request.setHTTPBody(m_requestEntityBody.release());
    }

    loadRequestAsynchronously(request);
}

void XMLHttpRequest::loadRequestSynchronously(ResourceRequest& request, ExceptionCode& ec)
{
    ASSERT(!m_async);
    Vector<char> data;
    ResourceError error;
    ResourceResponse response;

    if (document()->frame())
        m_identifier = document()->frame()->loader()->loadResourceSynchronously(request, error, response, data);

    m_loader = 0;

    // No exception for file:/// resources, see <rdar://problem/4962298>.
    // Also, if we have an HTTP response, then it wasn't a network error in fact.
    if (error.isNull() || request.url().isLocalFile() || response.httpStatusCode() > 0) {
        processSyncLoadResults(data, response, ec);
        return;
    }

    if (error.isCancellation()) {
        abortError();
        ec = XMLHttpRequestException::ABORT_ERR;
        return;
    }

    networkError();
    ec = XMLHttpRequestException::NETWORK_ERR;
}


void XMLHttpRequest::loadRequestAsynchronously(ResourceRequest& request)
{
    ASSERT(m_async);
    // SubresourceLoader::create can return null here, for example if we're no longer attached to a page.
    // This is true while running onunload handlers.
    // FIXME: We need to be able to send XMLHttpRequests from onunload, <http://bugs.webkit.org/show_bug.cgi?id=10904>.
    // FIXME: Maybe create can return null for other reasons too?
    // We need to keep content sniffing enabled for local files due to CFNetwork not providing a MIME type
    // for local files otherwise, <rdar://problem/5671813>.
    bool sendResourceLoadCallbacks = !m_inPreflight;
    m_loader = SubresourceLoader::create(document()->frame(), this, request, false, sendResourceLoadCallbacks, request.url().isLocalFile());

    if (m_loader) {
        // Neither this object nor the JavaScript wrapper should be deleted while
        // a request is in progress because we need to keep the listeners alive,
        // and they are referenced by the JavaScript wrapper.
        setPendingActivity(this);
    }
}

void XMLHttpRequest::abort()
{
    bool sendFlag = m_loader;

    internalAbort();

    // Clear headers as required by the spec
    m_requestHeaders.clear();
    
    if ((m_state <= OPENED && !sendFlag) || m_state == DONE)
        m_state = UNSENT;
     else {
        ASSERT(!m_loader);
        changeState(DONE);
        m_state = UNSENT;
    }

    dispatchAbortEvent();
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload)
            m_upload->dispatchAbortEvent();
    }
}

void XMLHttpRequest::internalAbort()
{
    bool hadLoader = m_loader;

    m_error = true;

    // FIXME: when we add the support for multi-part XHR, we will have to think be careful with this initialization.
    m_receivedLength = 0;

    if (hadLoader) {
        m_loader->cancel();
        m_loader = 0;
    }

    m_decoder = 0;

    if (hadLoader)
        dropProtection();
}

void XMLHttpRequest::clearResponse()
{
    m_response = ResourceResponse();
    {
        JSC::JSLock lock(false);
        m_responseText = "";
    }
    m_createdDocument = false;
    m_responseXML = 0;
}

void XMLHttpRequest::clearRequest()
{
    m_requestHeaders.clear();
    m_requestEntityBody = 0;
}

void XMLHttpRequest::genericError()
{
    clearResponse();
    clearRequest();
    m_error = true;

    // The spec says we should "Synchronously switch the state to DONE." and then "Synchronously dispatch a readystatechange event on the object"
    // but this does not match Firefox.
}

void XMLHttpRequest::networkError()
{
    genericError();
    dispatchErrorEvent();
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload)
            m_upload->dispatchErrorEvent();
    }
}

void XMLHttpRequest::abortError()
{
    genericError();
    dispatchAbortEvent();
    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload)
            m_upload->dispatchAbortEvent();
    }
}

void XMLHttpRequest::dropProtection()        
{
    // The XHR object itself holds on to the responseText, and
    // thus has extra cost even independent of any
    // responseText or responseXML objects it has handed
    // out. But it is protected from GC while loading, so this
    // can't be recouped until the load is done, so only
    // report the extra cost at that point.

    if (JSDOMWindow* window = toJSDOMWindow(document()->frame())) {
        if (JSC::JSValue* wrapper = getCachedDOMObjectWrapper(*window->globalData(), this))
            JSC::Heap::heap(wrapper)->reportExtraMemoryCost(m_responseText.size() * 2);
    }

    unsetPendingActivity(this);
}

void XMLHttpRequest::overrideMimeType(const String& override)
{
    m_mimeTypeOverride = override;
}
    
void XMLHttpRequest::setRequestHeader(const String& name, const String& value, ExceptionCode& ec)
{
    if (m_state != OPENED || m_loader) {
#if ENABLE(DASHBOARD_SUPPORT)
        Settings* settings = document() ? document()->settings() : 0;
        if (settings && settings->usesDashboardBackwardCompatibilityMode())
            return;
#endif

        ec = INVALID_STATE_ERR;
        return;
    }

    if (!isValidToken(name) || !isValidHeaderValue(value)) {
        ec = SYNTAX_ERR;
        return;
    }

    // A privileged script (e.g. a Dashboard widget) can set any headers.
    if (!document()->securityOrigin()->canLoadLocalResources() && !isSafeRequestHeader(name)) {
        if (document() && document()->frame())
            document()->frame()->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, "Refused to set unsafe header \"" + name + "\"", 1, String());
        return;
    }

    setRequestHeaderInternal(name, value);
}

void XMLHttpRequest::setRequestHeaderInternal(const String& name, const String& value)
{
    pair<HTTPHeaderMap::iterator, bool> result = m_requestHeaders.add(name, value); 
    if (!result.second)
        result.first->second += ", " + value;
}

String XMLHttpRequest::getRequestHeader(const String& name) const
{
    return m_requestHeaders.get(name);
}

String XMLHttpRequest::getAllResponseHeaders(ExceptionCode& ec) const
{
    if (m_state < LOADING) {
        ec = INVALID_STATE_ERR;
        return "";
    }

    Vector<UChar> stringBuilder;
    String separator(": ");

    HTTPHeaderMap::const_iterator end = m_response.httpHeaderFields().end();
    for (HTTPHeaderMap::const_iterator it = m_response.httpHeaderFields().begin(); it!= end; ++it) {
        if (!m_sameOriginRequest && !isOnAccessControlResponseHeaderWhitelist(it->first))
            continue;

        stringBuilder.append(it->first.characters(), it->first.length());
        stringBuilder.append(separator.characters(), separator.length());
        stringBuilder.append(it->second.characters(), it->second.length());
        stringBuilder.append((UChar)'\r');
        stringBuilder.append((UChar)'\n');
    }

    return String::adopt(stringBuilder);
}

String XMLHttpRequest::getResponseHeader(const String& name, ExceptionCode& ec) const
{
    if (m_state < LOADING) {
        ec = INVALID_STATE_ERR;
        return "";
    }

    if (!isValidToken(name))
        return "";

    if (!m_sameOriginRequest && !isOnAccessControlResponseHeaderWhitelist(name))
        return "";

    return m_response.httpHeaderField(name);
}

String XMLHttpRequest::responseMIMEType() const
{
    String mimeType = extractMIMETypeFromMediaType(m_mimeTypeOverride);
    if (mimeType.isEmpty()) {
        if (m_response.isHTTP())
            mimeType = extractMIMETypeFromMediaType(m_response.httpHeaderField("Content-Type"));
        else
            mimeType = m_response.mimeType();
    }
    if (mimeType.isEmpty())
        mimeType = "text/xml";
    
    return mimeType;
}

bool XMLHttpRequest::responseIsXML() const
{
    return DOMImplementation::isXMLMIMEType(responseMIMEType());
}

int XMLHttpRequest::status(ExceptionCode& ec) const
{
    if (m_response.httpStatusCode())
        return m_response.httpStatusCode();

    if (m_state == OPENED) {
        // Firefox only raises an exception in this state; we match it.
        // Note the case of local file requests, where we have no HTTP response code! Firefox never raises an exception for those, but we match HTTP case for consistency.
        ec = INVALID_STATE_ERR;
    }

    return 0;
}

String XMLHttpRequest::statusText(ExceptionCode& ec) const
{
    // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=3547> XMLHttpRequest.statusText returns always "OK".
    if (m_response.httpStatusCode())
        return "OK";

    if (m_state == OPENED) {
        // See comments in getStatus() above.
        ec = INVALID_STATE_ERR;
    }

    return String();
}

void XMLHttpRequest::processSyncLoadResults(const Vector<char>& data, const ResourceResponse& response, ExceptionCode& ec)
{
    if (m_sameOriginRequest && !document()->securityOrigin()->canRequest(response.url())) {
        abort();
        return;
    }
    
    didReceiveResponse(0, response);
    changeState(HEADERS_RECEIVED);

    const char* bytes = static_cast<const char*>(data.data());
    int len = static_cast<int>(data.size());
    didReceiveData(0, bytes, len);

    didFinishLoading(0);
    if (m_error)
        ec = XMLHttpRequestException::NETWORK_ERR;
}

void XMLHttpRequest::didFail(SubresourceLoader* loader, const ResourceError& error)
{
    // If we are already in an error state, for instance we called abort(), bail out early.
    if (m_error)
        return;

    if (error.isCancellation()) {
        abortError();
        return;
    }

    networkError();
    return;
}

void XMLHttpRequest::didFinishLoading(SubresourceLoader* loader)
{
    if (m_error)
        return;

    if (m_inPreflight) {
        didFinishLoadingPreflight(loader);
        return;
    }

    ASSERT(loader == m_loader);

    if (m_state < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    {
        JSC::JSLock lock(false);
        if (m_decoder)
            m_responseText += m_decoder->flush();
    }

    if (Frame* frame = document()->frame()) {
        if (Page* page = frame->page()) {
            page->inspectorController()->resourceRetrievedByXMLHttpRequest(m_loader ? m_loader->identifier() : m_identifier, m_responseText);
            page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, "XHR finished loading: \"" + m_url + "\".", m_lastSendLineNumber, m_lastSendURL);
        }
    }

    bool hadLoader = m_loader;
    m_loader = 0;

    changeState(DONE);
    m_decoder = 0;

    if (hadLoader)
        dropProtection();
}

void XMLHttpRequest::didFinishLoadingPreflight(SubresourceLoader* loader)
{
    ASSERT(m_inPreflight);
    ASSERT(!m_sameOriginRequest);

    // FIXME: this can probably be moved to didReceiveResponsePreflight.
    if (m_async)
        handleAsynchronousPreflightResult();

    if (m_loader)
        unsetPendingActivity(this);
}

void XMLHttpRequest::willSendRequest(SubresourceLoader*, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    // FIXME: This needs to be fixed to follow the redirect correctly even for cross-domain requests.
    if (!document()->securityOrigin()->canRequest(request.url())) {
        internalAbort();
        networkError();
    }
}

void XMLHttpRequest::didSendData(SubresourceLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (!m_upload)
        return;

    m_upload->dispatchProgressEvent(bytesSent, totalBytesToBeSent);

    if (bytesSent == totalBytesToBeSent && !m_uploadComplete) {
        m_uploadComplete = true;
        m_upload->dispatchLoadEvent();
    }
}

bool XMLHttpRequest::accessControlCheck(const ResourceResponse& response)
{
    const String& accessControlOriginString = response.httpHeaderField("Access-Control-Origin");
    if (accessControlOriginString == "*" && !m_includeCredentials)
        return true;

    KURL accessControlOriginURL(accessControlOriginString);
    if (!accessControlOriginURL.isValid())
        return false;

    RefPtr<SecurityOrigin> accessControlOrigin = SecurityOrigin::create(accessControlOriginURL);
    if (!accessControlOrigin->isSameSchemeHostPort(document()->securityOrigin()))
        return false;

    if (m_includeCredentials) {
        const String& accessControlCredentialsString = response.httpHeaderField("Access-Control-Credentials");
        if (accessControlCredentialsString != "true")
            return false;
    }

    return true;
}

void XMLHttpRequest::didReceiveResponse(SubresourceLoader* loader, const ResourceResponse& response)
{
    if (m_inPreflight) {
        didReceiveResponsePreflight(loader, response);
        return;
    }

    if (!m_sameOriginRequest) {
        if (!accessControlCheck(response)) {
            networkError();
            return;
        }
    }

    m_response = response;
    m_responseEncoding = extractCharsetFromMediaType(m_mimeTypeOverride);
    if (m_responseEncoding.isEmpty())
        m_responseEncoding = response.textEncodingName();
}

template<class HashType>
static bool parseAccessControlAllowList(const String& string, HashSet<String, HashType>* set)
{
    int start = 0;
    int end;
    while ((end = string.find(',', start)) != -1) {
        if (start == end)
            return false;

        // FIXME: this could be made more efficient by not not allocating twice.
        set->add(string.substring(start, end - start).stripWhiteSpace());
        start = end + 1;
    }
    if (start != static_cast<int>(string.length()))
        set->add(string.substring(start).stripWhiteSpace());

    return true;
}

static bool parseAccessControlMaxAge(const String& string, unsigned& expiryDelta)
{
    // FIXME: this will not do the correct thing for a number starting with a '+'
    bool ok = false;
    expiryDelta = string.toUIntStrict(&ok);
    return ok;
}

void XMLHttpRequest::didReceiveResponsePreflight(SubresourceLoader*, const ResourceResponse& response)
{
    ASSERT(m_inPreflight);
    ASSERT(!m_sameOriginRequest);

    if (!accessControlCheck(response)) {
        networkError();
        return;
    }

    OwnPtr<HashSet<String> > methods(new HashSet<String>);
    if (!parseAccessControlAllowList(response.httpHeaderField("Access-Control-Allow-Methods"), methods.get())) {
        networkError();
        return;
    }

    if (!methods->contains(m_method) && m_method != "GET" && m_method != "POST") {
        networkError();
        return;
    }

    OwnPtr<HashSet<String, CaseFoldingHash> > headers(new HashSet<String, CaseFoldingHash>);
    if (!parseAccessControlAllowList(response.httpHeaderField("Access-Control-Allow-Headers"), headers.get())) {
        networkError();
        return;
    }

    HTTPHeaderMap::const_iterator end = m_requestHeaders.end();
    for (HTTPHeaderMap::const_iterator it = m_requestHeaders.begin(); it != end; ++it) {
        if (!headers->contains(it->first) && !isOnAccessControlSimpleRequestHeaderWhitelist(it->first)) {
            networkError();
            return;
        }
    }

    unsigned expiryDelta = 0;
    if (!parseAccessControlMaxAge(response.httpHeaderField("Access-Control-Max-Age"), expiryDelta))
        expiryDelta = 5;

    appendPreflightResultCacheEntry(document()->securityOrigin()->toString(), m_url, expiryDelta, m_includeCredentials, methods.release(), headers.release());
}

void XMLHttpRequest::receivedCancellation(SubresourceLoader*, const AuthenticationChallenge& challenge)
{
    m_response = challenge.failureResponse();
}

void XMLHttpRequest::didReceiveData(SubresourceLoader*, const char* data, int len)
{
    if (m_inPreflight)
        return;

    if (m_state < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);
  
    if (!m_decoder) {
        if (!m_responseEncoding.isEmpty())
            m_decoder = TextResourceDecoder::create("text/plain", m_responseEncoding);
        // allow TextResourceDecoder to look inside the m_response if it's XML or HTML
        else if (responseIsXML())
            m_decoder = TextResourceDecoder::create("application/xml");
        else if (responseMIMEType() == "text/html")
            m_decoder = TextResourceDecoder::create("text/html", "UTF-8");
        else
            m_decoder = TextResourceDecoder::create("text/plain", "UTF-8");
    }
    if (len == 0)
        return;

    if (len == -1)
        len = strlen(data);

    String decoded = m_decoder->decode(data, len);

    {
        JSC::JSLock lock(false);
        m_responseText += decoded;
    }

    if (!m_error) {
        updateAndDispatchOnProgress(len);

        if (m_state != LOADING)
            changeState(LOADING);
        else
            // Firefox calls readyStateChanged every time it receives data, 4449442
            callReadyStateChangeListener();
    }
}

void XMLHttpRequest::updateAndDispatchOnProgress(unsigned int len)
{
    long long expectedLength = m_response.expectedContentLength();
    m_receivedLength += len;

    // FIXME: the spec requires that we dispatch the event according to the least
    // frequent method between every 350ms (+/-200ms) and for every byte received.
    dispatchProgressEvent(expectedLength);
}

void XMLHttpRequest::dispatchReadyStateChangeEvent()
{
    RefPtr<Event> evt = Event::create(eventNames().readystatechangeEvent, false, false);
    if (m_onReadyStateChangeListener) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        m_onReadyStateChangeListener->handleEvent(evt.get(), false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

void XMLHttpRequest::dispatchXMLHttpRequestProgressEvent(EventListener* listener, const AtomicString& type, bool lengthComputable, unsigned loaded, unsigned total)
{
    RefPtr<XMLHttpRequestProgressEvent> evt = XMLHttpRequestProgressEvent::create(type, lengthComputable, loaded, total);
    if (listener) {
        evt->setTarget(this);
        evt->setCurrentTarget(this);
        listener->handleEvent(evt.get(), false);
    }

    ExceptionCode ec = 0;
    dispatchEvent(evt.release(), ec);
    ASSERT(!ec);
}

void XMLHttpRequest::dispatchAbortEvent()
{
    dispatchXMLHttpRequestProgressEvent(m_onAbortListener.get(), eventNames().abortEvent, false, 0, 0);
}

void XMLHttpRequest::dispatchErrorEvent()
{
    dispatchXMLHttpRequestProgressEvent(m_onErrorListener.get(), eventNames().errorEvent, false, 0, 0);
}

void XMLHttpRequest::dispatchLoadEvent()
{
    dispatchXMLHttpRequestProgressEvent(m_onLoadListener.get(), eventNames().loadEvent, false, 0, 0);
}

void XMLHttpRequest::dispatchLoadStartEvent()
{
    dispatchXMLHttpRequestProgressEvent(m_onLoadStartListener.get(), eventNames().loadstartEvent, false, 0, 0);
}

void XMLHttpRequest::dispatchProgressEvent(long long expectedLength)
{
    dispatchXMLHttpRequestProgressEvent(m_onProgressListener.get(), eventNames().progressEvent, expectedLength && m_receivedLength <= expectedLength, 
                                        static_cast<unsigned>(m_receivedLength), static_cast<unsigned>(expectedLength));
}

void XMLHttpRequest::stop()
{
    internalAbort();
}

void XMLHttpRequest::contextDestroyed()
{
    ActiveDOMObject::contextDestroyed();
    internalAbort();
}

ScriptExecutionContext* XMLHttpRequest::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

} // namespace WebCore 
