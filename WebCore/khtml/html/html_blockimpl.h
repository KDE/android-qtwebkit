/*
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

// -------------------------------------------------------------------------
#ifndef HTML_BLOCKIMPL_H
#define HTML_BLOCKIMPL_H

#include "html_elementimpl.h"
#include "dtd.h"

namespace DOM {

class DOMString;

// -------------------------------------------------------------------------

class HTMLBlockquoteElementImpl : public HTMLElementImpl
{
public:
    HTMLBlockquoteElementImpl(DocumentPtr *doc);
    ~HTMLBlockquoteElementImpl();

    virtual NodeImpl::Id id() const;
};

// -------------------------------------------------------------------------

class DOMString;

class HTMLDivElementImpl : public HTMLElementImpl
{
public:
    HTMLDivElementImpl(DocumentPtr *doc);
    ~HTMLDivElementImpl();

    virtual NodeImpl::Id id() const;
    virtual void parseAttribute(AttributeImpl *token);
};

// -------------------------------------------------------------------------

class HTMLHRElementImpl : public HTMLElementImpl
{
public:
    HTMLHRElementImpl(DocumentPtr *doc);
    ~HTMLHRElementImpl();

    virtual NodeImpl::Id id() const;
    virtual void parseAttribute(AttributeImpl *);
    virtual void attach();

protected:
    bool noShade : 1;
};

// -------------------------------------------------------------------------

class HTMLHeadingElementImpl : public HTMLGenericElementImpl
{
public:
    HTMLHeadingElementImpl(DocumentPtr *doc, ushort _tagid);
};

// -------------------------------------------------------------------------

/*
 * were not using HTMLElementImpl as parent class, since a
 * paragraph should be able to flow around aligned objects. Thus
 * a <p> element has to be inline, and is rendered by
 * HTMLBlockImpl::calcParagraph
 */
class HTMLParagraphElementImpl : public HTMLElementImpl
{
public:
    HTMLParagraphElementImpl(DocumentPtr *doc);

    virtual void parseAttribute(AttributeImpl *attr);
    
    virtual NodeImpl::Id id() const;
};

// -------------------------------------------------------------------------

class HTMLPreElementImpl : public HTMLGenericElementImpl
{
public:
    HTMLPreElementImpl(DocumentPtr *doc, ushort _tagid);

    long width() const;
    void setWidth( long w );
};

// -------------------------------------------------------------------------

class HTMLMarqueeElementImpl : public HTMLElementImpl
{
public:
    HTMLMarqueeElementImpl(DocumentPtr *doc);

    virtual NodeImpl::Id id() const;
    virtual void parseAttribute(AttributeImpl *token);

    int minimumDelay() const { return m_minimumDelay; }
    
private:
    int m_minimumDelay;
};

// -------------------------------------------------------------------------

class HTMLLayerElementImpl : public HTMLDivElementImpl
{
public:
    HTMLLayerElementImpl( DocumentPtr *doc );
    ~HTMLLayerElementImpl();

    virtual NodeImpl::Id id() const;

    virtual void parseAttribute(AttributeImpl *);

    bool fixed;
};

}; //namespace
#endif
