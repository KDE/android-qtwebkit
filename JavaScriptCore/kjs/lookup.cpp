/*
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "lookup.h"

namespace KJS {

void HashTable::createTable() const
{
    ASSERT(!table);
    HashEntry* entries = new HashEntry[hashSizeMask + 1];
    for (int i = 0; i <= hashSizeMask; ++i)
        entries[i].key = 0;
    for (int i = 0; values[i].key; ++i) {
        UString::Rep* identifier = Identifier::add(values[i].key).releaseRef();
        int hashIndex = identifier->computedHash() & hashSizeMask;
        ASSERT(!entries[hashIndex].key);
        entries[hashIndex].key = identifier;
        entries[hashIndex].integerValue = values[i].value;
        entries[hashIndex].attributes = values[i].attributes;
        entries[hashIndex].length = values[i].length;
    }
    table = entries;
}

}
