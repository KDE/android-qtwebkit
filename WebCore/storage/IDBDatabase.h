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

#ifndef IDBDatabase_h
#define IDBDatabase_h

#include "PlatformString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class DOMStringList;
class Frame;
class IDBCallbacks;
class IDBObjectStore;

// This class is shared by IDBDatabaseRequest (async) and IDBDatabaseSync (sync).
// This is implemented by IDBDatabaseImpl and optionally others (in order to proxy
// calls across process barriers). All calls to these classes should be non-blocking and
// trigger work on a background thread if necessary.
class IDBDatabase : public ThreadSafeShared<IDBDatabase> {
public:
    virtual ~IDBDatabase() { }

    virtual String name() const = 0;
    virtual String description() const = 0;
    virtual String version() const = 0;
    virtual PassRefPtr<DOMStringList> objectStores() const = 0;

    // FIXME: Add transaction and setVersion.

    virtual void createObjectStore(const String& name, const String& keyPath, bool autoIncrement, PassRefPtr<IDBCallbacks>) = 0;
    virtual PassRefPtr<IDBObjectStore> objectStore(const String& name, unsigned short mode) = 0;
    virtual void removeObjectStore(const String& name, PassRefPtr<IDBCallbacks>) = 0;
};

} // namespace WebCore

#endif

#endif // IDBDatabase_h
