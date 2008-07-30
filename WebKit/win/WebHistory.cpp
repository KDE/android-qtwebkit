/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebKitDLL.h"
#include "WebHistory.h"

#include "CFDictionaryPropertyBag.h"
#include "WebKit.h"
#include "MarshallingHelpers.h"
#include "WebHistoryItem.h"
#include "WebKit.h"
#include "WebNotificationCenter.h"
#include "WebPreferences.h"
#include <CoreFoundation/CoreFoundation.h>
#pragma warning( push, 0 )
#include <wtf/Vector.h>
#include <WebCore/KURL.h>
#include <WebCore/PageGroup.h>
#pragma warning( pop )

using namespace WebCore;

CFStringRef DatesArrayKey = CFSTR("WebHistoryDates");
CFStringRef FileVersionKey = CFSTR("WebHistoryFileVersion");

#define currentFileVersion 1

static bool areEqualOrClose(double d1, double d2)
{
    double diff = d1-d2;
    return (diff < .000001 && diff > -.000001);
}

static CFDictionaryPropertyBag* createUserInfoFromArray(BSTR notificationStr, CFArrayRef arrayItem)
{
    RetainPtr<CFMutableDictionaryRef> dictionary(AdoptCF, 
        CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr<CFStringRef> key(AdoptCF, MarshallingHelpers::BSTRToCFStringRef(notificationStr));
    CFDictionaryAddValue(dictionary.get(), key.get(), arrayItem);

    CFDictionaryPropertyBag* result = CFDictionaryPropertyBag::createInstance();
    result->setDictionary(dictionary.get());
    return result;
}

static CFDictionaryPropertyBag* createUserInfoFromHistoryItem(BSTR notificationStr, IWebHistoryItem* item)
{
    // reference counting of item added to the array is managed by the CFArray value callbacks
    RetainPtr<CFArrayRef> itemList(AdoptCF, CFArrayCreate(0, (const void**) &item, 1, &MarshallingHelpers::kIUnknownArrayCallBacks));
    CFDictionaryPropertyBag* info = createUserInfoFromArray(notificationStr, itemList.get());
    return info;
}

static void releaseUserInfo(CFDictionaryPropertyBag* userInfo)
{
    // free the dictionary
    userInfo->setDictionary(0);
    int result = userInfo->Release();
    (void)result;
    ASSERT(result == 0);   // make sure no one else holds a reference to the userInfo.
}

// WebHistory -----------------------------------------------------------------

WebHistory::WebHistory()
: m_refCount(0)
, m_preferences(0)
{
    gClassCount++;
    gClassNameCount.add("WebHistory");

    m_entriesByURL.adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &MarshallingHelpers::kIUnknownDictionaryValueCallBacks));
    m_datesWithEntries.adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));
    m_entriesByDate.adoptCF(CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));

    m_preferences = WebPreferences::sharedStandardPreferences();
}

WebHistory::~WebHistory()
{
    gClassCount--;
    gClassNameCount.remove("WebHistory");
}

WebHistory* WebHistory::createInstance()
{
    WebHistory* instance = new WebHistory();
    instance->AddRef();
    return instance;
}

HRESULT WebHistory::postNotification(NotificationType notifyType, IPropertyBag* userInfo /*=0*/)
{
    IWebNotificationCenter* nc = WebNotificationCenter::defaultCenterInternal();
    HRESULT hr = nc->postNotificationName(getNotificationString(notifyType), static_cast<IWebHistory*>(this), userInfo);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

BSTR WebHistory::getNotificationString(NotificationType notifyType)
{
    static BSTR keys[6] = {0};
    if (!keys[0]) {
        keys[0] = SysAllocString(WebHistoryItemsAddedNotification);
        keys[1] = SysAllocString(WebHistoryItemsRemovedNotification);
        keys[2] = SysAllocString(WebHistoryAllItemsRemovedNotification);
        keys[3] = SysAllocString(WebHistoryLoadedNotification);
        keys[4] = SysAllocString(WebHistoryItemsDiscardedWhileLoadingNotification);
        keys[5] = SysAllocString(WebHistorySavedNotification);
    }
    return keys[notifyType];
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistory::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, CLSID_WebHistory))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebHistory*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistory))
        *ppvObject = static_cast<IWebHistory*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebHistory::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebHistory::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHistory ----------------------------------------------------------------

static inline COMPtr<WebHistory>& sharedHistoryStorage()
{
    static COMPtr<WebHistory> sharedHistory;
    return sharedHistory;
}

WebHistory* WebHistory::sharedHistory()
{
    return sharedHistoryStorage().get();
}

HRESULT STDMETHODCALLTYPE WebHistory::optionalSharedHistory( 
    /* [retval][out] */ IWebHistory** history)
{
    *history = sharedHistory();
    if (*history)
        (*history)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::setOptionalSharedHistory( 
    /* [in] */ IWebHistory* history)
{
    if (sharedHistoryStorage() == history)
        return S_OK;
    sharedHistoryStorage().query(history);
    PageGroup::setShouldTrackVisitedLinks(sharedHistoryStorage());
    PageGroup::removeAllVisitedLinks();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::loadFromURL( 
    /* [in] */ BSTR url,
    /* [out] */ IWebError** error,
    /* [retval][out] */ BOOL* succeeded)
{
    HRESULT hr = S_OK;
    RetainPtr<CFMutableArrayRef> discardedItems(AdoptCF, 
        CFArrayCreateMutable(0, 0, &MarshallingHelpers::kIUnknownArrayCallBacks));

    RetainPtr<CFURLRef> urlRef(AdoptCF, MarshallingHelpers::BSTRToCFURLRef(url));

    hr = loadHistoryGutsFromURL(urlRef.get(), discardedItems.get(), error);
    if (FAILED(hr))
        goto exit;

    hr = postNotification(kWebHistoryLoadedNotification);
    if (FAILED(hr))
        goto exit;

    if (CFArrayGetCount(discardedItems.get()) > 0) {
        CFDictionaryPropertyBag* userInfo = createUserInfoFromArray(getNotificationString(kWebHistoryItemsDiscardedWhileLoadingNotification), discardedItems.get());
        hr = postNotification(kWebHistoryItemsDiscardedWhileLoadingNotification, userInfo);
        releaseUserInfo(userInfo);
        if (FAILED(hr))
            goto exit;
    }

exit:
    if (succeeded)
        *succeeded = SUCCEEDED(hr);
    return hr;
}

static CFDictionaryRef createHistoryListFromStream(CFReadStreamRef stream, CFPropertyListFormat format)
{
    return (CFDictionaryRef)CFPropertyListCreateFromStream(0, stream, 0, kCFPropertyListImmutable, &format, 0);
}

HRESULT WebHistory::loadHistoryGutsFromURL(CFURLRef url, CFMutableArrayRef discardedItems, IWebError** /*error*/) //FIXME
{
    CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0 | kCFPropertyListXMLFormat_v1_0;
    HRESULT hr = S_OK;
    int numberOfItemsLoaded = 0;

    RetainPtr<CFReadStreamRef> stream(AdoptCF, CFReadStreamCreateWithFile(0, url));
    if (!stream) 
        return E_FAIL;

    if (!CFReadStreamOpen(stream.get())) 
        return E_FAIL;

    RetainPtr<CFDictionaryRef> historyList(AdoptCF, createHistoryListFromStream(stream.get(), format));
    CFReadStreamClose(stream.get());

    if (!historyList) 
        return E_FAIL;

    CFNumberRef fileVersionObject = (CFNumberRef)CFDictionaryGetValue(historyList.get(), FileVersionKey);
    int fileVersion;
    if (!CFNumberGetValue(fileVersionObject, kCFNumberIntType, &fileVersion)) 
        return E_FAIL;

    if (fileVersion > currentFileVersion) 
        return E_FAIL;
    
    CFArrayRef datesArray = (CFArrayRef)CFDictionaryGetValue(historyList.get(), DatesArrayKey);

    int itemCountLimit;
    hr = historyItemLimit(&itemCountLimit);
    if (FAILED(hr))
        return hr;

    CFAbsoluteTime limitDate;
    hr = ageLimitDate(&limitDate);
    if (FAILED(hr))
        return hr;

    bool ageLimitPassed = false;
    bool itemLimitPassed = false;

    CFIndex itemCount = CFArrayGetCount(datesArray);
    for (CFIndex i = 0; i < itemCount; ++i) {
        CFDictionaryRef itemAsDictionary = (CFDictionaryRef)CFArrayGetValueAtIndex(datesArray, i);
        COMPtr<WebHistoryItem> item(AdoptCOM, WebHistoryItem::createInstance());
        hr = item->initFromDictionaryRepresentation((void*)itemAsDictionary);
        if (FAILED(hr))
            return hr;

        // item without URL is useless; data on disk must have been bad; ignore
        BOOL hasURL;
        hr = item->hasURLString(&hasURL);
        if (FAILED(hr))
            return hr;
        
        if (hasURL) {
            // Test against date limit. Since the items are ordered newest to oldest, we can stop comparing
            // once we've found the first item that's too old.
            if (!ageLimitPassed) {
                DATE lastVisitedTime;
                hr = item->lastVisitedTimeInterval(&lastVisitedTime);
                if (FAILED(hr))
                    return hr;
                if (timeToDate(MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedTime)) <= limitDate)
                    ageLimitPassed = true;
            }
            if (ageLimitPassed || itemLimitPassed)
                CFArrayAppendValue(discardedItems, item.get());
            else {
                addItem(item.get()); // ref is added inside addItem
                ++numberOfItemsLoaded;
                if (numberOfItemsLoaded == itemCountLimit)
                    itemLimitPassed = true;
            }
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE WebHistory::saveToURL( 
    /* [in] */ BSTR url,
    /* [out] */ IWebError** error,
    /* [retval][out] */ BOOL* succeeded)
{
    HRESULT hr = S_OK;
    RetainPtr<CFURLRef> urlRef(AdoptCF, MarshallingHelpers::BSTRToCFURLRef(url));

    hr = saveHistoryGuts(urlRef.get(), error);

    if (succeeded)
        *succeeded = SUCCEEDED(hr);
    if (SUCCEEDED(hr))
        hr = postNotification(kWebHistorySavedNotification);

    return hr;
}

HRESULT WebHistory::saveHistoryGuts(CFURLRef url, IWebError** error)
{
    HRESULT hr = S_OK;

    // FIXME:  Correctly report error when new API is ready.
    if (error)
        *error = 0;

    RetainPtr<CFWriteStreamRef> stream(AdoptCF, CFWriteStreamCreateWithFile(kCFAllocatorDefault, url));
    if (!stream) 
        return E_FAIL;

    CFMutableArrayRef rawEntries;
    hr = datesArray(&rawEntries);
    if (FAILED(hr))
        return hr;
    RetainPtr<CFMutableArrayRef> entries(AdoptCF, rawEntries);

    // create the outer dictionary
    CFTypeRef keys[2];
    CFTypeRef values[2];
    keys[0]   = DatesArrayKey;
    values[0] = entries.get();
    keys[1]   = FileVersionKey;

    int version = currentFileVersion;
    RetainPtr<CFNumberRef> versionCF(AdoptCF, CFNumberCreate(0, kCFNumberIntType, &version));
    values[1] = versionCF.get();

    RetainPtr<CFDictionaryRef> dictionary(AdoptCF, 
        CFDictionaryCreate(0, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    if (!CFWriteStreamOpen(stream.get())) 
        return E_FAIL;

    if (!CFPropertyListWriteToStream(dictionary.get(), stream.get(), kCFPropertyListXMLFormat_v1_0, 0))
        hr = E_FAIL;
 
    CFWriteStreamClose(stream.get());

    return hr;
}

HRESULT WebHistory::datesArray(CFMutableArrayRef* datesArray)
{
    HRESULT hr = S_OK;

    RetainPtr<CFMutableArrayRef> result(AdoptCF, CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks));
    
    // for each date with entries
    int dateCount = CFArrayGetCount(m_entriesByDate.get());
    for (int i = 0; i < dateCount; ++i) {
        // get the entries for that date
        CFArrayRef entries = (CFArrayRef)CFArrayGetValueAtIndex(m_entriesByDate.get(), i);
        int entriesCount = CFArrayGetCount(entries);
        for (int j = entriesCount - 1; j >= 0; --j) {
            IWebHistoryItem* item = (IWebHistoryItem*) CFArrayGetValueAtIndex(entries, j);
            IWebHistoryItemPrivate* webHistoryItem;
            hr = item->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&webHistoryItem);
            if (FAILED(hr))
                return E_FAIL;
            
            CFDictionaryRef itemDict;
            hr = webHistoryItem->dictionaryRepresentation((void**)&itemDict);
            webHistoryItem->Release();
            if (FAILED(hr))
                return E_FAIL;

            CFArrayAppendValue(result.get(), itemDict);
            CFRelease(itemDict);
        }
    }

    if (SUCCEEDED(hr))
        *datesArray = result.releaseRef();
    return hr;
}

HRESULT STDMETHODCALLTYPE WebHistory::addItems( 
    /* [in] */ int itemCount,
    /* [in] */ IWebHistoryItem** items)
{
    // There is no guarantee that the incoming entries are in any particular
    // order, but if this is called with a set of entries that were created by
    // iterating through the results of orderedLastVisitedDays and orderedItemsLastVisitedOnDay
    // then they will be ordered chronologically from newest to oldest. We can make adding them
    // faster (fewer compares) by inserting them from oldest to newest.

    HRESULT hr;
    for (int i = itemCount - 1; i >= 0; --i) {
        hr = addItem(items[i]);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::removeItems( 
    /* [in] */ int itemCount,
    /* [in] */ IWebHistoryItem** items)
{
    HRESULT hr;
    for (int i = 0; i < itemCount; ++i) {
        hr = removeItem(items[i]);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::removeAllItems( void)
{
    CFArrayRemoveAllValues(m_entriesByDate.get());
    CFArrayRemoveAllValues(m_datesWithEntries.get());
    CFDictionaryRemoveAllValues(m_entriesByURL.get());

    PageGroup::removeAllVisitedLinks();

    return postNotification(kWebHistoryAllItemsRemovedNotification);
}

HRESULT STDMETHODCALLTYPE WebHistory::orderedLastVisitedDays( 
    /* [out][in] */ int* count,
    /* [in] */ DATE* calendarDates)
{
    int dateCount = CFArrayGetCount(m_datesWithEntries.get());
    if (!calendarDates) {
        *count = dateCount;
        return S_OK;
    }

    if (*count < dateCount) {
        *count = dateCount;
        return E_FAIL;
    }

    *count = dateCount;
    for (int i = 0; i < dateCount; i++) {
        CFNumberRef absoluteTimeNumberRef = (CFNumberRef)CFArrayGetValueAtIndex(m_datesWithEntries.get(), i);
        CFAbsoluteTime absoluteTime;
        if (!CFNumberGetValue(absoluteTimeNumberRef, kCFNumberDoubleType, &absoluteTime))
            return E_FAIL;
        calendarDates[i] = MarshallingHelpers::CFAbsoluteTimeToDATE(absoluteTime);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::orderedItemsLastVisitedOnDay( 
    /* [out][in] */ int* count,
    /* [in] */ IWebHistoryItem** items,
    /* [in] */ DATE calendarDate)
{
    int index;
    if (!findIndex(&index, MarshallingHelpers::DATEToCFAbsoluteTime(calendarDate))) {
        *count = 0;
        return 0;
    }

    CFArrayRef entries = (CFArrayRef)CFArrayGetValueAtIndex(m_entriesByDate.get(), index);
    if (!entries) {
        *count = 0;
        return 0;
    }

    int newCount = CFArrayGetCount(entries);

    if (!items) {
        *count = newCount;
        return S_OK;
    }

    if (*count < newCount) {
        *count = newCount;
        return E_FAIL;
    }

    *count = newCount;
    for (int i = 0; i < newCount; i++) {
        IWebHistoryItem* item = (IWebHistoryItem*)CFArrayGetValueAtIndex(entries, i);
        if (!item)
            return E_FAIL;
        item->AddRef();
        items[newCount-i-1] = item; // reverse when inserting to get the list sorted oldest to newest
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::setHistoryItemLimit( 
    /* [in] */ int limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->setHistoryItemLimit(limit);
}

HRESULT STDMETHODCALLTYPE WebHistory::historyItemLimit( 
    /* [retval][out] */ int* limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->historyItemLimit(limit);
}

HRESULT STDMETHODCALLTYPE WebHistory::setHistoryAgeInDaysLimit( 
    /* [in] */ int limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->setHistoryAgeInDaysLimit(limit);
}

HRESULT STDMETHODCALLTYPE WebHistory::historyAgeInDaysLimit( 
    /* [retval][out] */ int* limit)
{
    if (!m_preferences)
        return E_FAIL;
    return m_preferences->historyAgeInDaysLimit(limit);
}

HRESULT WebHistory::removeItem(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;
    BSTR urlBStr = 0;

    hr = entry->URLString(&urlBStr);
    if (FAILED(hr))
        return hr;

    RetainPtr<CFStringRef> urlString(AdoptCF, MarshallingHelpers::BSTRToCFStringRef(urlBStr));
    SysFreeString(urlBStr);

    // If this exact object isn't stored, then make no change.
    // FIXME: Is this the right behavior if this entry isn't present, but another entry for the same URL is?
    // Maybe need to change the API to make something like removeEntryForURLString public instead.
    IWebHistoryItem *matchingEntry = (IWebHistoryItem*)CFDictionaryGetValue(m_entriesByURL.get(), urlString.get());
    if (matchingEntry != entry)
        return E_FAIL;

    hr = removeItemForURLString(urlString.get());
    if (FAILED(hr))
        return hr;

    CFDictionaryPropertyBag* userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsRemovedNotification), entry);
    hr = postNotification(kWebHistoryItemsRemovedNotification, userInfo);
    releaseUserInfo(userInfo);

    return hr;
}

HRESULT WebHistory::addItem(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;

    if (!entry)
        return E_FAIL;

    BSTR urlBStr = 0;
    hr = entry->URLString(&urlBStr);
    if (FAILED(hr))
        return hr;

    RetainPtr<CFStringRef> urlString(AdoptCF, MarshallingHelpers::BSTRToCFStringRef(urlBStr));
    SysFreeString(urlBStr);

    COMPtr<IWebHistoryItem> oldEntry((IWebHistoryItem*) CFDictionaryGetValue(
        m_entriesByURL.get(), urlString.get()));
    
    if (oldEntry) {
        removeItemForURLString(urlString.get());

        // If we already have an item with this URL, we need to merge info that drives the
        // URL autocomplete heuristics from that item into the new one.
        IWebHistoryItemPrivate* entryPriv;
        hr = entry->QueryInterface(IID_IWebHistoryItemPrivate, (void**)&entryPriv);
        if (SUCCEEDED(hr)) {
            entryPriv->mergeAutoCompleteHints(oldEntry.get());
            entryPriv->Release();
        }
    }

    hr = addItemToDateCaches(entry);
    if (FAILED(hr))
        return hr;

    CFDictionarySetValue(m_entriesByURL.get(), urlString.get(), entry);

    CFDictionaryPropertyBag* userInfo = createUserInfoFromHistoryItem(
        getNotificationString(kWebHistoryItemsAddedNotification), entry);
    hr = postNotification(kWebHistoryItemsAddedNotification, userInfo);
    releaseUserInfo(userInfo);

    return hr;
}

void WebHistory::addItem(const KURL& url, const String& title)
{
    COMPtr<WebHistoryItem> item(AdoptCOM, WebHistoryItem::createInstance());
    if (!item)
        return;

    SYSTEMTIME currentTime;
    GetSystemTime(&currentTime);
    DATE lastVisited;
    if (!SystemTimeToVariantTime(&currentTime, &lastVisited))
        return;

    HRESULT hr = item->initWithURLString(BString(url.string()), BString(title), 0);
    if (FAILED(hr))
        return;

    hr = item->setLastVisitedTimeInterval(lastVisited); // also increments visitedCount
    if (FAILED(hr))
        return;

    addItem(item.get());
}

HRESULT WebHistory::itemForURLString(
    /* [in] */ CFStringRef urlString,
    /* [retval][out] */ IWebHistoryItem** item)
{
    if (!item)
        return E_FAIL;
    *item = 0;

    IWebHistoryItem* foundItem = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL.get(), urlString);
    if (!foundItem)
        return E_FAIL;

    foundItem->AddRef();
    *item = foundItem;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistory::itemForURL( 
    /* [in] */ BSTR url,
    /* [retval][out] */ IWebHistoryItem** item)
{
    RetainPtr<CFStringRef> urlString(AdoptCF, MarshallingHelpers::BSTRToCFStringRef(url));
    return itemForURLString(urlString.get(), item);
}

HRESULT WebHistory::removeItemForURLString(CFStringRef urlString)
{
    IWebHistoryItem* entry = (IWebHistoryItem*) CFDictionaryGetValue(m_entriesByURL.get(), urlString);
    if (!entry) 
        return E_FAIL;

    HRESULT hr = removeItemFromDateCaches(entry);
    CFDictionaryRemoveValue(m_entriesByURL.get(), urlString);

    if (!CFDictionaryGetCount(m_entriesByURL.get()))
        PageGroup::removeAllVisitedLinks();

    return hr;
}

HRESULT WebHistory::addItemToDateCaches(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;

    DATE lastVisitedCOMTime;
    entry->lastVisitedTimeInterval(&lastVisitedCOMTime);
    CFAbsoluteTime lastVisitedDate = timeToDate(MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedCOMTime));
    
    int dateIndex;
    if (findIndex(&dateIndex, lastVisitedDate)) {
        // other entries already exist for this date
        hr = insertItem(entry, dateIndex);
    } else {
        // no other entries exist for this date
        RetainPtr<CFNumberRef> lastVisitedDateRef(AdoptCF, CFNumberCreate(0, kCFNumberDoubleType, &lastVisitedDate));
        CFArrayInsertValueAtIndex(m_datesWithEntries.get(), dateIndex, lastVisitedDateRef.get());
        RetainPtr<CFMutableArrayRef> entryArray(AdoptCF, 
            CFArrayCreateMutable(0, 0, &MarshallingHelpers::kIUnknownArrayCallBacks));
        CFArrayAppendValue(entryArray.get(), entry);
        CFArrayInsertValueAtIndex(m_entriesByDate.get(), dateIndex, entryArray.get());
    }

    return hr;
}

HRESULT WebHistory::removeItemFromDateCaches(IWebHistoryItem* entry)
{
    HRESULT hr = S_OK;

    DATE lastVisitedCOMTime;
    entry->lastVisitedTimeInterval(&lastVisitedCOMTime);
    CFAbsoluteTime lastVisitedDate = timeToDate(MarshallingHelpers::DATEToCFAbsoluteTime(lastVisitedCOMTime));

    int dateIndex;
    if (!findIndex(&dateIndex, lastVisitedDate))
        return E_FAIL;

    CFMutableArrayRef entriesForDate = (CFMutableArrayRef) CFArrayGetValueAtIndex(m_entriesByDate.get(), dateIndex);
    CFIndex count = CFArrayGetCount(entriesForDate);
    for (int i = count - 1; i >= 0; --i) {
        if ((IWebHistoryItem*)CFArrayGetValueAtIndex(entriesForDate, i) == entry)
            CFArrayRemoveValueAtIndex(entriesForDate, i);
    }

    // remove this date entirely if there are no other entries on it
    if (CFArrayGetCount(entriesForDate) == 0) {
        CFArrayRemoveValueAtIndex(m_entriesByDate.get(), dateIndex);
        CFArrayRemoveValueAtIndex(m_datesWithEntries.get(), dateIndex);
    }

    return hr;
}

// Returns whether the day is already in the list of days,
// and fills in *index with the found or proposed index.
bool WebHistory::findIndex(int* index, CFAbsoluteTime forDay)
{
    CFAbsoluteTime forDayInDays = timeToDate(forDay);

    //FIXME: just does linear search through days; inefficient if many days
    int count = CFArrayGetCount(m_datesWithEntries.get());
    for (*index = 0; *index < count; ++*index) {
        CFNumberRef entryTimeNumberRef = (CFNumberRef) CFArrayGetValueAtIndex(m_datesWithEntries.get(), *index);
        CFAbsoluteTime entryTime;
        CFNumberGetValue(entryTimeNumberRef, kCFNumberDoubleType, &entryTime);
        CFAbsoluteTime entryInDays = timeToDate(entryTime);
        if (areEqualOrClose(forDayInDays, entryInDays))
            return true;
        else if (forDayInDays > entryInDays)
            return false;
    }
    return false;
}

HRESULT WebHistory::insertItem(IWebHistoryItem* entry, int dateIndex)
{
    HRESULT hr = S_OK;

    if (!entry)
        return E_FAIL;
    if (dateIndex < 0 || dateIndex >= CFArrayGetCount(m_entriesByDate.get()))
        return E_FAIL;

    //FIXME: just does linear search through entries; inefficient if many entries for this date
    DATE entryTime;
    entry->lastVisitedTimeInterval(&entryTime);
    CFMutableArrayRef entriesForDate = (CFMutableArrayRef) CFArrayGetValueAtIndex(m_entriesByDate.get(), dateIndex);
    int count = CFArrayGetCount(entriesForDate);
    // optimized for inserting oldest to youngest
    int index;
    for (index = 0; index < count; ++index) {
        IWebHistoryItem* indEntry = (IWebHistoryItem*) CFArrayGetValueAtIndex(entriesForDate, index);
        DATE indTime;
        hr = indEntry->lastVisitedTimeInterval(&indTime);
        if (FAILED(hr))
            return hr;
        if (entryTime < indTime)
            break;
    }    
    CFArrayInsertValueAtIndex(entriesForDate, index, entry);
    return S_OK;
}

CFAbsoluteTime WebHistory::timeToDate(CFAbsoluteTime time)
{
    // can't just divide/round since the day boundaries depend on our current time zone
    const double secondsPerDay = 60 * 60 * 24;
    CFTimeZoneRef timeZone = CFTimeZoneCopySystem();
    CFGregorianDate date = CFAbsoluteTimeGetGregorianDate(time, timeZone);
    date.hour = date.minute = 0;
    date.second = 0.0;
    CFAbsoluteTime timeInDays = CFGregorianDateGetAbsoluteTime(date, timeZone);
    if (areEqualOrClose(time - timeInDays, secondsPerDay))
        timeInDays += secondsPerDay;
    return timeInDays;
}

// Return a date that marks the age limit for history entries saved to or
// loaded from disk. Any entry older than this item should be rejected.
HRESULT WebHistory::ageLimitDate(CFAbsoluteTime* time)
{
    // get the current date as a CFAbsoluteTime
    CFAbsoluteTime currentDate = timeToDate(CFAbsoluteTimeGetCurrent());

    CFGregorianUnits ageLimit = {0};
    int historyLimitDays;
    HRESULT hr = historyAgeInDaysLimit(&historyLimitDays);
    if (FAILED(hr))
        return hr;
    ageLimit.days = -historyLimitDays;
    *time = CFAbsoluteTimeAddGregorianUnits(currentDate, CFTimeZoneCopySystem(), ageLimit);
    return S_OK;
}

static void addVisitedLinkToPageGroup(const void* key, const void*, void* context)
{
    CFStringRef url = static_cast<CFStringRef>(key);
    PageGroup* group = static_cast<PageGroup*>(context);

    CFIndex length = CFStringGetLength(url);
    const UChar* characters = reinterpret_cast<const UChar*>(CFStringGetCharactersPtr(url));
    if (characters)
        group->addVisitedLink(characters, length);
    else {
        Vector<UChar, 512> buffer(length);
        CFStringGetCharacters(url, CFRangeMake(0, length), reinterpret_cast<UniChar*>(buffer.data()));
        group->addVisitedLink(buffer.data(), length);
    }
}

void WebHistory::addVisitedLinksToPageGroup(PageGroup& group)
{
    CFDictionaryApplyFunction(m_entriesByURL.get(), addVisitedLinkToPageGroup, &group);
}
