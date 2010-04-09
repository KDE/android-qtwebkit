/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DrawingAreaUpdateChunk.h"

#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "UpdateChunk.h"
#include "WebCoreTypeArgumentMarshalling.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/GraphicsContext.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

namespace WebKit {

DrawingAreaUpdateChunk::DrawingAreaUpdateChunk(WebPage* webPage)
    : DrawingArea(DrawingAreaUpdateChunkType, webPage)
    , m_displayTimer(WebProcess::shared().runLoop(), this, &DrawingAreaUpdateChunk::display)
{
}

DrawingAreaUpdateChunk::~DrawingAreaUpdateChunk()
{
}

void DrawingAreaUpdateChunk::invalidateWindow(const IntRect& rect, bool immediate)
{
}

void DrawingAreaUpdateChunk::invalidateContentsAndWindow(const IntRect& rect, bool immediate)
{
    setNeedsDisplay(rect);
}

void DrawingAreaUpdateChunk::invalidateContentsForSlowScroll(const IntRect& rect, bool immediate)
{
    setNeedsDisplay(rect);
}

void DrawingAreaUpdateChunk::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    // FIXME: Do something much smarter.
    setNeedsDisplay(rectToScroll);
}

void DrawingAreaUpdateChunk::setNeedsDisplay(const IntRect& rect)
{
    // FIXME: Collect a set of rects/region instead of just the union
    // of all rects.
    m_dirtyRect.unite(rect);
    scheduleDisplay();
}

void DrawingAreaUpdateChunk::display()
{
    if (m_dirtyRect.isEmpty())
        return;

    // Layout if necessary.
    m_webPage->layoutIfNeeded();
 
    IntRect dirtyRect = m_dirtyRect;
    m_dirtyRect = IntRect();

    // FIXME: Move this code to draw into an UpdateChunk into a seperate function.

    // Create a new UpdateChunk and paint into it.
    UpdateChunk updateChunk(dirtyRect);

    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> bitmapContext(AdoptCF, CGBitmapContextCreate(updateChunk.data(), dirtyRect.width(), dirtyRect.height(), 8, dirtyRect.width() * 4, colorSpace.get(), kCGImageAlphaPremultipliedLast));

    // Now paint into the backing store.
    GraphicsContext graphicsContext(bitmapContext.get());
    graphicsContext.translate(-dirtyRect.x(), -dirtyRect.y());

    m_webPage->drawRect(graphicsContext, dirtyRect);

    WebProcess::shared().connection()->send(DrawingAreaProxyMessage::Update, m_webPage->pageID(), CoreIPC::In(updateChunk));

    m_displayTimer.stop();
}

void DrawingAreaUpdateChunk::scheduleDisplay()
{
    if (m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0);
}

void DrawingAreaUpdateChunk::setSize(const IntSize& viewSize)
{
    m_webPage->setSize(viewSize);

    // FIXME: Move this code to draw into an UpdateChunk into a seperate function.

    // Layout if necessary.
    m_webPage->layoutIfNeeded();

    IntRect rect(0, 0, viewSize.width(), viewSize.height());

    // Create a new UpdateChunk and paint into it.
    UpdateChunk updateChunk(rect);

    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> bitmapContext(AdoptCF, CGBitmapContextCreate(updateChunk.data(), rect.width(), rect.height(), 8, rect.width() * 4, colorSpace.get(), kCGImageAlphaPremultipliedLast));

    // Now paint into the backing store.
    GraphicsContext graphicsContext(bitmapContext.get());
    graphicsContext.translate(-rect.x(), -rect.y());

    m_webPage->drawRect(graphicsContext, rect);

    m_displayTimer.stop();

    WebProcess::shared().connection()->send(DrawingAreaProxyMessage::DidSetFrame, m_webPage->pageID(), CoreIPC::In(viewSize, updateChunk));
}

void DrawingAreaUpdateChunk::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder& arguments)
{
    switch (messageID.get<DrawingAreaMessage::Kind>()) {
        case DrawingAreaMessage::SetFrame: {
            IntSize size;
            if (!arguments.decode(CoreIPC::Out(size)))
                return;

            setSize(size);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

} // namespace WebKit
