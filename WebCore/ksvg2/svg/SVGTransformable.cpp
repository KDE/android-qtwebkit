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
#include "SVGTransformable.h"

#include "RegularExpression.h"
#include "AffineTransform.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGStyledElement.h"
#include "SVGTransformList.h"

namespace WebCore {

SVGTransformable::SVGTransformable() : SVGLocatable()
{
}

SVGTransformable::~SVGTransformable()
{
}

AffineTransform SVGTransformable::getCTM(const SVGElement* element) const
{
    AffineTransform ctm = SVGLocatable::getCTM(element);
    return localMatrix() * ctm;
}

AffineTransform SVGTransformable::getScreenCTM(const SVGElement* element) const
{
    AffineTransform ctm = SVGLocatable::getScreenCTM(element);
    return localMatrix() * ctm;
}

int parseTransformParamList(const UChar*& ptr, const UChar* end, double* x, int required, int optional)
{
    int optionalParams = 0, requiredParams = 0;
    skipOptionalSpaces(ptr, end);
    if (*ptr != '(')
        return -1;
    ptr++;
    skipOptionalSpaces(ptr, end);

    while (requiredParams < required) {
        if (!parseNumber(ptr, end, x[requiredParams], false))
            return -1;
        requiredParams++;
        if (requiredParams < required)
            skipOptionalSpacesOrDelimiter(ptr, end);
    }
    skipOptionalSpaces(ptr, end);
    bool delimParsed = skipOptionalSpacesOrDelimiter(ptr, end);

    if (*ptr == ')') { // skip optionals
        ptr++;
        if (delimParsed)
            return -1;
    } else {
         while (optionalParams < optional) {
            if (!parseNumber(ptr, end, x[requiredParams + optionalParams], false))
                return -1;
            optionalParams++;
            if (optionalParams < optional)
                skipOptionalSpacesOrDelimiter(ptr, end);
         }
         skipOptionalSpaces(ptr, end);
         delimParsed = skipOptionalSpacesOrDelimiter(ptr, end);

        if (*ptr != ')' || delimParsed)
            return -1;
        ptr++;
    }

    return requiredParams + optionalParams;
}

static const UChar skewXDesc[] =  {'s','k','e','w', 'X'};
static const UChar skewYDesc[] =  {'s','k','e','w', 'Y'};
static const UChar scaleDesc[] =  {'s','c','a','l', 'e'};
static const UChar translateDesc[] =  {'t','r','a','n', 's', 'l', 'a', 't', 'e'};
static const UChar rotateDesc[] =  {'r','o','t','a', 't', 'e'};
static const UChar matrixDesc[] =  {'m','a','t','r', 'i', 'x'};

bool SVGTransformable::parseTransformAttribute(SVGTransformList* list, const AtomicString& transform)
{
    double x[6] = {0, 0, 0, 0, 0, 0};
    int nr = 0, required = 0, optional = 0;
    
    const UChar* currTransform = transform.characters();
    const UChar* end = currTransform + transform.length();

    bool delimParsed = false;
    while (currTransform < end) {
        delimParsed = false;
        unsigned short type = SVGTransform::SVG_TRANSFORM_UNKNOWN;
        skipOptionalSpaces(currTransform, end);

        if (*currTransform == 's') {
            if (checkString(currTransform, end, skewXDesc, sizeof(skewXDesc) / sizeof(UChar))) {
                required = 1;
                optional = 0;
                type = SVGTransform::SVG_TRANSFORM_SKEWX;
            } else if (checkString(currTransform, end, skewYDesc, sizeof(skewYDesc) / sizeof(UChar))) {
                required = 1;
                optional = 0;
                type = SVGTransform::SVG_TRANSFORM_SKEWY;
            } else if (checkString(currTransform, end, scaleDesc, sizeof(scaleDesc) / sizeof(UChar))) {
                required = 1;
                optional = 1;
                type = SVGTransform::SVG_TRANSFORM_SCALE;
            } else
                goto bail_out;
        } else if (checkString(currTransform, end, translateDesc, sizeof(translateDesc) / sizeof(UChar))) {
            required = 1;
            optional = 1;
            type = SVGTransform::SVG_TRANSFORM_TRANSLATE;
        } else if (checkString(currTransform, end, rotateDesc, sizeof(rotateDesc) / sizeof(UChar))) {
            required = 1;
            optional = 2;
            type = SVGTransform::SVG_TRANSFORM_ROTATE;
        } else if (checkString(currTransform, end, matrixDesc, sizeof(matrixDesc) / sizeof(UChar))) {
            required = 6;
            optional = 0;
            type = SVGTransform::SVG_TRANSFORM_MATRIX;
        } else 
            goto bail_out;

        if ((nr = parseTransformParamList(currTransform, end, x, required, optional)) < 0)
            goto bail_out;

        SVGTransform t;

        switch (type) {
            case SVGTransform::SVG_TRANSFORM_SKEWX:
               t.setSkewX(x[0]);
                break;
            case SVGTransform::SVG_TRANSFORM_SKEWY:
               t.setSkewY(x[0]);
                break;
            case SVGTransform::SVG_TRANSFORM_SCALE:
                  if (nr == 1) // Spec: if only one param given, assume uniform scaling
                      t.setScale(x[0], x[0]);
                  else
                      t.setScale(x[0], x[1]);
                break;
            case SVGTransform::SVG_TRANSFORM_TRANSLATE:
                  if (nr == 1) // Spec: if only one param given, assume 2nd param to be 0
                      t.setTranslate(x[0], 0);
                  else
                      t.setTranslate(x[0], x[1]);
                break;
            case SVGTransform::SVG_TRANSFORM_ROTATE:
                  if (nr == 1)
                      t.setRotate(x[0], 0, 0);
                  else
                      t.setRotate(x[0], x[1], x[2]);
                break;
            case SVGTransform::SVG_TRANSFORM_MATRIX:
                t.setMatrix(AffineTransform(x[0], x[1], x[2], x[3], x[4], x[5]));
                break;
        }

        ExceptionCode ec = 0;
        list->appendItem(t, ec);
        skipOptionalSpaces(currTransform, end);
        if (currTransform < end && *currTransform == ',') {
            delimParsed = true;
            currTransform++;
        }
        skipOptionalSpaces(currTransform, end);
    }

    return !delimParsed;

bail_out: ;
    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

