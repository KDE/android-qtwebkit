/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginDocument.h"

#include "DocumentLoader.h"
#include "Element.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLEmbedElement.h"
#include "HTMLNames.h"
#include "MainResourceLoader.h"
#include "Page.h"
#include "RenderEmbeddedObject.h"
#include "RenderWidget.h"
#include "SegmentedString.h"
#include "Settings.h"
#include "Text.h"
#include "XMLDocumentParser.h"

namespace WebCore {
    
using namespace HTMLNames;
    
class PluginDocumentParser : public DocumentParser {
public:
    PluginDocumentParser(Document* document)
        : DocumentParser(document)
        , m_embedElement(0)
    {
    }

    static Widget* pluginWidgetFromDocument(Document*);
        
private:
    virtual void write(const SegmentedString&, bool appendData);
    virtual void finish();
    virtual bool finishWasCalled();
    virtual bool isWaitingForScripts() const;
        
    virtual bool wantsRawData() const { return true; }
    virtual bool writeRawData(const char* data, int len);
        
    void createDocumentStructure();

    HTMLEmbedElement* m_embedElement;
};

Widget* PluginDocumentParser::pluginWidgetFromDocument(Document* doc)
{
    ASSERT(doc);
    RefPtr<Element> body = doc->body();
    if (body) {
        RefPtr<Node> node = body->firstChild();
        if (node && node->renderer()) {
            ASSERT(node->renderer()->isEmbeddedObject());
            return toRenderEmbeddedObject(node->renderer())->widget();
        }
    }
    return 0;
}

void PluginDocumentParser::write(const SegmentedString&, bool)
{
    ASSERT_NOT_REACHED();
}
    
void PluginDocumentParser::createDocumentStructure()
{
    ExceptionCode ec;
    RefPtr<Element> rootElement = document()->createElement(htmlTag, false);
    document()->appendChild(rootElement, ec);

    RefPtr<Element> body = document()->createElement(bodyTag, false);
    body->setAttribute(marginwidthAttr, "0");
    body->setAttribute(marginheightAttr, "0");
    body->setAttribute(bgcolorAttr, "rgb(38,38,38)");

    rootElement->appendChild(body, ec);
        
    RefPtr<Element> embedElement = document()->createElement(embedTag, false);
        
    m_embedElement = static_cast<HTMLEmbedElement*>(embedElement.get());
    m_embedElement->setAttribute(widthAttr, "100%");
    m_embedElement->setAttribute(heightAttr, "100%");
    
    m_embedElement->setAttribute(nameAttr, "plugin");
    m_embedElement->setAttribute(srcAttr, document()->url().string());
    m_embedElement->setAttribute(typeAttr, document()->frame()->loader()->writer()->mimeType());
    
    body->appendChild(embedElement, ec);    
}
    
bool PluginDocumentParser::writeRawData(const char*, int)
{
    ASSERT(!m_embedElement);
    if (m_embedElement)
        return false;
    
    createDocumentStructure();

    if (Frame* frame = document()->frame()) {
        Settings* settings = frame->settings();
        if (settings && frame->loader()->subframeLoader()->allowPlugins(NotAboutToInstantiatePlugin)) {
            document()->updateLayout();

            if (RenderWidget* renderer = toRenderWidget(m_embedElement->renderer())) {
                frame->loader()->client()->redirectDataToPlugin(renderer->widget());
                frame->loader()->activeDocumentLoader()->mainResourceLoader()->setShouldBufferData(false);
            }

            finish();
        }
    }

    return false;
}

void PluginDocumentParser::finish()
{
    if (!m_parserStopped) 
        document()->finishedParsing();            
}

bool PluginDocumentParser::finishWasCalled()
{
    // finish() always calls document()->finishedParsing() so we'll be deleted
    // after finish().
    return false;
}

bool PluginDocumentParser::isWaitingForScripts() const
{
    // A plugin document is never waiting for scripts
    return false;
}
    
PluginDocument::PluginDocument(Frame* frame, const KURL& url)
    : HTMLDocument(frame, url)
{
    setParseMode(Compat);
}
    
DocumentParser* PluginDocument::createParser()
{
    return new PluginDocumentParser(this);
}

Widget* PluginDocument::pluginWidget()
{
    return PluginDocumentParser::pluginWidgetFromDocument(this);
}

Node* PluginDocument::pluginNode()
{
    RefPtr<Element> body_element = body();
    if (body_element)
        return body_element->firstChild();

    return 0;
}

}
