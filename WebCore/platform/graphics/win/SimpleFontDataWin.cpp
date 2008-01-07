/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimpleFontData.h"

#include <winsock2.h>
#include "Font.h"
#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include <wtf/MathExtras.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <ApplicationServices/ApplicationServices.h>
#include <mlang.h>
#include <tchar.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>

namespace WebCore {

using std::max;

const float cSmallCapsFontSizeMultiplier = 0.7f;

static bool shouldApplyMacAscentHack;

static inline float scaleEmToUnits(float x, unsigned unitsPerEm) { return unitsPerEm ? x / (float)unitsPerEm : x; }

void SimpleFontData::setShouldApplyMacAscentHack(bool b)
{
    shouldApplyMacAscentHack = b;
}

void SimpleFontData::platformInit()
{    
    m_syntheticBoldOffset = m_font.syntheticBold() ? 1.0f : 0.f;
    m_scriptCache = 0;
    m_scriptFontProperties = 0;
    m_isSystemFont = false;
    
    if (m_font.useGDI()) {
        HDC hdc = GetDC(0);
        HGDIOBJ oldFont = SelectObject(hdc, m_font.hfont());
        OUTLINETEXTMETRIC metrics;
        GetOutlineTextMetrics(hdc, sizeof(metrics), &metrics);
        TEXTMETRIC& textMetrics = metrics.otmTextMetrics;
        m_ascent = textMetrics.tmAscent;
        m_descent = textMetrics.tmDescent;
        m_lineGap = textMetrics.tmExternalLeading;
        m_lineSpacing = m_ascent + m_descent + m_lineGap;
        m_xHeight = m_ascent * 0.56f; // Best guess for xHeight if no x glyph is present.

        GLYPHMETRICS gm;
        MAT2 mat = { 1, 0, 0, 1 };
        DWORD len = GetGlyphOutline(hdc, 'x', GGO_METRICS, &gm, 0, 0, &mat);
        if (len != GDI_ERROR && gm.gmptGlyphOrigin.y > 0)
            m_xHeight = gm.gmptGlyphOrigin.y;

        m_unitsPerEm = metrics.otmEMSquare;

        SelectObject(hdc, oldFont);
        ReleaseDC(0, hdc);

        return;
    }

    CGFontRef font = m_font.cgFont();
    int iAscent = CGFontGetAscent(font);
    int iDescent = CGFontGetDescent(font);
    int iLineGap = CGFontGetLeading(font);
    m_unitsPerEm = CGFontGetUnitsPerEm(font);
    float pointSize = m_font.size();
    float fAscent = scaleEmToUnits(iAscent, m_unitsPerEm) * pointSize;
    float fDescent = -scaleEmToUnits(iDescent, m_unitsPerEm) * pointSize;
    float fLineGap = scaleEmToUnits(iLineGap, m_unitsPerEm) * pointSize;

    if (!isCustomFont()) {
        HDC dc = GetDC(0);
        HGDIOBJ oldFont = SelectObject(dc, m_font.hfont());
        int faceLength = GetTextFace(dc, 0, 0);
        Vector<TCHAR> faceName(faceLength);
        GetTextFace(dc, faceLength, faceName.data());
        m_isSystemFont = !_tcscmp(faceName.data(), _T("Lucida Grande"));
        SelectObject(dc, oldFont);
        ReleaseDC(0, dc);

        if (shouldApplyMacAscentHack) {
            // This code comes from FontDataMac.mm. We only ever do this when running regression tests so that our metrics will match Mac.

            // We need to adjust Times, Helvetica, and Courier to closely match the
            // vertical metrics of their Microsoft counterparts that are the de facto
            // web standard. The AppKit adjustment of 20% is too big and is
            // incorrectly added to line spacing, so we use a 15% adjustment instead
            // and add it to the ascent.
            if (!_tcscmp(faceName.data(), _T("Times")) || !_tcscmp(faceName.data(), _T("Helvetica")) || !_tcscmp(faceName.data(), _T("Courier")))
                fAscent += floorf(((fAscent + fDescent) * 0.15f) + 0.5f);
        }
    }

    m_ascent = lroundf(fAscent);
    m_descent = lroundf(fDescent);
    m_lineGap = lroundf(fLineGap);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    // Measure the actual character "x", because AppKit synthesizes X height rather than getting it from the font.
    // Unfortunately, NSFont will round this for us so we don't quite get the right value.
    GlyphPage* glyphPageZero = GlyphPageTreeNode::getRootChild(this, 0)->page();
    Glyph xGlyph = glyphPageZero ? glyphPageZero->glyphDataForCharacter('x').glyph : 0;
    if (xGlyph) {
        CGRect xBox;
        CGFontGetGlyphBBoxes(font, &xGlyph, 1, &xBox);
        // Use the maximum of either width or height because "x" is nearly square
        // and web pages that foolishly use this metric for width will be laid out
        // poorly if we return an accurate height. Classic case is Times 13 point,
        // which has an "x" that is 7x6 pixels.
        m_xHeight = scaleEmToUnits(max(CGRectGetMaxX(xBox), CGRectGetMaxY(xBox)), m_unitsPerEm) * pointSize;
    } else {
        int iXHeight = CGFontGetXHeight(font);
        m_xHeight = scaleEmToUnits(iXHeight, m_unitsPerEm) * pointSize;
    }
}

void SimpleFontData::platformDestroy()
{
    if (!isCustomFont()) {
        DeleteObject(m_font.hfont());
        CGFontRelease(m_font.cgFont());
    }

    // We don't hash this on Win32, so it's effectively owned by us.
    delete m_smallCapsFontData;

    ScriptFreeCache(&m_scriptCache);
    delete m_scriptFontProperties;
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        float smallCapsHeight = cSmallCapsFontSizeMultiplier * m_font.size();
        if (isCustomFont()) {
            FontPlatformData smallCapsFontData(m_font);
            smallCapsFontData.setSize(smallCapsHeight);
            m_smallCapsFontData = new SimpleFontData(smallCapsFontData, true, false);
        } else {
            LOGFONT winfont;
            GetObject(m_font.hfont(), sizeof(LOGFONT), &winfont);
            winfont.lfHeight = -lroundf(smallCapsHeight * (m_font.useGDI() ? 1 : 32));
            HFONT hfont = CreateFontIndirect(&winfont);
            m_smallCapsFontData = new SimpleFontData(FontPlatformData(hfont, smallCapsHeight, fontDescription.bold(), fontDescription.italic(), m_font.useGDI()));
        }
    }
    return m_smallCapsFontData;
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    // FIXME: Support custom fonts.
    if (isCustomFont())
        return false;

    // FIXME: Microsoft documentation seems to imply that characters can be output using a given font and DC
    // merely by testing code page intersection.  This seems suspect though.  Can't a font only partially
    // cover a given code page?
    IMLangFontLink2* langFontLink = FontCache::getFontLinkInterface();
    if (!langFontLink)
        return false;

    HDC dc = GetDC(0);
    
    DWORD acpCodePages;
    langFontLink->CodePageToCodePages(CP_ACP, &acpCodePages);

    DWORD fontCodePages;
    langFontLink->GetFontCodePages(dc, m_font.hfont(), &fontCodePages);

    DWORD actualCodePages;
    long numCharactersProcessed;
    long offset = 0;
    while (offset < length) {
        langFontLink->GetStrCodePages(characters, length, acpCodePages, &actualCodePages, &numCharactersProcessed);
        if ((actualCodePages & fontCodePages) == 0)
            return false;
        offset += numCharactersProcessed;
    }

    ReleaseDC(0, dc);

    return true;
}

void SimpleFontData::determinePitch()
{
    if (isCustomFont()) {
        m_treatAsFixedPitch = false;
        return;
    }

    // TEXTMETRICS have this.  Set m_treatAsFixedPitch based off that.
    HDC dc = GetDC(0);
    SaveDC(dc);
    SelectObject(dc, m_font.hfont());

    // Yes, this looks backwards, but the fixed pitch bit is actually set if the font
    // is *not* fixed pitch.  Unbelievable but true.
    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    m_treatAsFixedPitch = ((tm.tmPitchAndFamily & TMPF_FIXED_PITCH) == 0);

    RestoreDC(dc, -1);
    ReleaseDC(0, dc);
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    if (m_font.useGDI()) {
        HDC hdc = GetDC(0);
        HGDIOBJ oldFont = SelectObject(hdc, m_font.hfont());
        int width;
        GetCharWidthI(hdc, glyph, 1, 0, &width);
        SelectObject(hdc, oldFont);
        ReleaseDC(0, hdc);
        return width;
    }

    CGFontRef font = m_font.cgFont();
    float pointSize = m_font.size();
    CGSize advance;
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
    // FIXME: Need to add real support for printer fonts.
    bool isPrinterFont = false;
    wkGetGlyphAdvances(font, m, m_isSystemFont, isPrinterFont, glyph, advance);
    return advance.width + m_syntheticBoldOffset;
}

SCRIPT_FONTPROPERTIES* SimpleFontData::scriptFontProperties() const
{
    if (!m_scriptFontProperties) {
        m_scriptFontProperties = new SCRIPT_FONTPROPERTIES;
        memset(m_scriptFontProperties, 0, sizeof(SCRIPT_FONTPROPERTIES));
        m_scriptFontProperties->cBytes = sizeof(SCRIPT_FONTPROPERTIES);
        HRESULT result = ScriptGetFontProperties(0, scriptCache(), m_scriptFontProperties);
        if (result == E_PENDING) {
            HDC dc = GetDC(0);
            SaveDC(dc);
            SelectObject(dc, m_font.hfont());
            ScriptGetFontProperties(dc, scriptCache(), m_scriptFontProperties);
            RestoreDC(dc, -1);
            ReleaseDC(0, dc);
        }
    }
    return m_scriptFontProperties;
}

}
