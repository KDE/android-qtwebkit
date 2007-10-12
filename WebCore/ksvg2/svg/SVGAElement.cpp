/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
                  2007 Eric Seidel <eric@webkit.org>

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

#include "config.h"

#if ENABLE(SVG)

#include "SVGAElement.h"

#include "Attr.h"
#include "CSSHelper.h"
#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderSVGTransformableContainer.h"
#include "RenderSVGInline.h"
#include "ResourceRequest.h"
#include "SVGNames.h"
#include "XLinkNames.h"

namespace WebCore {

using namespace EventNames;

SVGAElement::SVGAElement(const QualifiedName& tagName, Document *doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGURIReference()
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
{
}

SVGAElement::~SVGAElement()
{
}

String SVGAElement::title() const
{
    return getAttribute(XLinkNames::titleAttr);
}

ANIMATED_PROPERTY_DEFINITIONS(SVGAElement, String, String, string, Target, target, SVGNames::targetAttr.localName(), m_target)

void SVGAElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::targetAttr)
        setTargetBaseValue(attr->value());
    else {
        if (SVGURIReference::parseMappedAttribute(attr)) {
            bool wasLink = m_isLink;
            m_isLink = !attr->isNull();
            if (wasLink != m_isLink)
                setChanged();
            return;
        }
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

RenderObject* SVGAElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    if (static_cast<SVGElement*>(parent())->isTextContent())
        return new (arena) RenderSVGInline(this);

    return new (arena) RenderSVGTransformableContainer(this);
}

void SVGAElement::defaultEventHandler(Event* evt)
{
    if (m_isLink && (evt->type() == clickEvent || (evt->type() == keydownEvent && m_focused))) {
        MouseEvent* e = 0;
        if (evt->type() == clickEvent && evt->isMouseEvent())
            e = static_cast<MouseEvent*>(evt);
        
        KeyboardEvent* k = 0;
        if (evt->type() == keydownEvent && evt->isKeyboardEvent())
            k = static_cast<KeyboardEvent*>(evt);
        
        if (e && e->button() == RightButton) {
            SVGStyledTransformableElement::defaultEventHandler(evt);
            return;
        }
        
        if (k) {
            if (k->keyIdentifier() != "Enter") {
                SVGStyledTransformableElement::defaultEventHandler(evt);
                return;
            }
            evt->setDefaultHandled();
            dispatchSimulatedClick(evt);
            return;
        }
        
        String target = getAttribute(SVGNames::targetAttr);
        String xlinktarget = getAttribute(XLinkNames::showAttr);
        if (e && e->button() == MiddleButton)
            target = "_blank";
        else if (target.isEmpty()) // if target is empty, default to "_self" or use xlink:target if set
            target = (xlinktarget == "new") ? "_blank" : "_self";

        String url = parseURL(href());
        if (!evt->defaultPrevented())
            if (document()->frame())
                document()->frame()->loader()->urlSelected(document()->completeURL(url), target, evt, false, true);

        evt->setDefaultHandled();
    }

    SVGStyledTransformableElement::defaultEventHandler(evt);
}

bool SVGAElement::supportsFocus() const
{
    if (isContentEditable())
        return SVGStyledTransformableElement::supportsFocus();
    return isFocusable() || (document() && !document()->haveStylesheetsLoaded());
}

bool SVGAElement::isFocusable() const
{
    if (isContentEditable())
        return SVGStyledTransformableElement::isFocusable();
    
    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Dave wants to fix that some day with a "has visible content" flag or the like.
    if (!renderer() || !(renderer()->style()->visibility() == VISIBLE))
        return false;
    
    return !renderer()->absoluteClippedOverflowRect().isEmpty();
}

bool SVGAElement::isMouseFocusable() const
{
    return false;
}

bool SVGAElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (!isFocusable())
        return false;
    
    if (!document()->frame())
        return false;
    
    return document()->frame()->eventHandler()->tabsToLinks(event);
}

bool SVGAElement::childShouldCreateRenderer(Node* child) const
{
    if (static_cast<SVGElement*>(parent())->isTextContent())
        return child->isTextNode();

    return SVGElement::childShouldCreateRenderer(child);
}


} // namespace WebCore

// vim:ts=4:noet
#endif // ENABLE(SVG)
