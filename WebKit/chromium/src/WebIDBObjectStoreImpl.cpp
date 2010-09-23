/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "WebIDBObjectStoreImpl.h"

#include "DOMStringList.h"
#include "IDBCallbacksProxy.h"
#include "IDBKeyRange.h"
#include "IDBObjectStoreBackendInterface.h"
#include "WebIDBIndexImpl.h"
#include "WebIDBKey.h"
#include "WebIDBKeyRange.h"
#include "WebIDBTransaction.h"
#include "WebSerializedScriptValue.h"

#if ENABLE(INDEXED_DATABASE)

using namespace WebCore;

namespace WebKit {

WebIDBObjectStoreImpl::WebIDBObjectStoreImpl(PassRefPtr<IDBObjectStoreBackendInterface> objectStore)
    : m_objectStore(objectStore)
{
}

WebIDBObjectStoreImpl::~WebIDBObjectStoreImpl()
{
}

WebString WebIDBObjectStoreImpl::name() const
{
    return m_objectStore->name();
}

WebString WebIDBObjectStoreImpl::keyPath() const
{
    return m_objectStore->keyPath();
}

WebDOMStringList WebIDBObjectStoreImpl::indexNames() const
{
    return m_objectStore->indexNames();
}

void WebIDBObjectStoreImpl::get(const WebIDBKey& key, WebIDBCallbacks* callbacks, const WebIDBTransaction& transaction)
{
    m_objectStore->get(key, IDBCallbacksProxy::create(callbacks), transaction.getIDBTransactionBackendInterface());
}

void WebIDBObjectStoreImpl::put(const WebSerializedScriptValue& value, const WebIDBKey& key, bool addOnly, WebIDBCallbacks* callbacks)
{
    m_objectStore->put(value, key, addOnly, IDBCallbacksProxy::create(callbacks));
}

void WebIDBObjectStoreImpl::remove(const WebIDBKey& key, WebIDBCallbacks* callbacks)
{
    m_objectStore->remove(key, IDBCallbacksProxy::create(callbacks));
}

void WebIDBObjectStoreImpl::createIndex(const WebString& name, const WebString& keyPath, bool unique, WebIDBCallbacks* callbacks)
{
    m_objectStore->createIndex(name, keyPath, unique, IDBCallbacksProxy::create(callbacks));
}

WebIDBIndex* WebIDBObjectStoreImpl::index(const WebString& name)
{
    RefPtr<IDBIndexBackendInterface> index = m_objectStore->index(name);
    if (!index)
        return 0;
    return new WebIDBIndexImpl(index);
}

void WebIDBObjectStoreImpl::removeIndex(const WebString& name, WebIDBCallbacks* callbacks)
{
    m_objectStore->removeIndex(name, IDBCallbacksProxy::create(callbacks));
}

void WebIDBObjectStoreImpl::openCursor(const WebIDBKeyRange& keyRange, unsigned short direction, WebIDBCallbacks* callbacks)
{
    m_objectStore->openCursor(IDBKeyRange::create(keyRange.left(), keyRange.right(), keyRange.flags()), direction, IDBCallbacksProxy::create(callbacks));
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
