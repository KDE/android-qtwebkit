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

#include "NetscapeBrowserFuncs.h"

#include "NPRuntimeUtilities.h"
#include "NetscapePlugin.h"
#include "NotImplemented.h"
#include <WebCore/IdentifierRep.h>

using namespace WebCore;

namespace WebKit {
    
static NPError NPN_GetURL(NPP instance, const char* url, const char* target)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

static NPError NPN_PostURL(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

static NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

static NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}
    
static int32_t NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer)
{
    notImplemented();    
    return -1;
}
    
static NPError NPN_DestroyStream(NPP npp, NPStream* stream, NPReason reason)
{
    RefPtr<NetscapePlugin> plugin = NetscapePlugin::fromNPP(npp);
    
    return plugin->destroyStream(stream, reason);
}

static void NPN_Status(NPP instance, const char* message)
{
    notImplemented();
}
    
static const char* NPN_UserAgent(NPP npp)
{
    if (!npp)
        return 0;
    
    RefPtr<NetscapePlugin> plugin = NetscapePlugin::fromNPP(npp);
    return plugin->userAgent();
}

static void* NPN_MemAlloc(uint32_t size)
{
    // We could use fastMalloc here, but there might be plug-ins that mix NPN_MemAlloc/NPN_MemFree with malloc and free,
    // so having them be equivalent seems like a good idea.
    return malloc(size);
}

static void NPN_MemFree(void* ptr)
{
    // We could use fastFree here, but there might be plug-ins that mix NPN_MemAlloc/NPN_MemFree with malloc and free,
    // so having them be equivalent seems like a good idea.
    free(ptr);
}

static uint32_t NPN_MemFlush(uint32_t size)
{
    return 0;
}

static void NPN_ReloadPlugins(NPBool reloadPages)
{
    notImplemented();
}

static JRIEnv* NPN_GetJavaEnv(void)
{
    notImplemented();
    return 0;
}

static jref NPN_GetJavaPeer(NPP instance)
{
    notImplemented();
    return 0;
}

static String makeURLString(const char* url)
{
    String urlString(url);
    
    // Strip return characters.
    urlString.replace('\r', "");
    urlString.replace('\n', "");

    return urlString;
}

static NPError NPN_GetURLNotify(NPP npp, const char* url, const char* target, void* notifyData)
{
    if (!url)
        return NPERR_GENERIC_ERROR;

    RefPtr<NetscapePlugin> plugin = NetscapePlugin::fromNPP(npp);
    plugin->loadURL(makeURLString(url), target, true, notifyData);
    
    return NPERR_NO_ERROR;
}

static NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32_t len, const char* buf, NPBool file, void* notifyData)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

static NPError NPN_GetValue(NPP npp, NPNVariable variable, void *value)
{
    switch (variable) {
        case NPNVWindowNPObject: {
            RefPtr<NetscapePlugin> plugin = NetscapePlugin::fromNPP(npp);

            NPObject* windowNPObject = plugin->windowScriptNPObject();
            *(NPObject**)value = windowNPObject;
            break;
        }
        case NPNVPluginElementNPObject: {
            RefPtr<NetscapePlugin> plugin = NetscapePlugin::fromNPP(npp);

            NPObject* pluginElementNPObject = plugin->pluginElementNPObject();
            *(NPObject**)value = pluginElementNPObject;
            break;
        }
#if PLATFORM(MAC)
        case NPNVsupportsCoreGraphicsBool:
            // Always claim to support the Core Graphics drawing model.
            *(NPBool *)value = true;
            break;

        case NPNVsupportsCocoaBool:
            // Always claim to support the Cocoa event model.
            *(NPBool *)value = true;
            break;
#endif
        default:
            notImplemented();
            return NPERR_GENERIC_ERROR;
    }

    return NPERR_NO_ERROR;
}

static NPError NPN_SetValue(NPP npp, NPPVariable variable, void *value)
{
    switch (variable) {
#if PLATFORM(MAC)
        case NPPVpluginDrawingModel: {
            RefPtr<NetscapePlugin> plugin = NetscapePlugin::fromNPP(npp);
            
            NPDrawingModel drawingModel = static_cast<NPDrawingModel>(reinterpret_cast<uintptr_t>(value));
            return plugin->setDrawingModel(drawingModel);
        }

        case NPPVpluginEventModel: {
            RefPtr<NetscapePlugin> plugin = NetscapePlugin::fromNPP(npp);
            
            NPEventModel eventModel = static_cast<NPEventModel>(reinterpret_cast<uintptr_t>(value));
            return plugin->setEventModel(eventModel);
        }
#endif

        default:
            notImplemented();
            return NPERR_GENERIC_ERROR;
    }
    
    return NPERR_NO_ERROR;
}

static void NPN_InvalidateRect(NPP npp, NPRect* invalidRect)
{
    RefPtr<NetscapePlugin> plugin = NetscapePlugin::fromNPP(npp);
    plugin->invalidate(invalidRect);
}

static void NPN_InvalidateRegion(NPP npp, NPRegion invalidRegion)
{
    // FIXME: We could at least figure out the bounding rectangle of the invalid region.
    RefPtr<NetscapePlugin> plugin = NetscapePlugin::fromNPP(npp);
    plugin->invalidate(0);
}

static void NPN_ForceRedraw(NPP instance)
{
    notImplemented();
}

static NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name)
{
    return static_cast<NPIdentifier>(IdentifierRep::get(name));
}
    
static void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount, NPIdentifier *identifiers)
{
    ASSERT(names);
    ASSERT(identifiers);

    if (!names || !identifiers)
        return;

    for (int32_t i = 0; i < nameCount; ++i)
        identifiers[i] = NPN_GetStringIdentifier(names[i]);
}

static NPIdentifier NPN_GetIntIdentifier(int32_t intid)
{
    return static_cast<NPIdentifier>(IdentifierRep::get(intid));
}

static bool NPN_IdentifierIsString(NPIdentifier identifier)
{
    return static_cast<IdentifierRep*>(identifier)->isString();
}

static NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
    const char* string = static_cast<IdentifierRep*>(identifier)->string();
    if (!string)
        return 0;

    uint32_t stringLength = strlen(string);
    char* utf8String = static_cast<char*>(NPN_MemAlloc(stringLength + 1));
    memcpy(utf8String, string, stringLength);
    utf8String[stringLength] = '\0';
    
    return utf8String;
}

static int32_t NPN_IntFromIdentifier(NPIdentifier identifier)
{
    return static_cast<IdentifierRep*>(identifier)->number();
}

static NPObject* NPN_CreateObject(NPP npp, NPClass *npClass)
{
    return createNPObject(npp, npClass);
}

static NPObject *NPN_RetainObject(NPObject *npObject)
{
    retainNPObject(npObject);
    return npObject;
}

static void NPN_ReleaseObject(NPObject *npObject)
{
    releaseNPObject(npObject);
}

static bool NPN_Invoke(NPP npp, NPObject *npobj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    notImplemented();
    return false;
}

static bool NPN_InvokeDefault(NPP npp, NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    notImplemented();
    return false;
}

static bool NPN_Evaluate(NPP npp, NPObject *npobj, NPString *script, NPVariant *result)
{
    notImplemented();
    return false;
}

static bool NPN_GetProperty(NPP npp, NPObject *npObject, NPIdentifier propertyName, NPVariant *result)
{
    if (npObject->_class->hasProperty && npObject->_class->getProperty) {
        if (npObject->_class->hasProperty(npObject, propertyName))
            return npObject->_class->getProperty(npObject, propertyName, result);
    }
    
    VOID_TO_NPVARIANT(*result);
    return false;
}

static bool NPN_SetProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName, const NPVariant *value)
{
    notImplemented();
    return false;
}

static bool NPN_RemoveProperty(NPP npp, NPObject *npobj, NPIdentifier propertyName)
{
    notImplemented();
    return false;
}

static bool NPN_HasProperty(NPP npp, NPObject *npObject, NPIdentifier propertyName)
{
    if (npObject->_class->hasProperty)
        return npObject->_class->hasProperty(npObject, propertyName);

    return false;
}

static bool NPN_HasMethod(NPP npp, NPObject *npobj, NPIdentifier methodName)
{
    notImplemented();
    return false;
}

static void NPN_ReleaseVariantValue(NPVariant *variant)
{
    releaseNPVariantValue(variant);
}

static void NPN_SetException(NPObject *npobj, const NPUTF8 *message)
{
    notImplemented();
}

static void NPN_PushPopupsEnabledState(NPP instance, NPBool enabled)
{
    notImplemented();
}
    
static void NPN_PopPopupsEnabledState(NPP instance)
{
    notImplemented();
}
    
static bool NPN_Enumerate(NPP npp, NPObject *npobj, NPIdentifier **identifier, uint32_t *count)
{
    notImplemented();
    return false;
}

static void NPN_PluginThreadAsyncCall(NPP instance, void (*func) (void *), void *userData)
{
    notImplemented();
}

static bool NPN_Construct(NPP npp, NPObject *npobj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    notImplemented();
    return false;
}

static NPError NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char *url, char **value, uint32_t *len)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

static NPError NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char *url, const char *value, uint32_t len)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

static NPError NPN_GetAuthenticationInfo(NPP instance, const char *protocol, const char *host, int32_t port, const char *scheme, 
                                         const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

static uint32_t NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

static void NPN_UnscheduleTimer(NPP instance, uint32_t timerID)
{
    notImplemented();
}

static NPError NPN_PopUpContextMenu(NPP instance, NPMenu* menu)
{
    notImplemented();
    return NPERR_GENERIC_ERROR;
}

static NPBool NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace)
{
    notImplemented();
    return false;
}

static void initializeBrowserFuncs(NPNetscapeFuncs &netscapeFuncs)
{
    netscapeFuncs.size = sizeof(NPNetscapeFuncs);
    netscapeFuncs.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    
    netscapeFuncs.geturl = NPN_GetURL;
    netscapeFuncs.posturl = NPN_PostURL;
    netscapeFuncs.requestread = NPN_RequestRead;
    netscapeFuncs.newstream = NPN_NewStream;
    netscapeFuncs.write = NPN_Write;
    netscapeFuncs.destroystream = NPN_DestroyStream;
    netscapeFuncs.status = NPN_Status;
    netscapeFuncs.uagent = NPN_UserAgent;
    netscapeFuncs.memalloc = NPN_MemAlloc;
    netscapeFuncs.memfree = NPN_MemFree;
    netscapeFuncs.memflush = NPN_MemFlush;
    netscapeFuncs.reloadplugins = NPN_ReloadPlugins;
    netscapeFuncs.getJavaEnv = NPN_GetJavaEnv;
    netscapeFuncs.getJavaPeer = NPN_GetJavaPeer;
    netscapeFuncs.geturlnotify = NPN_GetURLNotify;
    netscapeFuncs.posturlnotify = NPN_PostURLNotify;
    netscapeFuncs.getvalue = NPN_GetValue;
    netscapeFuncs.setvalue = NPN_SetValue;
    netscapeFuncs.invalidaterect = NPN_InvalidateRect;
    netscapeFuncs.invalidateregion = NPN_InvalidateRegion;
    netscapeFuncs.forceredraw = NPN_ForceRedraw;
    
    netscapeFuncs.getstringidentifier = NPN_GetStringIdentifier;
    netscapeFuncs.getstringidentifiers = NPN_GetStringIdentifiers;
    netscapeFuncs.getintidentifier = NPN_GetIntIdentifier;
    netscapeFuncs.identifierisstring = NPN_IdentifierIsString;
    netscapeFuncs.utf8fromidentifier = NPN_UTF8FromIdentifier;
    netscapeFuncs.intfromidentifier = NPN_IntFromIdentifier;
    netscapeFuncs.createobject = NPN_CreateObject;
    netscapeFuncs.retainobject = NPN_RetainObject;
    netscapeFuncs.releaseobject = NPN_ReleaseObject;
    netscapeFuncs.invoke = NPN_Invoke;
    netscapeFuncs.invokeDefault = NPN_InvokeDefault;
    netscapeFuncs.evaluate = NPN_Evaluate;
    netscapeFuncs.getproperty = NPN_GetProperty;
    netscapeFuncs.setproperty = NPN_SetProperty;
    netscapeFuncs.removeproperty = NPN_RemoveProperty;
    netscapeFuncs.hasproperty = NPN_HasProperty;
    netscapeFuncs.hasmethod = NPN_HasMethod;
    netscapeFuncs.releasevariantvalue = NPN_ReleaseVariantValue;
    netscapeFuncs.setexception = NPN_SetException;
    netscapeFuncs.pushpopupsenabledstate = NPN_PushPopupsEnabledState;
    netscapeFuncs.poppopupsenabledstate = NPN_PopPopupsEnabledState;
    netscapeFuncs.enumerate = NPN_Enumerate;
    netscapeFuncs.pluginthreadasynccall = NPN_PluginThreadAsyncCall;
    netscapeFuncs.construct = NPN_Construct;
    netscapeFuncs.getvalueforurl = NPN_GetValueForURL;
    netscapeFuncs.setvalueforurl = NPN_SetValueForURL;
    netscapeFuncs.getauthenticationinfo = NPN_GetAuthenticationInfo;
    netscapeFuncs.scheduletimer = NPN_ScheduleTimer;
    netscapeFuncs.unscheduletimer = NPN_UnscheduleTimer;
    netscapeFuncs.popupcontextmenu = NPN_PopUpContextMenu;
    netscapeFuncs.convertpoint = NPN_ConvertPoint;
}
    
NPNetscapeFuncs* netscapeBrowserFuncs()
{
    static NPNetscapeFuncs netscapeFuncs;
    static bool initialized = false;
    
    if (!initialized) {
        initializeBrowserFuncs(netscapeFuncs);
        initialized = true;
    }

    return &netscapeFuncs;
}

} // namespace WebKit
