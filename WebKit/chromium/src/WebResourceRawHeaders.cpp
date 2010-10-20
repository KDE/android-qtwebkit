/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebResourceRawHeaders.h"

#include "ResourceRawHeaders.h"
#include "ResourceResponse.h"
#include "WebHTTPHeaderVisitor.h"
#include "WebString.h"

using namespace WebCore;

namespace WebKit {

void WebResourceRawHeaders::initialize()
{
    m_private = adoptRef(new ResourceRawHeaders());
}

void WebResourceRawHeaders::reset()
{
    m_private.reset();
}

void WebResourceRawHeaders::assign(const WebResourceRawHeaders& r)
{
    m_private = r.m_private;
}

WebResourceRawHeaders::WebResourceRawHeaders(WTF::PassRefPtr<WebCore::ResourceRawHeaders> value)
{
    m_private = value;
}

WebResourceRawHeaders::operator WTF::PassRefPtr<WebCore::ResourceRawHeaders>() const
{
    return m_private.get();
}

static void addHeader(HTTPHeaderMap* map, const WebString& name, const WebString& value)
{
    pair<HTTPHeaderMap::iterator, bool> result = map->add(name, value);
    if (!result.second)
        result.first->second += String("\n") + value;
}

void WebResourceRawHeaders::addRequestHeader(const WebString& name, const WebString& value)
{
    ASSERT(!m_private.isNull());
    addHeader(&m_private->requestHeaders, name, value);
}

void WebResourceRawHeaders::addResponseHeader(const WebString& name, const WebString& value)
{
    ASSERT(!m_private.isNull());
    addHeader(&m_private->responseHeaders, name, value);
}

} // namespace WebKit
