/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#include "html_listimpl.h"

using namespace DOM;

#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "rendering/render_list.h"
#include "misc/htmlhashes.h"
#include "xml/dom_docimpl.h"

using namespace khtml;

NodeImpl::Id HTMLUListElementImpl::id() const
{
    return ID_UL;
}

bool HTMLUListElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr) {
        case ATTR_TYPE:
            result = eUnorderedList;
            return false;
        default:
            break;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLUListElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_TYPE:
        addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, attr->value());
        break;
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

// -------------------------------------------------------------------------

NodeImpl::Id HTMLDirectoryElementImpl::id() const
{
    return ID_DIR;
}

// -------------------------------------------------------------------------

NodeImpl::Id HTMLMenuElementImpl::id() const
{
    return ID_MENU;
}

// -------------------------------------------------------------------------

NodeImpl::Id HTMLOListElementImpl::id() const
{
    return ID_OL;
}

bool HTMLOListElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr) {
        case ATTR_TYPE:
            result = eListItem; // Share with <li>
            return false;
        default:
            break;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLOListElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_TYPE:
        if ( strcmp( attr->value(), "a" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_LOWER_ALPHA);
        else if ( strcmp( attr->value(), "A" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_UPPER_ALPHA);
        else if ( strcmp( attr->value(), "i" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_LOWER_ROMAN);
        else if ( strcmp( attr->value(), "I" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_UPPER_ROMAN);
        else if ( strcmp( attr->value(), "1" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_DECIMAL);
        break;
    case ATTR_START:
            _start = !attr->isNull() ? attr->value().toInt() : 1;
    default:
        HTMLUListElementImpl::parseHTMLAttribute(attr);
    }
}

// -------------------------------------------------------------------------

NodeImpl::Id HTMLLIElementImpl::id() const
{
    return ID_LI;
}

bool HTMLLIElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch (attr) {
        case ATTR_TYPE:
            result = eListItem; // Share with <ol> since all the values are the same
            return false;
        default:
            break;
    }
    
    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLLIElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch(attr->id())
    {
    case ATTR_VALUE:
        isValued = true;
        requestedValue = !attr->isNull() ? attr->value().toInt() : 0;

        if(m_render && m_render->isListItem())
        {
            RenderListItem *list = static_cast<RenderListItem *>(m_render);
            // ### work out what to do when attribute removed - use default of some sort?

            list->setValue(requestedValue);
        }
        break;
    case ATTR_TYPE:
        if ( strcmp( attr->value(), "a" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_LOWER_ALPHA);
        else if ( strcmp( attr->value(), "A" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_UPPER_ALPHA);
        else if ( strcmp( attr->value(), "i" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_LOWER_ROMAN);
        else if ( strcmp( attr->value(), "I" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_UPPER_ROMAN);
        else if ( strcmp( attr->value(), "1" ) == 0 )
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, CSS_VAL_DECIMAL);
        else
            addCSSProperty(attr, CSS_PROP_LIST_STYLE_TYPE, attr->value());
        break;
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

void HTMLLIElementImpl::attach()
{
    assert(!attached());

    HTMLElementImpl::attach();

    if ( m_render && m_render->style()->display() == LIST_ITEM ) {
        RenderListItem *render = static_cast<RenderListItem *>(m_render);
        
        // Find the enclosing list node.
        NodeImpl *listNode = 0;
        NodeImpl *n = this;
        while (!listNode && (n = n->parentNode())) {
            switch (n->id()) {
                case ID_UL:
                case ID_OL:
                    listNode = n;
            }
        }
        
        // If we are not in a list, tell the renderer so it can position us inside.
        // We don't want to change our style to say "inside" since that would affect nested nodes.
        if (!listNode)
            render->setNotInList(true);

	// If we are first, and the OL has a start attr, set the value.
	if (listNode && listNode->id() == ID_OL && !m_render->previousSibling()) {
	    HTMLOListElementImpl *ol = static_cast<HTMLOListElementImpl *>(listNode);
            render->setValue(ol->start());
	}

	// If we had a value attr.
	if (isValued)
	    render->setValue(requestedValue);
    }
}


// -------------------------------------------------------------------------


NodeImpl::Id HTMLDListElementImpl::id() const
{
    return ID_DL;
}

