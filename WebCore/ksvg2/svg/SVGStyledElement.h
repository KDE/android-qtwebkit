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

#ifndef KSVG_SVGStyledElementImpl_H
#define KSVG_SVGStyledElementImpl_H
#ifdef SVG_SUPPORT

#include "AffineTransform.h"
#include "RenderStyle.h"
#include "SVGElement.h"
#include "SVGStylable.h"
#include "Path.h"

namespace WebCore {

    class CSSStyleDeclaration;
    class KCanvasRenderingStyle;
    class KCanvasResource;
    class KRenderingDevice;
    class RenderPath;
    class RenderView;

    class SVGStyledElement : public SVGElement {
    public:
        SVGStyledElement(const QualifiedName&, Document*);
        virtual ~SVGStyledElement();
        
        virtual bool isStyled() const { return true; }

        // 'SVGStylable' functions
        // These need to be implemented.
        virtual bool rendererIsNeeded(RenderStyle*) { return false; }
        virtual Path toPathData() const { return Path(); }
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual KCanvasResource* canvasResource() { return 0; }
        
        virtual void parseMappedAttribute(MappedAttribute*);

        RenderView* view() const;
        virtual void notifyAttributeChange() const;
        virtual void attributeChanged(Attribute*, bool preserveDecls = false);

        // Imagine we're a <rect> inside of a <pattern> section with patternContentUnits="objectBoundingBox"
        // and our 'width' attribute is set to 50%. When the pattern gets referenced it knows the "bbox"
        // of it's user and has to push the "active client's bbox" as new attribute context to all attributes
        // of the 'rect'. This function also returns the old attribute context, to be able to restore it...
        virtual const SVGStyledElement* pushAttributeContext(const SVGStyledElement* context);

    protected:
        void updateCanvasItem(); // Handles "path data" object changes... (not for style/transform!)

    private:
        mutable RefPtr<CSSStyleDeclaration> m_pa;
        ANIMATED_PROPERTY_DECLARATIONS(String, String, ClassName, className)
        // Optimized updating logic
        bool m_updateVectorial : 1;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // KSVG_SVGStyledElementImpl_H

// vim:ts=4:noet
