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

#ifndef KSVG_SVGElementImpl_H
#define KSVG_SVGElementImpl_H
#ifdef SVG_SUPPORT

#include "Document.h"
#include "FloatRect.h"
#include "StyledElement.h"

#include "SVGNames.h"
#include "SVGAnimatedTemplate.h"
#include "SVGDocumentExtensions.h"

#define ANIMATED_PROPERTY_EMPTY_DECLARATIONS(BareType, NullType, UpperProperty, LowerProperty) \
public: \
    virtual BareType LowerProperty() const { ASSERT(false); return NullType; } \
    virtual void set##UpperProperty(BareType newValue) { ASSERT(false); }\
    virtual BareType LowerProperty##BaseValue() const { ASSERT(false); return NullType; } \
    virtual void set##UpperProperty##BaseValue(BareType newValue) { ASSERT(false); }

#define ANIMATED_PROPERTY_FORWARD_DECLARATIONS(ForwardClass, BareType, UpperProperty, LowerProperty) \
public: \
    virtual BareType LowerProperty() const { return ForwardClass::LowerProperty(); } \
    virtual void set##UpperProperty(BareType newValue) { ForwardClass::set##UpperProperty(newValue); } \
    virtual BareType LowerProperty##BaseValue() const { return ForwardClass::LowerProperty##BaseValue(); } \
    virtual void set##UpperProperty##BaseValue(BareType newValue) { ForwardClass::set##UpperProperty##BaseValue(newValue); }

#define ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(ClassType, ClassStorageType, BareType, StorageType, UpperProperty, LowerProperty) \
class SVGAnimatedTemplate##UpperProperty \
: public SVGAnimatedTemplate<BareType> \
{ \
public: \
    SVGAnimatedTemplate##UpperProperty(const ClassType* element); \
    virtual ~SVGAnimatedTemplate##UpperProperty() { } \
    virtual BareType baseVal() const; \
    virtual void setBaseVal(BareType newBaseVal); \
    virtual BareType animVal() const; \
    virtual void setAnimVal(BareType newAnimVal); \
protected: \
    ClassStorageType m_element; \
}; \
public: \
    BareType LowerProperty() const; \
    void set##UpperProperty(BareType newValue); \
    BareType LowerProperty##BaseValue() const; \
    void set##UpperProperty##BaseValue(BareType newValue); \
    PassRefPtr<SVGAnimatedTemplate##UpperProperty> LowerProperty##Animated() const; \
private: \
    StorageType m_##LowerProperty;

#define ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, ClassType, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter, ContextElement) \
ClassName::SVGAnimatedTemplate##UpperProperty::SVGAnimatedTemplate##UpperProperty(const ClassType* element) \
: m_element(const_cast<ClassType*>(element)) { } \
BareType ClassName::SVGAnimatedTemplate##UpperProperty::baseVal() const \
{ \
    return m_element->LowerProperty##BaseValue(); \
} \
void ClassName::SVGAnimatedTemplate##UpperProperty::setBaseVal(BareType newBaseVal) \
{ \
    m_element->set##UpperProperty##BaseValue(newBaseVal); \
} \
BareType ClassName::SVGAnimatedTemplate##UpperProperty::animVal() const \
{ \
    return m_element->LowerProperty(); \
}\
void ClassName::SVGAnimatedTemplate##UpperProperty::setAnimVal(BareType newAnimVal) \
{ \
    m_element->set##UpperProperty(newAnimVal); \
} \
BareType ClassName::LowerProperty() const \
{ \
    return StorageGetter; \
} \
void ClassName::set##UpperProperty(BareType newValue) \
{ \
    m_##LowerProperty = newValue; \
} \
BareType ClassName::LowerProperty##BaseValue() const \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context != 0); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions && extensions->hasBaseValue<BareType>(context, AttrName)) \
         return extensions->baseValue<BareType>(context, AttrName); \
    return LowerProperty(); \
} \
void ClassName::set##UpperProperty##BaseValue(BareType newValue) \
{ \
    const SVGElement* context = ContextElement; \
    ASSERT(context != 0); \
    SVGDocumentExtensions* extensions = (context->document() ? context->document()->accessSVGExtensions() : 0); \
    if (extensions && extensions->hasBaseValue<BareType>(context, AttrName)) \
        extensions->setBaseValue<BareType>(context, AttrName, newValue); \
    set##UpperProperty(newValue); \
}

// These are the macros which will be used to declare/implement the svg animated properties...
#define ANIMATED_PROPERTY_DECLARATIONS_WITH_CONTEXT(ClassName, BareType, StorageType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(SVGElement, RefPtr<SVGElement>, BareType, StorageType, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_DECLARATIONS(ClassName, BareType, StorageType, UpperProperty, LowerProperty) \
ANIMATED_PROPERTY_DECLARATIONS_INTERNAL(ClassName, RefPtr<ClassName>, BareType, StorageType, UpperProperty, LowerProperty)

#define ANIMATED_PROPERTY_DEFINITIONS_WITH_CONTEXT(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, SVGElement, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter, contextElement()) \
PassRefPtr<ClassName::SVGAnimatedTemplate##UpperProperty> ClassName::LowerProperty##Animated() const \
{ \
    const SVGElement* context = contextElement(); \
    ASSERT(context != 0); \
    return RefPtr<ClassName::SVGAnimatedTemplate##UpperProperty>(new ClassName::SVGAnimatedTemplate##UpperProperty(context)); \
}

#define ANIMATED_PROPERTY_DEFINITIONS(ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter) \
ANIMATED_PROPERTY_DEFINITIONS_INTERNAL(ClassName, ClassName, BareType, UpperClassName, LowerClassName, UpperProperty, LowerProperty, AttrName, StorageGetter, this) \
PassRefPtr<ClassName::SVGAnimatedTemplate##UpperProperty> ClassName::LowerProperty##Animated() const \
{ \
    return RefPtr<ClassName::SVGAnimatedTemplate##UpperProperty>(new ClassName::SVGAnimatedTemplate##UpperProperty(this)); \
}

namespace WebCore {
    class DocumentPtr;
    class Ecma;
    class SVGPreserveAspectRatio;
    class SVGMatrix;
    class SVGStyledElement;
    class SVGSVGElement;

    class SVGElement : public StyledElement {
    public:
        SVGElement(const QualifiedName&, Document*);
        virtual ~SVGElement();
        virtual bool isSVGElement() const { return true; }
        virtual bool isSupported(StringImpl* feature, StringImpl* version) const;
        
        String id() const;
        void setId(const String&, ExceptionCode&);
        String xmlbase() const;
        void setXmlbase(const String&, ExceptionCode&);

        SVGSVGElement* ownerSVGElement() const;
        SVGElement* viewportElement() const;

        // Helper methods that returns the attr value if attr is set, otherwise the default value.
        // It throws NO_MODIFICATION_ALLOWED_ERR if the element is read-only.
        AtomicString tryGetAttribute(const String& name, AtomicString defaultValue = AtomicString()) const;
        AtomicString tryGetAttributeNS(const String& namespaceURI, const String& localName, AtomicString defaultValue = AtomicString()) const;

        // Internal
        virtual void parseMappedAttribute(MappedAttribute*);
        
        virtual bool isStyled() const { return false; }
        virtual bool isStyledTransformable() const { return false; }
        virtual bool isStyledLocatable() const { return false; }
        virtual bool isSVG() const { return false; }
        virtual bool isFilterEffect() const { return false; }
        virtual bool isGradientStop() const { return false; }

        // For SVGTests
        virtual bool isValid() const { return true; }
  
        virtual void closeRenderer();
        virtual bool rendererIsNeeded(RenderStyle*) { return false; }
        virtual bool childShouldCreateRenderer(Node*) const;

        void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);

        // Forwarded properties (declared/defined anywhere else in the inheritance structure)

        // -> For SVGURIReference
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(String, String(), Href, href)

        // -> For SVGFitToViewBox
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(FloatRect, FloatRect(), ViewBox, viewBox)    
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(SVGPreserveAspectRatio*, 0, PreserveAspectRatio, preserveAspectRatio)

        // -> For SVGExternalResourcesRequired
        ANIMATED_PROPERTY_EMPTY_DECLARATIONS(bool, false, ExternalResourcesRequired, externalResourcesRequired)

    private:
        void addSVGEventListener(const AtomicString& eventType, const Attribute*);
        virtual bool haveLoadedRequiredResources();
    };


    static inline SVGElement* svg_dynamic_cast(Node* node)
    {
        SVGElement* svgElement = NULL;
        if (node && node->isSVGElement())
            svgElement = static_cast<SVGElement*>(node);
        return svgElement;
    }

} // namespace WebCore 

#endif // SVG_SUPPORT
#endif // KSVG_SVGElementImpl_H

// vim:ts=4:noet
