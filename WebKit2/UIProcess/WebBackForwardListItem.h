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

#ifndef WebBackForwardListItem_h
#define WebBackForwardListItem_h

#include <WebCore/PlatformString.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class WebPageProxy;

class WebBackForwardListItem : public RefCounted<WebBackForwardListItem> {
public:
    static PassRefPtr<WebBackForwardListItem> create(const WebCore::String& originalURL, const WebCore::String& url, const WebCore::String& title, uint64_t itemID)
    {
        return adoptRef(new WebBackForwardListItem(originalURL, url, title, itemID));
    }
    ~WebBackForwardListItem();

    uint64_t itemID() const { return m_itemID; }

    void setOriginalURL(const WebCore::String& originalURL) { m_originalURL = originalURL; }
    const WebCore::String& originalURL() const { return m_originalURL; }

    void setURL(const WebCore::String& url) { m_url = url; }
    const WebCore::String& url() const { return m_url; }

    void setTitle(const WebCore::String& title) { m_title = title; }
    const WebCore::String& title() const { return m_title; }

private:
    WebBackForwardListItem(const WebCore::String& originalURL, const WebCore::String& url, const WebCore::String& title, uint64_t itemID);

    WebCore::String m_originalURL;
    WebCore::String m_url;
    WebCore::String m_title;
    uint64_t m_itemID;
};

} // namespace WebKit

#endif // WebBackForwardListItem_h
