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

#ifndef NetscapePluginModule_h
#define NetscapePluginModule_h

#include <WebCore/npfunctions.h>
#include <WebCore/PlatformString.h>
#include <wtf/RefCounted.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#endif

namespace WebKit {

class NetscapePluginModule : public RefCounted<NetscapePluginModule> {
public:
    static PassRefPtr<NetscapePluginModule> getOrCreate(const WebCore::String& pluginPath);
    ~NetscapePluginModule();

    const NPPluginFuncs& pluginFuncs() const { return m_pluginFuncs; }

    void pluginCreated();
    void pluginDestroyed();

private:
    explicit NetscapePluginModule(const WebCore::String& pluginPath);

    bool tryLoad();
    bool load();
    void unload();

    void shutdown();

    WebCore::String m_pluginPath;
    bool m_isInitialized;
    unsigned m_pluginCount;

    NPP_ShutdownProcPtr m_shutdownProcPtr;
    NPPluginFuncs m_pluginFuncs;

#if PLATFORM(MAC)
    RetainPtr<CFBundleRef> m_bundle;
#endif
};
    
} // namespace WebKit

#endif // NetscapePluginModule_h
