/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(VIDEO)
#include "MediaDocument.h"

#include "DocumentLoader.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLEmbedElement.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "KeyboardEvent.h"
#include "MainResourceLoader.h"
#include "NodeList.h"
#include "Page.h"
#include "SegmentedString.h"
#include "Settings.h"
#include "Text.h"
#include "XMLDocumentParser.h"

namespace WebCore {

using namespace HTMLNames;

class MediaDocumentParser : public DocumentParser {
public:
    MediaDocumentParser(Document* document)
        : DocumentParser(document)
        , m_mediaElement(0)
    {
    }

private:
    virtual void write(const SegmentedString&, bool appendData);
    virtual void finish();
    virtual bool finishWasCalled();
    virtual bool isWaitingForScripts() const;
        
    virtual bool wantsRawData() const { return true; }
    virtual bool writeRawData(const char* data, int len);
        
    void createDocumentStructure();

    HTMLMediaElement* m_mediaElement;
};

void MediaDocumentParser::write(const SegmentedString&, bool)
{
    ASSERT_NOT_REACHED();
}
    
void MediaDocumentParser::createDocumentStructure()
{
    ExceptionCode ec;
    RefPtr<Element> rootElement = document()->createElement(htmlTag, false);
    document()->appendChild(rootElement, ec);
        
    RefPtr<Element> body = document()->createElement(bodyTag, false);
    body->setAttribute(styleAttr, "background-color: rgb(38,38,38);");

    rootElement->appendChild(body, ec);
        
    RefPtr<Element> mediaElement = document()->createElement(videoTag, false);
        
    m_mediaElement = static_cast<HTMLVideoElement*>(mediaElement.get());
    m_mediaElement->setAttribute(controlsAttr, "");
    m_mediaElement->setAttribute(autoplayAttr, "");
    m_mediaElement->setAttribute(styleAttr, "margin: auto; position: absolute; top: 0; right: 0; bottom: 0; left: 0;");

    m_mediaElement->setAttribute(nameAttr, "media");
    m_mediaElement->setSrc(document()->url());
    
    body->appendChild(mediaElement, ec);

    Frame* frame = document()->frame();
    if (!frame)
        return;

    frame->loader()->activeDocumentLoader()->mainResourceLoader()->setShouldBufferData(false);
}
    
bool MediaDocumentParser::writeRawData(const char*, int)
{
    ASSERT(!m_mediaElement);
    if (m_mediaElement)
        return false;
        
    createDocumentStructure();
    finish();
    return false;
}

void MediaDocumentParser::finish()
{
    if (!m_parserStopped) 
        document()->finishedParsing();
}

bool MediaDocumentParser::finishWasCalled()
{
    // finish() always calls document()->finishedParsing() so we'll be deleted
    // after finish().
    return false;
}

bool MediaDocumentParser::isWaitingForScripts() const
{
    // A media document is never waiting for scripts
    return false;
}
    
MediaDocument::MediaDocument(Frame* frame, const KURL& url)
    : HTMLDocument(frame, url)
    , m_replaceMediaElementTimer(this, &MediaDocument::replaceMediaElementTimerFired)
{
    setParseMode(Compat);
}

MediaDocument::~MediaDocument()
{
    ASSERT(!m_replaceMediaElementTimer.isActive());
}

DocumentParser* MediaDocument::createParser()
{
    return new MediaDocumentParser(this);
}

void MediaDocument::defaultEventHandler(Event* event)
{
    // Match the default Quicktime plugin behavior to allow 
    // clicking and double-clicking to pause and play the media.
    Node* targetNode = event->target()->toNode();
    if (targetNode && targetNode->hasTagName(videoTag)) {
        HTMLVideoElement* video = static_cast<HTMLVideoElement*>(targetNode);
        if (event->type() == eventNames().clickEvent) {
            if (!video->canPlay()) {
                video->pause(event->fromUserGesture());
                event->setDefaultHandled();
            }
        } else if (event->type() == eventNames().dblclickEvent) {
            if (video->canPlay()) {
                video->play(event->fromUserGesture());
                event->setDefaultHandled();
            }
        }
    }

    if (event->type() == eventNames().keydownEvent && event->isKeyboardEvent()) {
        HTMLVideoElement* video = 0;
        if (targetNode) {
            if (targetNode->hasTagName(videoTag))
                video = static_cast<HTMLVideoElement*>(targetNode);
            else {
                RefPtr<NodeList> nodeList = targetNode->getElementsByTagName("video");
                if (nodeList.get()->length() > 0)
                    video = static_cast<HTMLVideoElement*>(nodeList.get()->item(0));
            }
        }
        if (video) {
            KeyboardEvent* keyboardEvent = static_cast<KeyboardEvent*>(event);
            if (keyboardEvent->keyIdentifier() == "U+0020") { // space
                if (video->paused()) {
                    if (video->canPlay())
                        video->play(event->fromUserGesture());
                } else
                    video->pause(event->fromUserGesture());
                event->setDefaultHandled();
            }
        }
    }
}

void MediaDocument::mediaElementSawUnsupportedTracks()
{
    // The HTMLMediaElement was told it has something that the underlying 
    // MediaPlayer cannot handle so we should switch from <video> to <embed> 
    // and let the plugin handle this. Don't do it immediately as this 
    // function may be called directly from a media engine callback, and 
    // replaceChild will destroy the element, media player, and media engine.
    m_replaceMediaElementTimer.startOneShot(0);
}

void MediaDocument::replaceMediaElementTimerFired(Timer<MediaDocument>*)
{
    HTMLElement* htmlBody = body();
    if (!htmlBody)
        return;

    // Set body margin width and height to 0 as that is what a PluginDocument uses.
    htmlBody->setAttribute(marginwidthAttr, "0");
    htmlBody->setAttribute(marginheightAttr, "0");

    RefPtr<NodeList> nodeList = htmlBody->getElementsByTagName("video");

    if (nodeList.get()->length() > 0) {
        HTMLVideoElement* videoElement = static_cast<HTMLVideoElement*>(nodeList.get()->item(0));

        RefPtr<Element> element = Document::createElement(embedTag, false);
        HTMLEmbedElement* embedElement = static_cast<HTMLEmbedElement*>(element.get());

        embedElement->setAttribute(widthAttr, "100%");
        embedElement->setAttribute(heightAttr, "100%");
        embedElement->setAttribute(nameAttr, "plugin");
        embedElement->setAttribute(srcAttr, url().string());
        embedElement->setAttribute(typeAttr, frame()->loader()->writer()->mimeType());

        ExceptionCode ec;
        videoElement->parent()->replaceChild(embedElement, videoElement, ec);
    }
}

}
#endif
