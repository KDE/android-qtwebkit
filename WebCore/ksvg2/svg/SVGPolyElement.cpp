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
#include "SVGPolyElement.h"

#include "Document.h"
#include "SVGNames.h"
#include "SVGPointList.h"

namespace WebCore {

SVGPolyElement::SVGPolyElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGAnimatedPoints()
    , SVGPolyParser()
{
}

SVGPolyElement::~SVGPolyElement()
{
}

SVGPointList* SVGPolyElement::points() const
{
    if (!m_points)
        m_points = new SVGPointList();

    return m_points.get();
}

SVGPointList* SVGPolyElement::animatedPoints() const
{
    return 0;
}

void SVGPolyElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::pointsAttr) {
        ExceptionCode ec = 0;
        points()->clear(ec);
        parsePoints(attr->value().deprecatedString());
    } else {
        if (SVGTests::parseMappedAttribute(attr)) return;
        if (SVGLangSpace::parseMappedAttribute(attr)) return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

void SVGPolyElement::svgPolyTo(double x1, double y1, int) const
{
    ExceptionCode ec = 0;
    points()->appendItem(FloatPoint(x1, y1), ec);
}

void SVGPolyElement::notifyAttributeChange() const
{
    static bool ignoreNotifications = false;
    if (ignoreNotifications || ownerDocument()->parsing())
        return;

    SVGStyledElement::notifyAttributeChange();

    ExceptionCode ec = 0;

    // Spec: Additionally, the 'points' attribute on the original element
    // accessed via the XML DOM (e.g., using the getAttribute() method call)
    // will reflect any changes made to points.
    String _points;
    int len = points()->numberOfItems();
    for (int i = 0; i < len; ++i) {
        FloatPoint p = points()->getItem(i, ec);
        _points += String::format("%.6lg %.6lg ", p.x(), p.y());
    }

    String p("points");
    RefPtr<Attr> attr = const_cast<SVGPolyElement*>(this)->getAttributeNode(p.impl());
    if (attr) {
        ExceptionCode ec = 0;
        ignoreNotifications = true; // prevent recursion.
        attr->setValue(_points, ec);
        ignoreNotifications = false;
    }
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

