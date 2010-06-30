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

#ifndef ImmutableArray_h
#define ImmutableArray_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {

// ImmutableArray - An immutable array type suitable for vending to an API.

class ImmutableArray : public RefCounted<ImmutableArray> {
public:
    struct ImmutableArrayCallbacks {
        typedef void (*ImmutableArrayCallback)(const void*);
        ImmutableArrayCallback ref;
        ImmutableArrayCallback deref;
    };

    static PassRefPtr<ImmutableArray> create()
    {
        return adoptRef(new ImmutableArray);
    }

    static PassRefPtr<ImmutableArray> create(const void** entries, size_t size, const ImmutableArrayCallbacks* callbacks)
    {
        return adoptRef(new ImmutableArray(entries, size, callbacks));
    }
    static PassRefPtr<ImmutableArray> adopt(void** entries, size_t size, const ImmutableArrayCallbacks* callbacks)
    {
        return adoptRef(new ImmutableArray(entries, size, callbacks, Adopt));
    }
    ~ImmutableArray();

    const void* at(size_t i) { ASSERT(i < m_size); return m_entries[i]; }
    size_t size() { return m_size; }

private:
    ImmutableArray();
    ImmutableArray(const void** entries, size_t size, const ImmutableArrayCallbacks*);
    enum AdoptTag { Adopt };
    ImmutableArray(void** entries, size_t size, const ImmutableArrayCallbacks*, AdoptTag);

    void** m_entries;
    size_t m_size;
    ImmutableArrayCallbacks m_callbacks;
};

} // namespace WebKit

#endif // ImmutableArray_h
