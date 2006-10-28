/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *               2006 Alexander Kellett <lypanov@kde.org>
 *               2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>.
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
#ifdef SVG_SUPPORT
#include "RenderSVGText.h"

#include "GraphicsContext.h"
#include "KCanvasRenderingStyle.h"
#include "KRenderingDevice.h"
#include "SVGLengthList.h"
#include "SVGTextElement.h"
#include "SVGRootInlineBox.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

RenderSVGText::RenderSVGText(SVGTextElement* node) 
    : RenderBlock(node)
{
}

void RenderSVGText::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    AffineTransform transform = localTransform();
    r = transform.mapRect(r);
    RenderContainer::computeAbsoluteRepaintRect(r, f);
    r = transform.invert().mapRect(r);
}

bool RenderSVGText::requiresLayer()
{
    return false;
}

void RenderSVGText::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = m_absoluteBounds;
    SVGTextElement* text = static_cast<SVGTextElement*>(element());
    //FIXME:  need to allow floating point positions
    int xOffset = (int)(text->x()->getFirst() ? text->x()->getFirst()->value() : 0);
    int yOffset = (int)(text->y()->getFirst() ? text->y()->getFirst()->value() : 0);
    setPos(xOffset, yOffset);
    RenderBlock::layout();
    
    m_absoluteBounds = getAbsoluteRepaintRect();

    bool repainted = false;
    if (checkForRepaint)
        repainted = repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}

InlineBox* RenderSVGText::createInlineBox(bool makePlaceHolderBox, bool isRootLineBox, bool isOnlyRun)
{
    assert(!isInlineFlow());
    InlineFlowBox* flowBox = new (renderArena()) SVGRootInlineBox(this);
    
    if (!m_firstLineBox)
        m_firstLineBox = m_lastLineBox = flowBox;
    else {
        m_lastLineBox->setNextLineBox(flowBox);
        flowBox->setPreviousLineBox(m_lastLineBox);
        m_lastLineBox = flowBox;
    }
    
    return flowBox;
}

bool RenderSVGText::nodeAtPoint(HitTestResult& info, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    AffineTransform totalTransform = absoluteTransform();
    double localX, localY;
    totalTransform.invert().map(_x, _y, &localX, &localY);
    return RenderBlock::nodeAtPoint(info, (int)localX, (int)localY, _tx, _ty, hitTestAction);
}

void RenderSVGText::absoluteRects(Vector<IntRect>& rects, int tx, int ty)
{
    if (!m_firstLineBox)
        return;
    AffineTransform boxTransform = m_firstLineBox->object()->absoluteTransform();
    FloatRect boxRect = FloatRect(xPos() + m_firstLineBox->xPos(), yPos() + m_firstLineBox->yPos(),
                                  m_firstLineBox->width(), m_firstLineBox->height());
    rects.append(enclosingIntRect(boxTransform.mapRect(boxRect)));
}

void RenderSVGText::paint(PaintInfo& paintInfo, int _tx, int _ty)
{    
    KRenderingDevice* device = renderingDevice();
    RenderObject::PaintInfo pi = paintInfo;
    OwnPtr<GraphicsContext> c(device->currentContext()->createGraphicsContext());
    pi.p = c.get();
    pi.r = (absoluteTransform()).invert().mapRect(paintInfo.r);
    RenderBlock::paint(pi, _tx, _ty);
}

FloatRect RenderSVGText::relativeBBox(bool includeStroke) const
{
    FloatRect boundsRect;
    InlineBox* box = m_firstLineBox;
    if (box) {
        boundsRect = FloatRect(xPos() + box->xPos(), yPos() + box->yPos(), box->width(), box->height());
        boundsRect = localTransform().mapRect(boundsRect);
    }
    return boundsRect;
}

}

#endif // SVG_SUPPORT
