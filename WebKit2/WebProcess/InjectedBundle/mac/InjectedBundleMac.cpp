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

#include "InjectedBundle.h"

#include "WKBundleAPICast.h"
#include "WKBundleInitialize.h"
#include <wtf/RetainPtr.h>

using namespace WebCore;

namespace WebKit {

bool InjectedBundle::load()
{
    RetainPtr<CFStringRef> injectedBundlePathStr(AdoptCF, CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar*>(m_path.characters()), m_path.length()));
    if (!injectedBundlePathStr) {
        fprintf(stderr, "InjectedBundle::load failed - Could not create the path string.\n");
        return false;
    }
    
    RetainPtr<CFURLRef> bundleURL(AdoptCF, CFURLCreateWithFileSystemPath(0, injectedBundlePathStr.get(), kCFURLPOSIXPathStyle, false));
    if (!bundleURL) {
        fprintf(stderr, "InjectedBundle::load failed - Could not create the url from the path string.\n");
        return false;
    }

    m_platformBundle = CFBundleCreate(0, bundleURL.get());
    if (!m_platformBundle) {
        fprintf(stderr, "InjectedBundle::load failed - Could not create the bundle.\n");
        return false;
    }
        
    if (!CFBundleLoadExecutable(m_platformBundle)) {
        fprintf(stderr, "InjectedBundle::load failed - Could not load the executable from the bundle.\n");
        return false;
    }

    WKBundleInitializeFunctionPtr initializeFunction = reinterpret_cast<WKBundleInitializeFunctionPtr>(CFBundleGetFunctionPointerForName(m_platformBundle, CFSTR("WKBundleInitialize")));
    if (!initializeFunction) {
        fprintf(stderr, "InjectedBundle::load failed - Could not find the initialize function in the bundle executable.\n");
        return false;
    }

    initializeFunction(toRef(this));
    return true;
}

} // namespace WebKit
