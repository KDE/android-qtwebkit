/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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

#include "config.h"
#include "Font.h"

#include "FloatRect.h"
#include "FontFallbackList.h"
#include "IntPoint.h"
#include "GlyphBuffer.h"
#include "TextStyle.h"
#include <unicode/uchar.h>
#include <unicode/umachine.h>
#include <unicode/unorm.h>
#include <wtf/MathExtras.h>

namespace WebCore {

// According to http://www.unicode.org/Public/UNIDATA/UCD.html#Canonical_Combining_Class_Values
const uint8_t hiraganaKatakanaVoicingMarksCombiningClass = 8;

const uint8_t Font::gRoundingHackCharacterTable[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*\t*/, 1 /*\n*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*-*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*?*/,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*no-break space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

struct WidthIterator {
    WidthIterator(const Font* font, const TextRun& run, const TextStyle& style, const FontData* substituteFontData = 0);

    void advance(int to, GlyphBuffer* glyphBuffer = 0);
    bool advanceOneCharacter(float& width, GlyphBuffer* glyphBuffer = 0);
    
    const Font* m_font;

    const TextRun& m_run;
    int m_end;

    const TextStyle& m_style;
    
    const FontData* m_substituteFontData;

    unsigned m_currentCharacter;
    float m_runWidthSoFar;
    float m_widthToStart;
    float m_padding;
    float m_padPerSpace;
    float m_finalRoundingWidth;
    
private:
    UChar32 normalizeVoicingMarks(int currentCharacter);
};

WidthIterator::WidthIterator(const Font* font, const TextRun& run, const TextStyle& style, const FontData* substituteFontData)
    : m_font(font)
    , m_run(run)
    , m_end(style.rtl() ? run.length() : run.to())
    , m_style(style)
    , m_substituteFontData(substituteFontData)
    , m_currentCharacter(run.from())
    , m_runWidthSoFar(0)
    , m_finalRoundingWidth(0)
{
    // If the padding is non-zero, count the number of spaces in the run
    // and divide that by the padding for per space addition.
    m_padding = m_style.padding();
    if (!m_padding)
        m_padPerSpace = 0;
    else {
        float numSpaces = 0;
        for (int i = run.from(); i < m_end; i++)
            if (Font::treatAsSpace(m_run[i]))
                numSpaces++;

        if (numSpaces == 0)
            m_padPerSpace = 0;
        else
            m_padPerSpace = ceilf(m_style.padding() / numSpaces);
    }
    
    // Calculate width up to starting position of the run.  This is
    // necessary to ensure that our rounding hacks are always consistently
    // applied.
    if (run.from() == 0)
        m_widthToStart = 0;
    else {
        TextRun completeRun(run);
        completeRun.makeComplete();
        WidthIterator startPositionIterator(font, completeRun, style, m_substituteFontData);
        startPositionIterator.advance(run.from());
        m_widthToStart = startPositionIterator.m_runWidthSoFar;
    }
}

void WidthIterator::advance(int offset, GlyphBuffer* glyphBuffer)
{
    if (offset > m_end)
        offset = m_end;

    int currentCharacter = m_currentCharacter;
    const UChar* cp = m_run.data(currentCharacter);

    bool rtl = m_style.rtl();
    bool needCharTransform = rtl || m_font->isSmallCaps();
    bool hasExtraSpacing = m_font->letterSpacing() || m_font->wordSpacing() || m_padding;

    float runWidthSoFar = m_runWidthSoFar;
    float lastRoundingWidth = m_finalRoundingWidth;
    
    const FontData* primaryFont = m_font->primaryFont();

    while (currentCharacter < offset) {
        UChar32 c = *cp;
        unsigned clusterLength = 1;
        if (c >= 0x3041) {
            if (c <= 0x30FE) {
                // Deal with Hiragana and Katakana voiced and semi-voiced syllables.
                // Normalize into composed form, and then look for glyph with base + combined mark.
                // Check above for character range to minimize performance impact.
                UChar32 normalized = normalizeVoicingMarks(currentCharacter);
                if (normalized) {
                    c = normalized;
                    clusterLength = 2;
                }
            } else if (U16_IS_SURROGATE(c)) {
                if (!U16_IS_SURROGATE_LEAD(c))
                    break;

                // Do we have a surrogate pair?  If so, determine the full Unicode (32 bit)
                // code point before glyph lookup.
                // Make sure we have another character and it's a low surrogate.
                if (currentCharacter + 1 >= m_run.length())
                    break;
                UChar low = cp[1];
                if (!U16_IS_TRAIL(low))
                    break;
                c = U16_GET_SUPPLEMENTARY(c, low);
                clusterLength = 2;
            }
        }

        const FontData* fontData = m_substituteFontData ? m_substituteFontData : primaryFont;

        if (needCharTransform) {
            if (rtl)
                c = u_charMirror(c);

            // If small-caps, convert lowercase to upper.
            if (m_font->isSmallCaps() && !u_isUUppercase(c)) {
                UChar32 upperC = u_toupper(c);
                if (upperC != c) {
                    c = upperC;
                    fontData = fontData->smallCapsFontData(m_font->fontDescription());
                }
            }
        }

        // FIXME: Should go through fallback list eventually when we rework the glyph map.
        const GlyphData& glyphData = fontData->glyphDataForCharacter(c);
        Glyph glyph = glyphData.glyph;
        fontData = glyphData.fontData;

        // Try to find a substitute font if this font didn't have a glyph for a character in the
        // string. If one isn't found we end up drawing and measuring the 0 glyph, usually a box.
        if (glyph == 0 && !m_substituteFontData && m_style.attemptFontSubstitution()) {
            const FontData* substituteFontData = m_font->fontDataForCharacters(cp, clusterLength);
            if (substituteFontData) {
                GlyphBuffer localGlyphBuffer;
                m_font->floatWidthForSimpleText(TextRun((UChar*)cp, clusterLength), TextStyle(0, 0, 0, m_style.rtl(), m_style.directionalOverride(), 
                                                                                              false, m_style.applyWordRounding()), 
                                                                                              substituteFontData, 0, &localGlyphBuffer);
                if (localGlyphBuffer.size() == 1) {
                    assert(substituteFontData == localGlyphBuffer.fontDataAt(0));
                    glyph = localGlyphBuffer.glyphAt(0);
                    fontData->setGlyphDataForCharacter(c, glyph, substituteFontData);
                    fontData = substituteFontData;
                }
            }
        }

        // Now that we have a glyph and font data, get its width.
        float width;
        if (c == '\t' && m_style.tabWidth())
            width = m_style.tabWidth() - fmodf(m_style.xPos() + runWidthSoFar, m_style.tabWidth());
        else {
            width = fontData->widthForGlyph(glyph);
            // We special case spaces in two ways when applying word rounding.
            // First, we round spaces to an adjusted width in all fonts.
            // Second, in fixed-pitch fonts we ensure that all characters that
            // match the width of the space character have the same width as the space character.
            if (width == fontData->m_spaceWidth && (fontData->m_treatAsFixedPitch || glyph == fontData->m_spaceGlyph) && m_style.applyWordRounding())
                width = fontData->m_adjustedSpaceWidth;
        }

        if (hasExtraSpacing) {
            // Account for letter-spacing.
            if (width && m_font->letterSpacing())
                width += m_font->letterSpacing();

            if (Font::treatAsSpace(c)) {
                // Account for padding. WebCore uses space padding to justify text.
                // We distribute the specified padding over the available spaces in the run.
                if (m_padding) {
                    // Use left over padding if not evenly divisible by number of spaces.
                    if (m_padding < m_padPerSpace) {
                        width += m_padding;
                        m_padding = 0;
                    } else {
                        width += m_padPerSpace;
                        m_padding -= m_padPerSpace;
                    }
                }

                // Account for word spacing.
                // We apply additional space between "words" by adding width to the space character.
                if (currentCharacter != 0 && !Font::treatAsSpace(cp[-1]) && m_font->wordSpacing())
                    width += m_font->wordSpacing();
            }
        }

        // Advance past the character we just dealt with.
        cp += clusterLength;
        currentCharacter += clusterLength;

        // Account for float/integer impedance mismatch between CG and KHTML. "Words" (characters 
        // followed by a character defined by isRoundingHackCharacter()) are always an integer width.
        // We adjust the width of the last character of a "word" to ensure an integer width.
        // If we move KHTML to floats we can remove this (and related) hacks.

        float oldWidth = width;

        // Force characters that are used to determine word boundaries for the rounding hack
        // to be integer width, so following words will start on an integer boundary.
        if (m_style.applyWordRounding() && Font::isRoundingHackCharacter(c))
            width = ceilf(width);

        // Check to see if the next character is a "rounding hack character", if so, adjust
        // width so that the total run width will be on an integer boundary.
        if ((m_style.applyWordRounding() && currentCharacter < m_run.length() && Font::isRoundingHackCharacter(*cp))
                || (m_style.applyRunRounding() && currentCharacter >= m_end)) {
            float totalWidth = m_widthToStart + runWidthSoFar + width;
            width += ceilf(totalWidth) - totalWidth;
        }

        runWidthSoFar += width;

        if (glyphBuffer)
            glyphBuffer->add(glyph, fontData, (rtl ? oldWidth + lastRoundingWidth : width));

        lastRoundingWidth = width - oldWidth;
    }

    m_currentCharacter = currentCharacter;
    m_runWidthSoFar = runWidthSoFar;
    m_finalRoundingWidth = lastRoundingWidth;
}

bool WidthIterator::advanceOneCharacter(float& width, GlyphBuffer* glyphBuffer)
{
    glyphBuffer->clear();
    advance(m_currentCharacter + 1, glyphBuffer);
    float w = 0;
    for (int i = 0; i < glyphBuffer->size(); ++i)
        w += glyphBuffer->advanceAt(i);
    width = w;
    return !glyphBuffer->isEmpty();
}

UChar32 WidthIterator::normalizeVoicingMarks(int currentCharacter)
{
    if (currentCharacter + 1 < m_end) {
        if (u_getCombiningClass(m_run[currentCharacter + 1]) == hiraganaKatakanaVoicingMarksCombiningClass) {
            // Normalize into composed form using 3.2 rules.
            UChar normalizedCharacters[2] = { 0, 0 };
            UErrorCode uStatus = U_ZERO_ERROR;  
            int32_t resultLength = unorm_normalize(m_run.data(currentCharacter), 2,
                UNORM_NFC, UNORM_UNICODE_3_2, &normalizedCharacters[0], 2, &uStatus);
            if (resultLength == 1 && uStatus == 0)
                return normalizedCharacters[0];
        }
    }
    return 0;
}

// ============================================================================================
// Font Implementation (Cross-Platform Portion)
// ============================================================================================

Font::Font() :m_fontList(0), m_letterSpacing(0), m_wordSpacing(0) {}
Font::Font(const FontDescription& fd, short letterSpacing, short wordSpacing) 
: m_fontDescription(fd),
  m_fontList(0),
  m_letterSpacing(letterSpacing),
  m_wordSpacing(wordSpacing)
{}

Font::Font(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_fontList = other.m_fontList;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
}

Font& Font::operator=(const Font& other)
{
    if (&other != this) {
        m_fontDescription = other.m_fontDescription;
        m_fontList = other.m_fontList;
        m_letterSpacing = other.m_letterSpacing;
        m_wordSpacing = other.m_wordSpacing;
    }
    return *this;
}

Font::~Font()
{
}

const FontData* Font::primaryFont() const
{
    assert(m_fontList);
    return m_fontList->primaryFont(this);
}

const FontData* Font::fontDataAt(unsigned index) const
{
    assert(m_fontList);
    return m_fontList->fontDataAt(this, index);
}

const FontData* Font::fontDataForCharacters(const UChar* characters, int length) const
{
    assert(m_fontList);
    return m_fontList->fontDataForCharacters(this, characters, length);
}

void Font::update() const
{
    // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr, but it ends up 
    // being reasonably safe (because inherited fonts in the render tree pick up the new
    // style anyway.  Other copies are transient, e.g., the state in the GraphicsContext, and
    // won't stick around long enough to get you in trouble).  Still, this is pretty disgusting,
    // and could eventually be rectified by using RefPtrs for Fonts themselves.
    if (!m_fontList)
        m_fontList = new FontFallbackList();
    m_fontList->invalidate();
}

int Font::width(const TextRun& run) const
{
    return width(run, TextStyle());
}

int Font::width(const TextRun& run, const TextStyle& style) const
{
    return lroundf(floatWidth(run, style));
}

int Font::ascent() const
{
    return primaryFont()->ascent();
}

int Font::descent() const
{
    return primaryFont()->descent();
}

int Font::lineSpacing() const
{
    return primaryFont()->lineSpacing();
}

float Font::xHeight() const
{
    return primaryFont()->xHeight();
}

bool Font::isFixedPitch() const
{
    assert(m_fontList);
    return m_fontList->isFixedPitch(this);
}

// FIXME: These methods will eventually be cross-platform, but to keep Windows compiling we'll make this Apple-only for now.
bool Font::gAlwaysUseComplexPath = false;
void Font::setAlwaysUseComplexPath(bool alwaysUse)
{
    gAlwaysUseComplexPath = alwaysUse;
}

bool Font::canUseGlyphCache(const TextRun& run) const
{
    if (gAlwaysUseComplexPath)
        return false;
    
    // Start from 0 since drawing and highlighting also measure the characters before run->from
    for (int i = 0; i < run.to(); i++) {
        const UChar c = run[i];
        if (c < 0x300)      // U+0300 through U+036F Combining diacritical marks
            continue;
        if (c <= 0x36F)
            return false;

        if (c < 0x0591 || c == 0x05BE)     // U+0591 through U+05CF excluding U+05BE Hebrew combining marks, Hebrew punctuation Paseq, Sof Pasuq and Nun Hafukha
            continue;
        if (c <= 0x05CF)
            return false;

        if (c < 0x0600)     // U+0600 through U+1059 Arabic, Syriac, Thaana, Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
            continue;
        if (c <= 0x1059)
            return false;

        if (c < 0x1100)     // U+1100 through U+11FF Hangul Jamo (only Ancient Korean should be left here if you precompose; Modern Korean will be precomposed as a result of step A)
            continue;
        if (c <= 0x11FF)
            return false;

        if (c < 0x1780)     // U+1780 through U+18AF Khmer, Mongolian
            continue;
        if (c <= 0x18AF)
            return false;

        if (c < 0x1900)     // U+1900 through U+194F Limbu (Unicode 4.0)
            continue;
        if (c <= 0x194F)
            return false;

        if (c < 0x20D0)     // U+20D0 through U+20FF Combining marks for symbols
            continue;
        if (c <= 0x20FF)
            return false;

        if (c < 0xFE20)     // U+FE20 through U+FE2F Combining half marks
            continue;
        if (c <= 0xFE2F)
            return false;
    }

    return true;

}

void Font::drawSimpleText(GraphicsContext* context, const TextRun& run, const TextStyle& style, const FloatPoint& point) const
{
    // This glyph buffer holds our glyphs+advances+font data for each glyph.
    GlyphBuffer glyphBuffer;

    // Our measuring code will generate glyphs and advances for us.
    float startX;
    floatWidthForSimpleText(run, style, 0, &startX, &glyphBuffer);
    
    // We couldn't generate any glyphs for the run.  Give up.
    if (glyphBuffer.isEmpty())
        return;
    
    // Calculate the starting point of the glyphs to be displayed by adding
    // all the advances up to the first glyph.
    startX += point.x();
    FloatPoint startPoint(startX, point.y());

    // Swap the order of the glyphs if right-to-left.
    if (style.rtl())
        for (int i = 0, end = glyphBuffer.size() - 1; i < glyphBuffer.size() / 2; ++i, --end)
            glyphBuffer.swap(i, end);

    // Draw each contiguous run of glyphs that use the same font data.
    const FontData* fontData = glyphBuffer.fontDataAt(0);
    float nextX = startX;
    int lastFrom = 0;
    int nextGlyph = 0;
    while (nextGlyph < glyphBuffer.size()) {
        const FontData* nextFontData = glyphBuffer.fontDataAt(nextGlyph);
        if (nextFontData != fontData) {
            drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint, style.usePrinterFonts());
            lastFrom = nextGlyph;
            fontData = nextFontData;
            startPoint.setX(nextX);
        }
        nextX += glyphBuffer.advanceAt(nextGlyph);
        nextGlyph++;
    }
    drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint, style.usePrinterFonts());
}

void Font::drawText(GraphicsContext* context, const TextRun& run, const TextStyle& style, const FloatPoint& point) const
{
    if (canUseGlyphCache(run))
        drawSimpleText(context, run, style, point);
    else
        drawComplexText(context, run, style, point);
}

float Font::floatWidth(const TextRun& run, const TextStyle& style) const
{
    if (canUseGlyphCache(run))
        return floatWidthForSimpleText(run, style, 0, 0, 0);
    else
        return floatWidthForComplexText(run, style);
}

float Font::floatWidthForSimpleText(const TextRun& run, const TextStyle& style,
                                    const FontData* substituteFont, float* startPosition, GlyphBuffer* glyphBuffer) const
{
    
    WidthIterator it(this, run, style, substituteFont);
    it.advance(run.to(), glyphBuffer);
    float runWidth = it.m_runWidthSoFar;
    if (startPosition) {
        if (style.ltr())
            *startPosition = it.m_widthToStart;
        else {
            float finalRoundingWidth = it.m_finalRoundingWidth;
            it.advance(run.length());
            *startPosition = it.m_runWidthSoFar - runWidth + finalRoundingWidth;
        }
    }
    return runWidth;
}

FloatRect Font::selectionRectForText(const TextRun& run, const TextStyle& style, const IntPoint& point, int h) const
{
    if (canUseGlyphCache(run))
        return selectionRectForSimpleText(run, style, point, h);
    return selectionRectForComplexText(run, style, point, h);
}

FloatRect Font::selectionRectForSimpleText(const TextRun& run, const TextStyle& style, const IntPoint& point, int h) const
{
    TextRun completeRun(run);
    completeRun.makeComplete();

    WidthIterator it(this, completeRun, style);
    it.advance(run.from());
    float beforeWidth = it.m_runWidthSoFar;
    it.advance(run.to());
    float afterWidth = it.m_runWidthSoFar;

    // Using roundf() rather than ceilf() for the right edge as a compromise to ensure correct caret positioning
    if (style.rtl()) {
        it.advance(run.length());
        float totalWidth = it.m_runWidthSoFar;
        return FloatRect(point.x() + floorf(totalWidth - afterWidth), point.y(), roundf(totalWidth - beforeWidth) - floorf(totalWidth - afterWidth), h);
    } else {
        return FloatRect(point.x() + floorf(beforeWidth), point.y(), roundf(afterWidth) - floorf(beforeWidth), h);
    }
}

int Font::offsetForPosition(const TextRun& run, const TextStyle& style, int x, bool includePartialGlyphs) const
{
    if (canUseGlyphCache(run))
        return offsetForPositionForSimpleText(run, style, x, includePartialGlyphs);
    return offsetForPositionForComplexText(run, style, x, includePartialGlyphs);
}

int Font::offsetForPositionForSimpleText(const TextRun& run, const TextStyle& style, int x, bool includePartialGlyphs) const
{
    float delta = (float)x;

    WidthIterator it(this, run, style);    
    GlyphBuffer localGlyphBuffer;
    unsigned offset;
    if (style.rtl()) {
        delta -= floatWidthForSimpleText(run, style, 0, 0, 0);
        while (1) {
            offset = it.m_currentCharacter;
            float w;
            if (!it.advanceOneCharacter(w, &localGlyphBuffer))
                break;
            delta += w;
            if (includePartialGlyphs) {
                if (delta - w / 2 >= 0)
                    break;
            } else {
                if (delta >= 0)
                    break;
            }
        }
    } else {
        while (1) {
            offset = it.m_currentCharacter;
            float w;
            if (!it.advanceOneCharacter(w, &localGlyphBuffer))
                break;
            delta -= w;
            if (includePartialGlyphs) {
                if (delta + w / 2 <= 0)
                    break;
            } else {
                if (delta <= 0)
                    break;
            }
        }
    }

    return offset - run.from();
}

}
