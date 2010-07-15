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

#ifndef Plugin_h
#define Plugin_h

#include <WebCore/KURL.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {
    class GraphicsContext;
    class IntRect;
}

namespace WebKit {

class PluginController;

class Plugin : public RefCounted<Plugin> {
public:
    struct Parameters {
        WebCore::KURL url;
        Vector<WebCore::String> names;
        Vector<WebCore::String> values;
        WebCore::String mimeType;
        bool loadManually;
    };

    virtual ~Plugin();
    
    // Initializes the plug-in. If the plug-in fails to initialize this should return false.
    virtual bool initialize(PluginController*, const Parameters&) = 0;

    // Destroys the plug-in.
    virtual void destroy() = 0;

    // Tells the plug-in to paint itself into the given graphics context. The passed in dirty rect is in window coordinates.
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& dirtyRect) = 0;

    // Tells the plug-in that either the plug-ins frame rect or its clip rect has changed. Both rects are in window coordinates.
    virtual void geometryDidChange(const WebCore::IntRect& frameRect, const WebCore::IntRect& clipRect) = 0;

    // Tells the plug-in that a frame load request that the plug-in made by calling PluginController::loadURL has finished.
    virtual void frameDidFinishLoading(uint64_t requestID) = 0;

    // Tells the plug-in that a frame load request that the plug-in made by calling PluginController::loadURL has failed.
    virtual void frameDidFail(uint64_t requestID, bool wasCancelled) = 0;

    // Tells the plug-in that a request to evaluate JavaScript (using PluginController::loadURL) has been fulfilled and passes
    // back the result. If evaluating the script failed, result will be null.
    virtual void didEvaluateJavaScript(uint64_t requestID, const WebCore::String& requestURLString, const WebCore::String& result) = 0;

    // Tells the plug-in that a stream has received its HTTP response.
    virtual void streamDidReceiveResponse(uint64_t streamID, const WebCore::KURL& responseURL, uint32_t streamLength, 
                                          uint32_t lastModifiedTime, const WebCore::String& mimeType, const WebCore::String& headers) = 0;

    // Tells the plug-in that a stream did receive data.
    virtual void streamDidReceiveData(uint64_t streamID, const char* bytes, int length) = 0;

    // Tells the plug-in that a stream has finished loading.
    virtual void streamDidFinishLoading(uint64_t streamID) = 0;

    // Tells the plug-in that a stream has failed to load, either because of network errors or because the load was cancelled.
    virtual void streamDidFail(uint64_t streamID, bool wasCancelled) = 0;

    // Returns the plug-in controller for this plug-in.
    // FIXME: We could just have the controller be a member variable of Plugin.
    virtual PluginController* controller() = 0;

protected:
    Plugin();
};
    
} // namespace WebKit

#endif // Plugin_h
