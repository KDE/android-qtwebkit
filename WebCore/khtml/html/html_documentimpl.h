/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifndef HTML_DOCUMENTIMPL_H
#define HTML_DOCUMENTIMPL_H

#include "xml/dom_docimpl.h"
#include "misc/loader_client.h"
#include "html/html_miscimpl.h"

class KHTMLView;
class QString;

namespace DOM {

    class HTMLElementImpl;

class HTMLDocumentImpl : public DOM::DocumentImpl, public khtml::CachedObjectClient
{
    Q_OBJECT
public:
    HTMLDocumentImpl(DOMImplementationImpl *_implementation, KHTMLView *v = 0);
    ~HTMLDocumentImpl();

    virtual bool isHTMLDocument() const { return true; }
    virtual ElementImpl *documentElement() const;

    DOMString referrer() const;
    DOMString lastModified() const;
    DOMString cookie() const;
    void setCookie( const DOMString &);

    void setBody(HTMLElementImpl *_body, int& exceptioncode);

    virtual khtml::Tokenizer *createTokenizer();

    virtual bool childAllowed( NodeImpl *newChild );

    virtual ElementImpl *createElement ( const DOMString &tagName, int &exceptioncode );

    virtual void determineParseMode( const QString &str );

    void addNamedImageOrForm(const QString &name);
    void removeNamedImageOrForm(const QString &name);
    bool haveNamedImageOrForm(const QString &name);

    HTMLCollectionImpl::CollectionInfo *collectionInfo(int type) { return m_collection_info+type; }

protected:
    HTMLElementImpl *bodyElement;
    HTMLElementImpl *htmlElement;

protected slots:
    /**
     * Repaints, so that all links get the proper color
     */
    void slotHistoryChanged();
private:
    HTMLCollectionImpl::CollectionInfo m_collection_info[HTMLCollectionImpl::LAST_TYPE];
    // we actually store ints inside the pointer value itself; would use void *
    // but that makes the template unhappy.
    QDict<char> namedImageAndFormCounts;
};

}; //namespace

#endif
