/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KSVG_SVGRadialGradientElementImpl_H
#define KSVG_SVGRadialGradientElementImpl_H
#ifdef SVG_SUPPORT

#include <SVGGradientElement.h>

namespace WebCore
{
    class SVGLength;
    class SVGRadialGradientElement : public SVGGradientElement
    {
    public:
        SVGRadialGradientElement(const QualifiedName&, Document*);
        virtual ~SVGRadialGradientElement();

        // 'SVGRadialGradientElement' functions
        virtual void parseMappedAttribute(MappedAttribute *attr);

    protected:
        virtual void buildGradient(KRenderingPaintServerGradient *grad) const;
        virtual KCPaintServerType gradientType() const { return PS_RADIAL_GRADIENT; }

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGLength*, RefPtr<SVGLength>, Cx, cx)
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGLength*, RefPtr<SVGLength>, Cy, cy)
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGLength*, RefPtr<SVGLength>, R, r)
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGLength*, RefPtr<SVGLength>, Fx, fx)
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGLength*, RefPtr<SVGLength>, Fy, fy)
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
