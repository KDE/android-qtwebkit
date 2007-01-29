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

CachedResource::CachedResource(const String& URL, Type type, CachePolicy cachePolicy, unsigned size)
{
    m_url = URL;
    m_type = type;
    m_status = Pending;
    m_size = size;
    m_inCache = false;
    m_cachePolicy = cachePolicy;
    m_request = 0;
    m_allData = 0;
    m_expireDateChanged = false;
    m_accessCount = 0;
    m_nextInLRUList = 0;
    m_prevInLRUList = 0;
#ifndef NDEBUG
    m_deleted = false;
    m_lruIndex = 0;
#endif
}

CachedResource::~CachedResource()
{
    ASSERT(!inCache());
    ASSERT(!m_deleted);
#ifndef NDEBUG
    m_deleted = true;
#endif
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
    m_status = Cached;
    KURL url(m_url.deprecatedString());
    if (m_expireDateChanged && url.protocol().startsWith("http"))
        m_expireDateChanged = false;
}

bool CachedResource::isExpired() const
{
    if (!m_response.expirationDate())
        return false;
    time_t now = time(0);
    return (difftime(now, m_response.expirationDate()) >= 0);
}

void CachedResource::setRequest(Request* request)
{
    if (request && !m_request)
        m_status = Pending;
    m_request = request;
    if (canDelete() && !inCache())
        delete this;
}

void CachedResource::ref(CachedResourceClient *c)
{
    if (!referenced() && inCache())
        cache()->addToLiveObjectSize(size());
    m_clients.add(c);
}

void CachedResource::deref(CachedResourceClient *c)
{
    m_clients.remove(c);
    if (canDelete() && !inCache())
        delete this;
    else if (!referenced() && inCache()) {
        cache()->removeFromLiveObjectSize(size());
        cache()->prune();
    }
}

void CachedResource::setSize(unsigned size)
{
    if (size == m_size)
        return;

    unsigned oldSize = m_size;

    // The object must now be moved to a different queue, since its size has been changed.
    // We have to remove explicitly before updating m_size, so that we find the correct previous
    // queue.
    if (inCache())
        cache()->removeFromLRUList(this);
    
    m_size = size;
   
    if (inCache()) { 
        // Now insert into the new LRU list.
        cache()->insertInLRUList(this);
        
        // Update the cache's size totals.
        cache()->adjustSize(referenced(), oldSize, size);
    }
}

}
