/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
 *           (C) 2007 Rob Buis (buis@kde.org)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "HTMLStyleElement.h"

#include "Attribute.h"
#include "Document.h"
#include "HTMLNames.h"
#include "ScriptableDocumentParser.h"
#include "ScriptEventListener.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLStyleElement::HTMLStyleElement(const QualifiedName& tagName, Document* document, bool createdByParser)
    : HTMLElement(tagName, document)
    , m_loading(false)
    , m_createdByParser(createdByParser)
    , m_startLineNumber(0)
{
    ASSERT(hasTagName(styleTag));
    if (createdByParser && document && document->scriptableDocumentParser())
        m_startLineNumber = document->scriptableDocumentParser()->lineNumber();
}

PassRefPtr<HTMLStyleElement> HTMLStyleElement::create(const QualifiedName& tagName, Document* document, bool createdByParser)
{
    return adoptRef(new HTMLStyleElement(tagName, document, createdByParser));
}

void HTMLStyleElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == titleAttr && m_sheet)
        m_sheet->setTitle(attr->value());
    else if (attr->name() == onbeforeprocessAttr)
        setAttributeEventListener(eventNames().beforeprocessEvent, createAttributeEventListener(this, attr));
    else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLStyleElement::finishParsingChildren()
{
    StyleElement::process(this, m_startLineNumber);
    StyleElement::sheet(this);
    m_createdByParser = false;
    HTMLElement::finishParsingChildren();
}

void HTMLStyleElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();

    document()->addStyleSheetCandidateNode(this, m_createdByParser);
    if (!m_createdByParser)
        StyleElement::insertedIntoDocument(document(), this);
}

void HTMLStyleElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();
    document()->removeStyleSheetCandidateNode(this);
    StyleElement::removedFromDocument(document());
}

void HTMLStyleElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    if (!changedByParser)
        StyleElement::process(this, 0);
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

StyleSheet* HTMLStyleElement::sheet()
{
    return StyleElement::sheet(this);
}

bool HTMLStyleElement::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return static_cast<CSSStyleSheet *>(m_sheet.get())->isLoading();
}

bool HTMLStyleElement::sheetLoaded()
{
    if (!isLoading()) {
        document()->removePendingSheet();
        return true;
    }
    return false;
}

const AtomicString& HTMLStyleElement::media() const
{
    return getAttribute(mediaAttr);
}

const AtomicString& HTMLStyleElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLStyleElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{    
    HTMLElement::addSubresourceAttributeURLs(urls);

    if (StyleSheet* styleSheet = const_cast<HTMLStyleElement*>(this)->sheet())
        styleSheet->addSubresourceStyleURLs(urls);
}

}
