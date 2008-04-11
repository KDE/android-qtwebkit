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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGSetElement_h
#define SVGSetElement_h
#if ENABLE(SVG_ANIMATION)

#include "SVGAnimationElement.h"

namespace WebCore
{
    class SVGSetElement : public SVGAnimationElement
    {
    public:
        SVGSetElement(const QualifiedName&, Document*);
        virtual ~SVGSetElement();

    protected:
        virtual const SVGElement* contextElement() const { return this; }
        
        virtual bool calculateFromAndToValues(const String& fromString, const String& toString);
        virtual bool calculateFromAndByValues(const String& fromString, const String& byString);
        virtual bool updateAnimatedValue(float percentage);
        virtual void applyAnimatedValueToElement(unsigned repeat);

    private:
        String m_animatedValue;
    };

} // namespace WebCore

#endif // ENABLE(SVG_ANIMATION)
#endif

// vim:ts=4:noet
