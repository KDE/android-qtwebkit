/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGPathSegCurvetoCubicSmooth.h"

#include "SVGStyledElement.h"

namespace WebCore {

SVGPathSegCurvetoCubicSmoothAbs::SVGPathSegCurvetoCubicSmoothAbs(double x, double y, double x2, double y2)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
    , m_x2(x2)
    , m_y2(y2)
{
}

SVGPathSegCurvetoCubicSmoothAbs::~SVGPathSegCurvetoCubicSmoothAbs()
{
}

void SVGPathSegCurvetoCubicSmoothAbs::setX(double x)
{
    m_x = x;
}

double SVGPathSegCurvetoCubicSmoothAbs::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicSmoothAbs::setY(double y)
{
    m_y = y;
}

double SVGPathSegCurvetoCubicSmoothAbs::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicSmoothAbs::setX2(double x2)
{
    m_x2 = x2;
}

double SVGPathSegCurvetoCubicSmoothAbs::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicSmoothAbs::setY2(double y2)
{
    m_y2 = y2;
}

double SVGPathSegCurvetoCubicSmoothAbs::y2() const
{
    return m_y2;
}



SVGPathSegCurvetoCubicSmoothRel::SVGPathSegCurvetoCubicSmoothRel(double x, double y, double x2, double y2)
    : SVGPathSeg()
    , m_x(x)
    , m_y(y)
    , m_x2(x2)
    , m_y2(y2)
{
}

SVGPathSegCurvetoCubicSmoothRel::~SVGPathSegCurvetoCubicSmoothRel()
{
}

void SVGPathSegCurvetoCubicSmoothRel::setX(double x)
{
    m_x = x;
}

double SVGPathSegCurvetoCubicSmoothRel::x() const
{
    return m_x;
}

void SVGPathSegCurvetoCubicSmoothRel::setY(double y)
{
    m_y = y;
}

double SVGPathSegCurvetoCubicSmoothRel::y() const
{
    return m_y;
}

void SVGPathSegCurvetoCubicSmoothRel::setX2(double x2)
{
    m_x2 = x2;
}

double SVGPathSegCurvetoCubicSmoothRel::x2() const
{
    return m_x2;
}

void SVGPathSegCurvetoCubicSmoothRel::setY2(double y2)
{
    m_y2 = y2;
}

double SVGPathSegCurvetoCubicSmoothRel::y2() const
{
    return m_y2;
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
