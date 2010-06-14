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

#ifndef IDBDatabaseImpl_h
#define IDBDatabaseImpl_h

#include "IDBCallbacks.h"
#include "IDBDatabase.h"
#include "StringHash.h"
#include <wtf/HashMap.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBDatabaseImpl : public IDBDatabase {
public:
    static PassRefPtr<IDBDatabase> create(const String& name, const String& description, const String& version)
    {
        return adoptRef(new IDBDatabaseImpl(name, description, version));
    }
    virtual ~IDBDatabaseImpl();

    // Implements IDBDatabase
    virtual String name() const { return m_name; }
    virtual String description() const { return m_description; }
    virtual String version() const { return m_version; }
    virtual PassRefPtr<DOMStringList> objectStores() const;

    virtual void createObjectStore(const String& name, const String& keyPath, bool autoIncrement, PassRefPtr<IDBCallbacks>);
    virtual PassRefPtr<IDBObjectStore> objectStore(const String& name, unsigned short mode);
    virtual void removeObjectStore(const String& name, PassRefPtr<IDBCallbacks>);

private:
    IDBDatabaseImpl(const String& name, const String& description, const String& version);

    String m_name;
    String m_description;
    String m_version;

    typedef HashMap<String, RefPtr<IDBObjectStore> > ObjectStoreMap;
    ObjectStoreMap m_objectStores;
};

} // namespace WebCore

#endif

#endif // IDBDatabaseImpl_h
