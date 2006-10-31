/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
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

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "SVGMarkerElement.h"

#include "KRenderingDevice.h"
#include "PlatformString.h"
#include "RenderSVGContainer.h"
#include "SVGAngle.h"
#include "SVGFitToViewBox.h"
#include "SVGHelper.h"
#include "SVGLength.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGMarkerElement::SVGMarkerElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledElement(tagName, doc)
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGFitToViewBox()
    , m_refX(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_refY(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_markerWidth(new SVGLength(this, LM_WIDTH, viewportElement()))
    , m_markerHeight(new SVGLength(this, LM_HEIGHT, viewportElement()))
    , m_markerUnits(SVG_MARKERUNITS_STROKEWIDTH)
    , m_orientType(0)
    , m_orientAngle(new SVGAngle(this))
{
}

SVGMarkerElement::~SVGMarkerElement()
{
}

void SVGMarkerElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::markerUnitsAttr) {
        if (value == "userSpaceOnUse")
            setMarkerUnitsBaseValue(SVG_MARKERUNITS_USERSPACEONUSE);
    } else if (attr->name() == SVGNames::refXAttr)
        refXBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::refYAttr)
        refYBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::markerWidthAttr)
        markerWidthBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::markerHeightAttr)
        markerHeightBaseValue()->setValueAsString(value);
    else if (attr->name() == SVGNames::orientAttr) {
        if (value == "auto")
            setOrientToAuto();
        else {
            SVGAngle* angle = new SVGAngle(0);
            angle->setValueAsString(value);
            setOrientToAngle(angle);
        }
    } else {
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGFitToViewBox::parseMappedAttribute(attr))
            return;

        SVGStyledElement::parseMappedAttribute(attr);
    }
}

ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength*, Length, length, RefX, refX, SVGNames::refXAttr.localName(), m_refX.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength*, Length, length, RefY, refY, SVGNames::refYAttr.localName(), m_refY.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, int, Enumeration, enumeration, MarkerUnits, markerUnits, SVGNames::markerUnitsAttr.localName(), m_markerUnits)
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength*, Length, length, MarkerWidth, markerWidth, SVGNames::markerWidthAttr.localName(), m_markerWidth.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGLength*, Length, length, MarkerHeight, markerHeight, SVGNames::markerHeightAttr.localName(), m_markerHeight.get())
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, int, Enumeration, enumeration, OrientType, orientType, "orientType", m_orientType)
ANIMATED_PROPERTY_DEFINITIONS(SVGMarkerElement, SVGAngle*, Angle, angle, OrientAngle, orientAngle, "orientAngle", m_orientAngle.get())

void SVGMarkerElement::setOrientToAuto()
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_AUTO);
}

void SVGMarkerElement::setOrientToAngle(SVGAngle* angle)
{
    setOrientTypeBaseValue(SVG_MARKER_ORIENT_ANGLE);
    setOrientAngleBaseValue(angle);
}

SVGResource* SVGMarkerElement::canvasResource()
{
    if (!m_marker)
        m_marker = new SVGResourceMarker();
    
    m_marker->setMarker(static_cast<RenderSVGContainer*>(renderer()));

    // Spec: If the attribute is not specified, the effect is as if a
    // value of "0" were specified.
    if (!m_orientType)
        setOrientToAngle(SVGSVGElement::createSVGAngle());
    
    if (orientType() == SVG_MARKER_ORIENT_ANGLE)
        m_marker->setAngle(orientAngle()->value());
    else
        m_marker->setAutoAngle();

    m_marker->setRef(refX()->value(), refY()->value());
    m_marker->setUseStrokeWidth(markerUnits() == SVG_MARKERUNITS_STROKEWIDTH);
    
    return m_marker.get();
}

RenderObject* SVGMarkerElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    RenderSVGContainer* markerContainer = new (arena) RenderSVGContainer(this);
    markerContainer->setViewBox(viewBox());
    markerContainer->setAlign(KCAlign(preserveAspectRatio()->align() - 1));
    markerContainer->setSlice(preserveAspectRatio()->meetOrSlice() == SVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE);
    markerContainer->setDrawsContents(false); // Marker contents will be explicitly drawn.
    return markerContainer;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

