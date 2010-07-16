/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2010 Zoltan Herczeg (zherczeg@inf.u-szeged.hu)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef Lexer_h
#define Lexer_h

#include "JSParser.h"
#include "Lookup.h"
#include "ParserArena.h"
#include "SourceCode.h"
#include <wtf/ASCIICType.h>
#include <wtf/AlwaysInline.h>
#include <wtf/SegmentedVector.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace JSC {

    class RegExp;

    class Lexer : public Noncopyable {
    public:
        // Character manipulation functions.
        static bool isWhiteSpace(int character);
        static bool isLineTerminator(int character);
        static unsigned char convertHex(int c1, int c2);
        static UChar convertUnicode(int c1, int c2, int c3, int c4);

        // Functions to set up parsing.
        void setCode(const SourceCode&, ParserArena&);
        void setIsReparsing() { m_isReparsing = true; }

        // Functions for the parser itself.
        enum LexType { IdentifyReservedWords, IgnoreReservedWords };
        JSTokenType lex(JSTokenData* lvalp, JSTokenInfo* llocp, LexType);
        int lineNumber() const { return m_lineNumber; }
        void setLastLineNumber(int lastLineNumber) { m_lastLineNumber = lastLineNumber; }
        int lastLineNumber() const { return m_lastLineNumber; }
        bool prevTerminator() const { return m_terminator; }
        SourceCode sourceCode(int openBrace, int closeBrace, int firstLine);
        bool scanRegExp(const Identifier*& pattern, const Identifier*& flags, UChar patternPrefix = 0);
        bool skipRegExp();

        // Functions for use after parsing.
        bool sawError() const { return m_error; }
        void clear();
        int currentOffset() { return m_code - m_codeStart; }
        void setOffset(int offset)
        {
            m_code = m_codeStart + offset;
            m_current = *m_code;
        }

    private:
        friend class JSGlobalData;

        Lexer(JSGlobalData*);
        ~Lexer();

        void record8(int);
        void record16(int);
        void record16(UChar);

        void copyCodeWithoutBOMs();

        ALWAYS_INLINE void shift();
        ALWAYS_INLINE int peek(int offset);
        int getUnicodeCharacter();
        void shiftLineTerminator();

        ALWAYS_INLINE const UChar* currentCharacter() const;
        ALWAYS_INLINE int currentOffset() const;

        ALWAYS_INLINE const Identifier* makeIdentifier(const UChar* characters, size_t length);

        ALWAYS_INLINE bool lastTokenWasRestrKeyword() const;

        ALWAYS_INLINE bool parseString(JSTokenData* lvalp);

        static const size_t initialReadBufferCapacity = 32;

        int m_lineNumber;
        int m_lastLineNumber;

        Vector<char> m_buffer8;
        Vector<UChar> m_buffer16;
        bool m_terminator;
        bool m_delimited; // encountered delimiter like "'" and "}" on last run
        int m_lastToken;

        const SourceCode* m_source;
        const UChar* m_code;
        const UChar* m_codeStart;
        const UChar* m_codeEnd;
        bool m_isReparsing;
        bool m_atLineStart;
        bool m_error;

        // current and following unicode characters (int to allow for -1 for end-of-file marker)
        int m_current;

        IdentifierArena* m_arena;

        JSGlobalData* m_globalData;

        const HashTable m_keywordTable;
    };

    inline bool Lexer::isWhiteSpace(int ch)
    {
        return isASCII(ch) ? (ch == ' ' || ch == '\t' || ch == 0xB || ch == 0xC) : (WTF::Unicode::isSeparatorSpace(ch) || ch == 0xFEFF);
    }

    inline bool Lexer::isLineTerminator(int ch)
    {
        return ch == '\r' || ch == '\n' || (ch & ~1) == 0x2028;
    }

    inline unsigned char Lexer::convertHex(int c1, int c2)
    {
        return (toASCIIHexValue(c1) << 4) | toASCIIHexValue(c2);
    }

    inline UChar Lexer::convertUnicode(int c1, int c2, int c3, int c4)
    {
        return (convertHex(c1, c2) << 8) | convertHex(c3, c4);
    }

} // namespace JSC

#endif // Lexer_h
