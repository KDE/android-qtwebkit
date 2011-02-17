/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef InlineFlowBox_h
#define InlineFlowBox_h

#include "InlineBox.h"
#include "RenderOverflow.h"
#include "ShadowData.h"

namespace WebCore {

class HitTestRequest;
class HitTestResult;
class InlineTextBox;
class RenderLineBoxList;
class VerticalPositionCache;

typedef HashMap<const InlineTextBox*, pair<Vector<const SimpleFontData*>, GlyphOverflow> > GlyphOverflowAndFallbackFontsMap;

class InlineFlowBox : public InlineBox {
public:
    InlineFlowBox(RenderObject* obj)
        : InlineBox(obj)
        , m_firstChild(0)
        , m_lastChild(0)
        , m_prevLineBox(0)
        , m_nextLineBox(0)
        , m_includeLogicalLeftEdge(false)
        , m_includeLogicalRightEdge(false)
#ifndef NDEBUG
        , m_hasBadChildList(false)
#endif
    {
        // Internet Explorer and Firefox always create a marker for list items, even when the list-style-type is none.  We do not make a marker
        // in the list-style-type: none case, since it is wasteful to do so.  However, in order to match other browsers we have to pretend like
        // an invisible marker exists.  The side effect of having an invisible marker is that the quirks mode behavior of shrinking lines with no
        // text children must not apply.  This change also means that gaps will exist between image bullet list items.  Even when the list bullet
        // is an image, the line is still considered to be immune from the quirk.
        m_hasTextChildren = obj->style()->display() == LIST_ITEM;
    }

#ifndef NDEBUG
    virtual ~InlineFlowBox();
#endif

    InlineFlowBox* prevLineBox() const { return m_prevLineBox; }
    InlineFlowBox* nextLineBox() const { return m_nextLineBox; }
    void setNextLineBox(InlineFlowBox* n) { m_nextLineBox = n; }
    void setPreviousLineBox(InlineFlowBox* p) { m_prevLineBox = p; }

    InlineBox* firstChild() const { checkConsistency(); return m_firstChild; }
    InlineBox* lastChild() const { checkConsistency(); return m_lastChild; }

    virtual bool isLeaf() const { return false; }
    
    InlineBox* firstLeafChild() const;
    InlineBox* lastLeafChild() const;

    virtual void setConstructed()
    {
        InlineBox::setConstructed();
        for (InlineBox* child = firstChild(); child; child = child->next())
            child->setConstructed();
    }

    void addToLine(InlineBox* child);
    virtual void deleteLine(RenderArena*);
    virtual void extractLine();
    virtual void attachLine();
    virtual void adjustPosition(float dx, float dy);

    virtual void extractLineBoxFromRenderObject();
    virtual void attachLineBoxToRenderObject();
    virtual void removeLineBoxFromRenderObject();

    virtual void clearTruncation();

    IntRect roundedFrameRect() const;
    
    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty);
    virtual void paintMask(PaintInfo&, int tx, int ty);
    void paintFillLayers(const PaintInfo&, const Color&, const FillLayer*, int tx, int ty, int w, int h, CompositeOperator = CompositeSourceOver);
    void paintFillLayer(const PaintInfo&, const Color&, const FillLayer*, int tx, int ty, int w, int h, CompositeOperator = CompositeSourceOver);
    void paintBoxShadow(GraphicsContext*, RenderStyle*, ShadowStyle, int tx, int ty, int w, int h);
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty);

    virtual RenderLineBoxList* rendererLineBoxes() const;

    // logicalLeft = left in a horizontal line and top in a vertical line.
    int marginBorderPaddingLogicalLeft() const { return marginLogicalLeft() + borderLogicalLeft() + paddingLogicalLeft(); }
    int marginBorderPaddingLogicalRight() const { return marginLogicalRight() + borderLogicalRight() + paddingLogicalRight(); }
    int marginLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? boxModelObject()->marginLeft() : boxModelObject()->marginTop();
    }
    int marginLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? boxModelObject()->marginRight() : boxModelObject()->marginBottom();
    }
    int borderLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? renderer()->style()->borderLeftWidth() : renderer()->style()->borderTopWidth();
    }
    int borderLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? renderer()->style()->borderRightWidth() : renderer()->style()->borderBottomWidth();
    }
    int paddingLogicalLeft() const
    {
        if (!includeLogicalLeftEdge())
            return 0;
        return isHorizontal() ? boxModelObject()->paddingLeft() : boxModelObject()->paddingTop();
    }
    int paddingLogicalRight() const
    {
        if (!includeLogicalRightEdge())
            return 0;
        return isHorizontal() ? boxModelObject()->paddingRight() : boxModelObject()->paddingBottom();
    }

    bool includeLogicalLeftEdge() const { return m_includeLogicalLeftEdge; }
    bool includeLogicalRightEdge() const { return m_includeLogicalRightEdge; }
    void setEdges(bool includeLeft, bool includeRight)
    {
        m_includeLogicalLeftEdge = includeLeft;
        m_includeLogicalRightEdge = includeRight;
    }

    // Helper functions used during line construction and placement.
    void determineSpacingForFlowBoxes(bool lastLine, RenderObject* endObject);
    int getFlowSpacingLogicalWidth();
    bool onEndChain(RenderObject* endObject);
    float placeBoxesInInlineDirection(float logicalLeft, bool& needsWordSpacing, GlyphOverflowAndFallbackFontsMap&);
    void computeLogicalBoxHeights(int& maxPositionTop, int& maxPositionBottom,
                                  int& maxAscent, int& maxDescent, bool& setMaxAscent, bool& setMaxDescent,
                                  bool strictMode, GlyphOverflowAndFallbackFontsMap&, FontBaseline, VerticalPositionCache&);
    void adjustMaxAscentAndDescent(int& maxAscent, int& maxDescent,
                                   int maxPositionTop, int maxPositionBottom);
    void placeBoxesInBlockDirection(int logicalTop, int maxHeight, int maxAscent, bool strictMode, int& lineTop, int& lineBottom, bool& setLineTop,
                                    int& lineTopIncludingMargins, int& lineBottomIncludingMargins, bool& hasAnnotationsBefore, bool& hasAnnotationsAfter, FontBaseline);
    void flipLinesInBlockDirection(int lineTop, int lineBottom);
    bool requiresIdeographicBaseline(const GlyphOverflowAndFallbackFontsMap&) const;

    int computeOverAnnotationAdjustment(int allowedPosition) const;
    int computeUnderAnnotationAdjustment(int allowedPosition) const;

    void computeOverflow(int lineTop, int lineBottom, bool strictMode, GlyphOverflowAndFallbackFontsMap&);
    
    void removeChild(InlineBox* child);

    virtual RenderObject::SelectionState selectionState();

    virtual bool canAccommodateEllipsis(bool ltr, int blockEdge, int ellipsisWidth);
    virtual float placeEllipsisBox(bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, bool&);

    bool hasTextChildren() const { return m_hasTextChildren; }

    void checkConsistency() const;
    void setHasBadChildList();

    // Line visual and layout overflow are in the coordinate space of the block.  This means that they aren't purely physical directions.
    // For horizontal-tb and vertical-lr they will match physical directions, but for horizontal-bt and vertical-rl, the top/bottom and left/right
    // respectively are flipped when compared to their physical counterparts.  For example minX is on the left in vertical-lr, but it is on the right in vertical-rl.
    int minYLayoutOverflow() const { return m_overflow ? m_overflow->minYLayoutOverflow() : m_y; }
    int maxYLayoutOverflow() const { return m_overflow ? m_overflow->maxYLayoutOverflow() : ceilf(m_y + height()); }
    int minXLayoutOverflow() const { return m_overflow ? m_overflow->minXLayoutOverflow() : m_x; }
    int maxXLayoutOverflow() const { return m_overflow ? m_overflow->maxXLayoutOverflow() : ceilf(m_x + width()); }
    IntRect layoutOverflowRect() const { return m_overflow ? m_overflow->layoutOverflowRect() : enclosingIntRect(FloatRect(m_x, m_y, width(), height())); }
    int logicalLeftLayoutOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? minXLayoutOverflow() : minYLayoutOverflow(); }
    int logicalRightLayoutOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? maxXLayoutOverflow() : maxYLayoutOverflow(); }
    int logicalTopLayoutOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? minYVisualOverflow() : minXVisualOverflow(); }
    int logicalBottomLayoutOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? maxYLayoutOverflow() : maxXLayoutOverflow(); }
    IntRect logicalLayoutOverflowRect() const
    {
        IntRect result = layoutOverflowRect();
        if (!renderer()->style()->isHorizontalWritingMode())
            result = result.transposedRect();
        return result;
    }

    int minYVisualOverflow() const { return m_overflow ? m_overflow->minYVisualOverflow() : m_y; }
    int maxYVisualOverflow() const { return m_overflow ? m_overflow->maxYVisualOverflow() : ceilf(m_y + height()); }
    int minXVisualOverflow() const { return m_overflow ? m_overflow->minXVisualOverflow() : m_x; }
    int maxXVisualOverflow() const { return m_overflow ? m_overflow->maxXVisualOverflow() : ceilf(m_x + width()); }
    IntRect visualOverflowRect() const { return m_overflow ? m_overflow->visualOverflowRect() : enclosingIntRect(FloatRect(m_x, m_y, width(), height())); }
    int logicalLeftVisualOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? minXVisualOverflow() : minYVisualOverflow(); }
    int logicalRightVisualOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? maxXVisualOverflow() : maxYVisualOverflow(); }
    int logicalTopVisualOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? minYVisualOverflow() : minXVisualOverflow(); }
    int logicalBottomVisualOverflow() const { return renderer()->style()->isHorizontalWritingMode() ? maxYVisualOverflow() : maxXVisualOverflow(); }
    IntRect logicalVisualOverflowRect() const
    {
        IntRect result = visualOverflowRect();
        if (!renderer()->style()->isHorizontalWritingMode())
            result = result.transposedRect();
        return result;
    }

    void setOverflowFromLogicalRects(const IntRect& logicalLayoutOverflow, const IntRect& logicalVisualOverflow);
    void setLayoutOverflow(const IntRect&);
    void setVisualOverflow(const IntRect&);

private:
    void addBoxShadowVisualOverflow(IntRect& logicalVisualOverflow);
    void addTextBoxVisualOverflow(const InlineTextBox*, GlyphOverflowAndFallbackFontsMap&, IntRect& logicalVisualOverflow);
    void addReplacedChildOverflow(const InlineBox*, IntRect& logicalLayoutOverflow, IntRect& logicalVisualOverflow);

protected:
    OwnPtr<RenderOverflow> m_overflow;

    virtual bool isInlineFlowBox() const { return true; }

    InlineBox* m_firstChild;
    InlineBox* m_lastChild;
    
    InlineFlowBox* m_prevLineBox; // The previous box that also uses our RenderObject
    InlineFlowBox* m_nextLineBox; // The next box that also uses our RenderObject

    bool m_includeLogicalLeftEdge : 1;
    bool m_includeLogicalRightEdge : 1;
    bool m_hasTextChildren : 1;

#ifndef NDEBUG
    bool m_hasBadChildList;
#endif
};

#ifdef NDEBUG
inline void InlineFlowBox::checkConsistency() const
{
}
#endif

inline void InlineFlowBox::setHasBadChildList()
{
#ifndef NDEBUG
    m_hasBadChildList = true;
#endif
}

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::InlineFlowBox*);
#endif

#endif // InlineFlowBox_h
