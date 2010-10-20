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

#if ENABLE(PLUGIN_PROCESS)

#include "PluginControllerProxy.h"

#include "BackingStore.h"
#include "DataReference.h"
#include "NetscapePlugin.h"
#include "NotImplemented.h"
#include "PluginProcess.h"
#include "PluginProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessConnection.h"
#include <WebCore/GraphicsContext.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

PassOwnPtr<PluginControllerProxy> PluginControllerProxy::create(WebProcessConnection* connection, uint64_t pluginInstanceID, const String& userAgent, bool isPrivateBrowsingEnabled)
{
    return adoptPtr(new PluginControllerProxy(connection, pluginInstanceID, userAgent, isPrivateBrowsingEnabled));
}

PluginControllerProxy::PluginControllerProxy(WebProcessConnection* connection, uint64_t pluginInstanceID, const String& userAgent, bool isPrivateBrowsingEnabled)
    : m_connection(connection)
    , m_pluginInstanceID(pluginInstanceID)
    , m_userAgent(userAgent)
    , m_isPrivateBrowsingEnabled(isPrivateBrowsingEnabled)
    , m_paintTimer(RunLoop::main(), this, &PluginControllerProxy::paint)
    , m_waitingForDidUpdate(false)
{
}

PluginControllerProxy::~PluginControllerProxy()
{
    ASSERT(!m_plugin);
}

bool PluginControllerProxy::initialize(const Plugin::Parameters& parameters)
{
    ASSERT(!m_plugin);

    m_plugin = NetscapePlugin::create(PluginProcess::shared().netscapePluginModule());
    if (!m_plugin->initialize(this, parameters)) {
        m_plugin = 0;
        return false;
    }

    return true;
}

void PluginControllerProxy::destroy()
{
    ASSERT(m_plugin);

    m_plugin->destroy();
    m_plugin = 0;
}

void PluginControllerProxy::paint()
{
    ASSERT(!m_dirtyRect.isEmpty());
    m_paintTimer.stop();

    if (!m_backingStore)
        return;

    IntRect dirtyRect = m_dirtyRect;
    m_dirtyRect = IntRect();

    // Create a graphics context.
    OwnPtr<GraphicsContext> graphicsContext = m_backingStore->createGraphicsContext();
    graphicsContext->translate(-m_frameRect.x(), -m_frameRect.y());

    ASSERT(m_plugin);
    m_plugin->paint(graphicsContext.get(), dirtyRect);

    m_connection->connection()->send(Messages::PluginProxy::Update(dirtyRect), m_pluginInstanceID);
}

void PluginControllerProxy::startPaintTimer()
{
    // Check if we should start the timer.
    
    if (m_dirtyRect.isEmpty())
        return;

    // FIXME: Check clip rect.
    
    if (m_paintTimer.isActive())
        return;

    if (m_waitingForDidUpdate)
        return;

    // Start the timer.
    m_paintTimer.startOneShot(0);

    m_waitingForDidUpdate = true;
}

void PluginControllerProxy::invalidate(const IntRect& rect)
{
    // Convert the dirty rect to window coordinates.
    IntRect dirtyRect = rect;
    dirtyRect.move(m_frameRect.x(), m_frameRect.y());

    // Make sure that the dirty rect is not greater than the plug-in itself.
    dirtyRect.intersect(m_frameRect);

    m_dirtyRect.unite(dirtyRect);

    startPaintTimer();
}

String PluginControllerProxy::userAgent()
{
    return m_userAgent;
}

void PluginControllerProxy::loadURL(uint64_t requestID, const String& method, const String& urlString, const String& target, const HTTPHeaderMap& headerFields, const Vector<uint8_t>& httpBody, bool allowPopups)
{
    m_connection->connection()->send(Messages::PluginProxy::LoadURL(requestID, method, urlString, target, headerFields, httpBody, allowPopups), m_pluginInstanceID);
}

void PluginControllerProxy::cancelStreamLoad(uint64_t streamID)
{
    notImplemented();
}

void PluginControllerProxy::cancelManualStreamLoad()
{
    notImplemented();
}

NPObject* PluginControllerProxy::windowScriptNPObject()
{
    notImplemented();
    return 0;
}

NPObject* PluginControllerProxy::pluginElementNPObject()
{
    notImplemented();
    return 0;
}

bool PluginControllerProxy::evaluate(NPObject*, const String& scriptString, NPVariant* result, bool allowPopups)
{
    notImplemented();
    return false;
}

void PluginControllerProxy::setStatusbarText(const WTF::String&)
{
    notImplemented();
}

bool PluginControllerProxy::isAcceleratedCompositingEnabled()
{
    notImplemented();
    return false;
}

void PluginControllerProxy::pluginProcessCrashed()
{
    notImplemented();
}

String PluginControllerProxy::proxiesForURL(const String& urlString)
{
    String proxyString;
    
    if (!m_connection->connection()->sendSync(Messages::PluginProxy::CookiesForURL(urlString),
                                              Messages::PluginProxy::CookiesForURL::Reply(proxyString),
                                              m_pluginInstanceID, 0))
        return String();
    
    return proxyString;
}

String PluginControllerProxy::cookiesForURL(const String& urlString)
{
    String cookieString;

    if (!m_connection->connection()->sendSync(Messages::PluginProxy::CookiesForURL(urlString),
                                              Messages::PluginProxy::CookiesForURL::Reply(cookieString),
                                              m_pluginInstanceID, 0))
        return String();

    return cookieString;
}

void PluginControllerProxy::setCookiesForURL(const String& urlString, const String& cookieString)
{
    m_connection->connection()->send(Messages::PluginProxy::SetCookiesForURL(urlString, cookieString), m_pluginInstanceID);
}

bool PluginControllerProxy::isPrivateBrowsingEnabled()
{
    return m_isPrivateBrowsingEnabled;
}
    
void PluginControllerProxy::geometryDidChange(const IntRect& frameRect, const IntRect& clipRect, const SharedMemory::Handle& backingStoreHandle)
{
    m_frameRect = frameRect;
    m_clipRect = clipRect;

    ASSERT(m_plugin);

    if (!backingStoreHandle.isNull()) {
        // Create a new backing store.
        m_backingStore = BackingStore::create(frameRect.size(), backingStoreHandle);
    }

    m_plugin->geometryDidChange(frameRect, clipRect);
}

void PluginControllerProxy::didEvaluateJavaScript(uint64_t requestID, const String& requestURLString, const String& result)
{
    m_plugin->didEvaluateJavaScript(requestID, requestURLString, result);
}

void PluginControllerProxy::streamDidReceiveResponse(uint64_t streamID, const String& responseURLString, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers)
{
    m_plugin->streamDidReceiveResponse(streamID, KURL(ParsedURLString, responseURLString), streamLength, lastModifiedTime, mimeType, headers);
}

void PluginControllerProxy::streamDidReceiveData(uint64_t streamID, const CoreIPC::DataReference& data)
{
    m_plugin->streamDidReceiveData(streamID, reinterpret_cast<const char*>(data.data()), data.size());
}

void PluginControllerProxy::streamDidFinishLoading(uint64_t streamID)
{
    m_plugin->streamDidFinishLoading(streamID);
}

void PluginControllerProxy::streamDidFail(uint64_t streamID, bool wasCancelled)
{
    m_plugin->streamDidFail(streamID, wasCancelled);
}
    
void PluginControllerProxy::handleMouseEvent(const WebMouseEvent& mouseEvent, bool& handled)
{
    handled = m_plugin->handleMouseEvent(mouseEvent);
}

void PluginControllerProxy::handleWheelEvent(const WebWheelEvent& wheelEvent, bool& handled)
{
    handled = m_plugin->handleWheelEvent(wheelEvent);
}

void PluginControllerProxy::handleMouseEnterEvent(const WebMouseEvent& mouseEnterEvent, bool& handled)
{
    handled = m_plugin->handleMouseEnterEvent(mouseEnterEvent);
}

void PluginControllerProxy::handleMouseLeaveEvent(const WebMouseEvent& mouseLeaveEvent, bool& handled)
{
    handled = m_plugin->handleMouseLeaveEvent(mouseLeaveEvent);
}

void PluginControllerProxy::handleKeyboardEvent(const WebKeyboardEvent& keyboardEvent, bool& handled)
{
    handled = m_plugin->handleKeyboardEvent(keyboardEvent);
}

void PluginControllerProxy::paintEntirePlugin()
{
    m_dirtyRect = m_frameRect;
    paint();
}

void PluginControllerProxy::setFocus(bool hasFocus)
{
    m_plugin->setFocus(hasFocus);
}

void PluginControllerProxy::didUpdate()
{
    m_waitingForDidUpdate = false;
    startPaintTimer();
}

#if PLATFORM(MAC)
void PluginControllerProxy::windowFocusChanged(bool hasFocus)
{
    m_plugin->windowFocusChanged(hasFocus);
}

void PluginControllerProxy::windowFrameChanged(const IntRect& windowFrame)
{
    m_plugin->windowFrameChanged(windowFrame);
}

void PluginControllerProxy::windowVisibilityChanged(bool isVisible)
{
    m_plugin->windowVisibilityChanged(isVisible);
}
#endif

void PluginControllerProxy::privateBrowsingStateChanged(bool isPrivateBrowsingEnabled)
{
    m_plugin->privateBrowsingStateChanged(isPrivateBrowsingEnabled);
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
