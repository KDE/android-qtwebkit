/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "InspectorResource.h"

#if ENABLE(INSPECTOR)

#include "Base64.h"
#include "Cache.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "InspectorFrontend.h"
#include "InspectorResourceAgent.h"
#include "InspectorValues.h"
#include "ResourceLoadTiming.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "TextEncoding.h"
#include "WebSocketHandshakeRequest.h"
#include "WebSocketHandshakeResponse.h"

#include <wtf/Assertions.h>
#include <wtf/text/StringBuffer.h>

namespace WebCore {

#if ENABLE(WEB_SOCKETS)
// Create human-readable binary representation, like "01:23:45:67:89:AB:CD:EF".
static String createReadableStringFromBinary(const unsigned char* value, size_t length)
{
    ASSERT(length > 0);
    static const char hexDigits[17] = "0123456789ABCDEF";
    size_t bufferSize = length * 3 - 1;
    StringBuffer buffer(bufferSize);
    size_t index = 0;
    for (size_t i = 0; i < length; ++i) {
        if (i > 0)
            buffer[index++] = ':';
        buffer[index++] = hexDigits[value[i] >> 4];
        buffer[index++] = hexDigits[value[i] & 0xF];
    }
    ASSERT(index == bufferSize);
    return String::adopt(buffer);
}
#endif

InspectorResource::InspectorResource(unsigned long identifier, DocumentLoader* loader, const KURL& requestURL)
    : m_identifier(identifier)
    , m_loader(loader)
    , m_frame(loader ? loader->frame() : 0)
    , m_requestURL(requestURL)
    , m_expectedContentLength(0)
    , m_cached(false)
    , m_finished(false)
    , m_failed(false)
    , m_length(0)
    , m_responseStatusCode(0)
    , m_startTime(-1.0)
    , m_responseReceivedTime(-1.0)
    , m_endTime(-1.0)
    , m_connectionID(0)
    , m_connectionReused(false)
    , m_isMainResource(false)
#if ENABLE(WEB_SOCKETS)
    , m_isWebSocket(false)
#endif
{
}

InspectorResource::~InspectorResource()
{
}

PassRefPtr<InspectorResource> InspectorResource::appendRedirect(unsigned long identifier, const KURL& redirectURL)
{
    // Last redirect is always a container of all previous ones. Pass this container here.
    RefPtr<InspectorResource> redirect = InspectorResource::create(m_identifier, m_loader.get(), redirectURL);
    redirect->m_redirects = m_redirects;
    redirect->m_redirects.append(this);
    redirect->m_changes.set(RedirectsChange);

    m_identifier = identifier;
    // Re-send request info with new id.
    m_changes.set(RequestChange);
    m_redirects.clear();
    return redirect;
}

PassRefPtr<InspectorResource> InspectorResource::create(unsigned long identifier, DocumentLoader* loader, const KURL& requestURL)
{
    ASSERT(loader);
    return adoptRef(new InspectorResource(identifier, loader, requestURL));
}

PassRefPtr<InspectorResource> InspectorResource::createCached(unsigned long identifier, DocumentLoader* loader, const CachedResource* cachedResource)
{
    PassRefPtr<InspectorResource> resource = create(identifier, loader, KURL(ParsedURLString, cachedResource->url()));

    resource->m_finished = true;

    resource->updateResponse(cachedResource->response());

    resource->m_length = cachedResource->encodedSize();
    resource->m_cached = true;
    resource->m_startTime = currentTime();
    resource->m_responseReceivedTime = resource->m_startTime;
    resource->m_endTime = resource->m_startTime;

    resource->m_changes.setAll();

    return resource;
}

#if ENABLE(WEB_SOCKETS)
PassRefPtr<InspectorResource> InspectorResource::createWebSocket(unsigned long identifier, const KURL& requestURL, const KURL& documentURL)
{
    RefPtr<InspectorResource> resource = adoptRef(new InspectorResource(identifier, 0, requestURL));
    resource->markWebSocket();
    resource->m_documentURL = documentURL;
    resource->m_changes.setAll();
    return resource.release();
}
#endif

void InspectorResource::updateRequest(const ResourceRequest& request)
{
    m_requestHeaderFields = request.httpHeaderFields();
    m_requestMethod = request.httpMethod();
    if (request.httpBody() && !request.httpBody()->isEmpty())
        m_requestFormData = request.httpBody()->flattenToString();

    m_changes.set(RequestChange);
}

void InspectorResource::markAsCached()
{
    m_cached = true;
}

void InspectorResource::updateResponse(const ResourceResponse& response)
{
    m_expectedContentLength = response.expectedContentLength();
    m_mimeType = response.mimeType();
    if (m_mimeType.isEmpty() && response.httpStatusCode() == 304) {
        CachedResource* cachedResource = cache()->resourceForURL(response.url().string());
        if (cachedResource)
            m_mimeType = cachedResource->response().mimeType();
    }
    if (ResourceRawHeaders* headers = response.resourceRawHeaders().get()) {
        m_requestHeaderFields = headers->requestHeaders;
        m_responseHeaderFields = headers->responseHeaders;
        m_changes.set(RequestChange);
    } else
        m_responseHeaderFields = response.httpHeaderFields();
    m_responseStatusCode = response.httpStatusCode();
    m_responseStatusText = response.httpStatusText();
    m_suggestedFilename = response.suggestedFilename();

    m_connectionID = response.connectionID();
    m_connectionReused = response.connectionReused();
    m_loadTiming = response.resourceLoadTiming();
    m_cached = m_cached || response.wasCached();
    m_responseReceivedTime = currentTime();
    m_changes.set(TimingChange);
    m_changes.set(ResponseChange);
    m_changes.set(TypeChange);
}

#if ENABLE(WEB_SOCKETS)
void InspectorResource::updateWebSocketRequest(const WebSocketHandshakeRequest& request)
{
    m_requestHeaderFields = request.headerFields();
    m_requestMethod = "GET"; // Currently we always use "GET" to request handshake.
    m_webSocketRequestKey3.set(new WebSocketHandshakeRequest::Key3(request.key3()));
    m_changes.set(RequestChange);
    m_changes.set(TypeChange);
}

void InspectorResource::updateWebSocketResponse(const WebSocketHandshakeResponse& response)
{
    m_responseStatusCode = response.statusCode();
    m_responseStatusText = response.statusText();
    m_responseHeaderFields = response.headerFields();
    m_webSocketChallengeResponse.set(new WebSocketHandshakeResponse::ChallengeResponse(response.challengeResponse()));
    m_changes.set(ResponseChange);
    m_changes.set(TypeChange);
}
#endif // ENABLE(WEB_SOCKETS)

static PassRefPtr<InspectorObject> buildHeadersObject(const HTTPHeaderMap& headers)
{
    RefPtr<InspectorObject> object = InspectorObject::create();
    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
        object->setString(it->first.string(), it->second);
    }
    return object;
}

static PassRefPtr<InspectorObject> buildObjectForTiming(ResourceLoadTiming* timing)
{
    RefPtr<InspectorObject> jsonObject = InspectorObject::create();
    jsonObject->setNumber("requestTime", timing->requestTime);
    jsonObject->setNumber("proxyStart", timing->proxyStart);
    jsonObject->setNumber("proxyEnd", timing->proxyEnd);
    jsonObject->setNumber("dnsStart", timing->dnsStart);
    jsonObject->setNumber("dnsEnd", timing->dnsEnd);
    jsonObject->setNumber("connectStart", timing->connectStart);
    jsonObject->setNumber("connectEnd", timing->connectEnd);
    jsonObject->setNumber("sslStart", timing->sslStart);
    jsonObject->setNumber("sslEnd", timing->sslEnd);
    jsonObject->setNumber("sendStart", timing->sendStart);
    jsonObject->setNumber("sendEnd", timing->sendEnd);
    jsonObject->setNumber("receiveHeadersEnd", timing->receiveHeadersEnd);
    return jsonObject;
}


void InspectorResource::updateScriptObject(InspectorFrontend* frontend)
{
    if (m_changes.hasChange(NoChange))
        return;

    RefPtr<InspectorObject> jsonObject = InspectorObject::create();
    jsonObject->setNumber("id", m_identifier);
    if (m_changes.hasChange(RequestChange)) {
        if (m_frame)
            m_documentURL = m_frame->document()->url();
        jsonObject->setString("url", m_requestURL.string());
        jsonObject->setString("documentURL", m_documentURL.string());
        RefPtr<InspectorObject> requestHeaders = buildHeadersObject(m_requestHeaderFields);
        jsonObject->setObject("requestHeaders", requestHeaders);
        jsonObject->setBoolean("mainResource", m_isMainResource);
        jsonObject->setString("requestMethod", m_requestMethod);
        jsonObject->setString("requestFormData", m_requestFormData);
        jsonObject->setBoolean("didRequestChange", true);
#if ENABLE(WEB_SOCKETS)
        if (m_webSocketRequestKey3)
            jsonObject->setString("webSocketRequestKey3", createReadableStringFromBinary(m_webSocketRequestKey3->value, sizeof(m_webSocketRequestKey3->value)));
#endif
    }

    if (m_changes.hasChange(ResponseChange)) {
        jsonObject->setString("mimeType", m_mimeType);
        jsonObject->setString("suggestedFilename", m_suggestedFilename);
        jsonObject->setNumber("expectedContentLength", m_expectedContentLength);
        jsonObject->setNumber("statusCode", m_responseStatusCode);
        jsonObject->setString("statusText", m_responseStatusText);
        RefPtr<InspectorObject> responseHeaders = buildHeadersObject(m_responseHeaderFields);
        jsonObject->setObject("responseHeaders", responseHeaders);
        jsonObject->setNumber("connectionID", m_connectionID);
        jsonObject->setBoolean("connectionReused", m_connectionReused);
        jsonObject->setBoolean("cached", m_cached);
        if (m_loadTiming && !m_cached)
            jsonObject->setObject("timing", buildObjectForTiming(m_loadTiming.get()));
#if ENABLE(WEB_SOCKETS)
        if (m_webSocketChallengeResponse)
            jsonObject->setString("webSocketChallengeResponse", createReadableStringFromBinary(m_webSocketChallengeResponse->value, sizeof(m_webSocketChallengeResponse->value)));
#endif
        jsonObject->setBoolean("didResponseChange", true);
    }

    if (m_changes.hasChange(TypeChange)) {
        jsonObject->setNumber("type", static_cast<int>(type()));
        jsonObject->setBoolean("didTypeChange", true);
    }

    if (m_changes.hasChange(LengthChange)) {
        jsonObject->setNumber("resourceSize", m_length);
        jsonObject->setBoolean("didLengthChange", true);
    }

    if (m_changes.hasChange(CompletionChange)) {
        jsonObject->setBoolean("failed", m_failed);
        jsonObject->setString("localizedFailDescription", m_localizedFailDescription);
        jsonObject->setBoolean("finished", m_finished);
        jsonObject->setBoolean("didCompletionChange", true);
    }

    if (m_changes.hasChange(TimingChange)) {
        if (m_startTime > 0)
            jsonObject->setNumber("startTime", m_startTime);
        if (m_responseReceivedTime > 0)
            jsonObject->setNumber("responseReceivedTime", m_responseReceivedTime);
        if (m_endTime > 0)
            jsonObject->setNumber("endTime", m_endTime);
        jsonObject->setBoolean("didTimingChange", true);
    }

    if (m_changes.hasChange(RedirectsChange)) {
        for (size_t i = 0; i < m_redirects.size(); ++i)
            m_redirects[i]->updateScriptObject(frontend);
    }

    frontend->updateResource(jsonObject);
    m_changes.clearAll();
}

void InspectorResource::releaseScriptObject(InspectorFrontend* frontend)
{
    m_changes.setAll();

    for (size_t i = 0; i < m_redirects.size(); ++i)
        m_redirects[i]->releaseScriptObject(frontend);

    if (frontend)
        frontend->removeResource(m_identifier);
}

static InspectorResource::Type cachedResourceType(CachedResource* cachedResource)
{
    if (!cachedResource)
        return InspectorResource::Other;

    switch (cachedResource->type()) {
    case CachedResource::ImageResource:
        return InspectorResource::Image;
    case CachedResource::FontResource:
        return InspectorResource::Font;
    case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
        return InspectorResource::Stylesheet;
    case CachedResource::Script:
        return InspectorResource::Script;
    default:
        return InspectorResource::Other;
    }
}

InspectorResource::Type InspectorResource::type() const
{
    if (!m_overrideContent.isNull())
        return m_overrideContentType;

#if ENABLE(WEB_SOCKETS)
    if (m_isWebSocket)
        return WebSocket;
#endif

    ASSERT(m_loader);

    if (m_loader->frameLoader() && equalIgnoringFragmentIdentifier(m_requestURL, m_loader->frameLoader()->iconURL()))
        return Image;

    if (!m_frame)
        return Other;

    InspectorResource::Type resourceType = cachedResourceType(InspectorResourceAgent::cachedResource(m_frame.get(), m_requestURL));
    if (equalIgnoringFragmentIdentifier(m_requestURL, m_loader->requestURL()) && resourceType == Other)
        return Doc;

    return resourceType;
}

void InspectorResource::setOverrideContent(const String& data, Type type)
{
    m_overrideContent = data;
    m_overrideContentType = type;
    m_changes.set(TypeChange);
}

String InspectorResource::sourceString() const
{
    if (!m_overrideContent.isNull())
        return String(m_overrideContent);

    String result;
    if (!InspectorResourceAgent::resourceContent(m_frame.get(), m_requestURL, &result))
        return String();
    return result;
}

String InspectorResource::sourceBytes() const
{
    Vector<char> out;
    if (!m_overrideContent.isNull()) {
        Vector<char> data;
        String overrideContent = m_overrideContent;
        data.append(overrideContent.characters(), overrideContent.length());
        base64Encode(data, out);
        return String(out.data(), out.size());
    }

    String result;
    if (!InspectorResourceAgent::resourceContentBase64(m_frame.get(), m_requestURL, &result))
        return String();
    return result;
}

void InspectorResource::startTiming()
{
    m_startTime = currentTime();
    m_changes.set(TimingChange);
}

void InspectorResource::endTiming(double actualEndTime)
{
    if (actualEndTime)
        m_endTime = actualEndTime;
    else
        m_endTime = currentTime();

    m_finished = true;
    m_changes.set(TimingChange);
    m_changes.set(CompletionChange);
}

void InspectorResource::markFailed(const String& localizedDescription)
{
    m_failed = true;
    m_localizedFailDescription = localizedDescription;
    m_changes.set(CompletionChange);
}

void InspectorResource::addLength(int lengthReceived)
{
    m_length += lengthReceived;
    m_changes.set(LengthChange);

    // Update load time, otherwise the resource will
    // have start time == end time and  0 load duration
    // until its loading is completed.
    m_endTime = currentTime();
    m_changes.set(TimingChange);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
