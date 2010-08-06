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

#include "InjectedBundle.h"

#include "Arguments.h"
#include "ImmutableArray.h"
#include "WebContextMessageKinds.h"
#include "InjectedBundleMessageKinds.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/PageGroup.h>
#include <wtf/OwnArrayPtr.h>

using namespace WebCore;

namespace WebKit {

namespace {

// FIXME: We should try to abstract out the shared logic from these and
// and the PostMessageEncoder/PostMessageDecoders in WebContext.cpp into
// a shared baseclass.

// Encodes postMessage messages from the InjectedBundle -> UIProcess

//   - Array -> Array
//   - String -> String
//   - BundlePage -> Page

class PostMessageEncoder {
public:
    PostMessageEncoder(APIObject* root) 
        : m_root(root)
    {
    }

    void encode(CoreIPC::ArgumentEncoder* encoder) const 
    {
        APIObject::Type type = m_root->type();
        encoder->encode(static_cast<uint32_t>(type));
        switch (type) {
        case APIObject::TypeArray: {
            ImmutableArray* array = static_cast<ImmutableArray*>(m_root);
            encoder->encode(static_cast<uint64_t>(array->size()));
            for (size_t i = 0; i < array->size(); ++i)
                encoder->encode(PostMessageEncoder(array->at(i)));
            break;
        }
        case APIObject::TypeString: {
            WebString* string = static_cast<WebString*>(m_root);
            encoder->encode(string->string());
            break;
        }
        case APIObject::TypeBundlePage: {
            WebPage* page = static_cast<WebPage*>(m_root);
            encoder->encode(page->pageID());
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

private:
    APIObject* m_root;
};


// Decodes postMessage messages going from the UIProcess -> InjectedBundle

//   - Array -> Array
//   - String -> String
//   - Page -> BundlePage

class PostMessageDecoder {
public:
    PostMessageDecoder(APIObject** root)
        : m_root(root)
    {
    }

    static bool decode(CoreIPC::ArgumentDecoder* decoder, PostMessageDecoder& coder)
    {
        uint32_t type;
        if (!decoder->decode(type))
            return false;

        switch (type) {
        case APIObject::TypeArray: {
            uint64_t size;
            if (!decoder->decode(size))
                return false;
            
            OwnArrayPtr<APIObject*> array;
            array.set(new APIObject*[size]);
            
            for (size_t i = 0; i < size; ++i) {
                APIObject* element;
                PostMessageDecoder messageCoder(&element);
                if (!decoder->decode(messageCoder))
                    return false;
                array[i] = element;
            }

            *(coder.m_root) = ImmutableArray::create(array.release(), size).leakRef();
            break;
        }
        case APIObject::TypeString: {
            String string;
            if (!decoder->decode(string))
                return false;
            *(coder.m_root) = WebString::create(string).leakRef();
            break;
        }
        case APIObject::TypePage: {
            uint64_t pageID;
            if (!decoder->decode(pageID))
                return false;
            *(coder.m_root) = WebProcess::shared().webPage(pageID);
            break;
        }
        default:
            return false;
        }

        return true;
    }

private:
    APIObject** m_root;
};

}

InjectedBundle::InjectedBundle(const String& path)
    : m_path(path)
    , m_platformBundle(0)
{
    initializeClient(0);
}

InjectedBundle::~InjectedBundle()
{
}

void InjectedBundle::initializeClient(WKBundleClient* client)
{
    if (client && !client->version)
        m_client = *client;
    else
        memset(&m_client, 0, sizeof(m_client));
}

void InjectedBundle::postMessage(const String& messageName, APIObject* messageBody)
{
    WebProcess::shared().connection()->send(WebContextMessage::PostMessage, 0, CoreIPC::In(messageName, PostMessageEncoder(messageBody)));
}

void InjectedBundle::setShouldTrackVisitedLinks(bool shouldTrackVisitedLinks)
{
    PageGroup::setShouldTrackVisitedLinks(shouldTrackVisitedLinks);
}

void InjectedBundle::removeAllVisitedLinks()
{
    PageGroup::removeAllVisitedLinks();
}

void InjectedBundle::didCreatePage(WebPage* page)
{
    if (m_client.didCreatePage)
        m_client.didCreatePage(toRef(this), toRef(page), m_client.clientInfo);
}

void InjectedBundle::willDestroyPage(WebPage* page)
{
    if (m_client.willDestroyPage)
        m_client.willDestroyPage(toRef(this), toRef(page), m_client.clientInfo);
}

void InjectedBundle::didReceiveMessage(const String& messageName, APIObject* messageBody)
{
    if (m_client.didReceiveMessage)
        m_client.didReceiveMessage(toRef(this), toRef(messageName.impl()), toRef(messageBody), m_client.clientInfo);
}

void InjectedBundle::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    switch (messageID.get<InjectedBundleMessage::Kind>()) {
        case InjectedBundleMessage::PostMessage: {
            String messageName;
            // FIXME: This should be a RefPtr<APIObject>
            APIObject* messageBody = 0;
            PostMessageDecoder messageCoder(&messageBody);
            if (!arguments->decode(CoreIPC::Out(messageName, messageCoder)))
                return;

            didReceiveMessage(messageName, messageBody);

            messageBody->deref();

            return;
        }
    }

    ASSERT_NOT_REACHED();
}

} // namespace WebKit
