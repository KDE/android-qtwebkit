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
#ifndef HTML_ELEMENTIMPL_H
#define HTML_ELEMENTIMPL_H

#include "xml/dom_elementimpl.h"
#include "dom_atomicstringlist.h"
#include "css/css_valueimpl.h"
#include <qptrdict.h>

namespace DOM {

class DOMString;
class CSSStyleDeclarationImpl;
class HTMLFormElementImpl;
class DocumentFragmentImpl;

enum MappedAttributeEntry { eNone, eUniversal, ePersistent, eReplaced, eBlock, eHR, eUnorderedList, eListItem,
    eTable, eCell, eCaption };

class CSSMappedAttributeDeclarationImpl : public CSSStyleDeclarationImpl
{
public:
    CSSMappedAttributeDeclarationImpl(CSSRuleImpl *parentRule)
    : CSSStyleDeclarationImpl(parentRule), m_entryType(eNone), m_attrName(0)
    {}
    
    virtual ~CSSMappedAttributeDeclarationImpl();

    void setMappedState(MappedAttributeEntry type, NodeImpl::Id attr, const AtomicString& val)
    {
        m_entryType = type;
        m_attrName = attr;
        m_attrValue = val;
    }

private:
    MappedAttributeEntry m_entryType;
    NodeImpl::Id m_attrName;
    AtomicString m_attrValue;
};

class HTMLAttributeImpl : public AttributeImpl
{
public:
    HTMLAttributeImpl(NodeImpl::Id _id, const AtomicString& value, CSSMappedAttributeDeclarationImpl* decl = 0)
    : AttributeImpl(_id, value), m_styleDecl(decl)
    {
        if (decl)
            decl->ref();
    }

    ~HTMLAttributeImpl();

    virtual AttributeImpl* clone(bool preserveDecl=true) const;

    CSSMappedAttributeDeclarationImpl* decl() const { return m_styleDecl; }
    void setDecl(CSSMappedAttributeDeclarationImpl* decl) 
    { 
        if (m_styleDecl) m_styleDecl->deref();
        m_styleDecl = decl;
        if (m_styleDecl) m_styleDecl->ref();
    }

private:
    CSSMappedAttributeDeclarationImpl* m_styleDecl;
};

class HTMLNamedAttrMapImpl : public NamedAttrMapImpl
{
public:
    HTMLNamedAttrMapImpl(ElementImpl *e);

    virtual void clearAttributes();
    
    virtual bool isHTMLAttributeMap() const;
    
    virtual void parseClassAttribute(const DOMString& classAttr);
    const AtomicStringList* getClassList() const { return &m_classList; }
    
    bool hasMappedAttributes() const { return m_mappedAttributeCount > 0; }
    void declRemoved() { m_mappedAttributeCount--; }
    void declAdded() { m_mappedAttributeCount++; }
    
    bool mapsEquivalent(const HTMLNamedAttrMapImpl* otherMap) const;
    int declCount() const;

    HTMLAttributeImpl* attributeItem(unsigned long index) const
    { return attrs ? static_cast<HTMLAttributeImpl*>(attrs[index]) : 0; }
    
private:
    AtomicStringList m_classList;
    int m_mappedAttributeCount;
};
    
class HTMLElementImpl : public ElementImpl
{
public:
    HTMLElementImpl(DocumentPtr *doc);

    virtual ~HTMLElementImpl();

    virtual bool isHTMLElement() const { return true; }

    virtual bool isInline() const;
     
    virtual Id id() const = 0;

    virtual void attributeChanged(AttributeImpl* attr, bool preserveDecls = false);
    virtual void parseHTMLAttribute(HTMLAttributeImpl* attr);
    virtual void createAttributeMap() const;

    virtual const AtomicStringList* getClassList() const;

    bool hasMappedAttributes() const { return namedAttrMap ? static_cast<HTMLNamedAttrMapImpl*>(namedAttrMap)->hasMappedAttributes() : false; }
    const HTMLNamedAttrMapImpl* htmlAttributes() const { return static_cast<HTMLNamedAttrMapImpl*>(namedAttrMap); }
    virtual bool mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const;
    bool isMappedAttribute(NodeImpl::Id attr) const { MappedAttributeEntry res = eNone; mapToEntry(attr, res); return res != eNone; }

    void addCSSLength(HTMLAttributeImpl* attr, int id, const DOMString &value);
    void addCSSProperty(HTMLAttributeImpl* attr, int id, const DOMString &value);
    void addCSSProperty(HTMLAttributeImpl* attr, int id, int value);
    void addCSSStringProperty(HTMLAttributeImpl* attr, int id, const DOMString &value, DOM::CSSPrimitiveValue::UnitTypes = DOM::CSSPrimitiveValue::CSS_STRING);
    void addCSSImageProperty(HTMLAttributeImpl* attr, int id, const DOMString &URL);
    void addHTMLColor(HTMLAttributeImpl* attr, int id, const DOMString &c);
    void createMappedDecl(HTMLAttributeImpl* attr);
    
    DOMString innerHTML() const;
    DOMString outerHTML() const;
    DOMString innerText() const;
    DOMString outerText() const;
    DocumentFragmentImpl *createContextualFragment( const DOMString &html );
    bool setInnerHTML( const DOMString &html );
    bool setOuterHTML( const DOMString &html );
    bool setInnerText( const DOMString &text );
    bool setOuterText( const DOMString &text );

    virtual DOMString namespaceURI() const;
    
    virtual bool isFocusable() const;
    virtual bool isContentEditable() const;
    virtual DOMString contentEditable() const;
    virtual void setContentEditable(HTMLAttributeImpl* attr);
    virtual void setContentEditable(const DOMString &enabled);

    virtual void click();
    
    CSSStyleDeclarationImpl* inlineStyleDecl() const { return m_inlineStyleDecl; }
    virtual CSSStyleDeclarationImpl* additionalAttributeStyleDecl();
    CSSStyleDeclarationImpl* getInlineStyleDecl();
    void createInlineStyleDecl();
     
    virtual AttributeImpl* createAttribute(NodeImpl::Id id, DOMStringImpl* value);

#if APPLE_CHANGES
    virtual bool isGenericFormElement() const { return false; }
#endif

    virtual DOMString toString() const;

    static CSSMappedAttributeDeclarationImpl* getMappedAttributeDecl(MappedAttributeEntry type, AttributeImpl* attr);
    static void setMappedAttributeDecl(MappedAttributeEntry type, AttributeImpl* attr, CSSMappedAttributeDeclarationImpl* decl);
    static void removeMappedAttributeDecl(MappedAttributeEntry type, NodeImpl::Id attrName, const AtomicString& attrValue);
    static QPtrDict<QPtrDict<QPtrDict<CSSMappedAttributeDeclarationImpl> > >* m_mappedAttributeDecls;

protected:
    // for IMG, OBJECT and APPLET
    void addHTMLAlignment(HTMLAttributeImpl* htmlAttr);

    CSSStyleDeclarationImpl* m_inlineStyleDecl;
};

class HTMLGenericElementImpl : public HTMLElementImpl
{
public:
    HTMLGenericElementImpl(DocumentPtr *doc, ushort i);

    virtual ~HTMLGenericElementImpl();

    virtual Id id() const { return _id; };

protected:
    ushort _id;
};

}; //namespace

#endif
