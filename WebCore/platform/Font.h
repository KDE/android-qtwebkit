/*
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef Font_h
#define Font_h

#include "FontDescription.h"
#include <wtf/HashMap.h>

#if PLATFORM(QT)
class QFont;
#endif

namespace WebCore {

class FloatPoint;
class FloatRect;
class FontData;
class FontFallbackList;
class FontPlatformData;
class GlyphBuffer;
class GlyphPageTreeNode;
class GraphicsContext;
class IntPoint;
class TextStyle;

struct GlyphData;

class TextRun {
public:
    TextRun(const UChar* c, int len)
    :m_characters(c), m_len(len), m_from(0), m_to(len)
    {}

    TextRun(const UChar* c, int len, int from, int to) // This constructor is only used in Mac-specific code.
    :m_characters(c), m_len(len), m_from(from), m_to(to)
    {}

    TextRun(const StringImpl* s, int offset = 0, int length = -1, int from = -1, int to = -1)
    :m_characters(s->characters() + offset), m_len(length == -1 ? s->length() - offset : length), m_from(adjustFrom(from)), m_to(adjustTo(to))
    {}

    const UChar operator[](int i) const { return m_characters[i]; }
    const UChar* data(int i) const { return &m_characters[i]; }

    int adjustFrom(int from) const { return from == -1 ? 0 : from; }
    int adjustTo(int to) const { return to == -1 ? m_len : to; }
    void makeComplete() { m_from = 0; m_to = m_len; }

    const UChar* characters() const { return m_characters; }
    int length() const { return m_len; }
    int from() const { return m_from; }
    int to() const { return m_to; }

private:
    const UChar* m_characters;
    int m_len;
    int m_from;
    int m_to;
};

class Font {
public:
    Font();
    Font(const FontDescription&, short letterSpacing, short wordSpacing);
    Font(const FontPlatformData&, bool isPrinting); // This constructor is only used if the platform wants to start with a native font.
    ~Font();
    
    Font(const Font&);
    Font& operator=(const Font&);

    bool operator==(const Font& other) const {
        // Our FontData doesn't have to be checked, since
        // checking the font description will be fine.
        return (m_fontDescription == other.m_fontDescription &&
                m_letterSpacing == other.m_letterSpacing &&
                m_wordSpacing == other.m_wordSpacing);
    }

    bool operator!=(const Font& other) const {
        return !(*this == other);
    }

    const FontDescription& fontDescription() const { return m_fontDescription; }

    int pixelSize() const { return fontDescription().computedPixelSize(); }
    float size() const { return fontDescription().computedSize(); }
    
    void update() const;

    void drawText(GraphicsContext*, const TextRun&, const TextStyle&, const FloatPoint&) const;

    int width(const TextRun&, const TextStyle&) const;
    int width(const TextRun&) const;
    float floatWidth(const TextRun&, const TextStyle&) const;
    float floatWidth(const TextRun&) const;
    
    int offsetForPosition(const TextRun&, const TextStyle&, int position, bool includePartialGlyphs) const;
    FloatRect selectionRectForText(const TextRun&, const TextStyle&, const IntPoint&, int h) const;

    bool isSmallCaps() const { return m_fontDescription.smallCaps(); }

    short wordSpacing() const { return m_wordSpacing; }
    short letterSpacing() const { return m_letterSpacing; }
    void setWordSpacing(short s) { m_wordSpacing = s; }
    void setLetterSpacing(short s) { m_letterSpacing = s; }

    bool isFixedPitch() const;
    bool isPrinterFont() const { return m_fontDescription.usePrinterFont(); }

    FontFamily& firstFamily() { return m_fontDescription.firstFamily(); }
    const FontFamily& family() const { return m_fontDescription.family(); }

    bool italic() const { return m_fontDescription.italic(); }
    unsigned weight() const { return m_fontDescription.weight(); }
    bool bold() const { return m_fontDescription.bold(); }

#if PLATFORM(QT)
    operator QFont() const;
#endif
    
    // Metrics that we query the FontFallbackList for.
    int ascent() const;
    int descent() const;
    int height() const { return ascent() + descent(); }
    int lineSpacing() const;
    float xHeight() const;

    const FontData* primaryFont() const;
    const FontData* fontDataAt(unsigned) const;
    const GlyphData& glyphDataForCharacter(UChar32, const UChar* cluster, unsigned clusterLength, bool mirror, bool attemptFontSubstitution) const;
    // Used for complex text, and does not utilize the glyph map cache.
    const FontData* fontDataForCharacters(const UChar*, int length) const;

private:
    // FIXME: This will eventually be cross-platform, but we want to keep Windows compiling for now.
    bool canUseGlyphCache(const TextRun&) const;
    void drawSimpleText(GraphicsContext*, const TextRun&, const TextStyle&, const FloatPoint&) const;
    void drawGlyphs(GraphicsContext*, const FontData*, const GlyphBuffer&, int from, int to, const FloatPoint&) const;
    void drawComplexText(GraphicsContext*, const TextRun&, const TextStyle&, const FloatPoint&) const;
    float floatWidthForSimpleText(const TextRun&, const TextStyle&, float* startX, GlyphBuffer*) const;
    float floatWidthForComplexText(const TextRun&, const TextStyle&) const;
    int offsetForPositionForSimpleText(const TextRun&, const TextStyle&, int position, bool includePartialGlyphs) const;
    int offsetForPositionForComplexText(const TextRun&, const TextStyle&, int position, bool includePartialGlyphs) const;
    FloatRect selectionRectForSimpleText(const TextRun&, const TextStyle&, const IntPoint&, int h) const;
    FloatRect selectionRectForComplexText(const TextRun&, const TextStyle&, const IntPoint&, int h) const;

    friend struct WidthIterator;
    
    // Useful for debugging the different font rendering code paths.
public:
    enum CodePath { Auto, Simple, Complex };
    static void setCodePath(CodePath);
    static CodePath codePath;
    
    static const uint8_t gRoundingHackCharacterTable[256];
    static bool treatAsSpace(UChar c) { return c == ' ' || c == '\t' || c == '\n' || c == 0x00A0; }
    static bool isRoundingHackCharacter(UChar32 c)
    {
        return (((c & ~0xFF) == 0 && gRoundingHackCharacterTable[c])); 
    }

private:
    FontDescription m_fontDescription;
    mutable RefPtr<FontFallbackList> m_fontList;
    mutable HashMap<int, GlyphPageTreeNode*> m_pages;
    mutable GlyphPageTreeNode* m_pageZero;
    short m_letterSpacing;
    short m_wordSpacing;
};

}

#endif
