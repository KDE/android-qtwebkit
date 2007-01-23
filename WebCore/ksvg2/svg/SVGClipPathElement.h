/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGClipPathElement_h
#define SVGClipPathElement_h

#ifdef SVG_SUPPORT

#include "SVGResourceClipper.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"

namespace WebCore
{
    class SVGClipPathElement : public SVGStyledTransformableElement,
                               public SVGTests,
                               public SVGLangSpace,
                               public SVGExternalResourcesRequired
    {
    public:
        SVGClipPathElement(const QualifiedName&, Document*);
        virtual ~SVGClipPathElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual SVGResource* canvasResource();

        // 'SVGClipPathElement' functions
        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void notifyAttributeChange() const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)

        ANIMATED_PROPERTY_DECLARATIONS(SVGClipPathElement, int, int, ClipPathUnits, clipPathUnits)

        RefPtr<SVGResourceClipper> m_clipper;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
