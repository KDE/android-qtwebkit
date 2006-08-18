/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.LIB. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#include "IntRect.h"
#include "RenderPathQt.h"
#include "KCanvasMatrix.h"
#include "KCanvasPathQt.h"
#include "KCanvasClipperQt.h"
#include "GraphicsContext.h"
#include "KRenderingDeviceQt.h"
#include "KRenderingPaintServerSolidQt.h"
#include "KRenderingPaintServerGradientQt.h"
#include "KRenderingPaintServerPatternQt.h"

namespace WebCore {

KRenderingDeviceContextQt::KRenderingDeviceContextQt(QPainter* painter)
    : m_painter(painter)
      , m_path()
{
    Q_ASSERT(m_painter != 0);
}

KRenderingDeviceContextQt::~KRenderingDeviceContextQt()
{
}

KCanvasMatrix KRenderingDeviceContextQt::concatCTM(const KCanvasMatrix& worldMatrix)
{
    KCanvasMatrix ret = ctm();
    m_painter->setMatrix(worldMatrix.matrix(), true);
    return ret;
}

KCanvasMatrix KRenderingDeviceContextQt::ctm() const
{
    return KCanvasMatrix(m_painter->matrix());
}

IntRect KRenderingDeviceContextQt::mapFromVisual(const IntRect& rect)
{
    return IntRect();
}

IntRect KRenderingDeviceContextQt::mapToVisual(const IntRect& rect)
{
    return IntRect();
}

void KRenderingDeviceContextQt::clearPath()
{
    m_path = QPainterPath();
}

void KRenderingDeviceContextQt::addPath(const KCanvasPath* path)
{
    m_path.addPath(static_cast<const KCanvasPathQt*>(path)->qtPath());
}

GraphicsContext* KRenderingDeviceContextQt::createGraphicsContext()
{
    return new GraphicsContext(m_painter);
}

QPainter& KRenderingDeviceContextQt::painter()
{
    return *m_painter;
}

QRectF KRenderingDeviceContextQt::pathBBox() const
{
    return m_path.boundingRect();
}

void KRenderingDeviceContextQt::setFillRule(KCWindRule rule)
{
    m_path.setFillRule(rule == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);
}

void KRenderingDeviceContextQt::fillPath()
{
    m_painter->fillPath(m_path, m_painter->brush());
}

void KRenderingDeviceContextQt::strokePath()
{
    m_painter->strokePath(m_path, m_painter->pen());
}

// KRenderingDeviceQt
KRenderingDeviceQt::KRenderingDeviceQt() : KRenderingDevice()
{
}

KRenderingDeviceQt::~KRenderingDeviceQt()
{
}

KRenderingDeviceContext* KRenderingDeviceQt::popContext()
{
    // Any special things needed?
    return KRenderingDevice::popContext();
}

void KRenderingDeviceQt::pushContext(KRenderingDeviceContext* context)
{
    // Any special things needed?
    KRenderingDevice::pushContext(context);
}

// context management.
KRenderingDeviceContextQt* KRenderingDeviceQt::qtContext() const
{
    return static_cast<KRenderingDeviceContextQt*>(currentContext());
}

KRenderingDeviceContext* KRenderingDeviceQt::contextForImage(KCanvasImage* image) const
{
    qDebug("KRenderingDeviceQt::contextForImage() TODO!");
    return 0;
}

DeprecatedString KRenderingDeviceQt::stringForPath(const KCanvasPath* path)
{
    qDebug("KRenderingDeviceQt::stringForPath() TODO!");
    return 0;
}

// Resource creation
KCanvasResource* KRenderingDeviceQt::createResource(const KCResourceType& type) const
{
    switch (type)
    {
        case RS_CLIPPER:
            return new KCanvasClipperQt();
        case RS_MARKER:
            return new KCanvasMarker(); // Use default implementation...
        case RS_IMAGE:
            // return new KCanvasImageQt();
        case RS_FILTER:
            // return new KCanvasFilterQt();
        case RS_MASKER:
            // return new KCanvasMaskerQt();
        default:
            return 0;
    }
}

KRenderingPaintServer* KRenderingDeviceQt::createPaintServer(const KCPaintServerType& type) const
{
    switch (type)
    {
        case PS_SOLID:
            return new KRenderingPaintServerSolidQt();
        case PS_PATTERN:
            return new KRenderingPaintServerPatternQt();
        case PS_LINEAR_GRADIENT:
            return new KRenderingPaintServerLinearGradientQt();
        case PS_RADIAL_GRADIENT:
            return new KRenderingPaintServerRadialGradientQt();
        default:
            return 0;
    }
}

KCanvasFilterEffect* KRenderingDeviceQt::createFilterEffect(const KCFilterEffectType& type) const
{
    qDebug("KRenderingDeviceQt::createFilterEffect() TODO!");
    return 0;
}

KCanvasPath* KRenderingDeviceQt::createPath() const
{
    return new KCanvasPathQt();
}

// item creation
RenderPath* KRenderingDeviceQt::createItem(RenderArena* arena, RenderStyle* style, SVGStyledElement* node, KCanvasPath* path) const
{
    RenderPath* item = new (arena) RenderPathQt(style, node);
    item->setPath(path);
    return item;
}

KRenderingDevice* renderingDevice()
{
    static KRenderingDevice *sharedRenderingDevice = new KRenderingDeviceQt();
    return sharedRenderingDevice;
}

}

// vim:ts=4:noet
