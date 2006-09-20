/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "RenderPath.h"

#include <math.h>

#include "GraphicsContext.h"
#include "RenderSVGContainer.h"
#include "KCanvasClipper.h"
#include "KCanvasMasker.h"
#include "KCanvasMarker.h"
#include "KRenderingDevice.h"
#include "KRenderingFillPainter.h"
#include "KRenderingStrokePainter.h"
#include "SVGStyledElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

// RenderPath
RenderPath::RenderPath(RenderStyle* style, SVGStyledElement* node)
    : RenderObject(node)
{
    ASSERT(style != 0);
}

RenderPath::~RenderPath()
{
}

AffineTransform RenderPath::localTransform() const
{
    return m_matrix;
}

void RenderPath::setLocalTransform(const AffineTransform &matrix)
{
    m_matrix = matrix;
}


FloatPoint RenderPath::mapAbsolutePointToLocal(const FloatPoint& point) const
{
    // FIXME: does it make sense to map incoming points with the inverse of the
    // absolute transform? 
    double localX;
    double localY;
    absoluteTransform().invert().map(point.x(), point.y(), &localX, &localY);
    return FloatPoint(localX, localY);
}

bool RenderPath::fillContains(const FloatPoint& point, bool requiresFill) const
{
    if (m_path.isEmpty())
        return false;

    if (requiresFill && !KSVGPainterFactory::fillPaintServer(style(), this))
        return false;

    return path().contains(mapAbsolutePointToLocal(point),
                           KSVGPainterFactory::fillPainter(style(), this).fillRule());
}

FloatRect RenderPath::relativeBBox(bool includeStroke) const
{
    if (m_path.isEmpty())
        return FloatRect();

    if (includeStroke) {
        if (m_strokeBbox.isEmpty())
            m_strokeBbox = strokeBBox();
        return m_strokeBbox;
    }

    if (m_fillBBox.isEmpty())
        m_fillBBox = path().boundingRect();
    return m_fillBBox;
}

void RenderPath::setPath(const Path& newPath)
{
    m_path = newPath;
    m_strokeBbox = FloatRect();
    m_fillBBox = FloatRect();
}

const Path& RenderPath::path() const
{
    return m_path;
}

void RenderPath::layout()
{
    // FIXME: Currently the DOM does all of the % length calculations, so we
    // pretend that one of the attributes of the element has changed on the DOM
    // to force the DOM object to update this render object with new aboslute position values.

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (selfNeedsLayout() && checkForRepaint)
        oldBounds = m_absoluteBounds;

    static_cast<SVGStyledElement*>(element())->notifyAttributeChange();

    m_absoluteBounds = getAbsoluteRepaintRect();

    setWidth(m_absoluteBounds.width());
    setHeight(m_absoluteBounds.height());

    if (selfNeedsLayout() && checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
        
    setNeedsLayout(false);
}

IntRect RenderPath::getAbsoluteRepaintRect()
{
    FloatRect repaintRect = absoluteTransform().mapRect(relativeBBox(true));
    
    // Filters can expand the bounding box
    KCanvasFilter* filter = getFilterById(document(), style()->svgStyle()->filter().substring(1));
    if (filter)
        repaintRect.unite(filter->filterBBoxForItemBBox(repaintRect));
    
    if (!repaintRect.isEmpty())
        repaintRect.inflate(1); // inflate 1 pixel for antialiasing
    return enclosingIntRect(repaintRect);
}

bool RenderPath::requiresLayer()
{
    return false;
}

short RenderPath::lineHeight(bool b, bool isRootLineBox) const
{
    return static_cast<short>(relativeBBox(true).height());
}

short RenderPath::baselinePosition(bool b, bool isRootLineBox) const
{
    return static_cast<short>(relativeBBox(true).height());
}

void RenderPath::paint(PaintInfo &paintInfo, int parentX, int parentY)
{
    // No one should be transforming us via these.
    //ASSERT(parentX == 0);
    //ASSERT(parentY == 0);

    if (paintInfo.p->paintingDisabled() || (paintInfo.phase != PaintPhaseForeground) || style()->visibility() == HIDDEN || path().isEmpty())
        return;
    
    KRenderingDevice* device = renderingDevice();
    KRenderingDeviceContext *context = device->currentContext();
    bool shouldPopContext = false;
    if (context)
        paintInfo.p->save();
    else {
        // Need to set up KCanvas rendering if it hasn't already been done.
        context = paintInfo.p->createRenderingDeviceContext();
        device->pushContext(context);
        shouldPopContext = true;
    }

    context->concatCTM(localTransform());

    // setup to apply filters
    KCanvasFilter* filter = getFilterById(document(), style()->svgStyle()->filter().substring(1));
    if (filter) {
        filter->prepareFilter(relativeBBox(true));
        context = device->currentContext();
    }

    if (KCanvasClipper* clipper = getClipperById(document(), style()->svgStyle()->clipPath().substring(1)))
        clipper->applyClip(relativeBBox(true));

    if (KCanvasMasker* masker = getMaskerById(document(), style()->svgStyle()->maskElement().substring(1)))
        masker->applyMask(relativeBBox(true));

    context->clearPath();
    
    KRenderingPaintServer *fillPaintServer = KSVGPainterFactory::fillPaintServer(style(), this);
    if (fillPaintServer) {
        context->addPath(path());
        fillPaintServer->setActiveClient(this);
        fillPaintServer->draw(context, this, APPLY_TO_FILL);
    }
    KRenderingPaintServer *strokePaintServer = KSVGPainterFactory::strokePaintServer(style(), this);
    if (strokePaintServer) {
        context->addPath(path()); // path is cleared when filled.
        strokePaintServer->setActiveClient(this);
        strokePaintServer->draw(context, this, APPLY_TO_STROKE);
    }

    drawMarkersIfNeeded(paintInfo.p, paintInfo.r, path());

    // actually apply the filter
    if (filter)
        filter->applyFilter(relativeBBox(true));

    // restore drawing state
    if (!shouldPopContext)
        paintInfo.p->restore();
    else {
        device->popContext();
        delete context;
    }
}

void RenderPath::absoluteRects(Vector<IntRect>& rects, int _tx, int _ty)
{
    rects.append(getAbsoluteRepaintRect());
}

RenderPath::PointerEventsHitRules RenderPath::pointerEventsHitRules()
{
    PointerEventsHitRules hitRules;
    
    switch (style()->svgStyle()->pointerEvents())
    {
        case PE_VISIBLE_PAINTED:
            hitRules.requireVisible = true;
            hitRules.requireFill = true;
            hitRules.requireStroke = true;
            hitRules.canHitFill = true;
            hitRules.canHitStroke = true;
            break;
        case PE_VISIBLE_FILL:
            hitRules.requireVisible = true;
            hitRules.canHitFill = true;
            break;
        case PE_VISIBLE_STROKE:
            hitRules.requireVisible = true;
            hitRules.canHitStroke = true;
            break;
        case PE_VISIBLE:
            hitRules.requireVisible = true;
            hitRules.canHitFill = true;
            hitRules.canHitStroke = true;
            break;
        case PE_PAINTED:
            hitRules.requireFill = true;
            hitRules.requireStroke = true;
            hitRules.canHitFill = true;
            hitRules.canHitStroke = true;
            break;
        case PE_FILL:
            hitRules.canHitFill = true;
            break;
        case PE_STROKE:
            hitRules.canHitStroke = true;
            break;
        case PE_ALL:
            hitRules.canHitFill = true;
            hitRules.canHitStroke = true;
            break;
        case PE_NONE:
            // nothing to do here, defaults are all false.
            break;
    }
    return hitRules;
}

bool RenderPath::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;
    PointerEventsHitRules hitRules = pointerEventsHitRules();
    
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        bool hasFill = (style()->svgStyle()->fillPaint() && style()->svgStyle()->fillPaint()->paintType() != SVGPaint::SVG_PAINTTYPE_NONE);
        bool hasStroke = (style()->svgStyle()->strokePaint() && style()->svgStyle()->strokePaint()->paintType() != SVGPaint::SVG_PAINTTYPE_NONE);
        FloatPoint hitPoint(_x,_y);
        if ((hitRules.canHitStroke && (hasStroke || !hitRules.requireStroke) && strokeContains(hitPoint, hitRules.requireStroke))
            || (hitRules.canHitFill && (hasFill || !hitRules.requireFill) && fillContains(hitPoint, hitRules.requireFill))) {
            setInnerNode(info);
            return true;
        }
    }
    return false;
}

enum MarkerType {
    Start,
    Mid,
    End
};

struct MarkerData {
    FloatPoint origin;
    double strokeWidth;
    FloatPoint inslopePoints[2];
    FloatPoint outslopePoints[2];
    MarkerType type;
    KCanvasMarker *marker;
};

struct DrawMarkersData {
    DrawMarkersData(GraphicsContext*, KCanvasMarker* startMarker, KCanvasMarker* midMarker, double strokeWidth);
    GraphicsContext* context;
    int elementIndex;
    MarkerData previousMarkerData;
    KCanvasMarker* midMarker;
};

DrawMarkersData::DrawMarkersData(GraphicsContext* c, KCanvasMarker *start, KCanvasMarker *mid, double strokeWidth)
    : context(c)
    , elementIndex(0)
    , midMarker(mid)
{
    previousMarkerData.origin = FloatPoint();
    previousMarkerData.strokeWidth = strokeWidth;
    previousMarkerData.marker = start;
    previousMarkerData.type = Start;
}

static void drawMarkerWithData(GraphicsContext* context, MarkerData &data)
{
    if (!data.marker)
        return;
    
    FloatPoint inslopeChange = data.inslopePoints[1] - FloatSize(data.inslopePoints[0].x(), data.inslopePoints[0].y());
    FloatPoint outslopeChange = data.outslopePoints[1] - FloatSize(data.outslopePoints[0].x(), data.outslopePoints[0].y());
    
    static const double deg2rad = M_PI/180.0;
    double inslope = atan2(inslopeChange.y(), inslopeChange.x()) / deg2rad;
    double outslope = atan2(outslopeChange.y(), outslopeChange.x()) / deg2rad;
    
    double angle = 0.0;
    switch (data.type) {
        case Start:
            angle = outslope;
            break;
        case Mid:
            angle = (inslope + outslope) / 2;
            break;
        case End:
            angle = inslope;
    }
    
    data.marker->draw(context, FloatRect(), data.origin.x(), data.origin.y(), data.strokeWidth, angle);
}

static inline void updateMarkerDataForElement(MarkerData &previousMarkerData, const PathElement *element)
{
    FloatPoint *points = element->points;
    
    switch (element->type) {
    case PathElementAddQuadCurveToPoint:
        // TODO
        previousMarkerData.origin = points[1];
        break;
    case PathElementAddCurveToPoint:
        previousMarkerData.inslopePoints[0] = points[1];
        previousMarkerData.inslopePoints[1] = points[2];
        previousMarkerData.origin = points[2];
        break;
    case PathElementMoveToPoint:
    case PathElementAddLineToPoint:
    case PathElementCloseSubpath:
        previousMarkerData.inslopePoints[0] = previousMarkerData.origin;
        previousMarkerData.inslopePoints[1] = points[0];
        previousMarkerData.origin = points[0];
    }
}

static void drawStartAndMidMarkers(void *info, const PathElement *element)
{
    DrawMarkersData &data = *(DrawMarkersData *)info;

    int elementIndex = data.elementIndex;
    MarkerData &previousMarkerData = data.previousMarkerData;

    FloatPoint *points = element->points;

    // First update the outslope for the previous element
    previousMarkerData.outslopePoints[0] = previousMarkerData.origin;
    previousMarkerData.outslopePoints[1] = points[0];

    // Draw the marker for the previous element
    if (elementIndex != 0)
        drawMarkerWithData(data.context, previousMarkerData);

    // Update our marker data for this element
    updateMarkerDataForElement(previousMarkerData, element);

    if (elementIndex == 1) {
        // After drawing the start marker, switch to drawing mid markers
        previousMarkerData.marker = data.midMarker;
        previousMarkerData.type = Mid;
    }

    data.elementIndex++;
}

void RenderPath::drawMarkersIfNeeded(GraphicsContext* context, const FloatRect& rect, const Path& path) const
{
    Document *doc = document();
    const SVGRenderStyle *svgStyle = style()->svgStyle();

    KCanvasMarker* startMarker = getMarkerById(doc, svgStyle->startMarker().substring(1));
    KCanvasMarker* midMarker = getMarkerById(doc, svgStyle->midMarker().substring(1));
    KCanvasMarker* endMarker = getMarkerById(doc, svgStyle->endMarker().substring(1));
    
    if (!startMarker && !midMarker && !endMarker)
        return;

    double strokeWidth = KSVGPainterFactory::cssPrimitiveToLength(this, style()->svgStyle()->strokeWidth(), 1.0);

    DrawMarkersData data(context, startMarker, midMarker, strokeWidth);

    path.apply(&data, drawStartAndMidMarkers);

    data.previousMarkerData.marker = endMarker;
    data.previousMarkerData.type = End;
    drawMarkerWithData(context, data.previousMarkerData);
}

bool RenderPath::hasPercentageValues() const
{
    return static_cast<SVGStyledElement*>(element())->hasPercentageValues();
}

}

#endif // SVG_SUPPORT
