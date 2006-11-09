// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ResourceError_H_
#define ResourceError_H_

#include "PlatformString.h"

#if PLATFORM(MAC)
#include "RetainPtr.h"
#endif

#ifdef __OBJC__
@class NSError;
#else
class NSError;
#endif

namespace WebCore {

    class ResourceError {
    public:
        ResourceError()
            : m_errorCode(0)
#if PLATFORM(MAC)
            , m_dataIsUpToDate(true)
#endif
        {
        }

        ResourceError(const String& domain, int errorCode, const String& failingURL, const String& localizedDescription)
            : m_domain(domain)
            , m_errorCode(errorCode)
            , m_failingURL(failingURL)
            , m_localizedDescription(localizedDescription)
#if PLATFORM(MAC)
            , m_dataIsUpToDate(true)
#endif
        {
        }

#if PLATFORM(MAC)
        ResourceError(NSError* error)
            : m_dataIsUpToDate(false)
            , m_platformError(error)
        {
        }
#endif
        
#if 0
        static const String CocoaErrorDomain;
        static const String POSIXDomain;
        static const String OSStatusDomain;
        static const String MachDomain;
        static const String WebKitDomain;
#endif
        
        const String& domain() { unpackPlatformErrorIfNeeded(); return m_domain; }
        int errorCode() { unpackPlatformErrorIfNeeded(); return m_errorCode; }
        const String& failingURL() { unpackPlatformErrorIfNeeded(); return m_failingURL; }
        const String& localizedDescription() { unpackPlatformErrorIfNeeded(); return m_localizedDescription; }

#if PLATFORM(MAC)
        operator NSError*() const;
#endif

    private:
        void unpackPlatformErrorIfNeeded()
        {
#if PLATFORM(MAC)
            if (!m_dataIsUpToDate)
                unpackPlatformError();
#endif
        }

#if PLATFORM(MAC)
        void unpackPlatformError();
#endif

        String m_domain;
        int m_errorCode;
        String m_failingURL;
        String m_localizedDescription;
 
#if PLATFORM(MAC)
        bool m_dataIsUpToDate;
        mutable RetainPtr<NSError> m_platformError;
#endif

};

} // namespace WebCore

#endif // ResourceError_h_
