/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "SVGInlineTextBox.h"

#if ENABLE(SVG)
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "InlineFlowBox.h"
#include "RenderBlock.h"
#include "RenderSVGResource.h"
#include "SVGRootInlineBox.h"
#include "SVGTextLayoutUtilities.h"

#include <float.h>

using namespace std;

namespace WebCore {

SVGInlineTextBox::SVGInlineTextBox(RenderObject* object)
    : InlineTextBox(object)
    , m_height(0)
    , m_paintingResource(0)
    , m_paintingResourceMode(ApplyToDefaultMode)
{
}

SVGRootInlineBox* SVGInlineTextBox::svgRootInlineBox() const
{
    // Find associated root inline box
    InlineFlowBox* parentBox = parent();

    while (parentBox && !parentBox->isRootInlineBox())
        parentBox = parentBox->parent();

    ASSERT(parentBox);
    ASSERT(parentBox->isRootInlineBox());

    if (!parentBox->isSVGRootInlineBox())
        return 0;

    return static_cast<SVGRootInlineBox*>(parentBox);
}

void SVGInlineTextBox::measureCharacter(RenderStyle* style, int position, int& charsConsumed, String& glyphName, String& unicodeString, float& glyphWidth, float& glyphHeight) const
{
    ASSERT(style);

    int offset = direction() == RTL ? end() - position : start() + position;
    int extraCharsAvailable = len() - position - 1;
    const UChar* characters = textRenderer()->characters();

    const Font& font = style->font();
    glyphWidth = font.floatWidth(svgTextRunForInlineTextBox(characters + offset, 1, style, this), extraCharsAvailable, charsConsumed, glyphName);
    glyphHeight = font.height();

    // The unicodeString / glyphName pair is needed for kerning calculations.
    unicodeString = String(characters + offset, charsConsumed);
}

int SVGInlineTextBox::offsetForPosition(int xCoordinate, bool includePartialGlyphs) const
{
    ASSERT(!m_currentChunkPart.isValid());
    float x = xCoordinate;

    RenderText* textRenderer = this->textRenderer();
    ASSERT(textRenderer);

    RenderStyle* style = textRenderer->style();
    ASSERT(style);

    RenderBlock* containingBlock = textRenderer->containingBlock();
    ASSERT(containingBlock);

    // Move incoming relative x position to absolute position, as the character origins stored in the chunk parts use absolute coordinates
    x += containingBlock->x();

    // Figure out which text chunk part is hit
    SVGTextChunkPart hitPart;

    const Vector<SVGTextChunkPart>::const_iterator end = m_svgTextChunkParts.end();
    for (Vector<SVGTextChunkPart>::const_iterator it = m_svgTextChunkParts.begin(); it != end; ++it) {
        const SVGTextChunkPart& part = *it;

        // Check whether we're past the hit part.
        if (x < part.firstCharacter->x)
            break;

        hitPart = part;
    }

    // If we did not hit anything, just exit.
    if (!hitPart.isValid())
        return 0;

    // Before calling Font::offsetForPosition(), subtract the start position of the first character
    // in the hit text chunk part, to pass in coordinates, which are relative to the text chunk part, as
    // constructTextRun() only builds a TextRun for the current chunk part, not the whole inline text box.
    x -= hitPart.firstCharacter->x;

    m_currentChunkPart = hitPart;
    TextRun textRun(constructTextRun(style));
    m_currentChunkPart = SVGTextChunkPart();

    // Eventually handle lengthAdjust="spacingAndGlyphs".
    if (!m_chunkTransformation.isIdentity())
        textRun.setGlyphScale(narrowPrecisionToFloat(isVerticalWritingMode(style->svgStyle()) ? m_chunkTransformation.d() : m_chunkTransformation.a()));

    return hitPart.offset + style->font().offsetForPosition(textRun, x, includePartialGlyphs);
}

int SVGInlineTextBox::positionForOffset(int) const
{
    // SVG doesn't use the offset <-> position selection system. 
    ASSERT_NOT_REACHED();
    return 0;
}

IntRect SVGInlineTextBox::selectionRect(int, int, int startPos, int endPos)
{
    ASSERT(!m_currentChunkPart.isValid());

    int boxStart = start();
    startPos = max(startPos - boxStart, 0);
    endPos = min(endPos - boxStart, static_cast<int>(len()));

    if (startPos >= endPos)
        return IntRect();

    RenderText* text = textRenderer();
    ASSERT(text);

    RenderStyle* style = text->style();
    ASSERT(style);

    const Font& font = style->font();
    int baseline = font.ascent();
    FloatRect selectionRect;

    // Figure out which text chunk part is hit
    int partStartPos = 0;
    int partEndPos = 0;

    const Vector<SVGTextChunkPart>::const_iterator end = m_svgTextChunkParts.end();
    for (Vector<SVGTextChunkPart>::const_iterator it = m_svgTextChunkParts.begin(); it != end; ++it) {
        partStartPos = startPos;
        partEndPos = endPos;

        // Select current chunk part, map startPos/endPos positions into chunk part
        m_currentChunkPart = *it;
        mapStartEndPositionsIntoChunkPartCoordinates(partStartPos, partEndPos, m_currentChunkPart);

        if (partStartPos < partEndPos) {
            Vector<SVGChar>::const_iterator character = m_currentChunkPart.firstCharacter;
            FloatPoint textOrigin(character->x, character->y - baseline);
            FloatRect partRect(font.selectionRectForText(constructTextRun(style), textOrigin, m_currentChunkPart.height, partStartPos, partEndPos));
            selectionRect.unite(character->characterTransform().mapRect(partRect));
        }

        m_currentChunkPart = SVGTextChunkPart();
    }

    // Resepect possible chunk transformation
    if (m_chunkTransformation.isIdentity())
        return enclosingIntRect(selectionRect);

    return enclosingIntRect(m_chunkTransformation.mapRect(selectionRect));
}

void SVGInlineTextBox::paint(RenderObject::PaintInfo& paintInfo, int, int)
{
    ASSERT(renderer()->shouldPaintWithinRoot(paintInfo));
    ASSERT(paintInfo.phase == PaintPhaseForeground);
    ASSERT(truncation() == cNoTruncation);

    if (renderer()->style()->visibility() != VISIBLE)
        return;

    // Note: We're explicitely not supporting composition & custom underlines and custom highlighters - unlike InlineTextBox.
    // If we ever need that for SVG, it's very easy to refactor and reuse the code.

    RenderObject* parentRenderer = parent()->renderer();
    ASSERT(parentRenderer);

    RenderStyle* style = parentRenderer->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    bool hasFill = svgStyle->hasFill();
    bool hasStroke = svgStyle->hasStroke();

    // Determine whether or not we're selected.
    bool isPrinting = parentRenderer->document()->printing();
    bool hasSelection = !isPrinting && selectionState() != RenderObject::SelectionNone;

    RenderStyle* selectionStyle = style;
    if (hasSelection) {
        selectionStyle = parentRenderer->getCachedPseudoStyle(SELECTION);
        if (selectionStyle) {
            const SVGRenderStyle* svgSelectionStyle = selectionStyle->svgStyle();
            ASSERT(svgSelectionStyle);

            if (!hasFill)
                hasFill = svgSelectionStyle->hasFill();
            if (!hasStroke)
                hasStroke = svgSelectionStyle->hasStroke();
        } else
            selectionStyle = style;
    }

    ASSERT(!m_currentChunkPart.isValid());

    const Vector<SVGTextChunkPart>::const_iterator end = m_svgTextChunkParts.end();
    for (Vector<SVGTextChunkPart>::const_iterator it = m_svgTextChunkParts.begin(); it != end; ++it) {
        ASSERT(!m_paintingResource);

        // constructTextRun() uses the current chunk part to figure out what text to render.
        m_currentChunkPart = *it;
        paintInfo.context->save();

        // Prepare context and draw text
        if (!m_chunkTransformation.isIdentity())
            paintInfo.context->concatCTM(m_chunkTransformation);

        Vector<SVGChar>::const_iterator firstCharacter = m_currentChunkPart.firstCharacter;
        AffineTransform characterTransform = firstCharacter->characterTransform();
        if (!characterTransform.isIdentity())
            paintInfo.context->concatCTM(characterTransform);

        FloatPoint textOrigin(firstCharacter->x, firstCharacter->y);

        // Draw background once (not in both fill/stroke phases)
        if (!isPrinting) {
            // FIXME: paintDocumentMarkers(paintInfo.context, xOrigin, yOrigin, style, font, true);

            if (hasSelection)
                paintSelection(paintInfo.context, textOrigin, style);
        }

        // Spec: All text decorations except line-through should be drawn before the text is filled and stroked; thus, the text is rendered on top of these decorations.
        int decorations = style->textDecorationsInEffect();
        if (decorations & UNDERLINE)
            paintDecoration(paintInfo.context, textOrigin, UNDERLINE, hasSelection);
        if (decorations & OVERLINE)
            paintDecoration(paintInfo.context, textOrigin, OVERLINE, hasSelection);

        // Fill text
        if (hasFill) {
            m_paintingResourceMode = ApplyToFillMode | ApplyToTextMode;
            paintText(paintInfo.context, textOrigin, style, selectionStyle, hasSelection);
        }

        // Stroke text
        if (hasStroke) {
            m_paintingResourceMode = ApplyToStrokeMode | ApplyToTextMode;
            paintText(paintInfo.context, textOrigin, style, selectionStyle, hasSelection);
        }

        // Spec: Line-through should be drawn after the text is filled and stroked; thus, the line-through is rendered on top of the text.
        if (decorations & LINE_THROUGH)
            paintDecoration(paintInfo.context, textOrigin, LINE_THROUGH, hasSelection);

        // Paint document markers
        // FIXME: paintDocumentMarkers(paintInfo.context, xOrigin, yOrigin, style, font, false);

        m_paintingResourceMode = ApplyToDefaultMode;
        paintInfo.context->restore();
    }

    m_currentChunkPart = SVGTextChunkPart();
    ASSERT(!m_paintingResource);
}

bool SVGInlineTextBox::acquirePaintingResource(GraphicsContext*& context, RenderStyle* style)
{
    ASSERT(m_paintingResourceMode != ApplyToDefaultMode);

    RenderObject* parentRenderer = parent()->renderer();
    ASSERT(parentRenderer);

    if (m_paintingResourceMode & ApplyToFillMode)
        m_paintingResource = RenderSVGResource::fillPaintingResource(parentRenderer, style);
    else if (m_paintingResourceMode & ApplyToStrokeMode)
        m_paintingResource = RenderSVGResource::strokePaintingResource(parentRenderer, style);
    else {
        // We're either called for stroking or filling.
        ASSERT_NOT_REACHED();
    }

    if (!m_paintingResource)
        return false;

    m_paintingResource->applyResource(parentRenderer, style, context, m_paintingResourceMode);
    return true;
}

void SVGInlineTextBox::releasePaintingResource(GraphicsContext*& context)
{
    ASSERT(m_paintingResource);

    RenderObject* parentRenderer = parent()->renderer();
    ASSERT(parentRenderer);

    m_paintingResource->postApplyResource(parentRenderer, context, m_paintingResourceMode);
    m_paintingResource = 0;
}

bool SVGInlineTextBox::prepareGraphicsContextForTextPainting(GraphicsContext*& context, TextRun& textRun, RenderStyle* style)
{
    bool acquiredResource = acquirePaintingResource(context, style);

#if ENABLE(SVG_FONTS)
    // SVG Fonts need access to the painting resource used to draw the current text chunk.
    if (acquiredResource)
        textRun.setActivePaintingResource(m_paintingResource);
#endif

    return acquiredResource;
}

void SVGInlineTextBox::restoreGraphicsContextAfterTextPainting(GraphicsContext*& context, TextRun& textRun)
{
    releasePaintingResource(context);

#if ENABLE(SVG_FONTS)
    textRun.setActivePaintingResource(0);
#endif
}

TextRun SVGInlineTextBox::constructTextRun(RenderStyle* style) const
{
    ASSERT(m_currentChunkPart.isValid());
    return svgTextRunForInlineTextBox(textRenderer()->text()->characters() + start() + m_currentChunkPart.offset, m_currentChunkPart.length, style, this);
}

void SVGInlineTextBox::mapStartEndPositionsIntoChunkPartCoordinates(int& startPos, int& endPos, const SVGTextChunkPart& part) const
{
    if (startPos >= endPos)
        return;

    // Take <text x="10 50 100">ABC</text> as example. We're called three times from paint(), because all absolute positioned
    // characters are drawn on their own. For each of them we want to find out whehter it's selected. startPos=0, endPos=1
    // could mean A, B or C is hit, depending on which chunk part is processed at the moment. With the offset & length values
    // of each chunk part we can easily find out which one is meant to be selected. Bail out for the other chunk parts.
    // If starPos is behind the current chunk or the endPos ends before this text chunk part, we're not meant to be selected.
    if (startPos >= part.offset + part.length || endPos <= part.offset) {
        startPos = 0;
        endPos = -1;
        return;
    }

    // The current processed chunk part is hit. When painting the selection, constructTextRun() builds
    // a TextRun object whose startPos is 0 and endPos is chunk part length. The code below maps the incoming
    // startPos/endPos range into a [0, part length] coordinate system, relative to the current chunk part.
    if (startPos < part.offset)
        startPos = 0;
    else
        startPos -= part.offset;

    if (endPos > part.offset + part.length)
        endPos = part.length;
    else {
        ASSERT(endPos >= part.offset);
        endPos -= part.offset;
    }

    ASSERT(startPos < endPos);
}

void SVGInlineTextBox::selectionStartEnd(int& startPos, int& endPos)
{
    InlineTextBox::selectionStartEnd(startPos, endPos);

    if (!m_currentChunkPart.isValid())
        return;

    mapStartEndPositionsIntoChunkPartCoordinates(startPos, endPos, m_currentChunkPart);
}

static inline float positionOffsetForDecoration(ETextDecoration decoration, const Font& font, float thickness)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Opera.
    if (decoration == UNDERLINE)
        return font.ascent() + thickness * 1.5f;
    if (decoration == OVERLINE)
        return thickness;
    if (decoration == LINE_THROUGH)
        return font.ascent() * 5.0f / 8.0f;

    ASSERT_NOT_REACHED();
    return 0.0f;
}

static inline float thicknessForDecoration(ETextDecoration, const Font& font)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Opera
    return font.size() / 20.0f;
}
 
static inline RenderObject* findRenderObjectDefininingTextDecoration(InlineFlowBox* parentBox, ETextDecoration decoration)
{
    // Lookup render object which has text-decoration set.
    RenderObject* renderer = 0;
    while (parentBox) {
        renderer = parentBox->renderer();

        // Explicitely check textDecoration() not textDecorationsInEffect(), which is inherited to
        // children, as we want to lookup the render object whose style defined the text-decoration.
        if (renderer->style() && renderer->style()->textDecoration() & decoration)
            break;

        parentBox = parentBox->parent();
    }

    ASSERT(renderer);
    return renderer;
}

void SVGInlineTextBox::paintDecoration(GraphicsContext* context, const FloatPoint& textOrigin, ETextDecoration decoration, bool hasSelection)
{
    // Find out which render style defined the text-decoration, as its fill/stroke properties have to be used for drawing instead of ours.
    RenderObject* decorationRenderer = findRenderObjectDefininingTextDecoration(parent(), decoration);
    RenderStyle* decorationStyle = decorationRenderer->style();
    ASSERT(decorationStyle);

    const SVGRenderStyle* svgDecorationStyle = decorationStyle->svgStyle();
    ASSERT(svgDecorationStyle);

    bool hasDecorationFill = svgDecorationStyle->hasFill();
    bool hasDecorationStroke = svgDecorationStyle->hasStroke();

    if (hasSelection) {
        if (RenderStyle* pseudoStyle = decorationRenderer->getCachedPseudoStyle(SELECTION)) {
            decorationStyle = pseudoStyle;

            svgDecorationStyle = decorationStyle->svgStyle();
            ASSERT(svgDecorationStyle);

            if (!hasDecorationFill)
                hasDecorationFill = svgDecorationStyle->hasFill();
            if (!hasDecorationStroke)
                hasDecorationStroke = svgDecorationStyle->hasStroke();
        }
    }

    if (decorationStyle->visibility() == HIDDEN)
        return;

    if (hasDecorationFill) {
        m_paintingResourceMode = ApplyToFillMode;
        paintDecorationWithStyle(context, textOrigin, decorationStyle, decoration);
    }

    if (hasDecorationStroke) {
        m_paintingResourceMode = ApplyToStrokeMode;
        paintDecorationWithStyle(context, textOrigin, decorationStyle, decoration);
    }
}

void SVGInlineTextBox::paintDecorationWithStyle(GraphicsContext* context, const FloatPoint& textOrigin, RenderStyle* decorationStyle, ETextDecoration decoration)
{
    ASSERT(!m_paintingResource);
    ASSERT(m_paintingResourceMode != ApplyToDefaultMode);
    ASSERT(m_currentChunkPart.isValid());

    const Font& font = decorationStyle->font();

    // The initial y value refers to overline position.
    float thickness = thicknessForDecoration(decoration, font);
    float x = textOrigin.x();
    float y = textOrigin.y() - font.ascent() + positionOffsetForDecoration(decoration, font, thickness);

    context->save();
    context->beginPath();
    context->addPath(Path::createRectangle(FloatRect(x, y, m_currentChunkPart.width, thickness)));

    if (acquirePaintingResource(context, decorationStyle))
        releasePaintingResource(context);

    context->restore();
}

void SVGInlineTextBox::paintSelection(GraphicsContext* context, const FloatPoint& textOrigin, RenderStyle* style)
{
    // See if we have a selection to paint at all.
    int startPos, endPos;
    selectionStartEnd(startPos, endPos);
    if (startPos >= endPos)
        return;

    Color backgroundColor = renderer()->selectionBackgroundColor();
    if (!backgroundColor.isValid() || !backgroundColor.alpha())
        return;

    const Font& font = style->font();

    FloatPoint selectionOrigin = textOrigin;
    selectionOrigin.move(0, -font.ascent());

    context->save();
    context->setFillColor(backgroundColor, style->colorSpace());
    context->fillRect(font.selectionRectForText(constructTextRun(style), selectionOrigin, m_currentChunkPart.height, startPos, endPos), backgroundColor, style->colorSpace());
    context->restore();
}

void SVGInlineTextBox::paintText(GraphicsContext* context, const FloatPoint& textOrigin, RenderStyle* style, RenderStyle* selectionStyle, bool hasSelection)
{
    ASSERT(style);
    ASSERT(selectionStyle);
    ASSERT(m_currentChunkPart.isValid());

    int startPos = 0;
    int endPos = 0;
    selectionStartEnd(startPos, endPos);

    const Font& font = style->font();
    TextRun textRun(constructTextRun(style));

    // Fast path if there is no selection, just draw the whole chunk part using the regular style
    if (!hasSelection || startPos >= endPos) {
        if (prepareGraphicsContextForTextPainting(context, textRun, style)) {
            font.drawText(context, textRun, textOrigin, 0, m_currentChunkPart.length);
            restoreGraphicsContextAfterTextPainting(context, textRun);
        }

        return;
    }

    // Eventually draw text using regular style until the start position of the selection
    if (startPos > 0) {
        if (prepareGraphicsContextForTextPainting(context, textRun, style)) {
            font.drawText(context, textRun, textOrigin, 0, startPos);
            restoreGraphicsContextAfterTextPainting(context, textRun);
        }
    }

    // Draw text using selection style from the start to the end position of the selection
    TextRun selectionTextRun(constructTextRun(selectionStyle));
    if (prepareGraphicsContextForTextPainting(context, selectionTextRun, selectionStyle)) {
        selectionStyle->font().drawText(context, selectionTextRun, textOrigin, startPos, endPos);
        restoreGraphicsContextAfterTextPainting(context, selectionTextRun);
    }

    // Eventually draw text using regular style from the end position of the selection to the end of the current chunk part
    if (endPos < m_currentChunkPart.length) {
        if (prepareGraphicsContextForTextPainting(context, textRun, style)) {
            font.drawText(context, textRun, textOrigin, endPos, m_currentChunkPart.length);
            restoreGraphicsContextAfterTextPainting(context, textRun);
        }
    }
}


void SVGInlineTextBox::buildLayoutInformation(SVGCharacterLayoutInfo& info, SVGLastGlyphInfo& lastGlyph)
{
    RenderText* textRenderer = this->textRenderer();
    ASSERT(textRenderer);

    RenderStyle* style = textRenderer->style();
    ASSERT(style);

    const Font& font = style->font();
    const UChar* characters = textRenderer->characters();

    TextDirection textDirection = direction();
    unsigned startPosition = start();
    unsigned endPosition = end();
    unsigned length = len();

    const SVGRenderStyle* svgStyle = style->svgStyle();
    bool isVerticalText = isVerticalWritingMode(svgStyle);

    int charsConsumed = 0;
    for (unsigned i = 0; i < length; i += charsConsumed) {
        SVGChar svgChar;

        if (info.inPathLayout())
            svgChar.pathData = SVGCharOnPath::create();

        float glyphWidth = 0.0f;
        float glyphHeight = 0.0f;
        String glyphName;
        String unicodeString;
        measureCharacter(style, i, charsConsumed, glyphName, unicodeString, glyphWidth, glyphHeight);

        bool assignedX = false;
        bool assignedY = false;

        if (info.xValueAvailable() && (!info.inPathLayout() || (info.inPathLayout() && !isVerticalText))) {
            if (!isVerticalText)
                svgChar.newTextChunk = true;

            assignedX = true;
            svgChar.drawnSeperated = true;
            info.curx = info.xValueNext();
        }

        if (info.yValueAvailable() && (!info.inPathLayout() || (info.inPathLayout() && isVerticalText))) {
            if (isVerticalText)
                svgChar.newTextChunk = true;

            assignedY = true;
            svgChar.drawnSeperated = true;
            info.cury = info.yValueNext();
        }

        float dx = 0.0f;
        float dy = 0.0f;

        // Apply x-axis shift
        if (info.dxValueAvailable()) {
            svgChar.drawnSeperated = true;

            dx = info.dxValueNext();
            info.dx += dx;

            if (!info.inPathLayout())
                info.curx += dx;
        }

        // Apply y-axis shift
        if (info.dyValueAvailable()) {
            svgChar.drawnSeperated = true;

            dy = info.dyValueNext();
            info.dy += dy;

            if (!info.inPathLayout())
                info.cury += dy;
        }

        // Take letter & word spacing and kerning into account
        float spacing = font.letterSpacing() + calculateCSSKerning(style);

        const UChar* currentCharacter = characters + (textDirection == RTL ? endPosition - i : startPosition + i);
        const UChar* lastCharacter = 0;

        if (textDirection == RTL) {
            if (i < endPosition)
                lastCharacter = characters + endPosition - i +  1;
        } else {
            if (i > 0)
                lastCharacter = characters + startPosition + i - 1;
        }

        // FIXME: SVG Kerning doesn't get applied on texts on path.
        bool appliedSVGKerning = applySVGKerning(info, style, lastGlyph, unicodeString, glyphName, isVerticalText);
        if (info.nextDrawnSeperated || spacing != 0.0f || appliedSVGKerning) {
            info.nextDrawnSeperated = false;
            svgChar.drawnSeperated = true;
        }

        if (currentCharacter && Font::treatAsSpace(*currentCharacter) && lastCharacter && !Font::treatAsSpace(*lastCharacter)) {
            spacing += font.wordSpacing();

            if (spacing != 0.0f && !info.inPathLayout())
                info.nextDrawnSeperated = true;
        }

        float orientationAngle = glyphOrientationToAngle(svgStyle, isVerticalText, *currentCharacter);

        float xOrientationShift = 0.0f;
        float yOrientationShift = 0.0f;
        float glyphAdvance = applyGlyphAdvanceAndShiftRespectingOrientation(isVerticalText, orientationAngle, glyphWidth, glyphHeight,
                                                                            font, svgChar, xOrientationShift, yOrientationShift);

        // Handle textPath layout mode
        if (info.inPathLayout()) {
            float extraAdvance = isVerticalText ? dy : dx;
            float newOffset = FLT_MIN;

            if (assignedX && !isVerticalText)
                newOffset = info.curx;
            else if (assignedY && isVerticalText)
                newOffset = info.cury;

            float correctedGlyphAdvance = glyphAdvance;

            // Handle lengthAdjust="spacingAndGlyphs" by specifying per-character scale operations
            if (info.pathTextLength && info.pathChunkLength) { 
                if (isVerticalText) {
                    svgChar.pathData->yScale = info.pathChunkLength / info.pathTextLength;
                    spacing *= svgChar.pathData->yScale;
                    correctedGlyphAdvance *= svgChar.pathData->yScale;
                } else {
                    svgChar.pathData->xScale = info.pathChunkLength / info.pathTextLength;
                    spacing *= svgChar.pathData->xScale;
                    correctedGlyphAdvance *= svgChar.pathData->xScale;
                }
            }

            // Handle letter & word spacing on text path
            float pathExtraAdvance = info.pathExtraAdvance;
            info.pathExtraAdvance += spacing;

            svgChar.pathData->hidden = !info.nextPathLayoutPointAndAngle(correctedGlyphAdvance, extraAdvance, newOffset);
            svgChar.drawnSeperated = true;

            info.pathExtraAdvance = pathExtraAdvance;
        }

        // Apply rotation
        if (info.angleValueAvailable())
            info.angle = info.angleValueNext();

        // Apply baseline-shift
        if (info.baselineShiftValueAvailable()) {
            svgChar.drawnSeperated = true;
            float shift = info.baselineShiftValueNext();

            if (isVerticalText)
                info.shiftx += shift;
            else
                info.shifty -= shift;
        }

        // Take dominant-baseline / alignment-baseline into account
        yOrientationShift += alignmentBaselineToShift(isVerticalText, textRenderer, font);

        svgChar.x = info.curx;
        svgChar.y = info.cury;
        svgChar.angle = info.angle;

        // For text paths any shift (dx/dy/baseline-shift) has to be applied after the rotation
        if (!info.inPathLayout()) {
            svgChar.x += info.shiftx + xOrientationShift;
            svgChar.y += info.shifty + yOrientationShift;

            if (orientationAngle != 0.0f)
                svgChar.angle += orientationAngle;

            if (svgChar.angle != 0.0f)
                svgChar.drawnSeperated = true;
        } else {
            svgChar.pathData->orientationAngle = orientationAngle;

            if (isVerticalText)
                svgChar.angle -= 90.0f;

            svgChar.pathData->xShift = info.shiftx + xOrientationShift;
            svgChar.pathData->yShift = info.shifty + yOrientationShift;

            // Translate to glyph midpoint
            if (isVerticalText) {
                svgChar.pathData->xShift += info.dx;
                svgChar.pathData->yShift -= glyphAdvance / 2.0f;
            } else {
                svgChar.pathData->xShift -= glyphAdvance / 2.0f;
                svgChar.pathData->yShift += info.dy;
            }
        }

        // Advance to new position
        if (isVerticalText) {
            svgChar.drawnSeperated = true;
            info.cury += glyphAdvance + spacing;
        } else
            info.curx += glyphAdvance + spacing;

        // Advance to next character group
        for (int k = 0; k < charsConsumed; ++k) {
            info.svgChars.append(svgChar);
            info.processedSingleCharacter();
            svgChar.drawnSeperated = false;
            svgChar.newTextChunk = false;
        }
    }
}

FloatRect SVGInlineTextBox::calculateGlyphBoundaries(RenderStyle* style, int position, const SVGChar& character) const
{
    int charsConsumed = 0;
    String glyphName;
    String unicodeString;
    float glyphWidth = 0.0f;
    float glyphHeight = 0.0f;
    measureCharacter(style, position, charsConsumed, glyphName, unicodeString, glyphWidth, glyphHeight);

    FloatRect glyphRect(character.x, character.y - style->font().ascent(), glyphWidth, glyphHeight);
    glyphRect = character.characterTransform().mapRect(glyphRect);
    if (m_chunkTransformation.isIdentity())
        return glyphRect;

    return m_chunkTransformation.mapRect(glyphRect);
}

IntRect SVGInlineTextBox::calculateBoundaries() const
{
    FloatRect textRect;
    int baseline = baselinePosition(true);

    const Vector<SVGTextChunkPart>::const_iterator end = m_svgTextChunkParts.end();
    for (Vector<SVGTextChunkPart>::const_iterator it = m_svgTextChunkParts.begin(); it != end; ++it)
        textRect.unite(it->firstCharacter->characterTransform().mapRect(FloatRect(it->firstCharacter->x, it->firstCharacter->y - baseline, it->width, it->height)));

    return enclosingIntRect(m_chunkTransformation.mapRect(textRect));
}

} // namespace WebCore

#endif
