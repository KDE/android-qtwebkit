/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

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

#ifndef Cache_h
#define Cache_h

#include "CachePolicy.h"
#include "CachedResource.h"
#include "PlatformString.h"
#include "StringHash.h"
#include "loader.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore  {

class CachedCSSStyleSheet;
class CachedImage;
class CachedResource;
class CachedScript;
class CachedXSLStyleSheet;
class DocLoader;
class Image;
class KURL;

// This cache holds subresources used by Web pages: images, scripts, stylesheets, etc.
class Cache : Noncopyable {
public:
    friend Cache* cache();

    typedef HashMap<String, CachedResource*> CachedResourceMap;

    struct LRUList {
        CachedResource* m_head;
        CachedResource* m_tail;
        LRUList() : m_head(0), m_tail(0) { }
    };

    struct TypeStatistic {
        int count;
        int size;
        int liveSize;
        int decodedSize;
        TypeStatistic() : count(0), size(0), liveSize(0), decodedSize(0) { }
    };
    
    struct Statistics {
        TypeStatistic images;
        TypeStatistic cssStyleSheets;
        TypeStatistic scripts;
#if ENABLE(XSLT)
        TypeStatistic xslStyleSheets;
#endif
#if ENABLE(XBL)
        TypeStatistic xblDocs;
#endif
    };

    // The loader that fetches resources.
    Loader* loader() { return &m_loader; }

    // Request resources from the cache.  A load will be initiated and a cache object created if the object is not
    // found in the cache.
    CachedResource* requestResource(DocLoader*, CachedResource::Type, const KURL& url, const String* charset = 0, bool skipCanLoadCheck = false, bool sendResourceLoadCallbacks = true);

    // Set/retreive the size of the cache. This will only hold approximately, since the size some 
    // cached objects (like stylesheets) take up in memory is not exactly known.
    void setMaximumSize(unsigned bytes);
    unsigned maximumSize() const { return m_maximumSize; };

    // Turn the cache on and off.  Disabling the cache will remove all resources from the cache.  They may
    // still live on if they are referenced by some Web page though.
    void setDisabled(bool);
    bool disabled() const { return m_disabled; }
    
    // Remove an existing cache entry from both the resource map and from the LRU list.
    void remove(CachedResource*);

    // Flush the cache.  Any resources still referenced by Web pages will not be removed by this call.
    void pruneAllResources();

    // Flush live resources only.
    void pruneLiveResources();

    void addDocLoader(DocLoader*);
    void removeDocLoader(DocLoader*);

    CachedResource* resourceForURL(const String&);

    // Calls to put the cached resource into and out of LRU lists.
    void insertInLRUList(CachedResource*);
    void removeFromLRUList(CachedResource*);

    // Called to adjust the cache totals when a resource changes size.
    void adjustSize(bool live, int delta, int decodedDelta);

    // Track the size of all resources that are in the cache and still referenced by a Web page. 
    void insertInLiveResourcesList(CachedResource*);
    void removeFromLiveResourcesList(CachedResource*);

    void addToLiveResourcesSize(CachedResource*);
    void removeFromLiveResourcesSize(CachedResource*);

    // Function to collect cache statistics for the caches window in the Safari Debug menu.
    Statistics getStatistics();

private:
    Cache();
    ~Cache(); // Not implemented to make sure nobody accidentally calls delete -- WebCore does not delete singletons.
       
    LRUList* lruListFor(CachedResource*);
    LRUList* liveLRUListFor(CachedResource*);
    
    void resourceAccessed(CachedResource*);

    // Member variables.
    HashSet<DocLoader*> m_docLoaders;
    Loader m_loader;

    bool m_disabled;  // Whether or not the cache is enabled.

    unsigned m_maximumSize;  // The maximum size in bytes that the global cache can consume.
    unsigned m_currentSize;  // The current size of the global cache in bytes.
    unsigned m_liveResourcesSize; // The current size of "live" resources that cannot be flushed.
    
    unsigned m_currentDecodedSize; // The current size of all decoded data in the global cache.
    unsigned m_liveDecodedSize; // The current decoded size of "live" resources only.  We are willing to flush decoded data even off live resources.

    // Size-adjusted and popularity-aware LRU list collection for cache objects.  This collection can hold
    // more resources than the cached resource map, since it can also hold "stale" muiltiple versions of objects that are
    // waiting to die when the clients referencing them go away.
    Vector<LRUList, 32> m_allResources;
    
    // List just for live resources with decoded data.  Access to this list is based off of painting the resource.
    LRUList m_liveResources;
    
    // A URL-based map of all resources that are in the cache (including the freshest version of objects that are currently being 
    // referenced by a Web page).
    HashMap<String, CachedResource*> m_resources;
};

// Function to obtain the global cache.
Cache* cache();

}

#endif
