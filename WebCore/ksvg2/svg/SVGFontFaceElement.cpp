/*
Copyright (C) 2007 Eric Seidel <eric@webkit.org>

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

#include "config.h"
#if ENABLE(SVG)
#include "SVGFontFaceElement.h"

#include "CSSFontFaceRule.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "CSSValueList.h"
#include "SVGNames.h"
#include "SVGFontFaceSrcElement.h"
#include "SVGDefinitionSrcElement.h"

namespace WebCore {

using namespace SVGNames;

SVGFontFaceElement::SVGFontFaceElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , m_fontFaceRule(new CSSFontFaceRule(0))
    , m_styleDeclaration(new CSSMutableStyleDeclaration)
{
    m_styleDeclaration->setParent(document()->mappedElementSheet());
    m_styleDeclaration->setStrictParsing(true);
    m_fontFaceRule->setDeclaration(m_styleDeclaration.get());
    document()->mappedElementSheet()->append(m_fontFaceRule);
}

SVGFontFaceElement::~SVGFontFaceElement()
{
}

static inline void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, int>* propertyNameToIdMap, const QualifiedName& attrName, const char* cssPropertyName = 0)
{
    int propertyId = 0;
    if (cssPropertyName)
        propertyId = getPropertyID(cssPropertyName, strlen(cssPropertyName));
    else {
        DeprecatedString propertyName = attrName.localName().deprecatedString();
        propertyId = getPropertyID(propertyName.ascii(), propertyName.length());
    }
    if (propertyId < 1)
        fprintf(stderr, "Failed to find property: %s\n", attrName.localName().deprecatedString().ascii());
    ASSERT(propertyId > 0);
    propertyNameToIdMap->set(attrName.localName().impl(), propertyId);
}

static int cssPropertyIdForSVGAttributeName(const QualifiedName& attrName)
{
    if (!attrName.namespaceURI().isNull())
        return 0;
    
    static HashMap<AtomicStringImpl*, int>* propertyNameToIdMap = 0;
    if (!propertyNameToIdMap) {
        propertyNameToIdMap = new HashMap<AtomicStringImpl*, int>;
        // This is a list of all @font-face CSS properties which are exposed as SVG XML attributes
        // Those commented out are not yet supported by WebCore's style system
        //mapAttributeToCSSProperty(propertyNameToIdMap, accent_heightAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, alphabeticAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, ascentAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, bboxAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, cap_heightAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, descentAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_familyAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_sizeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_stretchAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_styleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_variantAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_weightAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, hangingAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, ideographicAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, mathematicalAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, overline_positionAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, overline_thicknessAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, panose_1Attr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, slopeAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, stemhAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, stemvAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, strikethrough_positionAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, strikethrough_thicknessAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, underline_positionAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, underline_thicknessAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, unicode_rangeAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, units_per_emAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, v_alphabeticAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, v_hangingAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, v_ideographicAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, v_mathematicalAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, widthsAttr);
        //mapAttributeToCSSProperty(propertyNameToIdMap, x_heightAttr);
    }
    
    return propertyNameToIdMap->get(attrName.localName().impl());
}

void SVGFontFaceElement::parseMappedAttribute(MappedAttribute* attr)
{    
    int propId = cssPropertyIdForSVGAttributeName(attr->name());
    if (propId > 0) {
        m_styleDeclaration->setProperty(propId, attr->value(), false);
        rebuildFontFace();
        return;
    }
    
    SVGElement::parseMappedAttribute(attr);
}

void SVGFontFaceElement::rebuildFontFace()
{
    // we currently ignore all but the first src element, alternatively we could concat them
    SVGFontFaceSrcElement* srcElement = 0;
    SVGDefinitionSrcElement* definitionSrc = 0;
    
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(font_face_srcTag) && !srcElement)
            srcElement = static_cast<SVGFontFaceSrcElement*>(child);
        else if (child->hasTagName(definition_srcTag) && !definitionSrc)
            definitionSrc = static_cast<SVGDefinitionSrcElement*>(child);
    }

#if 0
    // @font-face (CSSFontFace) does not yet support definition-src, as soon as it does this code should do the trick!
    if (definitionSrc)
        m_styleDeclaration->setProperty(CSS_PROP_DEFINITION_SRC, definitionSrc->getAttribute(XLinkNames::hrefAttr), false);
#endif
    
    if (parentNode() && parentNode()->localName() == "font") // SVGNames::fontTag isn't generated yet
        return; // this font-face applies to its parent font, which we ignore.
    else if (srcElement) {
        // This is the only class (other than CSSParser) to create CSSValue objects and set them on the CSSStyleDeclaration manually
        // we use the addParsedProperties method, and fake having an array of CSSProperty pointers.
        CSSProperty srcProperty(CSS_PROP_SRC, srcElement->srcValue());
        const CSSProperty* srcPropertyRef = &srcProperty;
        m_styleDeclaration->addParsedProperties(&srcPropertyRef, 1);
    }
    
    document()->updateStyleSelector();
}

void SVGFontFaceElement::childrenChanged()
{
    rebuildFontFace();
}

}

#endif // ENABLE(SVG)

