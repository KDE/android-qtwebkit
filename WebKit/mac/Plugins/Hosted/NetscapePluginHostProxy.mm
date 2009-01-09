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

#import <mach/mach.h>
#import <wtf/StdLibExtras.h>

#import "HostedNetscapePluginStream.h"
#import "NetscapePluginHostManager.h"
#import "NetscapePluginInstanceProxy.h"
#import "WebFrameInternal.h"
#import "WebHostedNetscapePluginView.h"
#import "WebKitSystemInterface.h"
#import <WebCore/Frame.h>
#import <WebCore/ScriptController.h>
#import <WebCore/npruntime_impl.h>

extern "C" {
#import "WebKitPluginHost.h"
#import "WebKitPluginClientServer.h"
}

using namespace std;
using namespace JSC;
using namespace WebCore;

namespace WebKit {

static String fromUTF8WithLatin1Fallback(const char* data, int length)
{
    String result = String::fromUTF8(data, length);
    
    if (result.isNull())
        result = String(data, length);
    
    return result;
}
    
typedef HashMap<mach_port_t, NetscapePluginHostProxy*> PluginProxyMap;
static PluginProxyMap& pluginProxyMap()
{
    DEFINE_STATIC_LOCAL(PluginProxyMap, pluginProxyMap, ());
    
    return pluginProxyMap;
}

NetscapePluginHostProxy::NetscapePluginHostProxy(mach_port_t clientPort, mach_port_t pluginHostPort)
    : m_clientPort(clientPort)
    , m_pluginHostPort(pluginHostPort)
{
    pluginProxyMap().add(m_clientPort, this);
    
    // FIXME: We should use libdispatch for this.
    CFMachPortContext context = { 0, this, 0, 0, 0 };
    m_deadNameNotificationPort.adoptCF(CFMachPortCreate(0, deadNameNotificationCallback, &context, 0));

    mach_port_t previous;
    mach_port_request_notification(mach_task_self(), pluginHostPort, MACH_NOTIFY_DEAD_NAME, 0, 
                                   CFMachPortGetPort(m_deadNameNotificationPort.get()), MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
    ASSERT(previous == MACH_PORT_NULL);
    
    RetainPtr<CFRunLoopSourceRef> deathPortSource(AdoptCF, CFMachPortCreateRunLoopSource(0, m_deadNameNotificationPort.get(), 0));
    
    CFRunLoopAddSource(CFRunLoopGetCurrent(), deathPortSource.get(), kCFRunLoopDefaultMode);
    
#ifdef USE_LIBDISPATCH
    // FIXME: Unfortunately we can't use a dispatch source here until <rdar://problem/6393180> has been resolved.
    m_clientPortSource = dispatch_source_mig_create(m_clientPort, WKPCWebKitPluginClient_subsystem.maxsize, 0, 
                                                    dispatch_get_main_queue(), WebKitPluginClient_server);
#else
    m_clientPortSource.adoptCF(WKCreateMIGServerSource((mig_subsystem_t)&WKPCWebKitPluginClient_subsystem, m_clientPort));
    CFRunLoopAddSource(CFRunLoopGetCurrent(), m_clientPortSource.get(), kCFRunLoopDefaultMode);
#endif
}

NetscapePluginHostProxy::~NetscapePluginHostProxy()
{
    pluginProxyMap().remove(m_clientPort);
    
    ASSERT(m_clientPortSource);
#ifdef USE_LIBDISPATCH
    dispatch_release(m_clientPortSource);
#else
    CFRunLoopSourceInvalidate(m_clientPortSource.get());
    m_clientPortSource = 0;
#endif
}

void NetscapePluginHostProxy::pluginHostDied()
{
    PluginInstanceMap instances;    
    m_instances.swap(instances);
  
    PluginInstanceMap::const_iterator end = instances.end();
    for (PluginInstanceMap::const_iterator it = instances.begin(); it != end; ++it)
        it->second->pluginHostDied();
    
    NetscapePluginHostManager::shared().pluginHostDied(this);
    
    delete this;
}
    
void NetscapePluginHostProxy::addPluginInstance(NetscapePluginInstanceProxy* instance)
{
    ASSERT(!m_instances.contains(instance->pluginID()));
    
    m_instances.set(instance->pluginID(), instance);
}
    
void NetscapePluginHostProxy::removePluginInstance(NetscapePluginInstanceProxy* instance)
{
    ASSERT(m_instances.get(instance->pluginID()) == instance);

    m_instances.remove(instance->pluginID());
}

NetscapePluginInstanceProxy* NetscapePluginHostProxy::pluginInstance(uint32_t pluginID)
{
    return m_instances.get(pluginID).get();
}

void NetscapePluginHostProxy::deadNameNotificationCallback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    ASSERT(msg && static_cast<mach_msg_header_t*>(msg)->msgh_id == MACH_NOTIFY_DEAD_NAME);
    
    static_cast<NetscapePluginHostProxy*>(info)->pluginHostDied();
}

} // namespace WebKit

using namespace WebKit;

// MiG callbacks
kern_return_t WKPCStatusText(mach_port_t clientPort, uint32_t pluginID, data_t text, mach_msg_type_number_t textCnt)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    if (!instanceProxy)
        return KERN_FAILURE;
    
    instanceProxy->status(text);
    return KERN_SUCCESS;
}

kern_return_t WKPCLoadURL(mach_port_t clientPort, uint32_t pluginID, data_t url, mach_msg_type_number_t urlLength, data_t target, mach_msg_type_number_t targetLength, 
                          data_t postData, mach_msg_type_number_t postDataLength, uint32_t flags,
                          uint16_t *outResult, uint32_t *outStreamID)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    if (!instanceProxy)
        return KERN_FAILURE;

    uint32_t streamID = 0;
    NPError result = instanceProxy->loadURL(url, target, postData, postDataLength, static_cast<LoadURLFlags>(flags), streamID);
    
    *outResult = result;
    *outStreamID = streamID;
    return KERN_SUCCESS;
}

kern_return_t WKPCCancelLoadURL(mach_port_t clientPort, uint32_t pluginID, uint32_t streamID, int16_t reason)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    if (!instanceProxy)
        return KERN_FAILURE;
    
    HostedNetscapePluginStream* pluginStream = instanceProxy->pluginStream(streamID);
    if (!pluginStream)
        return KERN_FAILURE;

    pluginStream->cancelLoad(reason);
    return KERN_SUCCESS;
}

kern_return_t WKPCInvalidateRect(mach_port_t clientPort, uint32_t pluginID, double x, double y, double width, double height)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    if (!instanceProxy)
        return KERN_FAILURE;

    [instanceProxy->pluginView() setNeedsDisplayInRect:NSMakeRect(x, y, width, height)];
    
    return KERN_SUCCESS;
}

kern_return_t WKPCInstantiatePluginReply(mach_port_t clientPort, uint32_t pluginID, kern_return_t result, uint32_t renderContextID, boolean_t useSoftwareRenderer)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    if (!instanceProxy)
        return KERN_FAILURE;

    instanceProxy->setCurrentReply(new NetscapePluginInstanceProxy::InstantiatePluginReply(result, renderContextID, useSoftwareRenderer));
    return KERN_SUCCESS;
}

kern_return_t WKPCGetWindowNPObject(mach_port_t clientPort, uint32_t pluginID, uint32_t* outObjectID)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    if (!instanceProxy)
        return KERN_FAILURE;

    uint32_t objectID;
    if (!instanceProxy->getWindowNPObject(objectID))
        return KERN_FAILURE;
    
    *outObjectID = objectID;    
    return KERN_SUCCESS;
}

kern_return_t WKPCReleaseObject(mach_port_t clientPort, uint32_t pluginID, uint32_t objectID)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    if (!instanceProxy)
        return KERN_FAILURE;

    instanceProxy->releaseObject(objectID);
    return KERN_SUCCESS;
}

static RetainPtr<CFDataRef> marshalValue(JSValuePtr value)
{
    RetainPtr<NSMutableArray*> array(AdoptNS, [[NSMutableArray alloc] init]);
    
    if (value->isString()) {
    } else if (value->isNumber()) {
    } else if (value->isBoolean()) {
    } else if (value->isNull()) {
    } else if (value->isObject()) {
    } else {
        [array.get() addObject:[NSNumber numberWithInt:VoidValueType]];
    }
    
    return (CFDataRef)[NSPropertyListSerialization dataFromPropertyList:array.get() format:NSPropertyListBinaryFormat_v1_0 errorDescription:0];
}

kern_return_t WKPCEvaluate(mach_port_t clientPort, uint32_t pluginID, uint32_t objectID, data_t scriptData, mach_msg_type_number_t scriptLength,
                           data_t arguments, mach_msg_type_number_t argumentsCnt, boolean_t *returnValue, data_t *resultData, mach_msg_type_number_t *resultLength)
{
    NetscapePluginHostProxy* hostProxy = pluginProxyMap().get(clientPort);
    if (!hostProxy)
        return KERN_FAILURE;
    
    NetscapePluginInstanceProxy* instanceProxy = hostProxy->pluginInstance(pluginID);
    
    String script = fromUTF8WithLatin1Fallback(scriptData, scriptLength);

    // FIXME: Demarshal the arguments and pass them to evaluate.
    JSValuePtr result = instanceProxy->evaluate(objectID, script);
    if (!result) {
        *returnValue = false;
        *resultData = 0;
        *resultLength = 0;
    } else {
        *returnValue = true;
        RetainPtr<CFDataRef> data = marshalValue(result);
        
        *resultLength = CFDataGetLength(data.get());
        mig_allocate(reinterpret_cast<vm_address_t*>(resultData), *resultLength);
        
        memcpy(*resultData, CFDataGetBytePtr(data.get()), *resultLength);
    }
    
    return KERN_SUCCESS;
}

kern_return_t WKPCGetStringIdentifier(mach_port_t clientPort, data_t name, mach_msg_type_number_t nameCnt, uint64_t *identifier)
{
    COMPILE_ASSERT(sizeof(*identifier) == sizeof(NPIdentifier), identifier_sizes);
    
    *identifier = reinterpret_cast<uint64_t>(_NPN_GetStringIdentifier(name));
    return KERN_SUCCESS;
}

#endif // USE(PLUGIN_HOST_PROCESS)
