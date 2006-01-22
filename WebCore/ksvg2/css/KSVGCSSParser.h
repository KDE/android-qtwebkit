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

#ifndef KSVG_SVGCSSParser_H
#define KSVG_SVGCSSParser_H
#if SVG_SUPPORT

#include <kdom/css/KDOMCSSParser.h>

namespace KSVG
{
    class SVGCSSParser : public KDOM::CSSParser
    {
    public:
        SVGCSSParser(bool strictParsing = true);
        virtual ~SVGCSSParser();

        virtual bool parseValue(int propId, bool important, int expected = 1);

        KDOM::CSSValueImpl *parsePaint();
        KDOM::CSSValueImpl *parseColor();
        KDOM::CSSValueImpl *parseStrokeDasharray();

        virtual bool parseShape(int propId, bool important);

        virtual KDOM::CSSStyleDeclarationImpl *createCSSStyleDeclaration(KDOM::CSSStyleRuleImpl *rule, Q3PtrList<KDOM::CSSProperty> *propList);
    };
}

#endif //SVG_SUPPORT
#endif

// vim:ts=4:noet
