/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
#include "CanvasRenderingContext2D.h"

#include "AffineTransform.h"
#include "CSSParser.h"
#include "CachedImage.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasPixelArray.h"
#include "CanvasStyle.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "FloatConversion.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "Page.h"
#include "RenderHTMLCanvas.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "TextMetrics.h"
#include <kjs/interpreter.h>
#include <stdio.h>
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const char* defaultFont = "10px sans-serif";

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* canvas)
    : m_canvas(canvas)
    , m_stateStack(1)
{
}

void CanvasRenderingContext2D::ref()
{
    m_canvas->ref();
}

void CanvasRenderingContext2D::deref()
{
    m_canvas->deref(); 
}

void CanvasRenderingContext2D::reset()
{
    m_stateStack.resize(1);
    m_stateStack.first() = State();
}

CanvasRenderingContext2D::State::State()
    : m_strokeStyle(CanvasStyle::create("black"))
    , m_fillStyle(CanvasStyle::create("black"))
    , m_lineWidth(1)
    , m_lineCap(ButtCap)
    , m_lineJoin(MiterJoin)
    , m_miterLimit(10)
    , m_shadowBlur(0)
    , m_shadowColor("black")
    , m_globalAlpha(1)
    , m_globalComposite(CompositeSourceOver)
    , m_textAlign(StartTextAlign)
    , m_textBaseline(AlphabeticTextBaseline)
    , m_unparsedFont(defaultFont)
    , m_realizedFont(false)
{
}

void CanvasRenderingContext2D::save()
{
    ASSERT(m_stateStack.size() >= 1);
    m_stateStack.append(state());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->save();
}

void CanvasRenderingContext2D::restore()
{
    ASSERT(m_stateStack.size() >= 1);
    if (m_stateStack.size() <= 1)
        return;
    m_path.transform(state().m_transform);
    m_stateStack.removeLast();
    m_path.transform(state().m_transform.inverse());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->restore();
}

CanvasStyle* CanvasRenderingContext2D::strokeStyle() const
{
    return state().m_strokeStyle.get();
}

void CanvasRenderingContext2D::setStrokeStyle(PassRefPtr<CanvasStyle> style)
{
    if (!style)
        return;

    if (m_canvas->originClean()) {
        if (CanvasPattern* pattern = style->canvasPattern()) {
            if (!pattern->originClean())
                m_canvas->setOriginTainted();
        }
    }

    state().m_strokeStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_strokeStyle->applyStrokeColor(c);
}

CanvasStyle* CanvasRenderingContext2D::fillStyle() const
{
    return state().m_fillStyle.get();
}

void CanvasRenderingContext2D::setFillStyle(PassRefPtr<CanvasStyle> style)
{
    if (!style)
        return;
 
    if (m_canvas->originClean()) {
        if (CanvasPattern* pattern = style->canvasPattern()) {
            if (!pattern->originClean())
                m_canvas->setOriginTainted();
        }
    }

    state().m_fillStyle = style;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_fillStyle->applyFillColor(c);
}

float CanvasRenderingContext2D::lineWidth() const
{
    return state().m_lineWidth;
}

void CanvasRenderingContext2D::setLineWidth(float width)
{
    if (!(width > 0))
        return;
    state().m_lineWidth = width;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setStrokeThickness(width);
}

String CanvasRenderingContext2D::lineCap() const
{
    return lineCapName(state().m_lineCap);
}

void CanvasRenderingContext2D::setLineCap(const String& s)
{
    LineCap cap;
    if (!parseLineCap(s, cap))
        return;
    state().m_lineCap = cap;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineCap(cap);
}

String CanvasRenderingContext2D::lineJoin() const
{
    return lineJoinName(state().m_lineJoin);
}

void CanvasRenderingContext2D::setLineJoin(const String& s)
{
    LineJoin join;
    if (!parseLineJoin(s, join))
        return;
    state().m_lineJoin = join;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineJoin(join);
}

float CanvasRenderingContext2D::miterLimit() const
{
    return state().m_miterLimit;
}

void CanvasRenderingContext2D::setMiterLimit(float limit)
{
    if (!(limit > 0))
        return;
    state().m_miterLimit = limit;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setMiterLimit(limit);
}

float CanvasRenderingContext2D::shadowOffsetX() const
{
    return state().m_shadowOffset.width();
}

void CanvasRenderingContext2D::setShadowOffsetX(float x)
{
    state().m_shadowOffset.setWidth(x);
    applyShadow();
}

float CanvasRenderingContext2D::shadowOffsetY() const
{
    return state().m_shadowOffset.height();
}

void CanvasRenderingContext2D::setShadowOffsetY(float y)
{
    state().m_shadowOffset.setHeight(y);
    applyShadow();
}

float CanvasRenderingContext2D::shadowBlur() const
{
    return state().m_shadowBlur;
}

void CanvasRenderingContext2D::setShadowBlur(float blur)
{
    state().m_shadowBlur = blur;
    applyShadow();
}

String CanvasRenderingContext2D::shadowColor() const
{
    // FIXME: What should this return if you called setShadow with a non-string color?
    return state().m_shadowColor;
}

void CanvasRenderingContext2D::setShadowColor(const String& color)
{
    state().m_shadowColor = color;
    applyShadow();
}

float CanvasRenderingContext2D::globalAlpha() const
{
    return state().m_globalAlpha;
}

void CanvasRenderingContext2D::setGlobalAlpha(float alpha)
{
    if (!(alpha >= 0 && alpha <= 1))
        return;
    state().m_globalAlpha = alpha;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setAlpha(alpha);
}

String CanvasRenderingContext2D::globalCompositeOperation() const
{
    return compositeOperatorName(state().m_globalComposite);
}

void CanvasRenderingContext2D::setGlobalCompositeOperation(const String& operation)
{
    CompositeOperator op;
    if (!parseCompositeOperator(operation, op))
        return;
    state().m_globalComposite = op;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setCompositeOperation(op);
}

void CanvasRenderingContext2D::scale(float sx, float sy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->scale(FloatSize(sx, sy));
    state().m_transform.scale(sx, sy);
    m_path.transform(AffineTransform().scale(1.0/sx, 1.0/sy));
}

void CanvasRenderingContext2D::rotate(float angleInRadians)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->rotate(angleInRadians);
    state().m_transform.rotate(angleInRadians / piDouble * 180.0);
    m_path.transform(AffineTransform().rotate(-angleInRadians / piDouble * 180.0));
}

void CanvasRenderingContext2D::translate(float tx, float ty)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->translate(tx, ty);
    state().m_transform.translate(tx, ty);
    m_path.transform(AffineTransform().translate(-tx, -ty));
}

void CanvasRenderingContext2D::transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    
    // HTML5 3.14.11.1 -- ignore any calls that pass non-finite numbers
    if (!isfinite(m11) | !isfinite(m21) | !isfinite(dx) | 
        !isfinite(m12) | !isfinite(m22) | !isfinite(dy))
        return;
    AffineTransform transform(m11, m12, m21, m22, dx, dy);
    c->concatCTM(transform);
    state().m_transform.multiply(transform);
    m_path.transform(transform.inverse());
}

void CanvasRenderingContext2D::setStrokeColor(const String& color)
{
    setStrokeStyle(CanvasStyle::create(color));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel)
{
    setStrokeStyle(CanvasStyle::create(grayLevel, 1));
}

void CanvasRenderingContext2D::setStrokeColor(const String& color, float alpha)
{
    setStrokeStyle(CanvasStyle::create(color, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel, float alpha)
{
    setStrokeStyle(CanvasStyle::create(grayLevel, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float r, float g, float b, float a)
{
    setStrokeStyle(CanvasStyle::create(r, g, b, a));
}

void CanvasRenderingContext2D::setStrokeColor(float c, float m, float y, float k, float a)
{
    setStrokeStyle(CanvasStyle::create(c, m, y, k, a));
}

void CanvasRenderingContext2D::setFillColor(const String& color)
{
    setFillStyle(CanvasStyle::create(color));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel)
{
    setFillStyle(CanvasStyle::create(grayLevel, 1));
}

void CanvasRenderingContext2D::setFillColor(const String& color, float alpha)
{
    setFillStyle(CanvasStyle::create(color, 1));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel, float alpha)
{
    setFillStyle(CanvasStyle::create(grayLevel, alpha));
}

void CanvasRenderingContext2D::setFillColor(float r, float g, float b, float a)
{
    setFillStyle(CanvasStyle::create(r, g, b, a));
}

void CanvasRenderingContext2D::setFillColor(float c, float m, float y, float k, float a)
{
    setFillStyle(CanvasStyle::create(c, m, y, k, a));
}

void CanvasRenderingContext2D::beginPath()
{
    m_path.clear();
}

void CanvasRenderingContext2D::closePath()
{
    m_path.closeSubpath();
}

void CanvasRenderingContext2D::moveTo(float x, float y)
{
    if (!isfinite(x) | !isfinite(y))
        return;
    m_path.moveTo(FloatPoint(x, y));
}

void CanvasRenderingContext2D::lineTo(float x, float y)
{
    if (!isfinite(x) | !isfinite(y))
        return;
    m_path.addLineTo(FloatPoint(x, y));
}

void CanvasRenderingContext2D::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
    if (!isfinite(cpx) | !isfinite(cpy) | !isfinite(x) | !isfinite(y))
        return;
    m_path.addQuadCurveTo(FloatPoint(cpx, cpy), FloatPoint(x, y));
}

void CanvasRenderingContext2D::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
    if (!isfinite(cp1x) | !isfinite(cp1y) | !isfinite(cp2x) | !isfinite(cp2y) | !isfinite(x) | !isfinite(y))
        return;
    m_path.addBezierCurveTo(FloatPoint(cp1x, cp1y), FloatPoint(cp2x, cp2y), FloatPoint(x, y));
}

void CanvasRenderingContext2D::arcTo(float x0, float y0, float x1, float y1, float r, ExceptionCode& ec)
{
    ec = 0;
    if (!isfinite(x0) | !isfinite(y0) | !isfinite(x1) | !isfinite(y1) | !isfinite(r))
        return;
    
    if (r < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    m_path.addArcTo(FloatPoint(x0, y0), FloatPoint(x1, y1), r);
}

void CanvasRenderingContext2D::arc(float x, float y, float r, float sa, float ea, bool anticlockwise, ExceptionCode& ec)
{
    ec = 0;
    if (!isfinite(x) | !isfinite(y) | !isfinite(r) | !isfinite(sa) | !isfinite(ea))
        return;
    
    if (r < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    m_path.addArc(FloatPoint(x, y), r, sa, ea, anticlockwise);
}
    
static bool validateRectForCanvas(float& x, float& y, float& width, float& height)
{
    if (!isfinite(x) | !isfinite(y) | !isfinite(width) | !isfinite(height))
        return false;
    
    if (width < 0) {
        width = -width;
        x -= width;
    }
    
    if (height < 0) {
        height = -height;
        y -= height;
    }
    
    return true;
}

void CanvasRenderingContext2D::rect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
        
    m_path.addRect(FloatRect(x, y, width, height));
}

#if ENABLE(DASHBOARD_SUPPORT)
void CanvasRenderingContext2D::clearPathForDashboardBackwardCompatibilityMode()
{
    if (Settings* settings = m_canvas->document()->settings())
        if (settings->usesDashboardBackwardCompatibilityMode())
            m_path.clear();
}
#endif

void CanvasRenderingContext2D::fill()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    c->beginPath();
    c->addPath(m_path);
    if (!m_path.isEmpty())
        willDraw(m_path.boundingRect());

    c->fillPath();

#if ENABLE(DASHBOARD_SUPPORT)
    clearPathForDashboardBackwardCompatibilityMode();
#endif
}

void CanvasRenderingContext2D::stroke()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->beginPath();
    c->addPath(m_path);

    if (!m_path.isEmpty()) {
        // FIXME: This is insufficient, need to use CGContextReplacePathWithStrokedPath to expand to required bounds
        float lineWidth = state().m_lineWidth;
        float inset = lineWidth / 2;
        FloatRect boundingRect = m_path.boundingRect();
        boundingRect.inflate(inset);
        willDraw(boundingRect);
    }

    c->strokePath();

#if ENABLE(DASHBOARD_SUPPORT)
    clearPathForDashboardBackwardCompatibilityMode();
#endif
}

void CanvasRenderingContext2D::clip()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->clip(m_path);
#if ENABLE(DASHBOARD_SUPPORT)
    clearPathForDashboardBackwardCompatibilityMode();
#endif
}

bool CanvasRenderingContext2D::isPointInPath(const float x, const float y)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return false;
    FloatPoint point(x, y);
    // We have to invert the current transform to ensure we correctly handle the
    // transforms applied to the current path.
    AffineTransform ctm = state().m_transform;
    if (!ctm.isInvertible())
        return false;
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);
    return m_path.contains(transformedPoint);
}

void CanvasRenderingContext2D::clearRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    FloatRect rect(x, y, width, height);
    willDraw(rect);
    c->clearRect(rect);
}

void CanvasRenderingContext2D::fillRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    FloatRect rect(x, y, width, height);
    willDraw(rect);

    c->save();
    c->fillRect(rect);
    c->restore();
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    strokeRect(x, y, width, height, state().m_lineWidth);
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height, float lineWidth)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    
    if (!(lineWidth >= 0))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    FloatRect rect(x, y, width, height);

    FloatRect boundingRect = rect;
    boundingRect.inflate(lineWidth / 2);
    willDraw(boundingRect);

    c->strokeRect(rect, lineWidth);
}

#if PLATFORM(CG)
static inline CGSize adjustedShadowSize(CGFloat width, CGFloat height)
{
    // Work around <rdar://problem/5539388> by ensuring that shadow offsets will get truncated
    // to the desired integer.
    static const CGFloat extraShadowOffset = narrowPrecisionToCGFloat(1.0 / 128);
    if (width > 0)
        width += extraShadowOffset;
    else if (width < 0)
        width -= extraShadowOffset;

    if (height > 0)
        height += extraShadowOffset;
    else if (height < 0)
        height -= extraShadowOffset;

    return CGSizeMake(width, height);
}
#endif

void CanvasRenderingContext2D::setShadow(float width, float height, float blur)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";
    applyShadow();
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = color;
    applyShadow();
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    const CGFloat components[2] = { grayLevel, 1 };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
    CGColorRef color = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c->platformContext(), adjustedShadowSize(width, -height), blur, color);
    CGColorRelease(color);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color, float alpha)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = color;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    RGBA32 rgba = 0; // default is transparent black
    CSSParser::parseColor(rgba, color);
    const CGFloat components[4] = {
        ((rgba >> 16) & 0xFF) / 255.0f,
        ((rgba >> 8) & 0xFF) / 255.0f,
        (rgba & 0xFF) / 255.0f,
        alpha
    };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c->platformContext(), adjustedShadowSize(width, -height), blur, shadowColor);
    CGColorRelease(shadowColor);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel, float alpha)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    const CGFloat components[2] = { grayLevel, alpha };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
    CGColorRef color = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c->platformContext(), adjustedShadowSize(width, -height), blur, color);
    CGColorRelease(color);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float r, float g, float b, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    const CGFloat components[4] = { r, g, b, a };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(c->platformContext(), adjustedShadowSize(width, -height), blur, shadowColor);
    CGColorRelease(shadowColor);
#endif
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float c, float m, float y, float k, float a)
{
    state().m_shadowOffset = FloatSize(width, height);
    state().m_shadowBlur = blur;
    state().m_shadowColor = "";

    GraphicsContext* dc = drawingContext();
    if (!dc)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    const CGFloat components[5] = { c, m, y, k, a };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceCMYK();
    CGColorRef shadowColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    CGContextSetShadowWithColor(dc->platformContext(), adjustedShadowSize(width, -height), blur, shadowColor);
    CGColorRelease(shadowColor);
#endif
}

void CanvasRenderingContext2D::clearShadow()
{
    state().m_shadowOffset = FloatSize();
    state().m_shadowBlur = 0;
    state().m_shadowColor = "";
    applyShadow();
}

void CanvasRenderingContext2D::applyShadow()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    // FIXME: Do this through platform-independent GraphicsContext API.
#if PLATFORM(CG)
    RGBA32 rgba = 0; // default is transparent black
    if (!state().m_shadowColor.isEmpty())
        CSSParser::parseColor(rgba, state().m_shadowColor);
    const CGFloat components[4] = {
        ((rgba >> 16) & 0xFF) / 255.0f,
        ((rgba >> 8) & 0xFF) / 255.0f,
        (rgba & 0xFF) / 255.0f,
        ((rgba >> 24) & 0xFF) / 255.0f
    };
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef color = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);

    CGContextSetShadowWithColor(c->platformContext(), adjustedShadowSize(state().m_shadowOffset.width(), -state().m_shadowOffset.height()), state().m_shadowBlur, color);
    CGColorRelease(color);
#endif
}

static IntSize size(HTMLImageElement* image)
{
    if (CachedImage* cachedImage = image->cachedImage())
        return cachedImage->imageSize(1.0f); // FIXME: Not sure about this.
    return IntSize();
}

static inline FloatRect normalizeRect(const FloatRect& rect)
{
    return FloatRect(min(rect.x(), rect.right()),
        min(rect.y(), rect.bottom()),
        max(rect.width(), -rect.width()),
        max(rect.height(), -rect.height()));
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image, float x, float y)
{
    ASSERT(image);
    IntSize s = size(image);
    ExceptionCode ec;
    drawImage(image, x, y, s.width(), s.height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image,
    float x, float y, float width, float height, ExceptionCode& ec)
{
    ASSERT(image);
    IntSize s = size(image);
    drawImage(image, FloatRect(0, 0, s.width(), s.height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::checkOrigin(const KURL& url)
{
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
    if (!m_canvas->document()->securityOrigin()->canAccess(origin.get()))
        m_canvas->setOriginTainted();
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image, const FloatRect& srcRect, const FloatRect& dstRect,
    ExceptionCode& ec)
{
    ASSERT(image);

    ec = 0;

    FloatRect imageRect = FloatRect(FloatPoint(), size(image));
    if (!imageRect.contains(normalizeRect(srcRect)) || srcRect.width() == 0 || srcRect.height() == 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!dstRect.width() || !dstRect.height())
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage)
        return;

    if (m_canvas->originClean())
        checkOrigin(cachedImage->response().url());

    if (m_canvas->originClean() && !cachedImage->image()->hasSingleSecurityOrigin())
        m_canvas->setOriginTainted();

    FloatRect sourceRect = c->roundToDevicePixels(srcRect);
    FloatRect destRect = c->roundToDevicePixels(dstRect);
    willDraw(destRect);
    c->drawImage(cachedImage->image(), destRect, sourceRect, state().m_globalComposite);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas, float x, float y)
{
    ASSERT(canvas);
    ExceptionCode ec;
    drawImage(canvas, x, y, canvas->width(), canvas->height(), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas,
    float x, float y, float width, float height, ExceptionCode& ec)
{
    ASSERT(canvas);
    drawImage(canvas, FloatRect(0, 0, canvas->width(), canvas->height()), FloatRect(x, y, width, height), ec);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* canvas, const FloatRect& srcRect,
    const FloatRect& dstRect, ExceptionCode& ec)
{
    ASSERT(canvas);

    ec = 0;

    FloatRect srcCanvasRect = FloatRect(FloatPoint(), canvas->size());
    if (!srcCanvasRect.contains(normalizeRect(srcRect)) || srcRect.width() == 0 || srcRect.height() == 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    if (!dstRect.width() || !dstRect.height())
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
        
    FloatRect sourceRect = c->roundToDevicePixels(srcRect);
    FloatRect destRect = c->roundToDevicePixels(dstRect);
        
    // FIXME: Do this through platform-independent GraphicsContext API.
    ImageBuffer* buffer = canvas->buffer();
    if (!buffer)
        return;

    if (!canvas->originClean())
        m_canvas->setOriginTainted();

    c->drawImage(buffer->image(), destRect, sourceRect, state().m_globalComposite);
    willDraw(destRect); // This call comes after drawImage, since the buffer we draw into may be our own, and we need to make sure it is dirty.
                        // FIXME: Arguably willDraw should become didDraw and occur after drawing calls and not before them to avoid problems like this.
}

// FIXME: Why isn't this just another overload of drawImage? Why have a different name?
void CanvasRenderingContext2D::drawImageFromRect(HTMLImageElement* image,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh,
    const String& compositeOperation)
{
    if (!image)
        return;

    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage)
        return;

    if (m_canvas->originClean())
        checkOrigin(cachedImage->response().url());

    if (m_canvas->originClean() && !cachedImage->image()->hasSingleSecurityOrigin())
        m_canvas->setOriginTainted();

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    CompositeOperator op;
    if (!parseCompositeOperator(compositeOperation, op))
        op = CompositeSourceOver;

    FloatRect destRect = FloatRect(dx, dy, dw, dh);
    willDraw(destRect);
    c->drawImage(cachedImage->image(), destRect, FloatRect(sx, sy, sw, sh), op);
}

void CanvasRenderingContext2D::setAlpha(float alpha)
{
    setGlobalAlpha(alpha);
}

void CanvasRenderingContext2D::setCompositeOperation(const String& operation)
{
    setGlobalCompositeOperation(operation);
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createLinearGradient(float x0, float y0, float x1, float y1)
{
    return CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1));
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1)
{
    return CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
}

PassRefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLImageElement* image,
    const String& repetitionType, ExceptionCode& ec)
{
    bool repeatX, repeatY;
    ec = 0;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, ec);
    if (ec)
        return 0;

    if (!image->complete()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    CachedImage* cachedImage = image->cachedImage();
    if (!cachedImage || !image->cachedImage()->image())
        return CanvasPattern::create(Image::nullImage(), repeatX, repeatY, true);

    KURL url(cachedImage->url());
    RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
    bool originClean = m_canvas->document()->securityOrigin()->canAccess(origin.get());
    return CanvasPattern::create(cachedImage->image(), repeatX, repeatY, originClean);
}

PassRefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLCanvasElement* canvas,
    const String& repetitionType, ExceptionCode& ec)
{
    bool repeatX, repeatY;
    ec = 0;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, ec);
    if (ec)
        return 0;
    return CanvasPattern::create(canvas->buffer()->image(), repeatX, repeatY, canvas->originClean());
}

void CanvasRenderingContext2D::willDraw(const FloatRect& r)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    m_canvas->willDraw(c->getCTM().mapRect(r));
}

GraphicsContext* CanvasRenderingContext2D::drawingContext() const
{
    return m_canvas->drawingContext();
}

static PassRefPtr<ImageData> createEmptyImageData(const IntSize& size)
{
    PassRefPtr<ImageData> data = ImageData::create(size.width(), size.height());
    memset(data->data()->data().data(), 0, data->data()->length());
    return data;
}

PassRefPtr<ImageData> CanvasRenderingContext2D::createImageData(float sw, float sh) const
{
    FloatSize unscaledSize(sw, sh);
    IntSize scaledSize = m_canvas->convertLogicalToDevice(unscaledSize);
    if (scaledSize.width() < 1)
        scaledSize.setWidth(1);
    if (scaledSize.height() < 1)
        scaledSize.setHeight(1);
    
    return createEmptyImageData(scaledSize);
}

PassRefPtr<ImageData> CanvasRenderingContext2D::getImageData(float sx, float sy, float sw, float sh, ExceptionCode& ec) const
{
    if (!m_canvas->originClean()) {
        ec = SECURITY_ERR;
        return 0;
    }
    
    FloatRect unscaledRect(sx, sy, sw, sh);
    IntRect scaledRect = m_canvas->convertLogicalToDevice(unscaledRect);
    if (scaledRect.width() < 1)
        scaledRect.setWidth(1);
    if (scaledRect.height() < 1)
        scaledRect.setHeight(1);
    ImageBuffer* buffer = m_canvas ? m_canvas->buffer() : 0;
    if (!buffer)
        return createEmptyImageData(scaledRect.size());
    return buffer->getImageData(scaledRect);
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, ExceptionCode& ec)
{
    if (!data) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    putImageData(data, dx, dy, 0, 0, data->width(), data->height(), ec);
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, float dirtyX, float dirtyY, 
                                            float dirtyWidth, float dirtyHeight, ExceptionCode& ec)
{
    if (!data) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    if (!isfinite(dx) || !isfinite(dy) || !isfinite(dirtyX) || 
        !isfinite(dirtyY) || !isfinite(dirtyWidth) || !isfinite(dirtyHeight)) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    ImageBuffer* buffer = m_canvas->buffer();
    if (!buffer)
        return;

    if (dirtyWidth < 0) {
        dirtyX += dirtyWidth;
        dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
        dirtyY += dirtyHeight;
        dirtyHeight = -dirtyHeight;
    }

    FloatRect clipRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    clipRect.intersect(IntRect(0, 0, data->width(), data->height()));
    IntSize destOffset(static_cast<int>(dx), static_cast<int>(dy));
    IntRect sourceRect = enclosingIntRect(clipRect);
    sourceRect.move(destOffset);
    sourceRect.intersect(IntRect(IntPoint(), buffer->size()));
    if (sourceRect.isEmpty())
        return;
    willDraw(sourceRect);
    sourceRect.move(-destOffset);
    IntPoint destPoint(destOffset.width(), destOffset.height());
    
    buffer->putImageData(data, sourceRect, destPoint);
}

String CanvasRenderingContext2D::font() const
{
    return state().m_unparsedFont;
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    RefPtr<CSSMutableStyleDeclaration> tempDecl = CSSMutableStyleDeclaration::create();
    CSSParser parser(!m_canvas->document()->inCompatMode()); // Use the parse mode of the canvas' document when parsing CSS.
        
    String declarationText("font: ");
    declarationText += newFont;
    parser.parseDeclaration(tempDecl.get(), declarationText);
    if (!tempDecl->length())
        return;
            
    // The parse succeeded.
    state().m_unparsedFont = newFont;
    
    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    RenderArena* arena = m_canvas->document()->renderArena();
    RenderStyle* newStyle = new (arena) RenderStyle();
    newStyle->ref();
    if (m_canvas->computedStyle())
        newStyle->setFontDescription(m_canvas->computedStyle()->fontDescription());

    // Now map the font property into the style.
    CSSStyleSelector* styleSelector = m_canvas->document()->styleSelector();
    styleSelector->applyPropertyToStyle(CSSPropertyFont, tempDecl->getPropertyCSSValue(CSSPropertyFont).get(), newStyle);
    
    state().m_font = newStyle->font();
    state().m_font.update(styleSelector->fontSelector());
    state().m_realizedFont = true;

    newStyle->deref(arena);
    
    // Set the font in the graphics context.
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setFont(state().m_font);
}
        
String CanvasRenderingContext2D::textAlign() const
{
    return textAlignName(state().m_textAlign);
}

void CanvasRenderingContext2D::setTextAlign(const String& s)
{
    TextAlign align;
    if (!parseTextAlign(s, align))
        return;
    state().m_textAlign = align;
}
        
String CanvasRenderingContext2D::textBaseline() const
{
    return textBaselineName(state().m_textBaseline);
}

void CanvasRenderingContext2D::setTextBaseline(const String& s)
{
    TextBaseline baseline;
    if (!parseTextBaseline(s, baseline))
        return;
    state().m_textBaseline = baseline;
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, true);
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, true, maxWidth, true);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, false);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, false, maxWidth, true);
}

PassRefPtr<TextMetrics> CanvasRenderingContext2D::measureText(const String& text)
{
    RefPtr<TextMetrics> metrics = TextMetrics::create();
    metrics->setWidth(accessFont().width(TextRun(text.characters(), text.length())));
    return metrics;
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, float x, float y, bool fill, float maxWidth, bool useMaxWidth)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    
    const Font& font = accessFont();

    // FIXME: Handle maxWidth.
    // FIXME: Need to turn off font smoothing.

    bool rtl = m_canvas->computedStyle() ? m_canvas->computedStyle()->direction() == RTL : false;
    bool override = m_canvas->computedStyle() ? m_canvas->computedStyle()->unicodeBidi() == Override : false;

    unsigned length = text.length();
    const UChar* string = text.characters();
    TextRun textRun(string, length, 0, 0, 0, rtl, override, false, false);

    // Draw the item text at the correct point.
    FloatPoint location(x, y);
    switch (state().m_textBaseline) {
        case TopTextBaseline:
        case HangingTextBaseline:
            location.setY(y + font.ascent());
            break;
        case BottomTextBaseline:
        case IdeographicTextBaseline:
            location.setY(y - font.descent());
            break;
        case MiddleTextBaseline:
            location.setY(y - font.descent() + font.height() / 2);
            break;
        case AlphabeticTextBaseline:
        default:
             // Do nothing.
            break;
    }
    
    float width = font.width(TextRun(text, false, 0, 0, rtl, override));

    TextAlign align = state().m_textAlign;
    if (align == StartTextAlign)
         align = rtl ? RightTextAlign : LeftTextAlign;
    else if (align == EndTextAlign)
        align = rtl ? LeftTextAlign : RightTextAlign;
    
    switch (align) {
        case CenterTextAlign:
            location.setX(location.x() - width / 2);
            break;
        case RightTextAlign:
            location.setX(location.x() - width);
            break;
        default:
            break;
    }
    
    // The slop built in to this mask rect matches the heuristic used in FontCGWin.cpp for GDI text.
    FloatRect textRect = FloatRect(location.x() - font.height() / 2, location.y() - font.ascent() - font.lineGap(),
                                   width + font.height(), font.lineSpacing());
    if (!fill)
        textRect.inflate(c->strokeThickness() / 2);
        
    CanvasStyle* drawStyle = fill ? state().m_fillStyle.get() : state().m_strokeStyle.get();
    if (drawStyle->canvasGradient() || drawStyle->canvasPattern()) {
        IntRect maskRect = enclosingIntRect(textRect);

        auto_ptr<ImageBuffer> maskImage = ImageBuffer::create(maskRect.size(), false);
        
        GraphicsContext* maskImageContext = maskImage->context();

        if (fill)
            maskImageContext->setFillColor(Color::black);
        else {
            maskImageContext->setStrokeColor(Color::black);
            maskImageContext->setStrokeThickness(c->strokeThickness());
        }

        maskImageContext->setTextDrawingMode(fill ? cTextFill : cTextStroke);
        maskImageContext->translate(-maskRect.x(), -maskRect.y());
        
        maskImageContext->setFont(font);
        maskImageContext->drawBidiText(textRun, location);
        
        c->save();
        c->clipToImageBuffer(maskRect, maskImage.get());
        drawStyle->applyFillColor(c);
        c->fillRect(maskRect);
        c->restore();

        return;
    }

    c->setTextDrawingMode(fill ? cTextFill : cTextStroke);
    c->drawBidiText(textRun, location);
}

const Font& CanvasRenderingContext2D::accessFont()
{
    if (!state().m_realizedFont)
        setFont(state().m_unparsedFont);
    return state().m_font;
}

} // namespace WebCore
