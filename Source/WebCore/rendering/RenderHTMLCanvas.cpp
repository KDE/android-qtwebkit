/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "RenderHTMLCanvas.h"

#include "CanvasRenderingContext.h"
#include "Document.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "HTMLCanvasElement.h"
#include "HTMLNames.h"
#include "PaintInfo.h"
#include "RenderView.h"

namespace WebCore {

using namespace HTMLNames;

RenderHTMLCanvas::RenderHTMLCanvas(HTMLCanvasElement* element)
    : RenderReplaced(element, element->size())
{
    view()->frameView()->setIsVisuallyNonEmpty();
}

bool RenderHTMLCanvas::requiresLayer() const
{
    if (RenderReplaced::requiresLayer())
        return true;
    
    HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(node());
    return canvas && canvas->renderingContext() && canvas->renderingContext()->isAccelerated();
}

void RenderHTMLCanvas::paintReplaced(PaintInfo& paintInfo, int tx, int ty)
{
    IntRect rect = contentBoxRect();
    rect.move(tx, ty);
    static_cast<HTMLCanvasElement*>(node())->paint(paintInfo.context, rect);
}

void RenderHTMLCanvas::canvasSizeChanged()
{
    IntSize canvasSize = static_cast<HTMLCanvasElement*>(node())->size();
    IntSize zoomedSize(canvasSize.width() * style()->effectiveZoom(), canvasSize.height() * style()->effectiveZoom());

    if (zoomedSize == intrinsicSize())
        return;

    setIntrinsicSize(zoomedSize);

    if (!parent())
        return;

    if (!preferredLogicalWidthsDirty())
        setPreferredLogicalWidthsDirty(true);

    IntSize oldSize = size();
    computeLogicalWidth();
    computeLogicalHeight();
    if (oldSize == size())
        return;

    if (!selfNeedsLayout())
        setNeedsLayout(true);
}

void RenderHTMLCanvas::recursiveSetNoNeedsLayout(RenderObject* obj)
{
    obj->setNeedsLayout(false);
    for (RenderObject* child = obj->firstChild(); child; child = child->nextSibling())
        recursiveSetNoNeedsLayout(child);
}

void RenderHTMLCanvas::layout()
{
    recursiveSetNoNeedsLayout(this);
    setNeedsLayout(true);
    RenderReplaced::layout();
}

bool RenderHTMLCanvas::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int xPos, int yPos, int tx, int ty, HitTestAction action)
{
    UNUSED_PARAM(request);
    tx += x();
    ty += y();

    // Ignore children (accessible fallback content that might be focusable but not clickable)
    // but do test our own bounds for a hit.
    IntRect boundsRect = IntRect(tx, ty, width(), height());
    if (visibleToHitTesting() && action == HitTestForeground && boundsRect.intersects(result.rectForPoint(xPos, yPos))) {
        updateHitTestResult(result, IntPoint(xPos - tx, yPos - ty));
        if (!result.addNodeToRectBasedTestResult(node(), xPos, yPos, boundsRect))
            return true;
    }

    return false;
}

} // namespace WebCore
