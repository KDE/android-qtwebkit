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

#include "NetscapePluginModule.h"

using namespace WebCore;

namespace WebKit {

static Vector<NetscapePluginModule*>& initializedNetscapePluginModules()
{
    DEFINE_STATIC_LOCAL(Vector<NetscapePluginModule*>, initializedNetscapePluginModules, ());
    return initializedNetscapePluginModules;
}

NetscapePluginModule::NetscapePluginModule(const String& pluginPath)
    : m_pluginPath(pluginPath)
    , m_isInitialized(false)
    , m_pluginCount(0)
    , m_shutdownProcPtr(0)
    , m_pluginFuncs()
{
}

NetscapePluginModule::~NetscapePluginModule()
{
    ASSERT(initializedNetscapePluginModules().find(this) == notFound);
}

void NetscapePluginModule::pluginCreated()
{
    m_pluginCount++;
}

void NetscapePluginModule::pluginDestroyed()
{
    ASSERT(m_pluginCount > 0);
    m_pluginCount--;
    
    if (!m_pluginCount)
        shutdown();
}

void NetscapePluginModule::shutdown()
{
    ASSERT(m_isInitialized);

    m_shutdownProcPtr();

    size_t pluginModuleIndex = initializedNetscapePluginModules().find(this);
    ASSERT(pluginModuleIndex != notFound);
    
    initializedNetscapePluginModules().remove(pluginModuleIndex);
}

PassRefPtr<NetscapePluginModule> NetscapePluginModule::getOrCreate(const String& pluginPath)
{
    // First, see if we already have a module with this plug-in path.
    for (size_t i = 0; i < initializedNetscapePluginModules().size(); ++i) {
        NetscapePluginModule* pluginModule = initializedNetscapePluginModules()[i];

        if (pluginModule->m_pluginPath == pluginPath)
            return pluginModule;
    }

    RefPtr<NetscapePluginModule> pluginModule(adoptRef(new NetscapePluginModule(pluginPath)));
    
    // Try to load and initialize the plug-in module.
    if (!pluginModule->load())
        return 0;
    
    return pluginModule.release();
}

bool NetscapePluginModule::load()
{
    if (!tryLoad()) {
        unload();
        return false;
    }
    
    m_isInitialized = true;

    ASSERT(initializedNetscapePluginModules().find(this) == notFound);
    initializedNetscapePluginModules().append(this);

    return true;
}

} // namespace WebKit

