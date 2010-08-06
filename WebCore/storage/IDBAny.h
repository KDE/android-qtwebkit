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

#ifndef IDBAny_h
#define IDBAny_h

#if ENABLE(INDEXED_DATABASE)

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class IDBCursor;
class IDBDatabase;
class IDBIndex;
class IDBKey;
class IDBObjectStore;
class IDBFactory;
class SerializedScriptValue;

class IDBAny : public RefCounted<IDBAny> {
public:
    static PassRefPtr<IDBAny> create();
    template<typename T>
    static PassRefPtr<IDBAny> create(T* idbObject)
    {
        RefPtr<IDBAny> any = IDBAny::create();
        any->set(idbObject);
        return any.release();
    }
    ~IDBAny();

    enum Type {
        UndefinedType = 0,
        NullType,
        IDBCursorType,
        IDBDatabaseType,
        IDBFactoryType,
        IDBIndexType,
        IDBKeyType,
        IDBObjectStoreType,
        SerializedScriptValueType
    };

    Type type() const { return m_type; }
    // Use type() to figure out which one of these you're allowed to call.
    PassRefPtr<IDBCursor> idbCursor();
    PassRefPtr<IDBDatabase> idbDatabase();
    PassRefPtr<IDBFactory> idbFactory();
    PassRefPtr<IDBIndex> idbIndex();
    PassRefPtr<IDBKey> idbKey();
    PassRefPtr<IDBObjectStore> idbObjectStore();
    PassRefPtr<SerializedScriptValue> serializedScriptValue();

    // Set can only be called once.
    void set(); // For "null".
    void set(PassRefPtr<IDBCursor>);
    void set(PassRefPtr<IDBDatabase>);
    void set(PassRefPtr<IDBFactory>);
    void set(PassRefPtr<IDBIndex>);
    void set(PassRefPtr<IDBKey>);
    void set(PassRefPtr<IDBObjectStore>);
    void set(PassRefPtr<SerializedScriptValue>);

private:
    IDBAny();

    Type m_type;

    // Only one of the following should ever be in use at any given time.
    RefPtr<IDBCursor> m_idbCursor;
    RefPtr<IDBDatabase> m_idbDatabase;
    RefPtr<IDBFactory> m_idbFactory;
    RefPtr<IDBIndex> m_idbIndex;
    RefPtr<IDBKey> m_idbKey;
    RefPtr<IDBObjectStore> m_idbObjectStore;
    RefPtr<SerializedScriptValue> m_serializedScriptValue;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBAny_h
