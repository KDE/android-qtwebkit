/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSStyleApplyProperty.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSStyleSelector.h"
#include "CSSValueList.h"
#include "Document.h"
#include "Element.h"
#include "RenderStyle.h"
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

using namespace std;

namespace WebCore {

class ApplyPropertyExpanding : public ApplyPropertyBase {
public:
    ApplyPropertyExpanding(ApplyPropertyBase* one = 0, ApplyPropertyBase* two = 0, ApplyPropertyBase *three = 0, ApplyPropertyBase* four = 0)
    {
        m_propertyMap[0] = one;
        m_propertyMap[1] = two;
        m_propertyMap[2] = three;
        m_propertyMap[3] = four;
        m_propertyMap[4] = 0; // always null terminated
    }

    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        for (ApplyPropertyBase* const* e = m_propertyMap; *e; e++)
            (*e)->applyInheritValue(selector);
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        for (ApplyPropertyBase* const* e = m_propertyMap; *e; e++)
            (*e)->applyInitialValue(selector);
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        for (ApplyPropertyBase* const* e = m_propertyMap; *e; e++)
            (*e)->applyValue(selector, value);
    }
private:
    ApplyPropertyBase* m_propertyMap[5];
};

class ApplyPropertyExpandingSuppressValue : public ApplyPropertyExpanding {
public:
    ApplyPropertyExpandingSuppressValue(ApplyPropertyBase* one = 0, ApplyPropertyBase* two = 0, ApplyPropertyBase *three = 0, ApplyPropertyBase* four = 0)
        : ApplyPropertyExpanding(one, two, three, four) {}

    virtual void applyValue(CSSStyleSelector*, CSSValue*) const
    {
        ASSERT_NOT_REACHED();
    }
};

template <typename T>
class ApplyPropertyDefaultBase : public ApplyPropertyBase {
public:
    typedef T (RenderStyle::*GetterFunction)() const;
    typedef void (RenderStyle::*SetterFunction)(T);
    typedef T (*InitialFunction)();

    ApplyPropertyDefaultBase(GetterFunction getter, SetterFunction setter, InitialFunction initial)
        : m_getter(getter)
        , m_setter(setter)
        , m_initial(initial)
    {
    }

private:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        (selector->style()->*m_setter)((selector->parentStyle()->*m_getter)());
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        (selector->style()->*m_setter)((*m_initial)());
    }

protected:
    GetterFunction m_getter;
    SetterFunction m_setter;
    InitialFunction m_initial;
};


template <typename T>
class ApplyPropertyDefault : public ApplyPropertyDefaultBase<T> {
public:
    ApplyPropertyDefault(typename ApplyPropertyDefaultBase<T>::GetterFunction getter, typename ApplyPropertyDefaultBase<T>::SetterFunction setter, typename ApplyPropertyDefaultBase<T>::InitialFunction initial)
        : ApplyPropertyDefaultBase<T>(getter, setter, initial)
    {
    }

protected:
    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (value->isPrimitiveValue())
            (selector->style()->*ApplyPropertyDefaultBase<T>::m_setter)(*static_cast<CSSPrimitiveValue*>(value));
    }
};

class ApplyPropertyColor : public ApplyPropertyBase {
public:
    typedef const Color& (RenderStyle::*GetterFunction)() const;
    typedef void (RenderStyle::*SetterFunction)(const Color&);
    typedef const Color& (RenderStyle::*DefaultFunction)() const;
    typedef Color (*InitialFunction)();

    ApplyPropertyColor(GetterFunction getter, SetterFunction setter, DefaultFunction defaultFunction, InitialFunction initialFunction = 0)
        : m_getter(getter)
        , m_setter(setter)
        , m_default(defaultFunction)
        , m_initial(initialFunction)
    {
    }

private:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        const Color& color = (selector->parentStyle()->*m_getter)();
        if (m_default && !color.isValid())
            (selector->style()->*m_setter)((selector->parentStyle()->*m_default)());
        else
            (selector->style()->*m_setter)(color);
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        (selector->style()->*m_setter)(m_initial ? m_initial() : Color());
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        if (m_initial && primitiveValue->getIdent() == CSSValueCurrentcolor)
            applyInheritValue(selector);
        else
            (selector->style()->*m_setter)(selector->getColorFromPrimitiveValue(primitiveValue));
    }

    GetterFunction m_getter;
    SetterFunction m_setter;
    DefaultFunction m_default;
    InitialFunction m_initial;
};

// CSSPropertyDirection
class ApplyPropertyDirection : public ApplyPropertyDefault<TextDirection> {
public:
    ApplyPropertyDirection(GetterFunction getter, SetterFunction setter, InitialFunction initial)
        : ApplyPropertyDefault<TextDirection>(getter, setter, initial)
    {
    }

private:
    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        ApplyPropertyDefault<TextDirection>::applyValue(selector, value);
        Element* element = selector->element();
        if (element && selector->element() == element->document()->documentElement())
            element->document()->setDirectionSetOnDocumentElement(true);
    }
};

template <typename T>
class ApplyPropertyFillLayer : public ApplyPropertyBase {
public:
    ApplyPropertyFillLayer(CSSPropertyID propertyId, EFillLayerType fillLayerType, FillLayer* (RenderStyle::*accessLayers)(),
                           const FillLayer* (RenderStyle::*layers)() const, bool (FillLayer::*test)() const, T (FillLayer::*get)() const,
                           void (FillLayer::*set)(T), void (FillLayer::*clear)(), T (*initial)(EFillLayerType),
                           void (CSSStyleSelector::*mapFill)(CSSPropertyID, FillLayer*, CSSValue*))
        : m_propertyId(propertyId)
        , m_fillLayerType(fillLayerType)
        , m_accessLayers(accessLayers)
        , m_layers(layers)
        , m_test(test)
        , m_get(get)
        , m_set(set)
        , m_clear(clear)
        , m_initial(initial)
        , m_mapFill(mapFill)
    {
    }

private:
    virtual void applyInheritValue(CSSStyleSelector* selector) const
    {
        FillLayer* currChild = (selector->style()->*m_accessLayers)();
        FillLayer* prevChild = 0;
        const FillLayer* currParent = (selector->parentStyle()->*m_layers)();
        while (currParent && (currParent->*m_test)()) {
            if (!currChild) {
                /* Need to make a new layer.*/
                currChild = new FillLayer(m_fillLayerType);
                prevChild->setNext(currChild);
            }
            (currChild->*m_set)((currParent->*m_get)());
            prevChild = currChild;
            currChild = prevChild->next();
            currParent = currParent->next();
        }

        while (currChild) {
            /* Reset any remaining layers to not have the property set. */
            (currChild->*m_clear)();
            currChild = currChild->next();
        }
    }

    virtual void applyInitialValue(CSSStyleSelector* selector) const
    {
        FillLayer* currChild = (selector->style()->*m_accessLayers)();
        (currChild->*m_set)((*m_initial)(m_fillLayerType));
        for (currChild = currChild->next(); currChild; currChild = currChild->next())
            (currChild->*m_clear)();
    }

    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        FillLayer* currChild = (selector->style()->*m_accessLayers)();
        FillLayer* prevChild = 0;
        if (value->isValueList()) {
            /* Walk each value and put it into a layer, creating new layers as needed. */
            CSSValueList* valueList = static_cast<CSSValueList*>(value);
            for (unsigned int i = 0; i < valueList->length(); i++) {
                if (!currChild) {
                    /* Need to make a new layer to hold this value */
                    currChild = new FillLayer(m_fillLayerType);
                    prevChild->setNext(currChild);
                }
                (selector->*m_mapFill)(m_propertyId, currChild, valueList->itemWithoutBoundsCheck(i));
                prevChild = currChild;
                currChild = currChild->next();
            }
        } else {
            (selector->*m_mapFill)(m_propertyId, currChild, value);
            currChild = currChild->next();
        }
        while (currChild) {
            /* Reset all remaining layers to not have the property set. */
            (currChild->*m_clear)();
            currChild = currChild->next();
        }
    }

protected:
    CSSPropertyID m_propertyId;
    EFillLayerType m_fillLayerType;
    FillLayer* (RenderStyle::*m_accessLayers)();
    const FillLayer* (RenderStyle::*m_layers)() const;
    bool (FillLayer::*m_test)() const;
    T (FillLayer::*m_get)() const;
    void (FillLayer::*m_set)(T);
    void (FillLayer::*m_clear)();
    T (*m_initial)(EFillLayerType);
    void (CSSStyleSelector::*m_mapFill)(CSSPropertyID, FillLayer*, CSSValue*);
};

class ApplyPropertyWidth : public ApplyPropertyDefaultBase<unsigned short> {
public:
    ApplyPropertyWidth(GetterFunction getter, SetterFunction setter, InitialFunction initial)
        : ApplyPropertyDefaultBase<unsigned short>(getter, setter, initial)
    {
    }

private:
    virtual void applyValue(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (!value->isPrimitiveValue())
            return;

        CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
        short width;
        switch (primitiveValue->getIdent()) {
        case CSSValueThin:
            width = 1;
            break;
        case CSSValueMedium:
            width = 3;
            break;
        case CSSValueThick:
            width = 5;
            break;
        case CSSValueInvalid:
            width = primitiveValue->computeLengthShort(selector->style(), selector->rootElementStyle(), selector->style()->effectiveZoom());
            // CSS2 box model does not allow negative lengths for borders.
            if (width < 0)
                return;
            break;
        default:
            return;
        }

        (selector->style()->*m_setter)(width);
    }
};

const CSSStyleApplyProperty& CSSStyleApplyProperty::sharedCSSStyleApplyProperty()
{
    DEFINE_STATIC_LOCAL(CSSStyleApplyProperty, cssStyleApplyPropertyInstance, ());
    return cssStyleApplyPropertyInstance;
}

CSSStyleApplyProperty::CSSStyleApplyProperty()
{
    for (int i = 0; i < numCSSProperties; ++i)
       m_propertyMap[i] = 0;

    setPropertyValue(CSSPropertyColor, new ApplyPropertyColor(&RenderStyle::color, &RenderStyle::setColor, 0, RenderStyle::initialColor));
    setPropertyValue(CSSPropertyDirection, new ApplyPropertyDirection(&RenderStyle::direction, &RenderStyle::setDirection, RenderStyle::initialDirection));

    setPropertyValue(CSSPropertyBackgroundAttachment, new ApplyPropertyFillLayer<EFillAttachment>(CSSPropertyBackgroundAttachment, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isAttachmentSet, &FillLayer::attachment, &FillLayer::setAttachment, &FillLayer::clearAttachment, &FillLayer::initialFillAttachment, &CSSStyleSelector::mapFillAttachment));
    setPropertyValue(CSSPropertyBackgroundClip, new ApplyPropertyFillLayer<EFillBox>(CSSPropertyBackgroundClip, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isClipSet, &FillLayer::clip, &FillLayer::setClip, &FillLayer::clearClip, &FillLayer::initialFillClip, &CSSStyleSelector::mapFillClip));
    setPropertyValue(CSSPropertyWebkitBackgroundClip, CSSPropertyBackgroundClip);
    setPropertyValue(CSSPropertyWebkitBackgroundComposite, new ApplyPropertyFillLayer<CompositeOperator>(CSSPropertyWebkitBackgroundComposite, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isCompositeSet, &FillLayer::composite, &FillLayer::setComposite, &FillLayer::clearComposite, &FillLayer::initialFillComposite, &CSSStyleSelector::mapFillComposite));

    setPropertyValue(CSSPropertyBackgroundImage, new ApplyPropertyFillLayer<StyleImage*>(CSSPropertyBackgroundImage, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                &FillLayer::isImageSet, &FillLayer::image, &FillLayer::setImage, &FillLayer::clearImage, &FillLayer::initialFillImage, &CSSStyleSelector::mapFillImage));

    setPropertyValue(CSSPropertyBackgroundOrigin, new ApplyPropertyFillLayer<EFillBox>(CSSPropertyBackgroundOrigin, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isOriginSet, &FillLayer::origin, &FillLayer::setOrigin, &FillLayer::clearOrigin, &FillLayer::initialFillOrigin, &CSSStyleSelector::mapFillOrigin));
    setPropertyValue(CSSPropertyWebkitBackgroundOrigin, CSSPropertyBackgroundOrigin);

    setPropertyValue(CSSPropertyBackgroundPositionX, new ApplyPropertyFillLayer<Length>(CSSPropertyBackgroundPositionX, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                &FillLayer::isXPositionSet, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::clearXPosition, &FillLayer::initialFillXPosition, &CSSStyleSelector::mapFillXPosition));
    setPropertyValue(CSSPropertyBackgroundPositionY, new ApplyPropertyFillLayer<Length>(CSSPropertyBackgroundPositionY, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                    &FillLayer::isYPositionSet, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::clearYPosition, &FillLayer::initialFillYPosition, &CSSStyleSelector::mapFillYPosition));
    setPropertyValue(CSSPropertyBackgroundPosition, new ApplyPropertyExpandingSuppressValue(propertyValue(CSSPropertyBackgroundPositionX), propertyValue(CSSPropertyBackgroundPositionY)));

    setPropertyValue(CSSPropertyBackgroundRepeatX, new ApplyPropertyFillLayer<EFillRepeat>(CSSPropertyBackgroundRepeatX, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                &FillLayer::isRepeatXSet, &FillLayer::repeatX, &FillLayer::setRepeatX, &FillLayer::clearRepeatX, &FillLayer::initialFillRepeatX, &CSSStyleSelector::mapFillRepeatX));
    setPropertyValue(CSSPropertyBackgroundRepeatY, new ApplyPropertyFillLayer<EFillRepeat>(CSSPropertyBackgroundRepeatY, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
                    &FillLayer::isRepeatYSet, &FillLayer::repeatY, &FillLayer::setRepeatY, &FillLayer::clearRepeatY, &FillLayer::initialFillRepeatY, &CSSStyleSelector::mapFillRepeatY));
    setPropertyValue(CSSPropertyBackgroundRepeat, new ApplyPropertyExpandingSuppressValue(propertyValue(CSSPropertyBackgroundRepeatX), propertyValue(CSSPropertyBackgroundRepeatY)));

    setPropertyValue(CSSPropertyBackgroundSize, new ApplyPropertyFillLayer<FillSize>(CSSPropertyBackgroundSize, BackgroundFillLayer, &RenderStyle::accessBackgroundLayers, &RenderStyle::backgroundLayers,
            &FillLayer::isSizeSet, &FillLayer::size, &FillLayer::setSize, &FillLayer::clearSize, &FillLayer::initialFillSize, &CSSStyleSelector::mapFillSize));
    setPropertyValue(CSSPropertyWebkitBackgroundSize, CSSPropertyBackgroundSize);

    setPropertyValue(CSSPropertyWebkitMaskAttachment, new ApplyPropertyFillLayer<EFillAttachment>(CSSPropertyWebkitMaskAttachment, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isAttachmentSet, &FillLayer::attachment, &FillLayer::setAttachment, &FillLayer::clearAttachment, &FillLayer::initialFillAttachment, &CSSStyleSelector::mapFillAttachment));
    setPropertyValue(CSSPropertyWebkitMaskClip, new ApplyPropertyFillLayer<EFillBox>(CSSPropertyWebkitMaskClip, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isClipSet, &FillLayer::clip, &FillLayer::setClip, &FillLayer::clearClip, &FillLayer::initialFillClip, &CSSStyleSelector::mapFillClip));
    setPropertyValue(CSSPropertyWebkitMaskComposite, new ApplyPropertyFillLayer<CompositeOperator>(CSSPropertyWebkitMaskComposite, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isCompositeSet, &FillLayer::composite, &FillLayer::setComposite, &FillLayer::clearComposite, &FillLayer::initialFillComposite, &CSSStyleSelector::mapFillComposite));

    setPropertyValue(CSSPropertyWebkitMaskImage, new ApplyPropertyFillLayer<StyleImage*>(CSSPropertyWebkitMaskImage, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                &FillLayer::isImageSet, &FillLayer::image, &FillLayer::setImage, &FillLayer::clearImage, &FillLayer::initialFillImage, &CSSStyleSelector::mapFillImage));

    setPropertyValue(CSSPropertyWebkitMaskOrigin, new ApplyPropertyFillLayer<EFillBox>(CSSPropertyWebkitMaskOrigin, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isOriginSet, &FillLayer::origin, &FillLayer::setOrigin, &FillLayer::clearOrigin, &FillLayer::initialFillOrigin, &CSSStyleSelector::mapFillOrigin));
    setPropertyValue(CSSPropertyWebkitMaskSize, new ApplyPropertyFillLayer<FillSize>(CSSPropertyWebkitMaskSize, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
            &FillLayer::isSizeSet, &FillLayer::size, &FillLayer::setSize, &FillLayer::clearSize, &FillLayer::initialFillSize, &CSSStyleSelector::mapFillSize));

    setPropertyValue(CSSPropertyWebkitMaskPositionX, new ApplyPropertyFillLayer<Length>(CSSPropertyWebkitMaskPositionX, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                &FillLayer::isXPositionSet, &FillLayer::xPosition, &FillLayer::setXPosition, &FillLayer::clearXPosition, &FillLayer::initialFillXPosition, &CSSStyleSelector::mapFillXPosition));
    setPropertyValue(CSSPropertyWebkitMaskPositionY, new ApplyPropertyFillLayer<Length>(CSSPropertyWebkitMaskPositionY, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                    &FillLayer::isYPositionSet, &FillLayer::yPosition, &FillLayer::setYPosition, &FillLayer::clearYPosition, &FillLayer::initialFillYPosition, &CSSStyleSelector::mapFillYPosition));
    setPropertyValue(CSSPropertyWebkitMaskPosition, new ApplyPropertyExpandingSuppressValue(propertyValue(CSSPropertyWebkitMaskPositionX), propertyValue(CSSPropertyWebkitMaskPositionY)));

    setPropertyValue(CSSPropertyWebkitMaskRepeatX, new ApplyPropertyFillLayer<EFillRepeat>(CSSPropertyWebkitMaskRepeatX, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                &FillLayer::isRepeatXSet, &FillLayer::repeatX, &FillLayer::setRepeatX, &FillLayer::clearRepeatX, &FillLayer::initialFillRepeatX, &CSSStyleSelector::mapFillRepeatX));
    setPropertyValue(CSSPropertyWebkitMaskRepeatY, new ApplyPropertyFillLayer<EFillRepeat>(CSSPropertyWebkitMaskRepeatY, MaskFillLayer, &RenderStyle::accessMaskLayers, &RenderStyle::maskLayers,
                    &FillLayer::isRepeatYSet, &FillLayer::repeatY, &FillLayer::setRepeatY, &FillLayer::clearRepeatY, &FillLayer::initialFillRepeatY, &CSSStyleSelector::mapFillRepeatY));
    setPropertyValue(CSSPropertyWebkitMaskRepeat, new ApplyPropertyExpandingSuppressValue(propertyValue(CSSPropertyBackgroundRepeatX), propertyValue(CSSPropertyBackgroundRepeatY)));

    setPropertyValue(CSSPropertyBackgroundColor, new ApplyPropertyColor(&RenderStyle::backgroundColor, &RenderStyle::setBackgroundColor, 0));
    setPropertyValue(CSSPropertyBorderBottomColor, new ApplyPropertyColor(&RenderStyle::borderBottomColor, &RenderStyle::setBorderBottomColor, &RenderStyle::color));
    setPropertyValue(CSSPropertyBorderLeftColor, new ApplyPropertyColor(&RenderStyle::borderLeftColor, &RenderStyle::setBorderLeftColor, &RenderStyle::color));
    setPropertyValue(CSSPropertyBorderRightColor, new ApplyPropertyColor(&RenderStyle::borderRightColor, &RenderStyle::setBorderRightColor, &RenderStyle::color));
    setPropertyValue(CSSPropertyBorderTopColor, new ApplyPropertyColor(&RenderStyle::borderTopColor, &RenderStyle::setBorderTopColor, &RenderStyle::color));

    setPropertyValue(CSSPropertyBorderTopStyle, new ApplyPropertyDefault<EBorderStyle>(&RenderStyle::borderTopStyle, &RenderStyle::setBorderTopStyle, &RenderStyle::initialBorderStyle));
    setPropertyValue(CSSPropertyBorderRightStyle, new ApplyPropertyDefault<EBorderStyle>(&RenderStyle::borderRightStyle, &RenderStyle::setBorderRightStyle, &RenderStyle::initialBorderStyle));
    setPropertyValue(CSSPropertyBorderBottomStyle, new ApplyPropertyDefault<EBorderStyle>(&RenderStyle::borderBottomStyle, &RenderStyle::setBorderBottomStyle, &RenderStyle::initialBorderStyle));
    setPropertyValue(CSSPropertyBorderLeftStyle, new ApplyPropertyDefault<EBorderStyle>(&RenderStyle::borderLeftStyle, &RenderStyle::setBorderLeftStyle, &RenderStyle::initialBorderStyle));

    setPropertyValue(CSSPropertyBorderTopWidth, new ApplyPropertyWidth(&RenderStyle::borderTopWidth, &RenderStyle::setBorderTopWidth, &RenderStyle::initialBorderWidth));
    setPropertyValue(CSSPropertyBorderRightWidth, new ApplyPropertyWidth(&RenderStyle::borderRightWidth, &RenderStyle::setBorderRightWidth, &RenderStyle::initialBorderWidth));
    setPropertyValue(CSSPropertyBorderBottomWidth, new ApplyPropertyWidth(&RenderStyle::borderBottomWidth, &RenderStyle::setBorderBottomWidth, &RenderStyle::initialBorderWidth));
    setPropertyValue(CSSPropertyBorderLeftWidth, new ApplyPropertyWidth(&RenderStyle::borderLeftWidth, &RenderStyle::setBorderLeftWidth, &RenderStyle::initialBorderWidth));
    setPropertyValue(CSSPropertyOutlineWidth, new ApplyPropertyWidth(&RenderStyle::outlineWidth, &RenderStyle::setOutlineWidth, &RenderStyle::initialBorderWidth));
    setPropertyValue(CSSPropertyWebkitColumnRuleWidth, new ApplyPropertyWidth(&RenderStyle::columnRuleWidth, &RenderStyle::setColumnRuleWidth, &RenderStyle::initialBorderWidth));

    setPropertyValue(CSSPropertyOutlineColor, new ApplyPropertyColor(&RenderStyle::outlineColor, &RenderStyle::setOutlineColor, &RenderStyle::color));

    setPropertyValue(CSSPropertyOverflowX, new ApplyPropertyDefault<EOverflow>(&RenderStyle::overflowX, &RenderStyle::setOverflowX, &RenderStyle::initialOverflowX));
    setPropertyValue(CSSPropertyOverflowY, new ApplyPropertyDefault<EOverflow>(&RenderStyle::overflowY, &RenderStyle::setOverflowY, &RenderStyle::initialOverflowY));
    setPropertyValue(CSSPropertyOverflow, new ApplyPropertyExpanding(propertyValue(CSSPropertyOverflowX), propertyValue(CSSPropertyOverflowY)));

    setPropertyValue(CSSPropertyWebkitColumnRuleColor, new ApplyPropertyColor(&RenderStyle::columnRuleColor, &RenderStyle::setColumnRuleColor, &RenderStyle::color));
    setPropertyValue(CSSPropertyWebkitTextEmphasisColor, new ApplyPropertyColor(&RenderStyle::textEmphasisColor, &RenderStyle::setTextEmphasisColor, &RenderStyle::color));
    setPropertyValue(CSSPropertyWebkitTextFillColor, new ApplyPropertyColor(&RenderStyle::textFillColor, &RenderStyle::setTextFillColor, &RenderStyle::color));
    setPropertyValue(CSSPropertyWebkitTextStrokeColor, new ApplyPropertyColor(&RenderStyle::textStrokeColor, &RenderStyle::setTextStrokeColor, &RenderStyle::color));
}


}
