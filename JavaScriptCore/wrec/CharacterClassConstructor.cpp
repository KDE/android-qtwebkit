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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CharacterClassConstructor.h"

#if ENABLE(WREC)

#include "pcre_internal.h"
#include <wtf/ASCIICType.h>

using namespace WTF;

namespace JSC {

static const UChar asciiNewlines[2] = { '\n', '\r' };
static const UChar unicodeNewlines[2] = { 0x2028, 0x2029 };
CharacterClass& getCharacterClassNewline() {
    static CharacterClass charClass = {
        asciiNewlines, 2,
        0, 0,
        unicodeNewlines, 2,
        0, 0,
    };
    
    return charClass;
}

static const CharacterClassRange asciiDigitsRange[1] = { { '0', '9' } };
CharacterClass& getCharacterClassDigits() {
    static CharacterClass charClass = {
        0, 0,
        asciiDigitsRange, 1,
        0, 0,
        0, 0,
    };
    
    return charClass;
}

static const UChar asciiSpaces[1] = { ' ' };
static const CharacterClassRange asciiSpacesRange[1] = { { '\t', '\r' } };
static const UChar unicodeSpaces[8] = { 0x00a0, 0x1680, 0x180e, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000 };
static const CharacterClassRange unicodeSpacesRange[1] = { { 0x2000, 0x200a } };
CharacterClass& getCharacterClassSpaces() {
    static CharacterClass charClass = {
        asciiSpaces, 1,
        asciiSpacesRange, 1,
        unicodeSpaces, 8,
        unicodeSpacesRange, 1,
    };
    
    return charClass;
}

static const UChar asciiWordchar[1] = { '_' };
static const CharacterClassRange asciiWordcharRange[3] = { { '0', '9' }, { 'A', 'Z' }, { 'a', 'z' } };
CharacterClass& getCharacterClassWordchar() {
    static CharacterClass charClass = {
        asciiWordchar, 1,
        asciiWordcharRange, 3,
        0, 0,
        0, 0,
    };
    
    return charClass;
}

static const CharacterClassRange asciiNondigitsRange[2] = { { 0, '0' - 1 }, { '9' + 1, 0x7f } };
static const CharacterClassRange unicodeNondigitsRange[1] = { { 0x0080, 0xffff } };
CharacterClass& getCharacterClassNondigits() {
    static CharacterClass charClass = {
        0, 0,
        asciiNondigitsRange, 2,
        0, 0,
        unicodeNondigitsRange, 1,
    };
    
    return charClass;
}

static const CharacterClassRange asciiNonspacesRange[3] = { { 0, '\t' - 1 }, { '\r' + 1, ' ' - 1 }, { ' ' + 1, 0x7f } }; 
static const CharacterClassRange unicodeNonspacesRange[9] = {
    { 0x0080, 0x009f },
    { 0x00a1, 0x167f },
    { 0x1681, 0x180d },
    { 0x180f, 0x1fff },
    { 0x200b, 0x2027 },
    { 0x202a, 0x202e },
    { 0x2030, 0x205e },
    { 0x2060, 0x2fff },
    { 0x3001, 0xffff }
}; 
CharacterClass& getCharacterClassNonspaces() {
    static CharacterClass charClass = {
        0, 0,
        asciiNonspacesRange, 3,
        0, 0,
        unicodeNonspacesRange, 9,
    };
    
    return charClass;
}

static const UChar asciiNonwordchar[1] = { '`' };
static const CharacterClassRange asciiNonwordcharRange[4] = { { 0, '0' - 1 }, { '9' + 1, 'A' - 1 }, { 'Z' + 1, '_' - 1 }, { 'z' + 1, 0x7f } };
static const CharacterClassRange unicodeNonwordcharRange[1] = { { 0x0080, 0xffff } };
CharacterClass& getCharacterClassNonwordchar() {
    static CharacterClass charClass = {
        asciiNonwordchar, 1,
        asciiNonwordcharRange, 4,
        0, 0,
        unicodeNonwordcharRange, 1,
    };
    
    return charClass;
}

void CharacterClassConstructor::addSorted(Vector<UChar>& matches, UChar ch)
{
    unsigned pos = 0;
    unsigned range = matches.size();

    // binary chop, find position to insert char.
    while (range) {
        unsigned index = range >> 1;

        int val = matches[pos+index] - ch;
        if (!val)
            return;
        else if (val > 0)
            range = index;
        else {
            pos += (index+1);
            range -= (index+1);
        }
    }
    
    if (pos == matches.size())
        matches.append(ch);
    else
        matches.insert(pos, ch);
}

void CharacterClassConstructor::addSortedRange(Vector<CharacterClassRange>& ranges, UChar lo, UChar hi)
{
    unsigned end = ranges.size();
    
    // Simple linear scan - I doubt there are that many ranges anyway...
    // feel free to fix this with something faster (eg binary chop).
    for (unsigned i = 0; i < end; ++i) {
        // does the new range fall before the current position in the array
        if (hi < ranges[i].begin) {
            // optional optimization: concatenate appending ranges? - may not be worthwhile.
            if (hi == (ranges[i].begin - 1)) {
                ranges[i].begin = lo;
                return;
            }
            CharacterClassRange r = {lo, hi};
            ranges.insert(i, r);
            return;
        }
        // Okay, since we didn't hit the last case, the end of the new range is definitely at or after the begining
        // If the new range start at or before the end of the last range, then the overlap (if it starts one after the
        // end of the last range they concatenate, which is just as good.
        if (lo <= (ranges[i].end + 1)) {
            // found an intersect! we'll replace this entry in the array.
            ranges[i].begin = std::min(ranges[i].begin, lo);
            ranges[i].end = std::max(ranges[i].end, hi);

            // now check if the new range can subsume any subsequent ranges.
            unsigned next = i+1;
            // each iteration of the loop we will either remove something from the list, or break the loop.
            while (next < ranges.size()) {
                if (ranges[next].begin <= (ranges[i].end + 1)) {
                    // the next entry now overlaps / concatenates this one.
                    ranges[i].end = std::max(ranges[i].end, ranges[next].end);
                    ranges.remove(next);
                } else
                    break;
            }
            
            return;
        }
    }

    // Range comes after all existing ranges.
    CharacterClassRange r = {lo, hi};
    ranges.append(r);
}

void CharacterClassConstructor::put(UChar ch)
{
    if (m_charBuffer != -1) {
        if (m_isPendingDash) {
            UChar lo = m_charBuffer;
            UChar hi = ch;
            m_charBuffer = -1;
            m_isPendingDash = false;
            
            if (lo > hi)
                m_isUpsideDown = true;
            
            if (lo <= 0x7f) {
                char asciiLo = lo;
                char asciiHi = std::min(hi, (UChar)0x7f);
                addSortedRange(m_ranges, lo, asciiHi);
                
                if (m_isCaseInsensitive) {
                    if ((asciiLo <= 'Z') && (asciiHi >= 'A'))
                        addSortedRange(m_ranges, std::max(asciiLo, 'A')+('a'-'A'), std::min(asciiHi, 'Z')+('a'-'A'));
                    if ((asciiLo <= 'z') && (asciiHi >= 'a'))
                        addSortedRange(m_ranges, std::max(asciiLo, 'a')+('A'-'a'), std::min(asciiHi, 'z')+('A'-'a'));
                }
            }
            if (hi >= 0x80) {
                UChar unicodeCurr = std::max(lo, (UChar)0x80);
                addSortedRange(m_rangesUnicode, unicodeCurr, hi);
                
                if (m_isCaseInsensitive) {
                    // we're going to scan along, updating the start of the range
                    while (unicodeCurr <= hi) {
                        // Spin forwards over any characters that don't have two cases.
                        for (; kjs_pcre_ucp_othercase(unicodeCurr) == -1; ++unicodeCurr) {
                            // if this was the last character in the range, we're done.
                            if (unicodeCurr == hi)
                                return;
                        }
                        // if we fall through to here, unicodeCurr <= hi & has another case. Get the other case.
                        UChar rangeStart = unicodeCurr;
                        UChar otherCurr = kjs_pcre_ucp_othercase(unicodeCurr);
                        
                        // If unicodeCurr is not yet hi, check the next char in the range.  If it also has another case,
                        // and if it's other case value is one greater then the othercase value for the current last
                        // character included in the range, we can include next into the range.
                        while ((unicodeCurr < hi) && (kjs_pcre_ucp_othercase(unicodeCurr + 1) == (otherCurr + 1))) {
                            // increment unicodeCurr; it points to the end of the range.
                            // increment otherCurr, due to the check above other for next must be 1 greater than the currrent other value.
                            ++unicodeCurr;
                            ++otherCurr;
                        }
                        
                        // otherChar is the last in the range of other case chars, calculate offset to get back to the start.
                        addSortedRange(m_rangesUnicode, otherCurr-(unicodeCurr-rangeStart), otherCurr);
                        
                        // unicodeCurr has been added, move on to the next char.
                        ++unicodeCurr;
                    }
                }
            }
        } else if (ch == '-') {
            m_isPendingDash = true;
        } else {
            flush();
            m_charBuffer = ch;
        }
    } else
        m_charBuffer = ch;
}

// When a character is added to the set we do not immediately add it to the arrays, in case it is actually defining a range.
// When we have determined the character is not used in specifing a range it is added, in a sorted fashion, to the appropriate
// array (either ascii or unicode).
// If the pattern is case insensitive we add entries for both cases.
void CharacterClassConstructor::flush()
{
    if (m_charBuffer != -1) {
        if (m_charBuffer <= 0x7f) {
            if (m_isCaseInsensitive && isASCIILower(m_charBuffer))
                addSorted(m_matches, toASCIIUpper(m_charBuffer));
            addSorted(m_matches, m_charBuffer);
            if (m_isCaseInsensitive && isASCIIUpper(m_charBuffer))
                addSorted(m_matches, toASCIILower(m_charBuffer));
        } else {
            addSorted(m_matchesUnicode, m_charBuffer);
            if (m_isCaseInsensitive) {
                int other = kjs_pcre_ucp_othercase(m_charBuffer);
                if (other != -1)
                    addSorted(m_matchesUnicode, other);
            }
        }
        m_charBuffer = -1;
    }
    
    if (m_isPendingDash) {
        addSorted(m_matches, '-');
    }
}

void CharacterClassConstructor::append(CharacterClass& other)
{
    // [x-\s] will add, 'x', '-', and all unicode spaces to new class (same as [x\s-]).
    // Need to check the spec, really, but think this matches PCRE behaviour.
    flush();
    
    if (other.numMatches) {
        for (size_t i = 0; i < other.numMatches; ++i)
            addSorted(m_matches, other.matches[i]);
    }
    if (other.numRanges) {
        for (size_t i = 0; i < other.numRanges; ++i)
            addSortedRange(m_ranges, other.ranges[i].begin, other.ranges[i].end);
    }
    if (other.numMatchesUnicode) {
        for (size_t i = 0; i < other.numMatchesUnicode; ++i)
            addSorted(m_matchesUnicode, other.matchesUnicode[i]);
    }
    if (other.numRangesUnicode) {
        for (size_t i = 0; i < other.numRangesUnicode; ++i)
            addSortedRange(m_rangesUnicode, other.rangesUnicode[i].begin, other.rangesUnicode[i].end);
    }
}

}

#endif // ENABLE(WREC)
