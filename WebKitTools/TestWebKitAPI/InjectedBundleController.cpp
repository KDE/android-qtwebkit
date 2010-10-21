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

#include "InjectedBundleController.h"

#include "InjectedBundleTest.h"
#include "PlatformUtilities.h"
#include <WebKit2/WebKit2.h>
#include <algorithm>
#include <assert.h>

namespace TestWebKitAPI {

InjectedBundleController& InjectedBundleController::shared()
{
    static InjectedBundleController& shared = *new InjectedBundleController;
    return shared;
}

InjectedBundleController::InjectedBundleController()
    : m_bundle(0)
    , m_currentTest(0)
{
}

void InjectedBundleController::initialize(WKBundleRef bundle)
{
    m_bundle = bundle;

    WKBundleClient client = {
        0,
        this,
        didCreatePage,
        willDestroyPage,
        didReceiveMessage
    };
    WKBundleSetClient(m_bundle, &client);
}

void InjectedBundleController::didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    InjectedBundleController* self = static_cast<InjectedBundleController*>(const_cast<void*>(clientInfo));
    assert(self->m_currentTest);
    self->m_currentTest->didCreatePage(bundle, page);
}

void InjectedBundleController::willDestroyPage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo)
{
    InjectedBundleController* self = static_cast<InjectedBundleController*>(const_cast<void*>(clientInfo));
    assert(self->m_currentTest);
    self->m_currentTest->willDestroyPage(bundle, page);
}

void InjectedBundleController::didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    InjectedBundleController* self = static_cast<InjectedBundleController*>(const_cast<void*>(clientInfo));

    if (WKStringIsEqualToUTF8CString(messageName, "BundleTestInstantiator")) {
        assert(WKGetTypeID(messageBody) == WKStringGetTypeID());
        WKStringRef messageBodyString = static_cast<WKStringRef>(messageBody);

        self->initializeTestNamed(Util::toSTD(messageBodyString));

        return;
    }

    assert(self->m_currentTest);
    self->m_currentTest->didReceiveMessage(bundle, messageName, messageBody);
}

void InjectedBundleController::dumpTestNames()
{
    std::map<std::string, CreateInjectedBundleTestFunction>::const_iterator it = m_createInjectedBundleTestFunctions.begin();
    std::map<std::string, CreateInjectedBundleTestFunction>::const_iterator end = m_createInjectedBundleTestFunctions.end();
    for (; it != end; ++it)
        printf("%s\n", (*it).first.c_str());
}

void InjectedBundleController::initializeTestNamed(const std::string& identifier)
{
    CreateInjectedBundleTestFunction createTestFunction = m_createInjectedBundleTestFunctions[identifier];
    if (!createTestFunction) {
        printf("ERROR: InjectedBundle test not found - %s\n", identifier.c_str());
        exit(1);
    }

    m_currentTest = createTestFunction(identifier);
    m_currentTest->initialize();
}

void InjectedBundleController::registerCreateInjectedBundleTestFunction(const std::string& identifier, CreateInjectedBundleTestFunction function)
{
    m_createInjectedBundleTestFunctions[identifier] = function;
}

} // namespace TestWebKitAPI
