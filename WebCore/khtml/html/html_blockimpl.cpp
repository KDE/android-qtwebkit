/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
// -------------------------------------------------------------------------
//#define DEBUG
#include "html_blockimpl.h"
#include "html_documentimpl.h"
#include "css/cssstyleselector.h"

#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "misc/htmlhashes.h"

#include <kdebug.h>

using namespace khtml;
using namespace DOM;

HTMLBlockquoteElementImpl::HTMLBlockquoteElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLBlockquoteElementImpl::~HTMLBlockquoteElementImpl()
{
}

NodeImpl::Id HTMLBlockquoteElementImpl::id() const
{
    return ID_BLOCKQUOTE;
}

// -------------------------------------------------------------------------

HTMLDivElementImpl::HTMLDivElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLDivElementImpl::~HTMLDivElementImpl()
{
}

NodeImpl::Id HTMLDivElementImpl::id() const
{
    return ID_DIV;
}

bool HTMLDivElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    if (attr == ATTR_ALIGN) {
        result = eBlock;
        return false;
    }
    return HTMLElementImpl::mapToEntry(attr, result);
}
        
void HTMLDivElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_ALIGN:
    {
        DOMString v = attr->value();
	if ( strcasecmp( attr->value(), "middle" ) == 0 || strcasecmp( attr->value(), "center" ) == 0 )
           addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_CENTER);
        else if (strcasecmp(attr->value(), "left") == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_LEFT);
        else if (strcasecmp(attr->value(), "right") == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_RIGHT);
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, v);
        break;
    }
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

// -------------------------------------------------------------------------

HTMLHRElementImpl::HTMLHRElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLHRElementImpl::~HTMLHRElementImpl()
{
}

NodeImpl::Id HTMLHRElementImpl::id() const
{
    return ID_HR;
}

bool HTMLHRElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr) {
        case ATTR_ALIGN:
        case ATTR_WIDTH:
        case ATTR_COLOR:
        case ATTR_SIZE:
        case ATTR_NOSHADE:
            result = eHR;
            return false;
        default:
            break;
    }
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLHRElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch( attr->id() )
    {
    case ATTR_ALIGN: {
        if (strcasecmp(attr->value(), "left") == 0) {
            addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, "0");
	    addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
	}
        else if (strcasecmp(attr->value(), "right") == 0) {
	    addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
	    addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, "0");
	}
	else {
      	    addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
            addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
	}
        break;
    }
    case ATTR_WIDTH:
    {
        // cheap hack to cause linebreaks
        // khtmltests/html/strange_hr.html
        bool ok;
        int v = attr->value().implementation()->toInt(&ok);
        if(ok && !v)
            addCSSLength(attr, CSS_PROP_WIDTH, "1");
        else
            addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
        break;
    }
    case ATTR_COLOR:
        addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        addHTMLColor(attr, CSS_PROP_BORDER_COLOR, attr->value());
        addHTMLColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
        break;
    case ATTR_NOSHADE:
        addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        addHTMLColor(attr, CSS_PROP_BORDER_COLOR, DOMString("grey"));
        addHTMLColor(attr, CSS_PROP_BACKGROUND_COLOR, DOMString("grey"));
        break;
    case ATTR_SIZE: {
        DOMStringImpl* si = attr->value().implementation();
        int size = si->toInt();
        if (size <= 1)
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_WIDTH, DOMString("0"));
        else
            addCSSLength(attr, CSS_PROP_HEIGHT, DOMString(QString::number(size-2)));
        break;
    }
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

// -------------------------------------------------------------------------

HTMLHeadingElementImpl::HTMLHeadingElementImpl(DocumentPtr *doc, ushort _tagid)
    : HTMLGenericElementImpl(doc, _tagid)
{
}

// -------------------------------------------------------------------------

HTMLParagraphElementImpl::HTMLParagraphElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

NodeImpl::Id HTMLParagraphElementImpl::id() const
{
    return ID_P;
}

bool HTMLParagraphElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    if (attr == ATTR_ALIGN) {
        result = eBlock; // We can share with DIV here.
        return false;
    }
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLParagraphElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch(attr->id())
    {
        case ATTR_ALIGN:
        {
            DOMString v = attr->value();
            if ( strcasecmp( attr->value(), "middle" ) == 0 || strcasecmp( attr->value(), "center" ) == 0 )
                addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_CENTER);
            else if (strcasecmp(attr->value(), "left") == 0)
                addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_LEFT);
            else if (strcasecmp(attr->value(), "right") == 0)
                addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_RIGHT);
            else
                addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, v);
            break;
        }
        default:
            HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

// -------------------------------------------------------------------------

HTMLPreElementImpl::HTMLPreElementImpl(DocumentPtr *doc, unsigned short _tagid)
    : HTMLGenericElementImpl(doc, _tagid)
{
}

long HTMLPreElementImpl::width() const
{
    // ###
    return 0;
}

void HTMLPreElementImpl::setWidth( long /*w*/ )
{
    // ###
}

// -------------------------------------------------------------------------

 // WinIE uses 60ms as the minimum delay by default.
const int defaultMinimumDelay = 60;

HTMLMarqueeElementImpl::HTMLMarqueeElementImpl(DocumentPtr *doc)
: HTMLElementImpl(doc),
  m_minimumDelay(defaultMinimumDelay)
{
}

NodeImpl::Id HTMLMarqueeElementImpl::id() const
{
    return ID_MARQUEE;
}

bool HTMLMarqueeElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr) {
        case ATTR_WIDTH:
        case ATTR_HEIGHT:
        case ATTR_BGCOLOR:
        case ATTR_VSPACE:
        case ATTR_HSPACE:
        case ATTR_SCROLLAMOUNT:
        case ATTR_SCROLLDELAY:
        case ATTR_LOOP:
        case ATTR_BEHAVIOR:
        case ATTR_DIRECTION:
            result = eUniversal;
            return false;
        default:
            break;
    }
    return HTMLElementImpl::mapToEntry(attr, result);
}
            
            
void HTMLMarqueeElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch(attr->id())
    {
        case ATTR_WIDTH:
            if (!attr->value().isEmpty())
                addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
            break;
        case ATTR_HEIGHT:
            if (!attr->value().isEmpty())
                addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
            break;
        case ATTR_BGCOLOR:
            if (!attr->value().isEmpty())
                addHTMLColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
            break;
        case ATTR_VSPACE:
            if (!attr->value().isEmpty()) {
                addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
                addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
            }
            break;
        case ATTR_HSPACE:
            if (!attr->value().isEmpty()) {
                addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
                addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
            }
            break;
        case ATTR_SCROLLAMOUNT:
            if (!attr->value().isEmpty())
                addCSSLength(attr, CSS_PROP__KHTML_MARQUEE_INCREMENT, attr->value());
            break;
        case ATTR_SCROLLDELAY:
            if (!attr->value().isEmpty())
                addCSSLength(attr, CSS_PROP__KHTML_MARQUEE_SPEED, attr->value());
            break;
        case ATTR_LOOP:
            if (!attr->value().isEmpty()) {
                if (attr->value() == "-1" || strcasecmp(attr->value(), "infinite") == 0)
                    addCSSProperty(attr, CSS_PROP__KHTML_MARQUEE_REPETITION, CSS_VAL_INFINITE);
                else
                    addCSSLength(attr, CSS_PROP__KHTML_MARQUEE_REPETITION, attr->value());
            }
            break;
        case ATTR_BEHAVIOR:
            if (!attr->value().isEmpty())
                addCSSProperty(attr, CSS_PROP__KHTML_MARQUEE_STYLE, attr->value());
            break;
        case ATTR_DIRECTION:
            if (!attr->value().isEmpty())
                addCSSProperty(attr, CSS_PROP__KHTML_MARQUEE_DIRECTION, attr->value());
            break;
        case ATTR_TRUESPEED:
            m_minimumDelay = !attr->isNull() ? 0 : defaultMinimumDelay;
            break;
        default:
            HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

// ------------------------------------------------------------------------

HTMLLayerElementImpl::HTMLLayerElementImpl(DocumentPtr *doc)
    : HTMLDivElementImpl( doc )
{
}

HTMLLayerElementImpl::~HTMLLayerElementImpl()
{
}

NodeImpl::Id HTMLLayerElementImpl::id() const
{
    return ID_LAYER;
}
