/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
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

#include "GraphicsContext.h"
#include "Path.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintDevice>
#include <QStack>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if SVG_SUPPORT
#include "KRenderingDeviceQt.h"
#endif

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

static QPainter::CompositionMode toQtCompositionMode(CompositeOperator op)
{
    switch (op) {
        case CompositeClear:
            return QPainter::CompositionMode_Clear;
        case CompositeCopy:
            return QPainter::CompositionMode_Source;
        case CompositeSourceOver:
            return QPainter::CompositionMode_SourceOver;
        case CompositeSourceIn:
            return QPainter::CompositionMode_SourceIn;
        case CompositeSourceOut:
            return QPainter::CompositionMode_SourceOut;
        case CompositeSourceAtop:
            return QPainter::CompositionMode_SourceAtop;
        case CompositeDestinationOver:
            return QPainter::CompositionMode_DestinationOver;
        case CompositeDestinationIn:
            return QPainter::CompositionMode_DestinationIn;
        case CompositeDestinationOut:
            return QPainter::CompositionMode_DestinationOut;
        case CompositeDestinationAtop:
            return QPainter::CompositionMode_DestinationAtop;
        case CompositeXOR:
            return QPainter::CompositionMode_Xor;
        case CompositePlusDarker:
            return QPainter::CompositionMode_SourceOver;
        case CompositeHighlight:
            return QPainter::CompositionMode_SourceOver;
        case CompositePlusLighter:
            return QPainter::CompositionMode_SourceOver;
    }

    return QPainter::CompositionMode_SourceOver;
}

static Qt::PenCapStyle toQtLineCap(LineCap lc)
{
    switch (lc) {
        case ButtCap:
            return Qt::FlatCap;
        case RoundCap:
            return Qt::RoundCap;
        case SquareCap:
            return Qt::SquareCap;
    }

    return Qt::FlatCap;
}

static Qt::PenJoinStyle toQtLineJoin(LineJoin lj)
{
    switch (lj) {
        case MiterJoin:
            return Qt::SvgMiterJoin;
        case RoundJoin:
            return Qt::RoundJoin;
        case BevelJoin:
            return Qt::BevelJoin;
    }

    return Qt::MiterJoin;
}

struct TransparencyLayer
{
    TransparencyLayer(const QPainter& p, int width, int height)
    {
        pixmap = QPixmap(width, height);

        painter = new QPainter(&pixmap);
        painter->setPen(p.pen());
        painter->setBrush(p.brush());
        painter->setMatrix(p.matrix());
    }

    TransparencyLayer()
        : painter(0)
    {
    }

    void cleanup()
    {
        delete painter;
    }

    QPixmap pixmap;
    QPainter* painter;
    qreal opacity;
};

struct TextShadow
{
    TextShadow()
        : x(0)
        , y(0)
        , blur(0)
    {
    }

    bool isNull() { return !x && !y && !blur; }

    int x;
    int y;
    int blur;

    Color color;
};

class GraphicsContextPlatformPrivate
{
public:
    GraphicsContextPlatformPrivate(QPainter* painter);
    ~GraphicsContextPlatformPrivate();

    QPainter& p()
    {
        if (layers.isEmpty()) {
            if (redirect)
                return *redirect;

            return *painter;
        } else
            return *layers.top().painter;
    }

    QPaintDevice* device;

    QStack<TransparencyLayer> layers;
    QPainter* redirect;

    IntRect focusRingClip;
    TextShadow shadow;

private:
    QPainter* painter;
};


GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(QPainter* p)
    : device(p->device())
{
    painter = p;
    redirect = 0;

    // FIXME: Maybe only enable in SVG mode?
    painter->setRenderHint(QPainter::Antialiasing);
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
}

GraphicsContext::GraphicsContext(PlatformGraphicsContext* context)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(context))
{
    setPaintingDisabled(!context);
}

GraphicsContext::~GraphicsContext()
{
    while(!m_data->layers.isEmpty())
        endTransparencyLayer();

    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    return &m_data->p();
}

void GraphicsContext::savePlatformState()
{
    m_data->p().save();
}

void GraphicsContext::restorePlatformState()
{
    m_data->p().restore();
}

/* FIXME: DISABLED WHILE MERGING BACK FROM UNITY
void GraphicsContext::drawTextShadow(const TextRun& run, const IntPoint& point, const TextStyle& style)
{
    if (paintingDisabled())
        return;

    if (m_data->shadow.isNull())
        return;

    TextShadow* shadow = &m_data->shadow;

    if (shadow->blur <= 0) {
        Pen p = pen();
        setPen(shadow->color);
        font().drawText(this, run, style, IntPoint(point.x() + shadow->x, point.y() + shadow->y));
        setPen(p);
    } else {
        const int thickness = shadow->blur;
        // FIXME: OPTIMIZE: limit the area to only the actually painted area + 2*thickness
        const int w = m_data->p().device()->width();
        const int h = m_data->p().device()->height();
        const QRgb color = qRgb(255, 255, 255);
        const QRgb bgColor = qRgb(0, 0, 0);
        QImage image(QSize(w, h), QImage::Format_ARGB32);
        image.fill(bgColor);
        QPainter p;

        Pen curPen = pen();
        p.begin(&image);
        setPen(color);
        m_data->redirect = &p;
        font().drawText(this, run, style, IntPoint(point.x() + shadow->x, point.y() + shadow->y));
        m_data->redirect = 0;
        p.end();
        setPen(curPen);

        int md = thickness * thickness; // max-dist^2

        // blur map/precalculated shadow-decay
        float* bmap = (float*) alloca(sizeof(float) * (md + 1));
        for (int n = 0; n <= md; n++) {
            float f;
            f = n / (float) (md + 1);
            f = 1.0 - f * f;
            bmap[n] = f;
        }

        float factor = 0.0; // maximal potential opacity-sum
        for (int n = -thickness; n <= thickness; n++) {
            for (int m = -thickness; m <= thickness; m++) {
                int d = n * n + m * m;
                if (d <= md)
                    factor += bmap[d];
            }
        }

        // alpha map
        float* amap = (float*) alloca(sizeof(float) * (h * w));
        memset(amap, 0, h * w * (sizeof(float)));

        for (int j = thickness; j<h-thickness; j++) {
            for (int i = thickness; i<w-thickness; i++) {
                QRgb col = image.pixel(i,j);
                if (col == bgColor)
                    continue;

                float g = qAlpha(col);
                g = g / 255;

                for (int n = -thickness; n <= thickness; n++) {
                    for (int m = -thickness; m <= thickness; m++) {
                        int d = n * n + m * m;
                        if (d > md)
                            continue;

                        float f = bmap[d];
                        amap[(i + m) + (j + n) * w] += (g * f);
                    }
                }
            }
        }

        QImage res(QSize(w,h),QImage::Format_ARGB32);
        int r = shadow->color.red();
        int g = shadow->color.green();
        int b = shadow->color.blue();
        int a1 = shadow->color.alpha();

        // arbitratry factor adjustment to make shadows more solid.
        factor = 1.333 / factor;

        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                int a = (int) (amap[i + j * w] * factor * a1);
                if (a > 255)
                    a = 255;

                res.setPixel(i,j, qRgba(r, g, b, a));
            }
        }

        m_data->p().drawImage(0, 0, res, 0, 0, -1, -1, Qt::DiffuseAlphaDither | Qt::ColorOnly | Qt::PreferDither);
    }
}
*/

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->p().drawRect(rect);
}

// FIXME: Now that this is refactored, it should be shared by all contexts.
static void adjustLineToPixelBounderies(FloatPoint& p1, FloatPoint& p2, float strokeWidth,
                                        const Pen::PenStyle& penStyle)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, KHTML will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == Pen::DotLine || penStyle == Pen::DashLine) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        } else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }

    if (((int) strokeWidth) % 2) {
        if (p1.x() == p2.x()) {
            // We're a vertical line.  Adjust our x.
            p1.setX(p1.x() + 0.5);
            p2.setX(p2.x() + 0.5);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.setY(p1.y() + 0.5);
            p2.setY(p2.y() + 0.5);
        }
    }
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;

    adjustLineToPixelBounderies(p1, p2, pen().width(), pen().style());
    m_data->p().drawLine(p1, p2);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->p().drawEllipse(rect);
}

void GraphicsContext::drawArc(const IntRect& rect, float thickness,
                              int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;

    const QPen oldPen = m_data->p().pen();
    QPen nPen = oldPen;
    nPen.setWidthF(thickness);
    m_data->p().setPen(nPen);
    m_data->p().drawArc(rect, startAngle, angleSpan);
    m_data->p().setPen(oldPen);
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const IntPoint* points)
{
    if (paintingDisabled())
        return;

    m_data->p().drawConvexPolygon(reinterpret_cast<const QPoint*>(points), npoints);
}

void GraphicsContext::fillRect(const IntRect& rect, const Color& c)
{
    if (paintingDisabled())
        return;

    m_data->p().fillRect(rect, QColor(c.red(), c.green(), c.blue(), c.alpha()));
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& c)
{
    if (paintingDisabled())
        return;

    m_data->p().fillRect(rect, QColor(c.red(), c.green(), c.blue(), c.alpha()));
}

void GraphicsContext::addClip(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    QPainterPath path;
    path.addRect(QRectF(rect));
    m_data->p().setClipPath(path, Qt::UniteClip);
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::setFocusRingClip(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->focusRingClip = rect;
}

void GraphicsContext::clearFocusRingClip()
{
    if (paintingDisabled())
        return;

    m_data->focusRingClip = IntRect();
}

void GraphicsContext::drawLineForText(const IntPoint& point, int yOffset,
                                      int width, bool printing)
{
    if (paintingDisabled())
        return;

    IntPoint origin = point + IntSize(0, yOffset + 1);
    IntPoint endPoint = origin + IntSize(width, 0);
    drawLine(origin, endPoint);
}

void GraphicsContext::drawLineForMisspelling(const IntPoint& point, int width)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& frect)
{
    QRectF rect(frect);
    rect = m_data->p().deviceMatrix().mapRect(rect);

    QRect result = rect.toRect(); //round it
    return FloatRect(QRectF(result));
}

void GraphicsContext::setShadow(const IntSize& pos, int blur, const Color &color)
{
    if (paintingDisabled())
        return;

    m_data->shadow.x = pos.width();
    m_data->shadow.y = pos.height();
    m_data->shadow.blur = blur;
    m_data->shadow.color = color;
}

void GraphicsContext::clearShadow()
{
    if (paintingDisabled())
        return;

    m_data->shadow = TextShadow();
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    TransparencyLayer layer(m_data->p(),
                            m_data->device->width(),
                            m_data->device->height());

    layer.opacity = opacity;
    m_data->layers.push(layer);
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    TransparencyLayer layer = m_data->layers.pop();
    layer.painter->end();

    m_data->p().save();
    m_data->p().setOpacity(layer.opacity);
    m_data->p().drawPixmap(0, 0, layer.pixmap);
    m_data->p().restore();
    m_data->p().end();

    layer.cleanup();
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->p().eraseRect(rect);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float width)
{
    if (paintingDisabled())
        return;

    QPainterPath path; path.addRect(rect);
    QPen nPen = m_data->p().pen();
    nPen.setWidthF(width);
    m_data->p().strokePath(path, nPen);
}

void GraphicsContext::setLineWidth(float width)
{
    if (paintingDisabled())
        return;

    QPen nPen = m_data->p().pen();
    nPen.setWidthF(width);
    m_data->p().setPen(nPen);
}

void GraphicsContext::setLineCap(LineCap lc)
{
    if (paintingDisabled())
        return;

    QPen nPen = m_data->p().pen();
    nPen.setCapStyle(toQtLineCap(lc));
    m_data->p().setPen(nPen);
}

void GraphicsContext::setLineJoin(LineJoin lj)
{
    if (paintingDisabled())
        return;

    QPen nPen = m_data->p().pen();
    nPen.setJoinStyle(toQtLineJoin(lj));
    m_data->p().setPen(nPen);
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;

    QPen nPen = m_data->p().pen();
    nPen.setMiterLimit(limit);
    m_data->p().setPen(nPen);
}

void GraphicsContext::setAlpha(float opacity)
{
    if (paintingDisabled())
        return;

    m_data->p().setOpacity(opacity);
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;

    m_data->p().setCompositionMode(toQtCompositionMode(op));
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    m_data->p().setClipPath(*path.platformPath());
}

void GraphicsContext::translate(const FloatSize& s)
{
    if (paintingDisabled())
        return;

    m_data->p().scale(s.width(), s.height());
}

void GraphicsContext::rotate(float radians)
{
    if (paintingDisabled())
        return;

    m_data->p().rotate(radians);
}

void GraphicsContext::scale(const FloatSize& s)
{
    if (paintingDisabled())
        return;

    m_data->p().scale(s.width(), s.height());
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect,
                                              int thickness)
{
    if (paintingDisabled())
        return;

    addClip(rect);
    QPainterPath path;

    // Add outer ellipse
    path.addEllipse(QRectF(rect.x(), rect.y(), rect.width(), rect.height()));

    // Add inner ellipse.
    path.addEllipse(QRectF(rect.x() + thickness, rect.y() + thickness,
                           rect.width() - (thickness * 2), rect.height() - (thickness * 2)));

    path.setFillRule(Qt::OddEvenFill);
    m_data->p().setClipPath(path, Qt::IntersectClip);
}

void GraphicsContext::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft,
                                         const IntSize& topRight, const IntSize& bottomLeft,
                                         const IntSize& bottomRight)
{
    if (paintingDisabled())
        return;

    // Need sufficient width and height to contain these curves.  Sanity check our top/bottom
    // values and our width/height values to make sure the curves can all fit.
    int requiredWidth = qMax(topLeft.width() + topRight.width(), bottomLeft.width() + bottomRight.width());
    if (requiredWidth > rect.width())
        return;

    int requiredHeight = qMax(topLeft.height() + bottomLeft.height(), topRight.height() + bottomRight.height());
    if (requiredHeight > rect.height())
        return;

    // Clip to our rect.
    addClip(rect);

    // OK, the curves can fit.
    QPainterPath path;

    // Add the four ellipses to the path.  Technically this really isn't good enough, since we could end up
    // not clipping the other 3/4 of the ellipse we don't care about.  We're relying on the fact that for
    // normal use cases these ellipses won't overlap one another (or when they do the curvature of one will
    // be subsumed by the other).
    path.addEllipse(QRectF(rect.x(), rect.y(), topLeft.width() * 2, topLeft.height() * 2));
    path.addEllipse(QRectF(rect.right() - topRight.width() * 2, rect.y(),
                           topRight.width() * 2, topRight.height() * 2));
    path.addEllipse(QRectF(rect.x(), rect.bottom() - bottomLeft.height() * 2,
                           bottomLeft.width() * 2, bottomLeft.height() * 2));
    path.addEllipse(QRectF(rect.right() - bottomRight.width() * 2,
                           rect.bottom() - bottomRight.height() * 2,
                           bottomRight.width() * 2, bottomRight.height() * 2));

    int topLeftRightHeightMax = qMax(topLeft.height(), topRight.height());
    int bottomLeftRightHeightMax = qMax(bottomLeft.height(), bottomRight.height());
    
    int topBottomLeftWidthMax = qMax(topLeft.width(), bottomLeft.width());  
    int topBottomRightWidthMax = qMax(topRight.width(), bottomRight.width());
    
    // Now add five rects (one for each edge rect in between the rounded corners and one for the interior).
    path.addRect(QRectF(rect.x() + topLeft.width(),
                        rect.y(),
                        rect.width() - topLeft.width() - topRight.width(),
                        topLeftRightHeightMax));

    path.addRect(QRectF(rect.x() + bottomLeft.width(), rect.bottom() - bottomLeftRightHeightMax,
                        rect.width() - bottomLeft.width() - bottomRight.width(), bottomLeftRightHeightMax));

    path.addRect(QRectF(rect.x(),
                        rect.y() + topLeft.height(),
                        topBottomLeftWidthMax,
                        rect.height() - topLeft.height() - bottomLeft.height()));
    
    path.addRect(QRectF(rect.right() - topBottomRightWidthMax,
                        rect.y() + topRight.height(),
                        topBottomRightWidthMax,
                        rect.height() - topRight.height() - bottomRight.height()));
    
    path.addRect(QRectF(rect.x() + topBottomLeftWidthMax,
                        rect.y() + topLeftRightHeightMax,
                        rect.width() - topBottomLeftWidthMax - topBottomRightWidthMax,
                        rect.height() - topLeftRightHeightMax - bottomLeftRightHeightMax));

    path.setFillRule(Qt::WindingFill);
    m_data->p().setClipPath(path, Qt::IntersectClip);
}

#if SVG_SUPPORT
KRenderingDeviceContext* GraphicsContext::createRenderingDeviceContext()
{
    return new KRenderingDeviceContextQt(platformContext());
}
#endif

}

// vim: ts=4 sw=4 et
