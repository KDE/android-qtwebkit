/**
 * This file is part of the XSL implementation.
 *
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
 */

#ifdef KHTML_XSLT

#include "dom/dom_string.h"
#include "dom/dom_exception.h"

#include "xml/dom_nodeimpl.h"
#include "html/html_documentimpl.h"
#include "misc/loader.h"
#include "xsl_stylesheetimpl.h"

#include <kdebug.h>

#include <libxslt/xsltutils.h>
#include <libxml/uri.h>

#define IS_BLANK_NODE(n)                                                \
    (((n)->type == XML_TEXT_NODE) && (xsltIsBlank((n)->content)))

using namespace khtml;
using namespace DOM;

namespace DOM {
    
XSLStyleSheetImpl::XSLStyleSheetImpl(XSLImportRuleImpl *parentRule, DOMString href)
    : StyleSheetImpl(parentRule, href)
{
    m_lstChildren = new QPtrList<StyleBaseImpl>;
    m_ownerDocument = 0;
    m_processed = false; // Child sheets get marked as processed when the libxslt engine has finally seen them.
}

XSLStyleSheetImpl::XSLStyleSheetImpl(NodeImpl *parentNode, DOMString href,  bool embedded)
    : StyleSheetImpl(parentNode, href)
{
    m_lstChildren = new QPtrList<StyleBaseImpl>;
    m_ownerDocument = parentNode->getDocument();
    m_embedded = embedded;
    m_processed = true; // The root sheet starts off processed.
}

XSLStyleSheetImpl::~XSLStyleSheetImpl()
{
    if (!m_embedded)
        xmlFreeDoc(m_stylesheetDoc);
}

bool XSLStyleSheetImpl::isLoading()
{
    for (StyleBaseImpl* rule = m_lstChildren->first(); rule; rule = m_lstChildren->next()) {
        if (rule->isImportRule()) {
            XSLImportRuleImpl* import = static_cast<XSLImportRuleImpl*>(rule);
            if (import->isLoading())
                return true;
        }
    }
    return false;
}

void XSLStyleSheetImpl::checkLoaded()
{
    if (isLoading()) 
        return;
    if (m_parent)
        m_parent->checkLoaded();
    if (m_parentNode)
        m_parentNode->sheetLoaded();
}

void XSLStyleSheetImpl::clearDocuments()
{
    m_stylesheetDoc = 0;
    for (StyleBaseImpl* rule = m_lstChildren->first(); rule; rule = m_lstChildren->next()) {
        if (rule->isImportRule()) {
            XSLImportRuleImpl* import = static_cast<XSLImportRuleImpl*>(rule);
            if (import->styleSheet())
                import->styleSheet()->clearDocuments();
        }
    }
}

khtml::DocLoader* XSLStyleSheetImpl::docLoader()
{
    if (!m_ownerDocument)
        return 0;
    return m_ownerDocument->docLoader();
}

bool XSLStyleSheetImpl::parseString(const DOMString &string, bool strict)
{
    // Parse in a single chunk into an xmlDocPtr
    const QChar BOM(0xFEFF);
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char *>(&BOM);
    m_stylesheetDoc = xmlReadMemory(reinterpret_cast<const char *>(string.unicode()),
                                    string.length() * sizeof(QChar),
                                    m_ownerDocument->URL().ascii(),
                                    BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE", 
                                    XML_PARSE_NOCDATA|XML_PARSE_DTDATTR|XML_PARSE_NOENT);
    loadChildSheets();
    return m_stylesheetDoc;
}

void XSLStyleSheetImpl::loadChildSheets()
{
    if (!m_stylesheetDoc)
        return;
    
    xmlNodePtr stylesheetRoot = m_stylesheetDoc->children;
    if (m_embedded) {
        // We have to locate (by ID) the appropriate embedded stylesheet element, so that we can walk the 
        // import/include list.
        xmlAttrPtr idNode = xmlGetID(m_stylesheetDoc, (const xmlChar*)(const char*)(href().string().utf8()));
        if (idNode == NULL)
            return;
        stylesheetRoot = idNode->parent;
    } else {
        // FIXME: Need to handle an external URI with a # in it.  This is a pretty minor edge case, so we'll deal
        // with it later.
    }
    
    if (stylesheetRoot) {
        // Walk the children of the root element and look for import/include elements.
        // Imports must occur first.
        xmlNodePtr curr = stylesheetRoot->children;
        while (curr) {
            if (IS_BLANK_NODE(curr)) {
                curr = curr->next;
                continue;
            }
            if (IS_XSLT_ELEM(curr) && IS_XSLT_NAME(curr, "import")) {
                xmlChar* uriRef = xsltGetNsProp(curr, (const xmlChar *)"href", XSLT_NAMESPACE);
                QString buff = QString::fromUtf8((const char*)uriRef);
                loadChildSheet(buff);
            } else
                break;
            curr = curr->next;
        }

        // Now handle includes.
        while (curr) {
            if (IS_XSLT_ELEM(curr) && IS_XSLT_NAME(curr, "include")) {
                xmlChar* uriRef = xsltGetNsProp(curr, (const xmlChar *)"href", XSLT_NAMESPACE);
                QString buff = QString::fromUtf8((const char*)uriRef);
                loadChildSheet(buff);
            }
            curr = curr->next;
        }
    }
}

void XSLStyleSheetImpl::loadChildSheet(const QString& href)
{
    XSLImportRuleImpl* childRule = new XSLImportRuleImpl(this, href);
    m_lstChildren->append(childRule);
    childRule->loadSheet();
}

XSLStyleSheetImpl* XSLImportRuleImpl::parentStyleSheet() const
{
    return (m_parent && m_parent->isXSLStyleSheet()) ? static_cast<XSLStyleSheetImpl*>(m_parent) : 0;
}

xsltStylesheetPtr XSLStyleSheetImpl::compileStyleSheet()
{
    // FIXME: Hook up error reporting for the stylesheet compilation process.
    if (m_embedded)
        return xsltLoadStylesheetPI(m_stylesheetDoc);
    else
        return xsltParseStylesheetDoc(m_stylesheetDoc);
}

xmlDocPtr XSLStyleSheetImpl::locateStylesheetSubResource(xmlDocPtr parentDoc, const xmlChar* uri)
{
    bool matchedParent = (parentDoc == m_stylesheetDoc);
    for (StyleBaseImpl* rule = m_lstChildren->first(); rule; rule = m_lstChildren->next()) {
        if (rule->isImportRule()) {
            XSLImportRuleImpl* import = static_cast<XSLImportRuleImpl*>(rule);
            XSLStyleSheetImpl* child = import->styleSheet();
            if (!child) continue;
            if (matchedParent) {
                if (child->processed()) continue; // libxslt has been given this sheet already.
                
                // Check the URI of the child stylesheet against the doc URI.
                // In order to ensure that libxml canonicalized both URLs, we get the original href
                // string from the import rule and canonicalize it using libxml before comparing it
                // with the URI argument.
                QCString importHref = import->href().string().utf8();
                xmlChar* base = xmlNodeGetBase(parentDoc, (xmlNodePtr)parentDoc);
                xmlChar* childURI = xmlBuildURI((const xmlChar*)(const char*)importHref, base);
                if (xmlStrEqual(uri, childURI)) {
                    child->markAsProcessed();
                    return child->document();
                }
            }
            else {
                xmlDocPtr result = import->styleSheet()->locateStylesheetSubResource(parentDoc, uri);
                if (result)
                    return result;
            }
        }
    }
    
    return NULL;
}

// ----------------------------------------------------------------------------------------------

XSLImportRuleImpl::XSLImportRuleImpl(StyleBaseImpl* parent, const DOMString &href)
: StyleBaseImpl(parent)
{
    m_strHref = href;
    m_styleSheet = 0;
    m_cachedSheet = 0;
    m_loading = false;
}

XSLImportRuleImpl::~XSLImportRuleImpl()
{
    if (m_styleSheet) {
        m_styleSheet->setParent(0);
        m_styleSheet->deref();
    }
    
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
}

void XSLImportRuleImpl::setStyleSheet(const DOMString& url, const DOMString& sheet)
{
    if (m_styleSheet) {
        m_styleSheet->setParent(0);
        m_styleSheet->deref();
    }
    m_styleSheet = new XSLStyleSheetImpl(this, url);
    m_styleSheet->ref();
    
    XSLStyleSheetImpl* parent = parentStyleSheet();
    if (parent)
        m_styleSheet->setOwnerDocument(parent->ownerDocument());

    m_styleSheet->parseString(sheet);
    m_loading = false;
    
    checkLoaded();
}

bool XSLImportRuleImpl::isLoading()
{
    return (m_loading || (m_styleSheet && m_styleSheet->isLoading()));
}

void XSLImportRuleImpl::loadSheet()
{
    DocLoader* docLoader = 0;
    StyleBaseImpl* root = this;
    StyleBaseImpl* parent;
    while ((parent = root->parent()))
	root = parent;
    if (root->isXSLStyleSheet())
	docLoader = static_cast<XSLStyleSheetImpl*>(root)->docLoader();
    
    DOMString absHref = m_strHref;
    XSLStyleSheetImpl* parentSheet = parentStyleSheet();
    if (!parentSheet->href().isNull())
        // use parent styleheet's URL as the base URL
        absHref = KURL(parentSheet->href().string(),m_strHref.string()).url();
    
    // Check for a cycle in our import chain.  If we encounter a stylesheet
    // in our parent chain with the same URL, then just bail.
    for (parent = static_cast<StyleBaseImpl*>(this)->parent(); parent; parent = parent->parent())
        if (absHref == parent->baseURL())
            return;
    
    m_cachedSheet = docLoader->requestXSLStyleSheet(absHref);
    
    if (m_cachedSheet) {
        m_cachedSheet->ref(this);
        
        // If the imported sheet is in the cache, then setStyleSheet gets called,
        // and the sheet even gets parsed (via parseString).  In this case we have
        // loaded (even if our subresources haven't), so if we have a stylesheet after
        // checking the cache, then we've clearly loaded.
        if (!m_styleSheet)
            m_loading = true;
    }
}

}
#endif
