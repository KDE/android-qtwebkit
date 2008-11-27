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

#if USE(PLUGIN_HOST_PROCESS)

#import "NetscapePluginHostProxy.h"

#import "NetscapePluginInstanceProxy.h"
#import "WebKitPluginHost.h"
#import <wtf/RetainPtr.h>

namespace WebKit {

NetscapePluginHostProxy::NetscapePluginHostProxy(mach_port_t pluginHostPort)
    : m_pluginHostPort(pluginHostPort)
{
}


PassRefPtr<NetscapePluginInstanceProxy> NetscapePluginHostProxy::instantiatePlugin(NSString *mimeType, NSArray *attributeKeys, NSArray *attributeValues, NSString *userAgent, NSURL *sourceURL)
{
    RetainPtr<NSMutableDictionary> properties(AdoptNS, [[NSMutableDictionary alloc] init]);
    
    if (mimeType)
        [properties.get() setObject:mimeType forKey:@"mimeType"];

    ASSERT_ARG(userAgent, userAgent);
    [properties.get() setObject:userAgent forKey:@"userAgent"];
    
    ASSERT_ARG(attributeKeys, attributeKeys);
    [properties.get() setObject:attributeKeys forKey:@"attributeKeys"];
    
    ASSERT_ARG(attributeValues, attributeValues);
    [properties.get() setObject:attributeValues forKey:@"attributeValues"];

    if (sourceURL)
        [properties.get() setObject:[sourceURL absoluteString] forKey:@"sourceURL"];
    
    NSData *data = [NSPropertyListSerialization dataFromPropertyList:properties.get() format:NSPropertyListBinaryFormat_v1_0 errorDescription:nil];
    ASSERT(data);
    
    uint32_t pluginID;
    uint32_t renderContextID;
    boolean_t useSoftwareRenderer;
    
    if (_WKPHInstantiatePlugin(m_pluginHostPort, (uint8_t*)[data bytes], [data length], &pluginID, &renderContextID, &useSoftwareRenderer) != KERN_SUCCESS)
        return 0;

    return NetscapePluginInstanceProxy::create(this, pluginID, renderContextID, useSoftwareRenderer);
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)
