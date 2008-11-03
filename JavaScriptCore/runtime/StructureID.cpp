/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "StructureID.h"

#include "JSObject.h"
#include "PropertyNameArray.h"
#include "StructureIDChain.h"
#include "identifier.h"
#include "lookup.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/RefPtr.h>

#if ENABLE(JSC_MULTIPLE_THREADS)
#include <wtf/Threading.h>
#endif

#define DUMP_STRUCTURE_ID_STATISTICS 0

#ifndef NDEBUG
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#else
#define DO_PROPERTYMAP_CONSTENCY_CHECK 0
#endif

using namespace std;
using WTF::doubleHash;

namespace JSC {

// Choose a number for the following so that most property maps are smaller,
// but it's not going to blow out the stack to allocate this number of pointers.
static const int smallMapThreshold = 1024;

// The point at which the function call overhead of the qsort implementation
// becomes small compared to the inefficiency of insertion sort.
static const unsigned tinyMapThreshold = 20;

#ifndef NDEBUG
static WTF::RefCountedLeakCounter structureIDCounter("StructureID");

#if ENABLE(JSC_MULTIPLE_THREADS)
static Mutex ignoreSetMutex;
#endif

static bool shouldIgnoreLeaks;
static HashSet<StructureID*> ignoreSet;
#endif

#if DUMP_STRUCTURE_ID_STATISTICS
static HashSet<StructureID*> liveStructureIDSet;
#endif

void StructureID::dumpStatistics()
{
#if DUMP_STRUCTURE_ID_STATISTICS
    unsigned numberLeaf = 0;
    unsigned numberUsingSingleSlot = 0;
    unsigned numberSingletons = 0;
    unsigned totalPropertyMapsSize = 0;

    HashSet<StructureID*>::const_iterator end = liveStructureIDSet.end();
    for (HashSet<StructureID*>::const_iterator it = liveStructureIDSet.begin(); it != end; ++it) {
        StructureID* structureID = *it;
        if (structureID->m_usingSingleTransitionSlot) {
            if (!structureID->m_transitions.singleTransition)
                ++numberLeaf;
            else
                ++numberUsingSingleSlot;

           if (!structureID->m_previous && !structureID->m_transitions.singleTransition)
                ++numberSingletons;
        }

        if (structureID->m_propertyTable)
            totalPropertyMapsSize += PropertyMapHashTable::allocationSize(structureID->m_propertyTable->size);
    }

    printf("Number of live StructureIDs: %d\n", liveStructureIDSet.size());
    printf("Number of StructureIDs using the single item optimization for transition map: %d\n", numberUsingSingleSlot);
    printf("Number of StructureIDs that are leaf nodes: %d\n", numberLeaf);
    printf("Number of StructureIDs that singletons: %d\n", numberSingletons);

    printf("Size of a single StructureIDs: %d\n", static_cast<unsigned>(sizeof(StructureID)));
    printf("Size of sum of all property maps: %d\n", totalPropertyMapsSize);
    printf("Size of average of all property maps: %f\n", static_cast<double>(totalPropertyMapsSize) / static_cast<double>(liveStructureIDSet.size()));
#else
    printf("Dumping StructureID statistics is not enabled.\n");
#endif
}

StructureID::StructureID(JSValue* prototype, const TypeInfo& typeInfo)
    : m_typeInfo(typeInfo)
    , m_prototype(prototype)
    , m_cachedPrototypeChain(0)
    , m_previous(0)
    , m_nameInPrevious(0)
    , m_transitionCount(0)
    , m_propertyTable(0)
    , m_propertyStorageCapacity(JSObject::inlineStorageCapacity)
    , m_cachedTransistionOffset(WTF::notFound)
    , m_isDictionary(false)
    , m_hasGetterSetterProperties(false)
    , m_usingSingleTransitionSlot(true)
    , m_attributesInPrevious(0)
{
    ASSERT(m_prototype);
    ASSERT(m_prototype->isObject() || m_prototype->isNull());

    m_transitions.singleTransition = 0;

#ifndef NDEBUG
#if ENABLE(JSC_MULTIPLE_THREADS)
    MutexLocker protect(ignoreSetMutex);
#endif
    if (shouldIgnoreLeaks)
        ignoreSet.add(this);
    else
        structureIDCounter.increment();
#endif

#if DUMP_STRUCTURE_ID_STATISTICS
    liveStructureIDSet.add(this);
#endif
}

StructureID::~StructureID()
{
    if (m_previous) {
        if (m_previous->m_usingSingleTransitionSlot) {
            m_previous->m_transitions.singleTransition = 0;
        } else {
            ASSERT(m_previous->m_transitions.table->contains(make_pair(m_nameInPrevious, m_attributesInPrevious)));
            m_previous->m_transitions.table->remove(make_pair(m_nameInPrevious, m_attributesInPrevious));
        }
    }

    if (m_cachedPropertyNameArrayData)
        m_cachedPropertyNameArrayData->setCachedStructureID(0);

    if (!m_usingSingleTransitionSlot)
        delete m_transitions.table;

    if (m_propertyTable) {
        unsigned entryCount = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount;
        for (unsigned i = 1; i <= entryCount; i++) {
            if (UString::Rep* key = m_propertyTable->entries()[i].key)
                key->deref();
        }
        fastFree(m_propertyTable);
    }

#ifndef NDEBUG
#if ENABLE(JSC_MULTIPLE_THREADS)
    MutexLocker protect(ignoreSetMutex);
#endif
    HashSet<StructureID*>::iterator it = ignoreSet.find(this);
    if (it != ignoreSet.end())
        ignoreSet.remove(it);
    else
        structureIDCounter.decrement();
#endif

#if DUMP_STRUCTURE_ID_STATISTICS
    liveStructureIDSet.remove(this);
#endif
}

void StructureID::startIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = true;
#endif
}

void StructureID::stopIgnoringLeaks()
{
#ifndef NDEBUG
    shouldIgnoreLeaks = false;
#endif
}

void StructureID::getEnumerablePropertyNames(ExecState* exec, PropertyNameArray& propertyNames, JSObject* baseObject)
{
    bool shouldCache = propertyNames.cacheable() && !(propertyNames.size() || m_isDictionary);

    if (shouldCache) {
        if (m_cachedPropertyNameArrayData) {
            if (structureIDChainsAreEqual(m_cachedPropertyNameArrayData->cachedPrototypeChain(), cachedPrototypeChain())) {
                propertyNames.setData(m_cachedPropertyNameArrayData);
                return;
            }
        }
        propertyNames.setCacheable(false);
    }

    getEnumerablePropertyNamesInternal(propertyNames);

    // Add properties from the static hashtables of properties
    for (const ClassInfo* info = baseObject->classInfo(); info; info = info->parentClass) {
        const HashTable* table = info->propHashTable(exec);
        if (!table)
            continue;
        table->initializeIfNeeded(exec);
        ASSERT(table->table);
        int hashSizeMask = table->hashSizeMask;
        const HashEntry* entry = table->table;
        for (int i = 0; i <= hashSizeMask; ++i, ++entry) {
            if (entry->key() && !(entry->attributes() & DontEnum))
                propertyNames.add(entry->key());
        }
    }

    if (m_prototype->isObject())
        asObject(m_prototype)->getPropertyNames(exec, propertyNames);

    if (shouldCache) {
        if (m_cachedPropertyNameArrayData)
            m_cachedPropertyNameArrayData->setCachedStructureID(0);

        m_cachedPropertyNameArrayData = propertyNames.data();

        StructureIDChain* chain = cachedPrototypeChain();
        if (!chain)
            chain = createCachedPrototypeChain();
        m_cachedPropertyNameArrayData->setCachedPrototypeChain(chain);
        m_cachedPropertyNameArrayData->setCachedStructureID(this);
    }
}

void StructureID::clearEnumerationCache()
{
    if (m_cachedPropertyNameArrayData)
        m_cachedPropertyNameArrayData->setCachedStructureID(0);
    m_cachedPropertyNameArrayData.clear();
}

void StructureID::growPropertyStorageCapacity()
{
    if (m_propertyStorageCapacity == JSObject::inlineStorageCapacity)
        m_propertyStorageCapacity = JSObject::nonInlineBaseStorageCapacity;
    else
        m_propertyStorageCapacity *= 2;
}

PassRefPtr<StructureID> StructureID::addPropertyTransition(StructureID* structureID, const Identifier& propertyName, unsigned attributes, size_t& offset)
{
    ASSERT(!structureID->m_isDictionary);
    ASSERT(structureID->typeInfo().type() == ObjectType);

    if (structureID->m_usingSingleTransitionSlot) {
        StructureID* existingTransition = structureID->m_transitions.singleTransition;
        if (existingTransition && existingTransition->m_nameInPrevious == propertyName.ustring().rep() && existingTransition->m_attributesInPrevious == attributes) {
            offset = structureID->m_transitions.singleTransition->cachedTransistionOffset();
            ASSERT(offset != WTF::notFound);
            return existingTransition;
        }
    } else {
        if (StructureID* existingTransition = structureID->m_transitions.table->get(make_pair(propertyName.ustring().rep(), attributes))) {
            offset = existingTransition->cachedTransistionOffset();
            ASSERT(offset != WTF::notFound);
            return existingTransition;
        }        
    }

    if (structureID->m_transitionCount > s_maxTransitionLength) {
        RefPtr<StructureID> transition = toDictionaryTransition(structureID);
        offset = transition->put(propertyName, attributes);
        if (transition->propertyStorageSize() > transition->propertyStorageCapacity())
            transition->growPropertyStorageCapacity();
        return transition.release();
    }

    RefPtr<StructureID> transition = create(structureID->m_prototype, structureID->typeInfo());
    transition->m_cachedPrototypeChain = structureID->m_cachedPrototypeChain;
    transition->m_previous = structureID;
    transition->m_nameInPrevious = propertyName.ustring().rep();
    transition->m_attributesInPrevious = attributes;
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyTable = structureID->copyPropertyTable();
    transition->m_deletedOffsets = structureID->m_deletedOffsets; 
    transition->m_propertyStorageCapacity = structureID->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structureID->m_hasGetterSetterProperties;

    offset = transition->put(propertyName, attributes);
    if (transition->propertyStorageSize() > transition->propertyStorageCapacity())
        transition->growPropertyStorageCapacity();

    transition->setCachedTransistionOffset(offset);

    if (structureID->m_usingSingleTransitionSlot) {
        if (!structureID->m_transitions.singleTransition) {
            structureID->m_transitions.singleTransition = transition.get();
            return transition.release();
        }

        StructureID* existingTransition = structureID->m_transitions.singleTransition;
        structureID->m_usingSingleTransitionSlot = false;
        StructureIDTransitionTable* transitionTable = new StructureIDTransitionTable;
        structureID->m_transitions.table = transitionTable;
        transitionTable->add(make_pair(existingTransition->m_nameInPrevious, existingTransition->m_attributesInPrevious), existingTransition);
    }
    structureID->m_transitions.table->add(make_pair(propertyName.ustring().rep(), attributes), transition.get());
    return transition.release();
}

PassRefPtr<StructureID> StructureID::removePropertyTransition(StructureID* structureID, const Identifier& propertyName, size_t& offset)
{
    ASSERT(!structureID->m_isDictionary);

    RefPtr<StructureID> transition = create(structureID->m_prototype, structureID->typeInfo());
    transition->m_isDictionary = true;
    transition->m_propertyTable = structureID->copyPropertyTable();
    transition->m_deletedOffsets = structureID->m_deletedOffsets;
    transition->m_propertyStorageCapacity = structureID->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structureID->m_hasGetterSetterProperties;

    offset = transition->remove(propertyName);

    return transition.release();
}

PassRefPtr<StructureID> StructureID::toDictionaryTransition(StructureID* structureID)
{
    ASSERT(!structureID->m_isDictionary);

    RefPtr<StructureID> transition = create(structureID->m_prototype, structureID->typeInfo());
    transition->m_isDictionary = true;
    transition->m_propertyTable = structureID->copyPropertyTable();
    transition->m_deletedOffsets = structureID->m_deletedOffsets;
    transition->m_propertyStorageCapacity = structureID->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structureID->m_hasGetterSetterProperties;
    return transition.release();
}

PassRefPtr<StructureID> StructureID::fromDictionaryTransition(StructureID* structureID)
{
    ASSERT(structureID->m_isDictionary);

    // Since dictionary StructureIDs are not shared, and no opcodes specialize
    // for them, we don't need to allocate a new StructureID when transitioning
    // to non-dictionary status.
    structureID->m_isDictionary = false;
    return structureID;
}

PassRefPtr<StructureID> StructureID::changePrototypeTransition(StructureID* structureID, JSValue* prototype)
{
    RefPtr<StructureID> transition = create(prototype, structureID->typeInfo());
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyTable = structureID->copyPropertyTable();
    transition->m_deletedOffsets = structureID->m_deletedOffsets;
    transition->m_propertyStorageCapacity = structureID->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = structureID->m_hasGetterSetterProperties;
    return transition.release();
}

PassRefPtr<StructureID> StructureID::getterSetterTransition(StructureID* structureID)
{
    RefPtr<StructureID> transition = create(structureID->storedPrototype(), structureID->typeInfo());
    transition->m_transitionCount = structureID->m_transitionCount + 1;
    transition->m_propertyTable = structureID->copyPropertyTable();
    transition->m_deletedOffsets = structureID->m_deletedOffsets;
    transition->m_propertyStorageCapacity = structureID->m_propertyStorageCapacity;
    transition->m_hasGetterSetterProperties = transition->m_hasGetterSetterProperties;
    return transition.release();
}

size_t StructureID::addPropertyWithoutTransition(const Identifier& propertyName, unsigned attributes)
{
    size_t offset = put(propertyName, attributes);
    if (propertyStorageSize() > propertyStorageCapacity())
        growPropertyStorageCapacity();
    clearEnumerationCache();
    return offset;
}

size_t StructureID::removePropertyWithoutTransition(const Identifier& propertyName)
{
    size_t offset = remove(propertyName);
    clearEnumerationCache();
    return offset;
}

StructureIDChain* StructureID::createCachedPrototypeChain()
{
    ASSERT(typeInfo().type() == ObjectType);
    ASSERT(!m_cachedPrototypeChain);

    JSValue* prototype = storedPrototype();
    if (JSImmediate::isImmediate(prototype))
        return 0;

    RefPtr<StructureIDChain> chain = StructureIDChain::create(asObject(prototype)->structureID());
    setCachedPrototypeChain(chain.release());
    return cachedPrototypeChain();
}

#if DUMP_PROPERTYMAP_STATS

static int numProbes;
static int numCollisions;
static int numRehashes;
static int numRemoves;

struct PropertyMapStatisticsExitLogger {
    ~PropertyMapStatisticsExitLogger();
};

static PropertyMapStatisticsExitLogger logger;

PropertyMapStatisticsExitLogger::~PropertyMapStatisticsExitLogger()
{
    printf("\nJSC::PropertyMap statistics\n\n");
    printf("%d probes\n", numProbes);
    printf("%d collisions (%.1f%%)\n", numCollisions, 100.0 * numCollisions / numProbes);
    printf("%d rehashes\n", numRehashes);
    printf("%d removes\n", numRemoves);
}

#endif

static const unsigned deletedSentinelIndex = 1;

#if !DO_PROPERTYMAP_CONSTENCY_CHECK

inline void StructureID::checkConsistency()
{
}   

#endif

PropertyMapHashTable* StructureID::copyPropertyTable()
{
    if (!m_propertyTable)
        return 0;

    size_t tableSize = PropertyMapHashTable::allocationSize(m_propertyTable->size);
    PropertyMapHashTable* newTable = static_cast<PropertyMapHashTable*>(fastMalloc(tableSize));
    memcpy(newTable, m_propertyTable, tableSize);

    unsigned entryCount = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; ++i) {
        if (UString::Rep* key = newTable->entries()[i].key)
            key->ref();
    }

    return newTable;
}

size_t StructureID::get(const Identifier& propertyName, unsigned& attributes) const
{
    ASSERT(!propertyName.isNull());

    if (!m_propertyTable)
        return WTF::notFound;

    UString::Rep* rep = propertyName._ustring.rep();

    unsigned i = rep->computedHash();

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    unsigned entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
    if (entryIndex == emptyEntryIndex)
        return WTF::notFound;

    if (rep == m_propertyTable->entries()[entryIndex - 1].key) {
        attributes = m_propertyTable->entries()[entryIndex - 1].attributes;
        return m_propertyTable->entries()[entryIndex - 1].offset;
    }

#if DUMP_PROPERTYMAP_STATS
    ++numCollisions;
#endif

    unsigned k = 1 | doubleHash(rep->computedHash());

    while (1) {
        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif

        entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return WTF::notFound;

        if (rep == m_propertyTable->entries()[entryIndex - 1].key) {
            attributes = m_propertyTable->entries()[entryIndex - 1].attributes;
            return m_propertyTable->entries()[entryIndex - 1].offset;
        }
    }
}

size_t StructureID::put(const Identifier& propertyName, unsigned attributes)
{
    ASSERT(!propertyName.isNull());
    ASSERT(get(propertyName) == WTF::notFound);

    checkConsistency();

    UString::Rep* rep = propertyName._ustring.rep();

    if (!m_propertyTable)
        createPropertyMapHashTable();

    // FIXME: Consider a fast case for tables with no deleted sentinels.

    unsigned i = rep->computedHash();
    unsigned k = 0;
    bool foundDeletedElement = false;
    unsigned deletedElementIndex = 0; // initialize to make the compiler happy

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    while (1) {
        unsigned entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            break;

        if (entryIndex == deletedSentinelIndex) {
            // If we find a deleted-element sentinel, remember it for use later.
            if (!foundDeletedElement) {
                foundDeletedElement = true;
                deletedElementIndex = i;
            }
        }

        if (k == 0) {
            k = 1 | doubleHash(rep->computedHash());
#if DUMP_PROPERTYMAP_STATS
            ++numCollisions;
#endif
        }

        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif
    }

    // Figure out which entry to use.
    unsigned entryIndex = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount + 2;
    if (foundDeletedElement) {
        i = deletedElementIndex;
        --m_propertyTable->deletedSentinelCount;

        // Since we're not making the table bigger, we can't use the entry one past
        // the end that we were planning on using, so search backwards for the empty
        // slot that we can use. We know it will be there because we did at least one
        // deletion in the past that left an entry empty.
        while (m_propertyTable->entries()[--entryIndex - 1].key) { }
    }

    // Create a new hash table entry.
    m_propertyTable->entryIndices[i & m_propertyTable->sizeMask] = entryIndex;

    // Create a new hash table entry.
    rep->ref();
    m_propertyTable->entries()[entryIndex - 1].key = rep;
    m_propertyTable->entries()[entryIndex - 1].attributes = attributes;
    m_propertyTable->entries()[entryIndex - 1].index = ++m_propertyTable->lastIndexUsed;

    unsigned newOffset;
    if (!m_deletedOffsets.isEmpty()) {
        newOffset = m_deletedOffsets.last();
        m_deletedOffsets.removeLast();
    } else
        newOffset = m_propertyTable->keyCount;
    m_propertyTable->entries()[entryIndex - 1].offset = newOffset;

    ++m_propertyTable->keyCount;

    if ((m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount) * 2 >= m_propertyTable->size)
        expandPropertyMapHashTable();

    checkConsistency();
    return newOffset;
}

size_t StructureID::remove(const Identifier& propertyName)
{
    ASSERT(!propertyName.isNull());

    checkConsistency();

    UString::Rep* rep = propertyName._ustring.rep();

    if (!m_propertyTable)
        return WTF::notFound;

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
    ++numRemoves;
#endif

    // Find the thing to remove.
    unsigned i = rep->computedHash();
    unsigned k = 0;
    unsigned entryIndex;
    UString::Rep* key = 0;
    while (1) {
        entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            return WTF::notFound;

        key = m_propertyTable->entries()[entryIndex - 1].key;
        if (rep == key)
            break;

        if (k == 0) {
            k = 1 | doubleHash(rep->computedHash());
#if DUMP_PROPERTYMAP_STATS
            ++numCollisions;
#endif
        }

        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif
    }

    // Replace this one element with the deleted sentinel. Also clear out
    // the entry so we can iterate all the entries as needed.
    m_propertyTable->entryIndices[i & m_propertyTable->sizeMask] = deletedSentinelIndex;

    size_t offset = m_propertyTable->entries()[entryIndex - 1].offset;

    key->deref();
    m_propertyTable->entries()[entryIndex - 1].key = 0;
    m_propertyTable->entries()[entryIndex - 1].attributes = 0;
    m_propertyTable->entries()[entryIndex - 1].offset = 0;
    m_deletedOffsets.append(offset);

    ASSERT(m_propertyTable->keyCount >= 1);
    --m_propertyTable->keyCount;
    ++m_propertyTable->deletedSentinelCount;

    if (m_propertyTable->deletedSentinelCount * 4 >= m_propertyTable->size)
        rehashPropertyMapHashTable();

    checkConsistency();
    return offset;
}

void StructureID::insertIntoPropertyMapHashTable(const PropertyMapEntry& entry)
{
    ASSERT(m_propertyTable);

    unsigned i = entry.key->computedHash();
    unsigned k = 0;

#if DUMP_PROPERTYMAP_STATS
    ++numProbes;
#endif

    while (1) {
        unsigned entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
        if (entryIndex == emptyEntryIndex)
            break;

        if (k == 0) {
            k = 1 | doubleHash(entry.key->computedHash());
#if DUMP_PROPERTYMAP_STATS
            ++numCollisions;
#endif
        }

        i += k;

#if DUMP_PROPERTYMAP_STATS
        ++numRehashes;
#endif
    }

    unsigned entryIndex = m_propertyTable->keyCount + 2;
    m_propertyTable->entryIndices[i & m_propertyTable->sizeMask] = entryIndex;
    m_propertyTable->entries()[entryIndex - 1] = entry;

    ++m_propertyTable->keyCount;
}

void StructureID::expandPropertyMapHashTable()
{
    ASSERT(m_propertyTable);
    rehashPropertyMapHashTable(m_propertyTable->size * 2);
}

void StructureID::createPropertyMapHashTable()
{
    const unsigned newTableSize = 16;

    ASSERT(!m_propertyTable);

    checkConsistency();

    m_propertyTable = static_cast<PropertyMapHashTable*>(fastZeroedMalloc(PropertyMapHashTable::allocationSize(newTableSize)));
    m_propertyTable->size = newTableSize;
    m_propertyTable->sizeMask = newTableSize - 1;

    checkConsistency();
}

void StructureID::rehashPropertyMapHashTable()
{
    ASSERT(m_propertyTable);
    ASSERT(m_propertyTable->size);
    rehashPropertyMapHashTable(m_propertyTable->size);
}

void StructureID::rehashPropertyMapHashTable(unsigned newTableSize)
{
    ASSERT(m_propertyTable);

    checkConsistency();

    PropertyMapHashTable* oldTable = m_propertyTable;

    m_propertyTable = static_cast<PropertyMapHashTable*>(fastZeroedMalloc(PropertyMapHashTable::allocationSize(newTableSize)));
    m_propertyTable->size = newTableSize;
    m_propertyTable->sizeMask = newTableSize - 1;

    unsigned lastIndexUsed = 0;
    unsigned entryCount = oldTable->keyCount + oldTable->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; ++i) {
        if (oldTable->entries()[i].key) {
            lastIndexUsed = max(oldTable->entries()[i].index, lastIndexUsed);
            insertIntoPropertyMapHashTable(oldTable->entries()[i]);
        }
    }
    m_propertyTable->lastIndexUsed = lastIndexUsed;

    fastFree(oldTable);

    checkConsistency();
}

static int comparePropertyMapEntryIndices(const void* a, const void* b)
{
    unsigned ia = static_cast<PropertyMapEntry* const*>(a)[0]->index;
    unsigned ib = static_cast<PropertyMapEntry* const*>(b)[0]->index;
    if (ia < ib)
        return -1;
    if (ia > ib)
        return +1;
    return 0;
}

void StructureID::getEnumerablePropertyNamesInternal(PropertyNameArray& propertyNames) const
{
    if (!m_propertyTable)
        return;

    if (m_propertyTable->keyCount < tinyMapThreshold) {
        PropertyMapEntry* a[tinyMapThreshold];
        int i = 0;
        unsigned entryCount = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount;
        for (unsigned k = 1; k <= entryCount; k++) {
            if (m_propertyTable->entries()[k].key && !(m_propertyTable->entries()[k].attributes & DontEnum)) {
                PropertyMapEntry* value = &m_propertyTable->entries()[k];
                int j;
                for (j = i - 1; j >= 0 && a[j]->index > value->index; --j)
                    a[j + 1] = a[j];
                a[j + 1] = value;
                ++i;
            }
        }
        if (!propertyNames.size()) {
            for (int k = 0; k < i; ++k)
                propertyNames.addKnownUnique(a[k]->key);
        } else {
            for (int k = 0; k < i; ++k)
                propertyNames.add(a[k]->key);
        }

        return;
    }

    // Allocate a buffer to use to sort the keys.
    Vector<PropertyMapEntry*, smallMapThreshold> sortedEnumerables(m_propertyTable->keyCount);

    // Get pointers to the enumerable entries in the buffer.
    PropertyMapEntry** p = sortedEnumerables.data();
    unsigned entryCount = m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount;
    for (unsigned i = 1; i <= entryCount; i++) {
        if (m_propertyTable->entries()[i].key && !(m_propertyTable->entries()[i].attributes & DontEnum))
            *p++ = &m_propertyTable->entries()[i];
    }

    size_t enumerableCount = p - sortedEnumerables.data();
    // Sort the entries by index.
    qsort(sortedEnumerables.data(), enumerableCount, sizeof(PropertyMapEntry*), comparePropertyMapEntryIndices);
    sortedEnumerables.resize(enumerableCount);

    // Put the keys of the sorted entries into the list.
    if (!propertyNames.size()) {
        for (size_t i = 0; i < sortedEnumerables.size(); ++i)
            propertyNames.addKnownUnique(sortedEnumerables[i]->key);
    } else {
        for (size_t i = 0; i < sortedEnumerables.size(); ++i)
            propertyNames.add(sortedEnumerables[i]->key);
    }
}

#if DO_PROPERTYMAP_CONSTENCY_CHECK

void StructureID::checkConsistency()
{
    if (!m_propertyTable)
        return;

    ASSERT(m_propertyTable->size >= 16);
    ASSERT(m_propertyTable->sizeMask);
    ASSERT(m_propertyTable->size == m_propertyTable->sizeMask + 1);
    ASSERT(!(m_propertyTable->size & m_propertyTable->sizeMask));

    ASSERT(m_propertyTable->keyCount <= m_propertyTable->size / 2);
    ASSERT(m_propertyTable->deletedSentinelCount <= m_propertyTable->size / 4);

    ASSERT(m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount <= m_propertyTable->size / 2);

    unsigned indexCount = 0;
    unsigned deletedIndexCount = 0;
    for (unsigned a = 0; a != m_propertyTable->size; ++a) {
        unsigned entryIndex = m_propertyTable->entryIndices[a];
        if (entryIndex == emptyEntryIndex)
            continue;
        if (entryIndex == deletedSentinelIndex) {
            ++deletedIndexCount;
            continue;
        }
        ASSERT(entryIndex > deletedSentinelIndex);
        ASSERT(entryIndex - 1 <= m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount);
        ++indexCount;

        for (unsigned b = a + 1; b != m_propertyTable->size; ++b)
            ASSERT(m_propertyTable->entryIndices[b] != entryIndex);
    }
    ASSERT(indexCount == m_propertyTable->keyCount);
    ASSERT(deletedIndexCount == m_propertyTable->deletedSentinelCount);

    ASSERT(m_propertyTable->entries()[0].key == 0);

    unsigned nonEmptyEntryCount = 0;
    for (unsigned c = 1; c <= m_propertyTable->keyCount + m_propertyTable->deletedSentinelCount; ++c) {
        UString::Rep* rep = m_propertyTable->entries()[c].key;
        if (!rep)
            continue;
        ++nonEmptyEntryCount;
        unsigned i = rep->computedHash();
        unsigned k = 0;
        unsigned entryIndex;
        while (1) {
            entryIndex = m_propertyTable->entryIndices[i & m_propertyTable->sizeMask];
            ASSERT(entryIndex != emptyEntryIndex);
            if (rep == m_propertyTable->entries()[entryIndex - 1].key)
                break;
            if (k == 0)
                k = 1 | doubleHash(rep->computedHash());
            i += k;
        }
        ASSERT(entryIndex == c + 1);
    }

    ASSERT(nonEmptyEntryCount == m_propertyTable->keyCount);
}

#endif // DO_PROPERTYMAP_CONSTENCY_CHECK

} // namespace JSC
