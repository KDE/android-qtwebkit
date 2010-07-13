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
#include "IndexedDatabaseRequest.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "IDBDatabase.h"
#include "IDBKeyRange.h"
#include "IDBRequest.h"
#include "IndexedDatabase.h"

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

IndexedDatabaseRequest::IndexedDatabaseRequest(IndexedDatabase* indexedDatabase)
    : m_indexedDatabase(indexedDatabase)
{
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
}

IndexedDatabaseRequest::~IndexedDatabaseRequest()
{
}

PassRefPtr<IDBRequest> IndexedDatabaseRequest::open(ScriptExecutionContext* context, const String& name, const String& description)
{
    if (!context->isDocument()) {
        // FIXME: make this work with workers.
        return 0;
    }

    Document* document = static_cast<Document*>(context);
    if (!document->frame())
        return 0;

    RefPtr<IDBRequest> request = IDBRequest::create(document, IDBAny::create(this));
    m_indexedDatabase->open(name, description, request, document->securityOrigin(), document->frame());
    return request;
}

PassRefPtr<IDBKeyRange> IndexedDatabaseRequest::makeSingleKeyRange(PassRefPtr<SerializedScriptValue> prpValue)
{
    RefPtr<SerializedScriptValue> value = prpValue;
    return IDBKeyRange::create(value, value, IDBKeyRange::SINGLE);
}

PassRefPtr<IDBKeyRange> IndexedDatabaseRequest::makeLeftBoundKeyRange(PassRefPtr<SerializedScriptValue> bound, bool open)
{
    return IDBKeyRange::create(bound, SerializedScriptValue::create(), open ? IDBKeyRange::LEFT_OPEN : IDBKeyRange::LEFT_BOUND);
}

PassRefPtr<IDBKeyRange> IndexedDatabaseRequest::makeRightBoundKeyRange(PassRefPtr<SerializedScriptValue> bound, bool open)
{
    return IDBKeyRange::create(SerializedScriptValue::create(), bound, open ? IDBKeyRange::RIGHT_OPEN : IDBKeyRange::RIGHT_BOUND);
}

PassRefPtr<IDBKeyRange> IndexedDatabaseRequest::makeBoundKeyRange(PassRefPtr<SerializedScriptValue> left, PassRefPtr<SerializedScriptValue> right, bool openLeft, bool openRight)
{
    unsigned short flags = openLeft ? IDBKeyRange::LEFT_OPEN : IDBKeyRange::LEFT_BOUND;
    flags |= openRight ? IDBKeyRange::RIGHT_OPEN : IDBKeyRange::RIGHT_BOUND;
    return IDBKeyRange::create(left, right, flags);
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
