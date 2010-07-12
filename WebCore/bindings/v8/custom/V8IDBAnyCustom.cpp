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

#if ENABLE(INDEXED_DATABASE)

#include "V8IDBAny.h"

#include "SerializedScriptValue.h"
#include "V8IDBDatabaseRequest.h"
#include "V8IDBIndexRequest.h"
#include "V8IDBKey.h"
#include "V8IDBObjectStoreRequest.h"
#include "V8IndexedDatabaseRequest.h"

namespace WebCore {

v8::Handle<v8::Value> toV8(IDBAny* impl)
{
    if (!impl)
        return v8::Null();

    switch (impl->type()) {
    case IDBAny::UndefinedType:
        return v8::Undefined();
    case IDBAny::NullType:
        return v8::Null();
    case IDBAny::IDBDatabaseRequestType:
        return toV8(impl->idbDatabaseRequest());
    case IDBAny::IDBIndexRequestType:
        return toV8(impl->idbIndexRequest());
    case IDBAny::IDBKeyType:
        return toV8(impl->idbKey());
    case IDBAny::IDBObjectStoreRequestType:
        return toV8(impl->idbObjectStoreRequest());
    case IDBAny::IndexedDatabaseRequestType:
        return toV8(impl->indexedDatabaseRequest());
    case IDBAny::SerializedScriptValueType:
        return impl->serializedScriptValue()->deserialize();
    }

    ASSERT_NOT_REACHED();
    return v8::Undefined();
}

#endif // ENABLE(INDEXED_DATABASE)

} // namespace WebCore
