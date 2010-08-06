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
#include "WebIDBKeyRange.h"

#include "IDBKey.h"
#include "IDBKeyRange.h"
#include "WebIDBKey.h"

using WebCore::IDBKeyRange;

namespace WebKit {

void WebIDBKeyRange::assign(const WebIDBKeyRange& other)
{
    m_private = other.m_private;
}

void WebIDBKeyRange::assign(const WebIDBKey& left, const WebIDBKey& right, unsigned short flags)
{
    m_private = IDBKeyRange::create(left, right, flags);
}

void WebIDBKeyRange::reset()
{
    m_private.reset();
}

WebIDBKey WebIDBKeyRange::left() const
{
    return m_private->left();
}

WebIDBKey WebIDBKeyRange::right() const
{
    return m_private->right();
}

unsigned short WebIDBKeyRange::flags() const
{
    return m_private->flags();
}

WebIDBKeyRange::WebIDBKeyRange(const PassRefPtr<IDBKeyRange>& value)
    : m_private(value)
{
}

WebIDBKeyRange& WebIDBKeyRange::operator=(const PassRefPtr<IDBKeyRange>& value)
{
    m_private = value;
    return *this;
}

WebIDBKeyRange::operator PassRefPtr<IDBKeyRange>() const
{
    return m_private.get();
}

} // namespace WebKit
