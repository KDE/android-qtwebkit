/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
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
 */
// -------------------------------------------------------------------------

#include "html/html_inlineimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_documentimpl.h"

#include "misc/htmlhashes.h"
#include "khtmlview.h"
#include "khtml_part.h"
#include "css/csshelper.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/cssstyleselector.h"
#include "xml/dom2_eventsimpl.h"
#include "rendering/render_br.h"
#include "rendering/render_image.h"

#include <kdebug.h>

using namespace khtml;
using namespace DOM;

HTMLAnchorElementImpl::HTMLAnchorElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    m_hasTarget = false;
}

HTMLAnchorElementImpl::~HTMLAnchorElementImpl()
{
}

bool HTMLAnchorElementImpl::isFocusable() const
{
    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Dave wants to fix that some day with a "has visible content" flag or the like.
    if (!(m_hasAnchor && m_render && m_render->style()->visibility() == VISIBLE))
        return false;

    // Before calling absoluteRects, check for the common case where the renderer
    // or one of the continuations is non-empty, since this is a faster check and
    // almost always returns true.
    for (RenderObject *r = m_render; r; r = r->continuation()) {
        if (r->width() > 0 && r->height() > 0)
            return true;
    }

    QValueList<QRect> rects;
    int x = 0, y = 0;
    m_render->absolutePosition(x, y);
    m_render->absoluteRects(rects, x, y);
    for (QValueList<QRect>::ConstIterator it = rects.begin(); it != rects.end(); ++it) {
        if ((*it).isValid())
            return true;
    }

    return false;
}

bool HTMLAnchorElementImpl::isMouseFocusable() const
{
#if APPLE_CHANGES
    return false;
#else
    return isFocusable();
#endif
}

bool HTMLAnchorElementImpl::isKeyboardFocusable() const
{
    if (!isFocusable())
        return false;
    
    if (!getDocument()->part())
	return false;

    return getDocument()->part()->tabsToLinks();
}

NodeImpl::Id HTMLAnchorElementImpl::id() const
{
    return ID_A;
}

void HTMLAnchorElementImpl::defaultEventHandler(EventImpl *evt)
{
    // React on clicks and on keypresses.
    // Don't make this KEYUP_EVENT again, it makes khtml follow links it shouldn't,
    // when pressing Enter in the combo.
    if ( ( evt->id() == EventImpl::KHTML_CLICK_EVENT ||
         ( evt->id() == EventImpl::KEYDOWN_EVENT && m_focused)) && m_hasAnchor) {
        MouseEventImpl *e = 0;
        if ( evt->id() == EventImpl::KHTML_CLICK_EVENT )
            e = static_cast<MouseEventImpl*>( evt );

        KeyboardEventImpl *k = 0;
        if (evt->id() == EventImpl::KEYDOWN_EVENT)
            k = static_cast<KeyboardEventImpl *>( evt );

        QString utarget;
        QString url;

        if ( e && e->button() == 2 ) {
	    HTMLElementImpl::defaultEventHandler(evt);
	    return;
        }

        if ( k ) {
            if (k->keyIdentifier() != "Enter") {
                HTMLElementImpl::defaultEventHandler(evt);
                return;
            }
            if (k->qKeyEvent()) {
                k->qKeyEvent()->accept();
                evt->setDefaultHandled();
                click();
                return;
            }
        }

        url = khtml::parseURL(getAttribute(ATTR_HREF)).string();

        utarget = getAttribute(ATTR_TARGET).string();

        if ( e && e->button() == 1 )
            utarget = "_blank";

        if ( evt->target()->id() == ID_IMG ) {
            HTMLImageElementImpl* img = static_cast<HTMLImageElementImpl*>( evt->target() );
            if ( img && img->isServerMap() )
            {
                khtml::RenderImage *r = static_cast<khtml::RenderImage *>(img->renderer());
                if(r && e)
                {
                    int absx, absy;
                    r->absolutePosition(absx, absy);
                    int x(e->clientX() - absx), y(e->clientY() - absy);
                    url += QString("?%1,%2").arg( x ).arg( y );
                }
                else {
                    evt->setDefaultHandled();
		    HTMLElementImpl::defaultEventHandler(evt);
		    return;
                }
            }
        }
        if ( !evt->defaultPrevented() ) {
            int state = 0;
            int button = 0;

            if ( e ) {
                if ( e->ctrlKey() )
                    state |= Qt::ControlButton;
                if ( e->shiftKey() )
                    state |= Qt::ShiftButton;
                if ( e->altKey() )
                    state |= Qt::AltButton;
                if ( e->metaKey() )
                    state |= Qt::MetaButton;

                if ( e->button() == 0 )
                    button = Qt::LeftButton;
                else if ( e->button() == 1 )
                    button = Qt::MidButton;
                else if ( e->button() == 2 )
                    button = Qt::RightButton;
            }
	    else if ( k )
	    {
	      if ( k->shiftKey() )
                state |= Qt::ShiftButton;
	      if ( k->altKey() )
                state |= Qt::AltButton;
	      if ( k->ctrlKey() )
                state |= Qt::ControlButton;
	    }

            if (getDocument() && getDocument()->view() && getDocument()->part()) {
                getDocument()->view()->resetCursor();
                getDocument()->part()->
                    urlSelected( url, button, state, utarget );
            }
        }
        evt->setDefaultHandled();
    }
    HTMLElementImpl::defaultEventHandler(evt);
}


void HTMLAnchorElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_HREF:
        m_hasAnchor = !attr->isNull();
        break;
    case ATTR_TARGET:
        m_hasTarget = !attr->isNull();
        break;
    case ATTR_NAME:
    case ATTR_TITLE:
    case ATTR_REL:
	break;
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

void HTMLAnchorElementImpl::accessKeyAction()
{
    click();
}

bool HTMLAnchorElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->id() == ATTR_HREF;
}

// -------------------------------------------------------------------------

HTMLBRElementImpl::HTMLBRElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
}

HTMLBRElementImpl::~HTMLBRElementImpl()
{
}

NodeImpl::Id HTMLBRElementImpl::id() const
{
    return ID_BR;
}

bool HTMLBRElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    if (attr == ATTR_CLEAR) {
        result = eUniversal;
        return false;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLBRElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_CLEAR:
    {
        DOMString str = attr->value();
        // If the string is empty, then don't add the clear property. 
        // <br clear> and <br clear=""> are just treated like <br> by Gecko,
        // Mac IE, etc. -dwh
        if (!str.isEmpty()) {
            if (strcasecmp(str,"all") == 0) 
                str = "both";
            addCSSProperty(attr, CSS_PROP_CLEAR, str);
        }
        break;
    }
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

RenderObject *HTMLBRElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
     return new (arena) RenderBR(this);
}

// -------------------------------------------------------------------------

HTMLFontElementImpl::HTMLFontElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLFontElementImpl::~HTMLFontElementImpl()
{
}

NodeImpl::Id HTMLFontElementImpl::id() const
{
    return ID_FONT;
}

// Allows leading spaces.
// Allows trailing nonnumeric characters.
// Returns 10 for any size greater than 9.
static bool parseFontSizeNumber(const DOMString &s, int &size)
{
    unsigned pos = 0;
    
    // Skip leading spaces.
    while (pos < s.length() && s[pos].isSpace())
        ++pos;
    
    // Skip a plus or minus.
    bool sawPlus = false;
    bool sawMinus = false;
    if (pos < s.length() && s[pos] == '+') {
        ++pos;
        sawPlus = true;
    } else if (pos < s.length() && s[pos] == '-') {
        ++pos;
        sawMinus = true;
    }
    
    // Parse a single digit.
    if (pos >= s.length() || !s[pos].isNumber())
        return false;
    int num = s[pos++].digitValue();
    
    // Check for an additional digit.
    if (pos < s.length() && s[pos].isNumber())
        num = 10;
    
    if (sawPlus) {
        size = num + 3;
        return true;
    }
    
    // Don't return 0 (which means 3) or a negative number (which means the same as 1).
    if (sawMinus) {
        size = num == 1 ? 2 : 1;
        return true;
    }
    
    size = num;
    return true;
}

bool HTMLFontElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch(attr)
    {
        case ATTR_SIZE:
        case ATTR_COLOR:
        case ATTR_FACE:
            result = eUniversal;
            return false;
        default:
            break;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLFontElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_SIZE:
    {
        int num;
        if (parseFontSizeNumber(attr->value(), num)) {
            int size;
            switch (num)
            {
            case 2: size = CSS_VAL_SMALL; break;
            case 0: // treat 0 the same as 3, because people expect it to be between -1 and +1
            case 3: size = CSS_VAL_MEDIUM; break;
            case 4: size = CSS_VAL_LARGE; break;
            case 5: size = CSS_VAL_X_LARGE; break;
            case 6: size = CSS_VAL_XX_LARGE; break;
            default:
                if (num > 6)
                    size = CSS_VAL__KHTML_XXX_LARGE;
                else
                    size = CSS_VAL_X_SMALL;
            }
            addCSSProperty(attr, CSS_PROP_FONT_SIZE, size);
        }
        break;
    }
    case ATTR_COLOR:
        addHTMLColor(attr, CSS_PROP_COLOR, attr->value());
        break;
    case ATTR_FACE:
        addCSSProperty(attr, CSS_PROP_FONT_FAMILY, attr->value());
        break;
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
    }
}
