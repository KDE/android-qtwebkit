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

#ifndef DrawingAreaProxyUpdateChunk_h
#define DrawingAreaProxyUpdateChunk_h

#include "DrawingAreaProxy.h"
#include <WebCore/IntSize.h>
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@class WKView;
#else
class WKView;
#endif

namespace WebKit {

class UpdateChunk;

class DrawingAreaProxyUpdateChunk : public DrawingAreaProxy {
public:
    DrawingAreaProxyUpdateChunk(WKView*);
    virtual ~DrawingAreaProxyUpdateChunk();

    virtual void drawRectIntoContext(CGRect, CGContextRef);
    virtual void setSize(const WebCore::IntSize&);

    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder&);

    // The DrawingAreaProxy should never be decoded itself. Instead, the DrawingArea should be decoded.
    virtual void encode(CoreIPC::ArgumentEncoder& encoder) const
    {
        DrawingAreaProxy::encode(encoder);
    }

private:
    void drawUpdateChunkIntoBackingStore(UpdateChunk&);
    void ensureBackingStore();

    void didSetSize(const WebCore::IntSize& viewSize, UpdateChunk& updateChunk);

    bool m_isInitialized;
    bool m_isWaitingForDidSetFrameNotification;

    WebCore::IntSize m_viewSize; // Size of the BackingStore as well.
    WebCore::IntSize m_lastSetViewSize;

    RetainPtr<CGContextRef> m_bitmapContext;

    WKView* m_webView;
};
    
} // namespace WebKit

#endif // DrawingAreaProxyUpdateChunk_h
