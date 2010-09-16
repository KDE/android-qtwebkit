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
#include "IDBIndex.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursorBackendInterface.h"
#include "IDBIndexBackendInterface.h"
#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "IDBRequest.h"

namespace WebCore {

IDBIndex::IDBIndex(PassRefPtr<IDBIndexBackendInterface> backend)
    : m_backend(backend)
{
}

IDBIndex::~IDBIndex()
{
}

PassRefPtr<IDBRequest> IDBIndex::openObjectCursor(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, unsigned short direction)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_backend->openObjectCursor(keyRange, direction, request);
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::openCursor(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, unsigned short direction)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_backend->openCursor(keyRange, direction, request);
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::getObject(ScriptExecutionContext* context, PassRefPtr<IDBKey> key)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_backend->getObject(key, request);
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::get(ScriptExecutionContext* context, PassRefPtr<IDBKey> key)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this));
    m_backend->get(key, request);
    return request;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
