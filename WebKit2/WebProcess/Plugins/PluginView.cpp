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

#include "PluginView.h"

#include "Plugin.h"
#include <WebCore/DocumentLoader.h>
#include <WebCore/Event.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/HostWindow.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/RenderLayer.h>
#include <WebCore/ScrollView.h>

using namespace JSC;
using namespace WebCore;

namespace WebKit {

class PluginView::URLRequest : public RefCounted<URLRequest> {
public:
    static PassRefPtr<PluginView::URLRequest> create(uint64_t requestID, const FrameLoadRequest& request, bool allowPopups)
    {
        return adoptRef(new URLRequest(requestID, request, allowPopups));
    }

    uint64_t requestID() const { return m_requestID; }
    const String& target() const { return m_request.frameName(); }
    const ResourceRequest & request() const { return m_request.resourceRequest(); }
    bool allowPopups() const { return m_allowPopups; }

private:
    URLRequest(uint64_t requestID, const FrameLoadRequest& request, bool allowPopups)
        : m_requestID(requestID)
        , m_request(request)
        , m_allowPopups(allowPopups)
    {
    }

    uint64_t m_requestID;
    FrameLoadRequest m_request;
    bool m_allowPopups;
};

class PluginView::Stream : public RefCounted<PluginView::Stream>, NetscapePlugInStreamLoaderClient {
public:
    static PassRefPtr<Stream> create(PluginView* pluginView, uint64_t streamID, const ResourceRequest& request)
    {
        return adoptRef(new Stream(pluginView, streamID, request));
    }
    ~Stream();

    void start();
    void cancel();

    uint64_t streamID() const { return m_streamID; }

private:
    Stream(PluginView* pluginView, uint64_t streamID, const ResourceRequest& request)
        : m_pluginView(pluginView)
        , m_streamID(streamID)
        , m_request(request)
        , m_streamWasCancelled(false)
    {
    }

    // NetscapePluginStreamLoaderClient
    virtual void didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse&);
    virtual void didReceiveData(NetscapePlugInStreamLoader*, const char*, int);
    virtual void didFail(NetscapePlugInStreamLoader*, const ResourceError&);
    virtual void didFinishLoading(NetscapePlugInStreamLoader*);

    PluginView* m_pluginView;
    uint64_t m_streamID;
    const ResourceRequest m_request;
    
    // True if the stream was explicitly cancelled by calling cancel().
    // (As opposed to being cancelled by the user hitting the stop button for example.
    bool m_streamWasCancelled;
    
    RefPtr<NetscapePlugInStreamLoader> m_loader;
};

PluginView::Stream::~Stream()
{
    ASSERT(!m_pluginView);
}
    
void PluginView::Stream::start()
{
    ASSERT(!m_loader);

    Frame* frame = m_pluginView->m_pluginElement->document()->frame();
    ASSERT(frame);

    m_loader = NetscapePlugInStreamLoader::create(frame, this);
    m_loader->setShouldBufferData(false);
    
    m_loader->documentLoader()->addPlugInStreamLoader(m_loader.get());
    m_loader->load(m_request);
}

void PluginView::Stream::cancel()
{
    ASSERT(m_loader);

    m_streamWasCancelled = true;
    m_loader->cancel(m_loader->cancelledError());
    m_loader = 0;
}

void PluginView::Stream::didReceiveResponse(NetscapePlugInStreamLoader*, const ResourceResponse& response)
{
    // Compute the stream related data from the resource response.
    const KURL& responseURL = response.url();
    const String& mimeType = response.mimeType();
    long long expectedContentLength = response.expectedContentLength();
    
    String headers;
    if (response.isHTTP()) {
        Vector<UChar> stringBuilder;
        String separator(": ");

        String statusLine = String::format("HTTP %d ", response.httpStatusCode());
        stringBuilder.append(statusLine.characters(), statusLine.length());
        stringBuilder.append(response.httpStatusText().characters(), response.httpStatusText().length());
        stringBuilder.append('\n');

        HTTPHeaderMap::const_iterator end = response.httpHeaderFields().end();
        for (HTTPHeaderMap::const_iterator it = response.httpHeaderFields().begin(); it != end; ++it) {
            stringBuilder.append(it->first.characters(), it->first.length());
            stringBuilder.append(separator.characters(), separator.length());
            stringBuilder.append(it->second.characters(), it->second.length());
            stringBuilder.append('\n');
        }

        headers = String::adopt(stringBuilder);

        // If the content is encoded (most likely compressed), then don't send its length to the plugin,
        // which is only interested in the decoded length, not yet known at the moment.
        // <rdar://problem/4470599> tracks a request for -[NSURLResponse expectedContentLength] to incorporate this logic.
        String contentEncoding = response.httpHeaderField("Content-Encoding");
        if (!contentEncoding.isNull() && contentEncoding != "identity")
            expectedContentLength = -1;
    }

    uint32_t streamLength = 0;
    if (expectedContentLength > 0)
        streamLength = expectedContentLength;

    m_pluginView->m_plugin->streamDidReceiveResponse(m_streamID, responseURL, streamLength, response.lastModifiedDate(), mimeType, headers);
}

void PluginView::Stream::didReceiveData(NetscapePlugInStreamLoader*, const char* bytes, int length)
{
    m_pluginView->m_plugin->streamDidReceiveData(m_streamID, bytes, length);
}

void PluginView::Stream::didFail(NetscapePlugInStreamLoader*, const ResourceError& error) 
{
    // Calling streamDidFail could cause us to be deleted, so we hold on to a reference here.
    RefPtr<Stream> protect(this);

    // We only want to call streamDidFail if the stream was not explicitly cancelled by the plug-in.
    if (!m_streamWasCancelled)
        m_pluginView->m_plugin->streamDidFail(m_streamID, error.isCancellation());

    m_pluginView->removeStream(this);
    m_pluginView = 0;
}

void PluginView::Stream::didFinishLoading(NetscapePlugInStreamLoader*)
{
    // Calling streamDidFinishLoading could cause us to be deleted, so we hold on to a reference here.
    RefPtr<Stream> protect(this);

    m_pluginView->m_plugin->streamDidFinishLoading(m_streamID);
    m_pluginView->removeStream(this);
    m_pluginView = 0;
}

PluginView::PluginView(WebCore::HTMLPlugInElement* pluginElement, PassRefPtr<Plugin> plugin, const Plugin::Parameters& parameters)
    : m_pluginElement(pluginElement)
    , m_plugin(plugin)
    , m_parameters(parameters)
    , m_isInitialized(false)
    , m_isWaitingUntilMediaCanStart(false)
    , m_pendingURLRequestsTimer(RunLoop::main(), this, &PluginView::pendingURLRequestsTimerFired)
    , m_npJSObjectMap(this)
{
}

PluginView::~PluginView()
{
    if (m_isWaitingUntilMediaCanStart)
        m_pluginElement->document()->removeMediaCanStartListener(this);

    // Cancel all pending frame loads.
    FrameLoadMap::iterator end = m_pendingFrameLoads.end();
    for (FrameLoadMap::iterator it = m_pendingFrameLoads.begin(), end = m_pendingFrameLoads.end(); it != end; ++it)
        it->first->setLoadListener(0);

    if (m_plugin && m_isInitialized)
        m_plugin->destroy();

    // Invalidate the NPObject map.
    m_npJSObjectMap.invalidate();

    // Cancel all streams.
    cancelAllStreams();
}

Frame* PluginView::frame()
{
    return m_pluginElement->document()->frame();
}

void PluginView::initializePlugin()
{
    if (m_isInitialized)
        return;

    if (!m_plugin) {
        // We've already tried and failed to initialize the plug-in.
        return;
    }

    if (Frame* frame = m_pluginElement->document()->frame()) {
        if (Page* page = frame->page()) {
            
            // We shouldn't initialize the plug-in right now, add a listener.
            if (!page->canStartMedia()) {
                if (m_isWaitingUntilMediaCanStart)
                    return;
                
                m_isWaitingUntilMediaCanStart = true;
                m_pluginElement->document()->addMediaCanStartListener(this);
                return;
            }
        }
    }
    
    if (!m_plugin->initialize(this, m_parameters)) {
        // We failed to initialize the plug-in.
        m_plugin = 0;

        return;
    }
    
    m_isInitialized = true;
}

void PluginView::setFrameRect(const WebCore::IntRect& rect)
{
    Widget::setFrameRect(rect);
    viewGeometryDidChange();
}

void PluginView::paint(GraphicsContext* context, const IntRect& dirtyRect)
{
    if (context->paintingDisabled() || !m_plugin || !m_isInitialized)
        return;
    
    IntRect dirtyRectInWindowCoordinates = parent()->contentsToWindow(dirtyRect);
    
    IntRect paintRectInWindowCoordinates = intersection(dirtyRectInWindowCoordinates, clipRectInWindowCoordinates());
    if (paintRectInWindowCoordinates.isEmpty())
        return;

    context->save();

    // Translate the context so that the origin is at the top left corner of the plug-in view.
    context->translate(frameRect().x(), frameRect().y());

    m_plugin->paint(context, paintRectInWindowCoordinates);
    context->restore();
}

void PluginView::frameRectsChanged()
{
    Widget::frameRectsChanged();
    viewGeometryDidChange();
}

void PluginView::setParent(ScrollView* scrollView)
{
    Widget::setParent(scrollView);
    
    if (scrollView)
        initializePlugin();

    viewGeometryDidChange();
}

void PluginView::handleEvent(Event*)
{
    // FIXME: Implement.
}
    
void PluginView::viewGeometryDidChange()
{
    if (!parent() || !m_plugin || !m_isInitialized)
        return;

    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());

    m_plugin->geometryDidChange(frameRectInWindowCoordinates, clipRectInWindowCoordinates());
}

IntRect PluginView::clipRectInWindowCoordinates() const
{
    ASSERT(parent());

    // Get the frame rect in window coordinates.
    IntRect frameRectInWindowCoordinates = parent()->contentsToWindow(frameRect());

    // Get the window clip rect for the enclosing layer (in window coordinates).
    RenderLayer* layer = m_pluginElement->renderer()->enclosingLayer();
    FrameView* parentView = m_pluginElement->document()->frame()->view();
    IntRect windowClipRect = parentView->windowClipRectForLayer(layer, true);

    // Intersect the two rects to get the view clip rect in window coordinates.
    return intersection(frameRectInWindowCoordinates, windowClipRect);
}

void PluginView::pendingURLRequestsTimerFired()
{
    ASSERT(!m_pendingURLRequests.isEmpty());
    
    RefPtr<URLRequest> urlRequest = m_pendingURLRequests.takeFirst();

    // If there are more requests to perform, reschedule the timer.
    if (!m_pendingURLRequests.isEmpty())
        m_pendingURLRequestsTimer.startOneShot(0);
    
    performURLRequest(urlRequest.get());
}
    
void PluginView::performURLRequest(URLRequest* request)
{
    // First, check if this is a javascript: url.
    if (protocolIsJavaScript(request->request().url())) {
        performJavaScriptURLRequest(request);
        return;
    }

    if (!request->target().isNull()) {
        performFrameLoadURLRequest(request);
        return;
    }

    // This request is to load a URL and create a stream.
    RefPtr<Stream> stream = PluginView::Stream::create(this, request->requestID(), request->request());
    addStream(stream.get());
    stream->start();
}

void PluginView::performFrameLoadURLRequest(URLRequest* request)
{
    ASSERT(!request->target().isNull());

    Frame* frame = m_pluginElement->document()->frame();
    if (!frame)
        return;

    // Check if this is URL can be loaded.
    if (!SecurityOrigin::canLoad(request->request().url(), String(), m_pluginElement->document())) {
        // We can't load the request, send back a reply to the plug-in.
        m_plugin->frameDidFail(request->requestID(), false);
        return;
    }

    // First, try to find a target frame.
    Frame* targetFrame = frame->loader()->findFrameForNavigation(request->target());
    if (!targetFrame) {
        // We did not find a target frame. Ask our frame to load the page. This may or may not create a popup window.
        frame->loader()->load(request->request(), request->target(), false);

        // FIXME: We don't know whether the window was successfully created here so we just assume that it worked.
        // It's better than not telling the plug-in anything.
        m_plugin->frameDidFinishLoading(request->requestID());
        return;
    }

    // Now ask the frame to load the request.
    targetFrame->loader()->load(request->request(), false);

    WebFrame* targetWebFrame = static_cast<WebFrameLoaderClient*>(targetFrame->loader()->client())->webFrame();
    if (WebFrame::LoadListener* loadListener = targetWebFrame->loadListener()) {
        // Check if another plug-in view or even this view is waiting for the frame to load.
        // If it is, tell it that the load was cancelled because it will be anyway.
        loadListener->didFailLoad(targetWebFrame, true);
    }
    
    m_pendingFrameLoads.set(targetWebFrame, request);
    targetWebFrame->setLoadListener(this);
}

void PluginView::performJavaScriptURLRequest(URLRequest* request)
{
    ASSERT(protocolIsJavaScript(request->request().url()));

    RefPtr<Frame> frame = m_pluginElement->document()->frame();
    if (!frame)
        return;
    
    String jsString = decodeURLEscapeSequences(request->request().url().string().substring(11));

    if (!request->target().isNull()) {
        // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
        if (frame->tree()->find(request->target()) != frame) {
            // Let the plug-in know that its frame load failed.
            m_plugin->frameDidFail(request->requestID(), false);
            return;
        }
    }

    // Evaluate the JavaScript code. Note that running JavaScript here could cause the plug-in to be destroyed, so we
    // grab references to the plug-in here.
    RefPtr<Plugin> plugin = m_plugin;
    
    ScriptValue result = m_pluginElement->document()->frame()->script()->executeScript(jsString);

    // Check if evaluating the JavaScript destroyed the plug-in.
    if (!plugin->controller())
        return;

    ScriptState* scriptState = m_pluginElement->document()->frame()->script()->globalObject(pluginWorld())->globalExec();
    String resultString;
    result.getString(scriptState, resultString);
  
    if (!request->target().isNull()) {
        // Just send back whether the frame load succeeded or not.
        if (resultString.isNull())
            m_plugin->frameDidFail(request->requestID(), false);
        else
            m_plugin->frameDidFinishLoading(request->requestID());
        return;
    }

    // Send the result back to the plug-in.
    plugin->didEvaluateJavaScript(request->requestID(), decodeURLEscapeSequences(request->request().url()), resultString);
}

void PluginView::addStream(Stream* stream)
{
    ASSERT(!m_streams.contains(stream->streamID()));
    m_streams.set(stream->streamID(), stream);
}
    
void PluginView::removeStream(Stream* stream)
{
    ASSERT(m_streams.get(stream->streamID()) == stream);
    
    m_streams.remove(stream->streamID());
}

void PluginView::cancelAllStreams()
{
    Vector<RefPtr<Stream> > streams;
    copyValuesToVector(m_streams, streams);
    
    for (size_t i = 0; i < streams.size(); ++i)
        streams[i]->cancel();

    // Cancelling a stream removes it from the m_streams map, so if we cancel all streams the map should be empty.
    ASSERT(m_streams.isEmpty());
}

void PluginView::invalidateRect(const IntRect& dirtyRect)
{
    if (!parent() || !m_plugin || !m_isInitialized)
        return;

    IntRect dirtyRectInWindowCoordinates = convertToContainingWindow(dirtyRect);

    parent()->hostWindow()->invalidateContentsAndWindow(intersection(dirtyRectInWindowCoordinates, clipRectInWindowCoordinates()), false);
}

void PluginView::mediaCanStart()
{
    ASSERT(m_isWaitingUntilMediaCanStart);
    m_isWaitingUntilMediaCanStart = false;
    
    initializePlugin();
}

void PluginView::invalidate(const IntRect& dirtyRect)
{
    invalidateRect(dirtyRect);
}

String PluginView::userAgent(const KURL& url)
{
    Frame* frame = m_pluginElement->document()->frame();
    if (!frame)
        return String();
    
    return frame->loader()->client()->userAgent(url);
}

void PluginView::loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, 
                         const HTTPHeaderMap& headerFields, const Vector<char>& httpBody, bool allowPopups)
{
    FrameLoadRequest frameLoadRequest;
    frameLoadRequest.setFrameName(target);
    frameLoadRequest.resourceRequest().setHTTPMethod(method);
    frameLoadRequest.resourceRequest().setURL(m_pluginElement->document()->completeURL(urlString));
    frameLoadRequest.resourceRequest().addHTTPHeaderFields(headerFields);
    frameLoadRequest.resourceRequest().setHTTPBody(FormData::create(httpBody.data(), httpBody.size()));

    m_pendingURLRequests.append(URLRequest::create(requestID, frameLoadRequest, allowPopups));
    m_pendingURLRequestsTimer.startOneShot(0);
}

void PluginView::cancelStreamLoad(uint64_t streamID)
{
    // Keep a reference to the stream. Stream::cancel might remove the stream from the map, and thus
    // releasing its last reference.
    RefPtr<Stream> stream = m_streams.get(streamID).get();
    if (!stream)
        return;

    // Cancelling the stream here will remove it from the map.
    stream->cancel();
    ASSERT(!m_streams.contains(streamID));
}

NPObject* PluginView::windowScriptNPObject()
{
    if (!frame())
        return 0;

    // FIXME: Handle JavaScript being disabled.
    ASSERT(frame()->script()->canExecuteScripts(NotAboutToExecuteScript));

    return m_npJSObjectMap.getOrCreateObject(frame()->script()->windowShell(pluginWorld())->window());
}

NPObject* PluginView::pluginElementNPObject()
{
    if (!frame())
        return 0;

    // FIXME: Handle JavaScript being disabled.
    JSObject* object = frame()->script()->jsObjectForPluginElement(m_pluginElement);
    ASSERT(object);

    return m_npJSObjectMap.getOrCreateObject(object);
}

void PluginView::didFinishLoad(WebFrame* webFrame)
{
    RefPtr<URLRequest> request = m_pendingFrameLoads.take(webFrame);
    ASSERT(request);
    webFrame->setLoadListener(0);

    m_plugin->frameDidFinishLoading(request->requestID());
}

void PluginView::didFailLoad(WebFrame* webFrame, bool wasCancelled)
{
    RefPtr<URLRequest> request = m_pendingFrameLoads.take(webFrame);
    ASSERT(request);
    webFrame->setLoadListener(0);
    
    m_plugin->frameDidFail(request->requestID(), wasCancelled);
}

} // namespace WebKit
