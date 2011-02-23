/*
 * Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "RootInlineBox.h"

#include "BidiResolver.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "EllipsisBox.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderArena.h"
#include "RenderBlock.h"

using namespace std;

namespace WebCore {
    
typedef WTF::HashMap<const RootInlineBox*, EllipsisBox*> EllipsisBoxMap;
static EllipsisBoxMap* gEllipsisBoxMap = 0;

RootInlineBox::RootInlineBox(RenderBlock* block)
    : InlineFlowBox(block)
    , m_lineBreakObj(0)
    , m_lineBreakPos(0)
    , m_lineTop(0)
    , m_lineBottom(0)
    , m_paginationStrut(0)
    , m_blockLogicalHeight(0)
    , m_baselineType(AlphabeticBaseline)
    , m_hasAnnotationsBefore(false)
    , m_hasAnnotationsAfter(false)
{
    setIsHorizontal(block->style()->isHorizontalWritingMode());
}


void RootInlineBox::destroy(RenderArena* arena)
{
    detachEllipsisBox(arena);
    InlineFlowBox::destroy(arena);
}

void RootInlineBox::detachEllipsisBox(RenderArena* arena)
{
    if (hasEllipsisBox()) {
        EllipsisBox* box = gEllipsisBoxMap->take(this);
        box->setParent(0);
        box->destroy(arena);
        setHasEllipsisBox(false);
    }
}

RenderLineBoxList* RootInlineBox::rendererLineBoxes() const
{
    return block()->lineBoxes();
}

void RootInlineBox::clearTruncation()
{
    if (hasEllipsisBox()) {
        detachEllipsisBox(renderer()->renderArena());
        InlineFlowBox::clearTruncation();
    }
}

bool RootInlineBox::lineCanAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth)
{
    // First sanity-check the unoverflowed width of the whole line to see if there is sufficient room.
    int delta = ltr ? lineBoxEdge - blockEdge : blockEdge - lineBoxEdge;
    if (logicalWidth() - delta < ellipsisWidth)
        return false;

    // Next iterate over all the line boxes on the line.  If we find a replaced element that intersects
    // then we refuse to accommodate the ellipsis.  Otherwise we're ok.
    return InlineFlowBox::canAccommodateEllipsis(ltr, blockEdge, ellipsisWidth);
}

void RootInlineBox::placeEllipsis(const AtomicString& ellipsisStr,  bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth,
                                  InlineBox* markupBox)
{
    // Create an ellipsis box.
    EllipsisBox* ellipsisBox = new (renderer()->renderArena()) EllipsisBox(renderer(), ellipsisStr, this,
                                                              ellipsisWidth - (markupBox ? markupBox->logicalWidth() : 0), logicalHeight(),
                                                              y(), !prevRootBox(), isHorizontal(), markupBox);
    
    if (!gEllipsisBoxMap)
        gEllipsisBoxMap = new EllipsisBoxMap();
    gEllipsisBoxMap->add(this, ellipsisBox);
    setHasEllipsisBox(true);

    // FIXME: Do we need an RTL version of this?
    if (ltr && (x() + logicalWidth() + ellipsisWidth) <= blockRightEdge) {
        ellipsisBox->m_x = x() + logicalWidth();
        return;
    }

    // Now attempt to find the nearest glyph horizontally and place just to the right (or left in RTL)
    // of that glyph.  Mark all of the objects that intersect the ellipsis box as not painting (as being
    // truncated).
    bool foundBox = false;
    ellipsisBox->m_x = placeEllipsisBox(ltr, blockLeftEdge, blockRightEdge, ellipsisWidth, foundBox);
}

float RootInlineBox::placeEllipsisBox(bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, bool& foundBox)
{
    float result = InlineFlowBox::placeEllipsisBox(ltr, blockLeftEdge, blockRightEdge, ellipsisWidth, foundBox);
    if (result == -1)
        result = ltr ? blockRightEdge - ellipsisWidth : blockLeftEdge;
    return result;
}

void RootInlineBox::paintEllipsisBox(PaintInfo& paintInfo, int tx, int ty) const
{
    if (hasEllipsisBox() && paintInfo.shouldPaintWithinRoot(renderer()) && renderer()->style()->visibility() == VISIBLE
            && paintInfo.phase == PaintPhaseForeground)
        ellipsisBox()->paint(paintInfo, tx, ty);
}

#if PLATFORM(MAC)

void RootInlineBox::addHighlightOverflow()
{
    Frame* frame = renderer()->frame();
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;

    // Highlight acts as a selection inflation.
    FloatRect rootRect(0, selectionTop(), logicalWidth(), selectionHeight());
    IntRect inflatedRect = enclosingIntRect(page->chrome()->client()->customHighlightRect(renderer()->node(), renderer()->style()->highlight(), rootRect));
    setOverflowFromLogicalRects(inflatedRect, inflatedRect);
}

void RootInlineBox::paintCustomHighlight(PaintInfo& paintInfo, int tx, int ty, const AtomicString& highlightType)
{
    if (!paintInfo.shouldPaintWithinRoot(renderer()) || renderer()->style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseForeground)
        return;

    Frame* frame = renderer()->frame();
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;

    // Get the inflated rect so that we can properly hit test.
    FloatRect rootRect(tx + x(), ty + selectionTop(), logicalWidth(), selectionHeight());
    FloatRect inflatedRect = page->chrome()->client()->customHighlightRect(renderer()->node(), highlightType, rootRect);
    if (inflatedRect.intersects(paintInfo.rect))
        page->chrome()->client()->paintCustomHighlight(renderer()->node(), highlightType, rootRect, rootRect, false, true);
}

#endif

void RootInlineBox::paint(PaintInfo& paintInfo, int tx, int ty)
{
    InlineFlowBox::paint(paintInfo, tx, ty);
    paintEllipsisBox(paintInfo, tx, ty);
#if PLATFORM(MAC)
    RenderStyle* styleToUse = renderer()->style(m_firstLine);
    if (styleToUse->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
        paintCustomHighlight(paintInfo, tx, ty, styleToUse->highlight());
#endif
}

bool RootInlineBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    if (hasEllipsisBox() && visibleToHitTesting()) {
        if (ellipsisBox()->nodeAtPoint(request, result, x, y, tx, ty)) {
            renderer()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
            return true;
        }
    }
    return InlineFlowBox::nodeAtPoint(request, result, x, y, tx, ty);
}

void RootInlineBox::adjustPosition(float dx, float dy)
{
    InlineFlowBox::adjustPosition(dx, dy);
    int blockDirectionDelta = isHorizontal() ? dy : dx; // The block direction delta will always be integral.
    m_lineTop += blockDirectionDelta;
    m_lineBottom += blockDirectionDelta;
    m_blockLogicalHeight += blockDirectionDelta;
}

void RootInlineBox::childRemoved(InlineBox* box)
{
    if (box->renderer() == m_lineBreakObj)
        setLineBreakInfo(0, 0, BidiStatus());

    for (RootInlineBox* prev = prevRootBox(); prev && prev->lineBreakObj() == box->renderer(); prev = prev->prevRootBox()) {
        prev->setLineBreakInfo(0, 0, BidiStatus());
        prev->markDirty();
    }
}

int RootInlineBox::alignBoxesInBlockDirection(int heightOfBlock, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache)
{
#if ENABLE(SVG)
    // SVG will handle vertical alignment on its own.
    if (isSVGRootInlineBox())
        return 0;
#endif

    int maxPositionTop = 0;
    int maxPositionBottom = 0;
    int maxAscent = 0;
    int maxDescent = 0;
    bool setMaxAscent = false;
    bool setMaxDescent = false;

    // Figure out if we're in no-quirks mode.
    bool noQuirksMode = renderer()->document()->inNoQuirksMode();

    m_baselineType = requiresIdeographicBaseline(textBoxDataMap) ? IdeographicBaseline : AlphabeticBaseline;

    computeLogicalBoxHeights(maxPositionTop, maxPositionBottom, maxAscent, maxDescent, setMaxAscent, setMaxDescent, noQuirksMode,
                             textBoxDataMap, baselineType(), verticalPositionCache);

    if (maxAscent + maxDescent < max(maxPositionTop, maxPositionBottom))
        adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);

    int maxHeight = maxAscent + maxDescent;
    int lineTop = heightOfBlock;
    int lineBottom = heightOfBlock;
    int lineTopIncludingMargins = heightOfBlock;
    int lineBottomIncludingMargins = heightOfBlock;
    bool setLineTop = false;
    bool hasAnnotationsBefore = false;
    bool hasAnnotationsAfter = false;
    placeBoxesInBlockDirection(heightOfBlock, maxHeight, maxAscent, noQuirksMode, lineTop, lineBottom, setLineTop,
                               lineTopIncludingMargins, lineBottomIncludingMargins, hasAnnotationsBefore, hasAnnotationsAfter, baselineType());
    m_hasAnnotationsBefore = hasAnnotationsBefore;
    m_hasAnnotationsAfter = hasAnnotationsAfter;
    setLineTopBottomPositions(lineTop, lineBottom);

    int annotationsAdjustment = beforeAnnotationsAdjustment();
    if (annotationsAdjustment) {
        // FIXME: Need to handle pagination here. We might have to move to the next page/column as a result of the
        // ruby expansion.
        adjustBlockDirectionPosition(annotationsAdjustment);
        heightOfBlock += annotationsAdjustment;
    }

    maxHeight = max(0, maxHeight);

    return heightOfBlock + maxHeight;
}

int RootInlineBox::beforeAnnotationsAdjustment() const
{
    int result = 0;

    if (!renderer()->style()->isFlippedLinesWritingMode()) {
        // Annotations under the previous line may push us down.
        if (prevRootBox() && prevRootBox()->hasAnnotationsAfter())
            result = prevRootBox()->computeUnderAnnotationAdjustment(lineTop());

        if (!hasAnnotationsBefore())
            return result;

        // Annotations over this line may push us further down.
        int highestAllowedPosition = prevRootBox() ? min(prevRootBox()->lineBottom(), lineTop()) + result : block()->borderBefore();
        result = computeOverAnnotationAdjustment(highestAllowedPosition);
    } else {
        // Annotations under this line may push us up.
        if (hasAnnotationsBefore())
            result = computeUnderAnnotationAdjustment(prevRootBox() ? prevRootBox()->lineBottom() : block()->borderBefore());

        if (!prevRootBox() || !prevRootBox()->hasAnnotationsAfter())
            return result;

        // We have to compute the expansion for annotations over the previous line to see how much we should move.
        int lowestAllowedPosition = max(prevRootBox()->lineBottom(), lineTop()) - result;
        result = prevRootBox()->computeOverAnnotationAdjustment(lowestAllowedPosition);
    }

    return result;
}

GapRects RootInlineBox::lineSelectionGap(RenderBlock* rootBlock, const IntPoint& rootBlockPhysicalPosition, const IntSize& offsetFromRootBlock, 
                                         int selTop, int selHeight, const PaintInfo* paintInfo)
{
    RenderObject::SelectionState lineState = selectionState();

    bool leftGap, rightGap;
    block()->getSelectionGapInfo(lineState, leftGap, rightGap);

    GapRects result;

    InlineBox* firstBox = firstSelectedBox();
    InlineBox* lastBox = lastSelectedBox();
    if (leftGap)
        result.uniteLeft(block()->logicalLeftSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock,
                                                          firstBox->parent()->renderer(), firstBox->logicalLeft(), selTop, selHeight, paintInfo));
    if (rightGap)
        result.uniteRight(block()->logicalRightSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock,
                                                            lastBox->parent()->renderer(), lastBox->logicalRight(), selTop, selHeight, paintInfo));

    // When dealing with bidi text, a non-contiguous selection region is possible.
    // e.g. The logical text aaaAAAbbb (capitals denote RTL text and non-capitals LTR) is layed out
    // visually as 3 text runs |aaa|bbb|AAA| if we select 4 characters from the start of the text the
    // selection will look like (underline denotes selection):
    // |aaa|bbb|AAA|
    //  ___       _
    // We can see that the |bbb| run is not part of the selection while the runs around it are.
    if (firstBox && firstBox != lastBox) {
        // Now fill in any gaps on the line that occurred between two selected elements.
        int lastLogicalLeft = firstBox->logicalRight();
        bool isPreviousBoxSelected = firstBox->selectionState() != RenderObject::SelectionNone;
        for (InlineBox* box = firstBox->nextLeafChild(); box; box = box->nextLeafChild()) {
            if (box->selectionState() != RenderObject::SelectionNone) {
                IntRect logicalRect(lastLogicalLeft, selTop, box->logicalLeft() - lastLogicalLeft, selHeight);
                logicalRect.move(renderer()->style()->isHorizontalWritingMode() ? offsetFromRootBlock : IntSize(offsetFromRootBlock.height(), offsetFromRootBlock.width()));
                IntRect gapRect = rootBlock->logicalRectToPhysicalRect(rootBlockPhysicalPosition, logicalRect);
                if (isPreviousBoxSelected && gapRect.width() > 0 && gapRect.height() > 0) {
                    if (paintInfo && box->parent()->renderer()->style()->visibility() == VISIBLE)
                        paintInfo->context->fillRect(gapRect, box->parent()->renderer()->selectionBackgroundColor(), box->parent()->renderer()->style()->colorSpace());
                    // VisibleSelection may be non-contiguous, see comment above.
                    result.uniteCenter(gapRect);
                }
                lastLogicalLeft = box->logicalRight();
            }
            if (box == lastBox)
                break;
            isPreviousBoxSelected = box->selectionState() != RenderObject::SelectionNone;
        }
    }

    return result;
}

void RootInlineBox::setHasSelectedChildren(bool b)
{
    if (m_hasSelectedChildren == b)
        return;
    m_hasSelectedChildren = b;
}

RenderObject::SelectionState RootInlineBox::selectionState()
{
    // Walk over all of the selected boxes.
    RenderObject::SelectionState state = RenderObject::SelectionNone;
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild()) {
        RenderObject::SelectionState boxState = box->selectionState();
        if ((boxState == RenderObject::SelectionStart && state == RenderObject::SelectionEnd) ||
            (boxState == RenderObject::SelectionEnd && state == RenderObject::SelectionStart))
            state = RenderObject::SelectionBoth;
        else if (state == RenderObject::SelectionNone ||
                 ((boxState == RenderObject::SelectionStart || boxState == RenderObject::SelectionEnd) &&
                  (state == RenderObject::SelectionNone || state == RenderObject::SelectionInside)))
            state = boxState;
        if (state == RenderObject::SelectionBoth)
            break;
    }

    return state;
}

InlineBox* RootInlineBox::firstSelectedBox()
{
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild()) {
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    }

    return 0;
}

InlineBox* RootInlineBox::lastSelectedBox()
{
    for (InlineBox* box = lastLeafChild(); box; box = box->prevLeafChild()) {
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    }

    return 0;
}

int RootInlineBox::selectionTop() const
{
    int selectionTop = m_lineTop;

    if (m_hasAnnotationsBefore)
        selectionTop -= !renderer()->style()->isFlippedLinesWritingMode() ? computeOverAnnotationAdjustment(m_lineTop) : computeUnderAnnotationAdjustment(m_lineTop);

    if (renderer()->style()->isFlippedLinesWritingMode())
        return selectionTop;

    int prevBottom = prevRootBox() ? prevRootBox()->selectionBottom() : block()->borderBefore() + block()->paddingBefore();
    if (prevBottom < selectionTop && block()->containsFloats()) {
        // This line has actually been moved further down, probably from a large line-height, but possibly because the
        // line was forced to clear floats.  If so, let's check the offsets, and only be willing to use the previous
        // line's bottom if the offsets are greater on both sides.
        int prevLeft = block()->logicalLeftOffsetForLine(prevBottom, false);
        int prevRight = block()->logicalRightOffsetForLine(prevBottom, false);
        int newLeft = block()->logicalLeftOffsetForLine(selectionTop, false);
        int newRight = block()->logicalRightOffsetForLine(selectionTop, false);
        if (prevLeft > newLeft || prevRight < newRight)
            return selectionTop;
    }

    return prevBottom;
}

int RootInlineBox::selectionBottom() const
{
    int selectionBottom = m_lineBottom;

    if (m_hasAnnotationsAfter)
        selectionBottom += !renderer()->style()->isFlippedLinesWritingMode() ? computeUnderAnnotationAdjustment(m_lineBottom) : computeOverAnnotationAdjustment(m_lineBottom);

    if (!renderer()->style()->isFlippedLinesWritingMode() || !nextRootBox())
        return selectionBottom;

    int nextTop = nextRootBox()->selectionTop();
    if (nextTop > selectionBottom && block()->containsFloats()) {
        // The next line has actually been moved further over, probably from a large line-height, but possibly because the
        // line was forced to clear floats.  If so, let's check the offsets, and only be willing to use the next
        // line's top if the offsets are greater on both sides.
        int nextLeft = block()->logicalLeftOffsetForLine(nextTop, false);
        int nextRight = block()->logicalRightOffsetForLine(nextTop, false);
        int newLeft = block()->logicalLeftOffsetForLine(selectionBottom, false);
        int newRight = block()->logicalRightOffsetForLine(selectionBottom, false);
        if (nextLeft > newLeft || nextRight < newRight)
            return selectionBottom;
    }

    return nextTop;
}

RenderBlock* RootInlineBox::block() const
{
    return toRenderBlock(renderer());
}

static bool isEditableLeaf(InlineBox* leaf)
{
    return leaf && leaf->renderer() && leaf->renderer()->node() && leaf->renderer()->node()->isContentEditable();
}

InlineBox* RootInlineBox::closestLeafChildForLogicalLeftPosition(int leftPosition, bool onlyEditableLeaves)
{
    InlineBox* firstLeaf = firstLeafChild();
    InlineBox* lastLeaf = lastLeafChild();
    if (firstLeaf == lastLeaf && (!onlyEditableLeaves || isEditableLeaf(firstLeaf)))
        return firstLeaf;

    // Avoid returning a list marker when possible.
    if (leftPosition <= firstLeaf->logicalLeft() && !firstLeaf->renderer()->isListMarker() && (!onlyEditableLeaves || isEditableLeaf(firstLeaf)))
        // The leftPosition coordinate is less or equal to left edge of the firstLeaf.
        // Return it.
        return firstLeaf;

    if (leftPosition >= lastLeaf->logicalRight() && !lastLeaf->renderer()->isListMarker() && (!onlyEditableLeaves || isEditableLeaf(lastLeaf)))
        // The leftPosition coordinate is greater or equal to right edge of the lastLeaf.
        // Return it.
        return lastLeaf;

    InlineBox* closestLeaf = 0;
    for (InlineBox* leaf = firstLeaf; leaf; leaf = leaf->nextLeafChild()) {
        if (!leaf->renderer()->isListMarker() && (!onlyEditableLeaves || isEditableLeaf(leaf))) {
            closestLeaf = leaf;
            if (leftPosition < leaf->logicalRight())
                // The x coordinate is less than the right edge of the box.
                // Return it.
                return leaf;
        }
    }

    return closestLeaf ? closestLeaf : lastLeaf;
}

BidiStatus RootInlineBox::lineBreakBidiStatus() const
{ 
    return BidiStatus(m_lineBreakBidiStatusEor, m_lineBreakBidiStatusLastStrong, m_lineBreakBidiStatusLast, m_lineBreakContext);
}

void RootInlineBox::setLineBreakInfo(RenderObject* obj, unsigned breakPos, const BidiStatus& status)
{
    m_lineBreakObj = obj;
    m_lineBreakPos = breakPos;
    m_lineBreakBidiStatusEor = status.eor;
    m_lineBreakBidiStatusLastStrong = status.lastStrong;
    m_lineBreakBidiStatusLast = status.last;
    m_lineBreakContext = status.context;
}

EllipsisBox* RootInlineBox::ellipsisBox() const
{
    if (!hasEllipsisBox())
        return 0;
    return gEllipsisBoxMap->get(this);
}

void RootInlineBox::removeLineBoxFromRenderObject()
{
    block()->lineBoxes()->removeLineBox(this);
}

void RootInlineBox::extractLineBoxFromRenderObject()
{
    block()->lineBoxes()->extractLineBox(this);
}

void RootInlineBox::attachLineBoxToRenderObject()
{
    block()->lineBoxes()->attachLineBox(this);
}

IntRect RootInlineBox::paddedLayoutOverflowRect(int endPadding) const
{
    IntRect lineLayoutOverflow = layoutOverflowRect();
    if (!endPadding)
        return lineLayoutOverflow;
    
    if (isHorizontal()) {
        if (isLeftToRightDirection())
            lineLayoutOverflow.shiftMaxXEdgeTo(max(lineLayoutOverflow.maxX(), pixelSnappedLogicalRight() + endPadding));
        else
            lineLayoutOverflow.shiftXEdgeTo(min(lineLayoutOverflow.x(), pixelSnappedLogicalLeft() - endPadding));
    } else {
        if (isLeftToRightDirection())
            lineLayoutOverflow.shiftMaxYEdgeTo(max(lineLayoutOverflow.maxY(), pixelSnappedLogicalRight() + endPadding));
        else
            lineLayoutOverflow.shiftYEdgeTo(min(lineLayoutOverflow.y(), pixelSnappedLogicalLeft() - endPadding));
    }
    
    return lineLayoutOverflow;
}

} // namespace WebCore
