/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
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
#include "Cache.h"

#include "CachedCSSStyleSheet.h"
#include "CachedImage.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "DocLoader.h"
#include "Document.h"
#include "Image.h"
#include "TransferJob.h"
#include "loader.h"

using namespace std;

namespace WebCore {

const int defaultCacheSize = 8192 * 1024;

// maxCacheableObjectSize is cache size divided by 128, but with this as a minimum
const int minMaxCacheableObjectSize = 80 * 1024;

const int maxLRULists = 20;
    
struct LRUList {
    CachedResource* m_head;
    CachedResource* m_tail;
    LRUList() : m_head(0), m_tail(0) { }
};

static bool cacheDisabled;

typedef HashMap<RefPtr<StringImpl>, CachedResource*> CacheMap;

static CacheMap* cache = 0;

HashSet<DocLoader*>* Cache::docloaders = 0;
Loader *Cache::m_loader = 0;

int Cache::maxSize = defaultCacheSize;
int Cache::maxCacheable = minMaxCacheableObjectSize;
int Cache::flushCount = 0;

Image *Cache::nullImage = 0;
Image *Cache::brokenImage = 0;

CachedResource *Cache::m_headOfUncacheableList = 0;
int Cache::m_totalSizeOfLRULists = 0;
int Cache::m_countOfLRUAndUncacheableLists;
LRUList *Cache::m_LRULists = 0;

void Cache::init()
{
    if (!cache)
        cache = new CacheMap;

    if (!docloaders)
        docloaders = new HashSet<DocLoader*>;

    if (!nullImage)
        nullImage = new Image;

    if (!brokenImage)
        brokenImage = Image::loadResource("missingImage");

    if (!m_loader)
        m_loader = new Loader();
}

void Cache::clear()
{
    if (!cache)
        return;

    deleteAllValues(*cache);

    delete cache; cache = 0;
    delete nullImage; nullImage = 0;
    delete brokenImage; brokenImage = 0;
    delete m_loader; m_loader = 0;
    ASSERT(docloaders->isEmpty());
    delete docloaders; docloaders = 0;
}

void Cache::updateCacheStatus(DocLoader* dl, CachedResource* o)
{
    moveToHeadOfLRUList(o);
    if (dl) {
        ASSERT(!o->url().isNull());
        if (cacheDisabled)
            dl->m_docObjects.remove(o->url());
        else
            dl->m_docObjects.set(o->url(), o);
    }
}

CachedImage* Cache::requestImage(DocLoader* dl, const String& url, bool reload, time_t expireDate)
{
    // this brings the _url to a standard form...
    KURL kurl;
    if (dl)
        kurl = dl->m_doc->completeURL(url.deprecatedString());
    else
        kurl = url.deprecatedString();
    return requestImage(dl, kurl, reload, expireDate);
}

CachedImage* Cache::requestImage(DocLoader* dl, const KURL& url, bool reload, time_t expireDate)
{
    CachePolicy cachePolicy;
    if (dl)
        cachePolicy = dl->cachePolicy();
    else
        cachePolicy = CachePolicyVerify;

    // Checking if the URL is malformed is lots of extra work for little benefit.

    if (!dl->doc()->shouldCreateRenderers())
        return 0;

    CachedResource *o = 0;
    if (!reload)
        o = cache->get(String(url.url()).impl());
    if (!o) {
#ifdef CACHE_DEBUG
        kdDebug(6060) << "Cache: new: " << url.url() << endl;
#endif
        CachedImage *im = new CachedImage(dl, url.url(), cachePolicy, expireDate);
        if (dl && dl->autoloadImages()) Cache::loader()->load(dl, im, true);
        if (cacheDisabled)
            im->setFree(true);
        else {
            cache->set(String(url.url()).impl(), im);
            moveToHeadOfLRUList(im);
        }
        o = im;
    }

    
    if (o->type() != CachedResource::ImageResource)
        return 0;

#ifdef CACHE_DEBUG
    if (o->status() == CachedResource::Pending)
        kdDebug(6060) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug(6060) << "Cache: using cached: " << kurl.url() << ", status " << o->status() << endl;
#endif

    updateCacheStatus(dl, o);
    return static_cast<CachedImage *>(o);
}

CachedCSSStyleSheet* Cache::requestStyleSheet(DocLoader* dl, const String& url, bool reload, time_t expireDate, const DeprecatedString& charset)
{
    // this brings the _url to a standard form...
    KURL kurl;
    CachePolicy cachePolicy;
    if (dl) {
        kurl = dl->m_doc->completeURL(url.deprecatedString());
        cachePolicy = dl->cachePolicy();
    } else {
        kurl = url.deprecatedString();
        cachePolicy = CachePolicyVerify;
    }

    // Checking if the URL is malformed is lots of extra work for little benefit.

    CachedResource *o = cache->get(String(kurl.url()).impl());
    if (!o) {
#ifdef CACHE_DEBUG
        kdDebug(6060) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedCSSStyleSheet *sheet = new CachedCSSStyleSheet(dl, kurl.url(), cachePolicy, expireDate, charset);
        if (cacheDisabled)
            sheet->setFree(true);
        else {
            cache->set(String(kurl.url()).impl(), sheet);
            moveToHeadOfLRUList(sheet);
        }
        o = sheet;
    }

    
    if (o->type() != CachedResource::CSSStyleSheet)
    {
#ifdef CACHE_DEBUG
        kdDebug(6060) << "Cache::Internal Error in requestStyleSheet url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }

#ifdef CACHE_DEBUG
    if (o->status() == CachedResource::Pending)
        kdDebug(6060) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug(6060) << "Cache: using cached: " << kurl.url() << endl;
#endif

    updateCacheStatus(dl, o);
    return static_cast<CachedCSSStyleSheet *>(o);
}

CachedScript* Cache::requestScript(DocLoader* dl, const String& url, bool reload, time_t expireDate, const DeprecatedString& charset)
{
    // this brings the _url to a standard form...
    KURL kurl;
    CachePolicy cachePolicy;
    if (dl) {
        kurl = dl->m_doc->completeURL(url.deprecatedString());
        cachePolicy = dl->cachePolicy();
    } else {
        kurl = url.deprecatedString();
        cachePolicy = CachePolicyVerify;
    }

    // Checking if the URL is malformed is lots of extra work for little benefit.

    CachedResource *o = cache->get(String(kurl.url()).impl());
    if (!o)
    {
#ifdef CACHE_DEBUG
        kdDebug(6060) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedScript *script = new CachedScript(dl, kurl.url(), cachePolicy, expireDate, charset);
        if (cacheDisabled)
            script->setFree(true);
        else {
            cache->set(String(kurl.url()).impl(), script);
            moveToHeadOfLRUList(script);
        }
        o = script;
    }

    
    if (!(o->type() == CachedResource::Script)) {
#ifdef CACHE_DEBUG
        kdDebug(6060) << "Cache::Internal Error in requestScript url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }
    
    
#ifdef CACHE_DEBUG
    if (o->status() == CachedResource::Pending)
        kdDebug(6060) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug(6060) << "Cache: using cached: " << kurl.url() << endl;
#endif

    updateCacheStatus(dl, o);
    return static_cast<CachedScript *>(o);
}

#ifdef KHTML_XSLT
CachedXSLStyleSheet* Cache::requestXSLStyleSheet(DocLoader* dl, const String& url, bool reload, time_t expireDate)
{
    // this brings the _url to a standard form...
    KURL kurl;
    CachePolicy cachePolicy;
    if (dl) {
        kurl = dl->m_doc->completeURL(url.deprecatedString());
        cachePolicy = dl->cachePolicy();
    }
    else {
        kurl = url.deprecatedString();
        cachePolicy = CachePolicyVerify;
    }
    
    // Checking if the URL is malformed is lots of extra work for little benefit.
    
    CachedResource *o = cache->get(String(kurl.url()).impl());
    if (!o) {
#ifdef CACHE_DEBUG
        kdDebug(6060) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedXSLStyleSheet* doc = new CachedXSLStyleSheet(dl, kurl.url(), cachePolicy, expireDate);
        if (cacheDisabled)
            doc->setFree(true);
        else {
            cache->set(String(kurl.url()).impl(), doc);
            moveToHeadOfLRUList(doc);
        }
        o = doc;
    }
    
    
    if (o->type() != CachedResource::XSLStyleSheet) {
#ifdef CACHE_DEBUG
        kdDebug(6060) << "Cache::Internal Error in requestXSLStyleSheet url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }
    
#ifdef CACHE_DEBUG
    if (o->status() == CachedResource::Pending)
        kdDebug(6060) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug(6060) << "Cache: using cached: " << kurl.url() << endl;
#endif
    
    updateCacheStatus(dl, o);
    return static_cast<CachedXSLStyleSheet*>(o);
}
#endif

#ifndef KHTML_NO_XBL
CachedXBLDocument* Cache::requestXBLDocument(DocLoader* dl, const String& url, bool reload, 
                                             time_t expireDate)
{
    // this brings the _url to a standard form...
    KURL kurl;
    CachePolicy cachePolicy;
    if (dl) {
        kurl = dl->m_doc->completeURL(url.deprecatedString());
        cachePolicy = dl->cachePolicy();
    } else {
        kurl = url.deprecatedString();
        cachePolicy = CachePolicyVerify;
    }
    
    // Checking if the URL is malformed is lots of extra work for little benefit.
    
    CachedResource *o = cache->get(String(kurl.url()).impl());
    if (!o) {
#ifdef CACHE_DEBUG
        kdDebug(6060) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedXBLDocument* doc = new CachedXBLDocument(dl, kurl.url(), cachePolicy, expireDate);
        if (cacheDisabled)
            doc->setFree(true);
        else {
            cache->set(String(kurl.url()).impl(), doc);
            moveToHeadOfLRUList(doc);
        }
        o = doc;
    }
    
    
    if (o->type() != CachedResource::XBL) {
#ifdef CACHE_DEBUG
        kdDebug(6060) << "Cache::Internal Error in requestXBLDocument url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }
    
#ifdef CACHE_DEBUG
    if (o->status() == CachedResource::Pending)
        kdDebug(6060) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug(6060) << "Cache: using cached: " << kurl.url() << endl;
#endif
    
    updateCacheStatus(dl, o);
    return static_cast<CachedXBLDocument*>(o);
}
#endif

void Cache::flush(bool force)
{
    if (force)
       flushCount = 0;
    // Don't flush for every image.
    if (m_countOfLRUAndUncacheableLists < flushCount)
       return;

    init();

    while (m_headOfUncacheableList)
        remove(m_headOfUncacheableList);

    for (int i = maxLRULists-1; i>=0; i--) {
        if (m_totalSizeOfLRULists <= maxSize)
            break;
            
        while (m_totalSizeOfLRULists > maxSize && m_LRULists[i].m_tail)
            remove(m_LRULists[i].m_tail);
    }

    flushCount = m_countOfLRUAndUncacheableLists+10; // Flush again when the cache has grown.
}


void Cache::setSize(int bytes)
{
    maxSize = bytes;
    maxCacheable = max(maxSize / 128, minMaxCacheableObjectSize);

    // may be we need to clear parts of the cache
    flushCount = 0;
    flush(true);
}

void Cache::remove(CachedResource *object)
{
  // this indicates the deref() method of CachedResource to delete itself when the reference counter
  // drops down to zero
  object->setFree(true);

  cache->remove(object->url().impl());
  removeFromLRUList(object);

  HashSet<DocLoader*>::iterator end = docloaders->end();
  for (HashSet<DocLoader*>::iterator itr = docloaders->begin(); itr != end; ++itr)
      (*itr)->removeCachedObject(object);

  if (object->canDelete())
     delete object;
}

static inline int FastLog2(uint32_t i)
{
    int log2 = 0;
    if (i & (i - 1))
        log2 += 1;
    if (i >> 16)
        log2 += 16, i >>= 16;
    if (i >> 8)
        log2 += 8, i >>= 8;
    if (i >> 4)
        log2 += 4, i >>= 4;
    if (i >> 2)
        log2 += 2, i >>= 2;
    if (i >> 1)
        log2 += 1;
    return log2;
}

LRUList* Cache::getLRUListFor(CachedResource* o)
{
    int accessCount = o->accessCount();
    int queueIndex;
    if (accessCount == 0) {
        queueIndex = 0;
    } else {
        int sizeLog = FastLog2(o->size());
        queueIndex = sizeLog / o->accessCount() - 1;
        if (queueIndex < 0)
            queueIndex = 0;
        if (queueIndex >= maxLRULists)
            queueIndex = maxLRULists-1;
    }
    if (!m_LRULists)
        m_LRULists = new LRUList [maxLRULists];
    return &m_LRULists[queueIndex];
}

void Cache::removeFromLRUList(CachedResource *object)
{
    CachedResource *next = object->m_nextInLRUList;
    CachedResource *prev = object->m_prevInLRUList;
    bool uncacheable = object->status() == CachedResource::Uncacheable;
    
    LRUList* list = uncacheable ? 0 : getLRUListFor(object);
    CachedResource *&head = uncacheable ? m_headOfUncacheableList : list->m_head;
    
    if (next == 0 && prev == 0 && head != object) {
        return;
    }
    
    object->m_nextInLRUList = 0;
    object->m_prevInLRUList = 0;
    
    if (next)
        next->m_prevInLRUList = prev;
    else if (!uncacheable && list->m_tail == object)
        list->m_tail = prev;

    if (prev)
        prev->m_nextInLRUList = next;
    else if (head == object)
        head = next;
    
    --m_countOfLRUAndUncacheableLists;
    
    if (!uncacheable)
        m_totalSizeOfLRULists -= object->size();
}

void Cache::moveToHeadOfLRUList(CachedResource *object)
{
    insertInLRUList(object);
}

void Cache::insertInLRUList(CachedResource *object)
{
    removeFromLRUList(object);
    
    if (!object->allowInLRUList())
        return;
    
    LRUList* list = getLRUListFor(object);
    
    bool uncacheable = object->status() == CachedResource::Uncacheable;
    CachedResource *&head = uncacheable ? m_headOfUncacheableList : list->m_head;

    object->m_nextInLRUList = head;
    if (head)
        head->m_prevInLRUList = object;
    head = object;
    
    if (object->m_nextInLRUList == 0 && !uncacheable)
        list->m_tail = object;
    
    ++m_countOfLRUAndUncacheableLists;
    
    if (!uncacheable)
        m_totalSizeOfLRULists += object->size();
}

bool Cache::adjustSize(CachedResource *object, int delta)
{
    if (object->status() == CachedResource::Uncacheable)
        return false;

    if (object->m_nextInLRUList == 0 && object->m_prevInLRUList == 0 &&
        getLRUListFor(object)->m_head != object)
        return false;
    
    m_totalSizeOfLRULists += delta;
    return delta != 0;
}

Cache::Statistics Cache::getStatistics()
{
    Statistics stats;

    if (!cache)
        return stats;

    CacheMap::iterator e = cache->end();
    for (CacheMap::iterator i = cache->begin(); i != e; ++i) {
        CachedResource *o = i->second;
        switch (o->type()) {
            case CachedResource::ImageResource:
                stats.images.count++;
                stats.images.size += o->size();
                break;

            case CachedResource::CSSStyleSheet:
                stats.styleSheets.count++;
                stats.styleSheets.size += o->size();
                break;

            case CachedResource::Script:
                stats.scripts.count++;
                stats.scripts.size += o->size();
                break;
#ifdef KHTML_XSLT
            case CachedResource::XSLStyleSheet:
                stats.xslStyleSheets.count++;
                stats.xslStyleSheets.size += o->size();
                break;
#endif
#ifndef KHTML_NO_XBL
            case CachedResource::XBL:
                stats.xblDocs.count++;
                stats.xblDocs.size += o->size();
                break;
#endif
            default:
                stats.other.count++;
                stats.other.size += o->size();
        }
    }
    
    return stats;
}

void Cache::flushAll()
{
    if (!cache)
        return;

    for (;;) {
        CacheMap::iterator i = cache->begin();
        if (i == cache->end())
            break;
        remove(i->second);
    }
}

void Cache::setCacheDisabled(bool disabled)
{
    cacheDisabled = disabled;
    if (disabled)
        flushAll();
}

CachedResource* Cache::get(const String& s)
{
    return (cache && s.impl()) ? cache->get(s.impl()) : 0;
}

}
