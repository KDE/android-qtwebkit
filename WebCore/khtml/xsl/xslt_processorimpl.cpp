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

#include "xslt_processorimpl.h"
#include "xsl_stylesheetimpl.h"
#include "html_documentimpl.h"
#include "loader.h"
#include "khtmlview.h"
#include "khtml_part.h"

#include <libxslt/xsltutils.h>
#include <libxslt/documents.h>
#include <libxslt/imports.h>

using namespace khtml;
using namespace DOM;

namespace DOM {
    
XSLTProcessorImpl::XSLTProcessorImpl(XSLStyleSheetImpl* sheet, DocumentImpl* source)
:m_stylesheet(sheet), m_sourceDocument(source)
{
    if (m_stylesheet)
        m_stylesheet->ref();
    if (m_sourceDocument)
        m_sourceDocument->ref();
}

XSLTProcessorImpl::~XSLTProcessorImpl()
{
    if (m_stylesheet)
        m_stylesheet->deref();
    if (m_sourceDocument)
        m_sourceDocument->deref();
}

static XSLStyleSheetImpl* globalSheet = 0;
static xmlDocPtr stylesheetLoadFunc(const xmlChar* uri,
                                    xmlDictPtr dict,
                                    int options,
                                    void* ctxt,
                                    xsltLoadType type)
{
    if (type != XSLT_LOAD_STYLESHEET)
        return NULL; // FIXME: Add support for XSLT_LOAD_DOCUMENT for the document() function.
    
    if (!globalSheet)
        return NULL;
    return globalSheet->locateStylesheetSubResource(((xsltStylesheetPtr)ctxt)->doc, uri);
}

DocumentImpl* XSLTProcessorImpl::transformDocument(DocumentImpl* doc)
{
    // FIXME: Right now we assume |doc| is unparsed, but if it has been parsed we will need to serialize it
    // and then feed that resulting source to libxslt.
    m_resultOutput = "";

    if (!m_stylesheet || !m_stylesheet->document()) return 0;
        
    globalSheet = m_stylesheet;
    xsltSetLoaderFunc(stylesheetLoadFunc);

    xsltStylesheetPtr sheet = m_stylesheet->compileStyleSheet();

    globalSheet = 0;
    xsltSetLoaderFunc(0);

    if (!sheet) return 0;
    m_stylesheet->clearDocuments();
  
    // Get the parsed source document.
    xmlDocPtr sourceDoc = (xmlDocPtr)doc->transformSource();
    xmlDocPtr resultDoc = xsltApplyStylesheet(sheet, sourceDoc, NULL);
    DocumentImpl* result = documentFromXMLDocPtr(resultDoc, sheet);
    xsltFreeStylesheet(sheet);
    return result;
}

static int bufferWrite(void* context, const char* buffer, int len)
{
    static_cast<XSLTProcessorImpl*>(context)->addToResult(buffer, len);
    return len;
}

void XSLTProcessorImpl::addToResult(const char* buffer, int len)
{
    m_resultOutput += QString(buffer, len);
}

DocumentImpl* XSLTProcessorImpl::documentFromXMLDocPtr(xmlDocPtr resultDoc, xsltStylesheetPtr sheet)
{
    // FIXME: For now we serialize and then reparse.  It might be more optimal to write a DOM
    // converter.
    if (!resultDoc || !sheet) return 0;
    DocumentImpl* result = 0;
    xmlOutputBufferPtr outputBuf = xmlAllocOutputBuffer(NULL);
    if (outputBuf) {
        outputBuf->context = this;
        outputBuf->writecallback = bufferWrite;
        
        if (xsltSaveResultTo(outputBuf, resultDoc, sheet) < 0)
            return 0;
        
        // There are three types of output we need to be able to deal with:
        // HTML (create an HTML document), XML (create an XML document), and text (wrap in a <pre> and
        // make an XML document).
        KHTMLView* view = m_sourceDocument->view();
        const xmlChar* method;
        XSLT_GET_IMPORT_PTR(method, sheet, method);
        if (method == NULL && resultDoc->type == XML_HTML_DOCUMENT_NODE)
            method = (const xmlChar*)"html";
        if (xmlStrEqual(method, (const xmlChar*)"html"))
            result = m_sourceDocument->implementation()->createHTMLDocument(view);
        else
            result = m_sourceDocument->implementation()->createDocument(view);
        result->attach();
        result->setURL(m_sourceDocument->URL());
        result->setBaseURL(m_sourceDocument->baseURL());
        result->setDecoder(m_sourceDocument->decoder()); // FIXME: Should just be UTF-16.
        result->docLoader()->setShowAnimations(m_sourceDocument->docLoader()->showAnimations());
        result->setTransformSourceDocument(m_sourceDocument);

        if (xmlStrEqual(method, (const xmlChar*)"text")) {
            // Modify the output so that it is a well-formed XHTML document with a <pre> tag enclosing
            // the text.
            QString beforeString("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/strict.dtd\">\n<html xmlns=\"http://www.w3.org/1999/xhtml\">\n<body>\n<pre>\n<![CDATA[");
            QString afterString("]]>\n</pre>\n</body>\n</html>\n");
            m_resultOutput = beforeString + m_resultOutput + afterString;
        }
        
        // Before parsing, we need to detach the old document completely and get the new document
        // in place.  We have to do this only if we're rendering the result document.
        if (view) {
            view->clear();
            view->part()->replaceDocImpl(result);
        }
        
        result->open();
        result->determineParseMode(m_resultOutput); // Make sure we parse in the correct mode.
        result->write(m_resultOutput);
        result->finishParsing();
        if (view)
            view->part()->checkCompleted();
        else
            result->close(); // FIXME: Even viewless docs can load subresources. onload will fire too early.
                             // This is probably a bug in XMLHttpRequestObjects as well.
    }
    return result;
}

}
#endif
