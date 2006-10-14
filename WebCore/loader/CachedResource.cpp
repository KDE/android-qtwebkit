/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "CachedResource.h"

#include "Cache.h"
#include "Request.h"
#include <KURL.h>
#include <wtf/Vector.h>

namespace WebCore {

CachedResource::~CachedResource()
{
    if (m_deleted)
        abort();
    Cache::removeFromLRUList(this);
    m_deleted = true;
    setResponse(0);
    setAllData(0);
}

Vector<char>& CachedResource::bufferData(const char* bytes, int addedSize, Request* request)
{
    // Add new bytes to the buffer in the Request object.
    Vector<char>& buffer = request->buffer();

    unsigned oldSize = buffer.size();
    buffer.resize(oldSize + addedSize);
    memcpy(buffer.data() + oldSize, bytes, addedSize);
    
    return buffer;
}

void CachedResource::finish()
{
    if (m_size > Cache::maxCacheableObjectSize())
        m_status = Uncacheable;
    else
        m_status = Cached;
    KURL url(m_url.deprecatedString());
    if (m_expireDateChanged && url.protocol().startsWith("http"))
        m_expireDateChanged = false;
}

void CachedResource::setExpireDate(time_t expireDate, bool changeHttpCache)
{
    if (expireDate == m_expireDate)
        return;

    if (m_status == Uncacheable || m_status == Cached)
        finish();

    m_expireDate = expireDate;
    if (changeHttpCache && m_expireDate)
       m_expireDateChanged = true;
}

bool CachedResource::isExpired() const
{
    if (!m_expireDate) return false;
    time_t now = time(0);
    return (difftime(now, m_expireDate) >= 0);
}

void CachedResource::setRequest(Request *_request)
{
    if ( _request && !m_request )
        m_status = Pending;
    m_request = _request;
    if (canDelete() && m_free)
        delete this;
    else if (allowInLRUList())
        Cache::insertInLRUList(this);
}

void CachedResource::ref(CachedResourceClient *c)
{
    m_clients.add(c);
    Cache::removeFromLRUList(this);
    increaseAccessCount();
}

void CachedResource::deref(CachedResourceClient *c)
{
    m_clients.remove(c);
    if (allowInLRUList())
        Cache::insertInLRUList(this);
}

void CachedResource::setSize(int size)
{
    bool sizeChanged = Cache::adjustSize(this, size - m_size);

    // The object must now be moved to a different queue, since its size has been changed.
    if (sizeChanged && allowInLRUList())
        Cache::removeFromLRUList(this);

    m_size = size;
    
    if (sizeChanged && allowInLRUList())
        Cache::insertInLRUList(this);
}

}
