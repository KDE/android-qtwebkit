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

#include "NPObjectMessageReceiver.h"

#include "NPIdentifierData.h"
#include "NPRemoteObjectMap.h"
#include "NPRuntimeUtilities.h"
#include "NPVariantData.h"
#include "NotImplemented.h"

namespace WebKit {

PassOwnPtr<NPObjectMessageReceiver> NPObjectMessageReceiver::create(NPRemoteObjectMap* npRemoteObjectMap, uint64_t npObjectID, NPObject* npObject)
{
    return adoptPtr(new NPObjectMessageReceiver(npRemoteObjectMap, npObjectID, npObject));
}

NPObjectMessageReceiver::NPObjectMessageReceiver(NPRemoteObjectMap* npRemoteObjectMap, uint64_t npObjectID, NPObject* npObject)
    : m_npRemoteObjectMap(npRemoteObjectMap)
    , m_npObjectID(npObjectID)
    , m_npObject(npObject)
{
    retainNPObject(m_npObject);
}

NPObjectMessageReceiver::~NPObjectMessageReceiver()
{
    m_npRemoteObjectMap->unregisterNPObject(m_npObjectID);

    releaseNPObject(m_npObject);
}

void NPObjectMessageReceiver::deallocate()
{
    delete this;
}

void NPObjectMessageReceiver::hasMethod(const NPIdentifierData& methodNameData, bool& returnValue)
{
    if (!m_npObject->_class->hasMethod) {
        returnValue = false;
        return;
    }
    
    returnValue = m_npObject->_class->hasMethod(m_npObject, methodNameData.createNPIdentifier());
}

void NPObjectMessageReceiver::invoke(const NPIdentifierData& methodNameData, const Vector<NPVariantData>& argumentsData, bool& returnValue, NPVariantData& resultData)
{
    if (!m_npObject->_class->invoke) {
        returnValue = false;
        return;
    }

    Vector<NPVariant> arguments;
    for (size_t i = 0; i < argumentsData.size(); ++i)
        arguments.append(m_npRemoteObjectMap->npVariantDataToNPVariant(argumentsData[i]));

    NPVariant result;
    returnValue = m_npObject->_class->invoke(m_npObject, methodNameData.createNPIdentifier(), arguments.data(), arguments.size(), &result);
    if (!returnValue)
        return;
    
    // Convert the NPVariant to an NPVariantData.
    resultData = m_npRemoteObjectMap->npVariantToNPVariantData(result);

    // Release all arguments.
    for (size_t i = 0; i < argumentsData.size(); ++i)
        releaseNPVariantValue(&arguments[i]);
    
    // And release the result.
    releaseNPVariantValue(&result);
}

void NPObjectMessageReceiver::hasProperty(const NPIdentifierData& propertyNameData, bool& returnValue)
{
    if (!m_npObject->_class->hasProperty) {
        returnValue = false;
        return;
    }

    returnValue = m_npObject->_class->hasProperty(m_npObject, propertyNameData.createNPIdentifier());
}

void NPObjectMessageReceiver::getProperty(const NPIdentifierData& propertyNameData, bool& returnValue, NPVariantData& resultData)
{
    if (!m_npObject->_class->getProperty) {
        returnValue = false;
        return;
    }

    NPVariant result;
    returnValue = m_npObject->_class->getProperty(m_npObject, propertyNameData.createNPIdentifier(), &result);
    if (!returnValue)
        return;

    // Convert the NPVariant to an NPVariantData.
    resultData = m_npRemoteObjectMap->npVariantToNPVariantData(result);

    // And release the result.
    releaseNPVariantValue(&result);
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)

