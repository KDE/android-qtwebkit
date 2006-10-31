/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
    Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>

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

#ifndef KSVG_SVGFilterElementImpl_H
#define KSVG_SVGFilterElementImpl_H
#ifdef SVG_SUPPORT

#include "KCanvasFilters.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"

namespace WebCore {
    class SVGLength;

    class SVGFilterElement : public SVGStyledElement,
                             public SVGURIReference,
                             public SVGLangSpace,
                             public SVGExternalResourcesRequired
    {
    public:
        SVGFilterElement(const QualifiedName&, Document*);
        virtual ~SVGFilterElement();

        virtual KCanvasFilter* canvasResource();

        // 'SVGFilterElement' functions
        void setFilterRes(unsigned long filterResX, unsigned long filterResY) const;

        virtual void parseMappedAttribute(MappedAttribute* attr);

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGURIReference, String, Href, href)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
 
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, int, int, FilterUnits, filterUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, int, int, PrimitiveUnits, primitiveUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGLength*, RefPtr<SVGLength>, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGLength*, RefPtr<SVGLength>, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGLength*, RefPtr<SVGLength>, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGLength*, RefPtr<SVGLength>, Height, height)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, long, long, FilterResX, filterResX)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, long, long, FilterResY, filterResY)

        KCanvasFilter* m_filter;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
