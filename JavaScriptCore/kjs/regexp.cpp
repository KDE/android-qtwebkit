/*
 *  Copyright (C) 1999-2001, 2004 Harri Porten (porten@kde.org)
 *  Copyright (c) 2007, 2008 Apple Inc. All rights reserved.
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
#include "regexp.h"

#include "CTI.h"
#include "lexer.h"
#include <pcre/pcre.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wrec/WREC.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>

namespace JSC {

inline RegExp::RegExp(JSGlobalData* globalData, const UString& pattern)
    : m_pattern(pattern)
    , m_flagBits(0)
    , m_regExp(0)
    , m_constructionError(0)
    , m_numSubpatterns(0)
{
#if ENABLE(WREC)
    m_wrecFunction = CTI::compileRegExp(globalData->machine, pattern, &m_numSubpatterns, &m_constructionError);
    if (m_wrecFunction)
        return;
    // Fall through to non-WREC case.
#else
    UNUSED_PARAM(globalData);
#endif
    m_regExp = jsRegExpCompile(reinterpret_cast<const UChar*>(pattern.data()), pattern.size(),
        JSRegExpDoNotIgnoreCase, JSRegExpSingleLine, &m_numSubpatterns, &m_constructionError);
}

PassRefPtr<RegExp> RegExp::create(JSGlobalData* globalData, const UString& pattern)
{
    return adoptRef(new RegExp(globalData, pattern));
}

inline RegExp::RegExp(JSGlobalData* globalData, const UString& pattern, const UString& flags)
    : m_pattern(pattern)
    , m_flags(flags)
    , m_flagBits(0)
    , m_regExp(0)
    , m_constructionError(0)
    , m_numSubpatterns(0)
{
    // NOTE: The global flag is handled on a case-by-case basis by functions like
    // String::match and RegExpObject::match.
    if (flags.find('g') != -1)
        m_flagBits |= Global;

    // FIXME: Eliminate duplication by adding a way ask a JSRegExp what its flags are?
    JSRegExpIgnoreCaseOption ignoreCaseOption = JSRegExpDoNotIgnoreCase;
    if (flags.find('i') != -1) {
        m_flagBits |= IgnoreCase;
        ignoreCaseOption = JSRegExpIgnoreCase;
    }

    JSRegExpMultilineOption multilineOption = JSRegExpSingleLine;
    if (flags.find('m') != -1) {
        m_flagBits |= Multiline;
        multilineOption = JSRegExpMultiline;
    }

#if ENABLE(WREC)
    m_wrecFunction = CTI::compileRegExp(globalData->machine, pattern, &m_numSubpatterns, &m_constructionError, (m_flagBits & IgnoreCase), (m_flagBits & Multiline));
    if (m_wrecFunction)
        return;
    // Fall through to non-WREC case.
#else
    UNUSED_PARAM(globalData);
#endif
    m_regExp = jsRegExpCompile(reinterpret_cast<const UChar*>(pattern.data()), pattern.size(),
        ignoreCaseOption, multilineOption, &m_numSubpatterns, &m_constructionError);
}

PassRefPtr<RegExp> RegExp::create(JSGlobalData* globalData, const UString& pattern, const UString& flags)
{
    return adoptRef(new RegExp(globalData, pattern, flags));
}

RegExp::~RegExp()
{
    jsRegExpFree(m_regExp);
#if ENABLE(WREC)
    if (m_wrecFunction)
        WTF::fastFreeExecutable(m_wrecFunction);
#endif
}

int RegExp::match(const UString& s, int i, OwnArrayPtr<int>* ovector)
{
    if (i < 0)
        i = 0;
    if (ovector)
        ovector->clear();

    if (i > s.size() || s.isNull())
        return -1;

#if ENABLE(WREC)
    if (m_wrecFunction) {
        int offsetVectorSize = (m_numSubpatterns + 1) * 2;
        int* offsetVector = new int [offsetVectorSize];
        for (int j = 0; j < offsetVectorSize; ++j)
            offsetVector[j] = -1;

        OwnArrayPtr<int> nonReturnedOvector;
        if (!ovector)
            nonReturnedOvector.set(offsetVector);
        else
            ovector->set(offsetVector);

        int result = reinterpret_cast<WRECFunction>(m_wrecFunction)(s.data(), i, s.size(), offsetVector);

        if (result < 0) {
#ifndef NDEBUG
            // TODO: define up a symbol, rather than magic -1
            if (result != -1)
                fprintf(stderr, "jsRegExpExecute failed with result %d\n", result);
#endif
            if (ovector)
                ovector->clear();
        }
        return result;
    } else
#endif
    if (m_regExp) {
        // Set up the offset vector for the result.
        // First 2/3 used for result, the last third used by PCRE.
        int* offsetVector;
        int offsetVectorSize;
        int fixedSizeOffsetVector[3];
        if (!ovector) {
            offsetVectorSize = 3;
            offsetVector = fixedSizeOffsetVector;
        } else {
            offsetVectorSize = (m_numSubpatterns + 1) * 3;
            offsetVector = new int [offsetVectorSize];
            ovector->set(offsetVector);
        }

        int numMatches = jsRegExpExecute(m_regExp, reinterpret_cast<const UChar*>(s.data()), s.size(), i, offsetVector, offsetVectorSize);
    
        if (numMatches < 0) {
#ifndef NDEBUG
            if (numMatches != JSRegExpErrorNoMatch)
                fprintf(stderr, "jsRegExpExecute failed with result %d\n", numMatches);
#endif
            if (ovector)
                ovector->clear();
            return -1;
        }

        return offsetVector[0];
    }

    return -1;
}

} // namespace JSC
