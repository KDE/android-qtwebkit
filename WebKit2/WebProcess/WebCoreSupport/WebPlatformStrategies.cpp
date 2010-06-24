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

#include "WebPlatformStrategies.h"

#if USE(PLATFORM_STRATEGIES)

#include "PluginInfoStore.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessProxyMessageKinds.h"

using namespace WebCore;

namespace WebKit {

void WebPlatformStrategies::initialize()
{
    DEFINE_STATIC_LOCAL(WebPlatformStrategies, platformStrategies, ());
    setPlatformStrategies(&platformStrategies);
}

WebPlatformStrategies::WebPlatformStrategies()
    : m_pluginCacheIsPopulated(false)
    , m_shouldRefreshPlugins(false)
{
}

// PluginStrategy

PluginStrategy* WebPlatformStrategies::createPluginStrategy()
{
    return this;
}

void WebPlatformStrategies::populatePluginCache()
{
    if (m_pluginCacheIsPopulated)
        return;

    ASSERT(m_cachedPlugins.isEmpty());
    
    Vector<PluginInfo> plugins;
    
    // FIXME: Should we do something in case of error here?
    WebProcess::shared().connection()->sendSync(WebProcessProxyMessage::GetPlugins, 0, CoreIPC::In(m_shouldRefreshPlugins), CoreIPC::Out(plugins), CoreIPC::Connection::NoTimeout);
    m_cachedPlugins.swap(plugins);
    
    m_shouldRefreshPlugins = false;
    m_pluginCacheIsPopulated = true;
}

void WebPlatformStrategies::refreshPlugins()
{
    m_cachedPlugins.clear();
    m_pluginCacheIsPopulated = false;
    m_shouldRefreshPlugins = true;

    populatePluginCache();
}

void WebPlatformStrategies::getPluginInfo(Vector<WebCore::PluginInfo>& plugins)
{
    populatePluginCache();
    plugins = m_cachedPlugins;
}

} // namespace WebKit

#endif // USE(PLATFORM_STRATEGIES)
