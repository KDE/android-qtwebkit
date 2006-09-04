/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * 
 * All rights reserved.
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

#ifndef FrameQtClient_H
#define FrameQtClient_H

#include "ResourceLoaderClient.h"

namespace WebCore {

class String;
class FrameQt;
class FormData;

class FrameQtClient {
public:
    virtual ~FrameQtClient() { }

    virtual void setFrame(const FrameQt*) = 0;

    virtual void openURL(const KURL&) = 0;
    virtual void submitForm(const String& method, const KURL&, const FormData*) = 0;
};

class FrameQtClientDefault : public FrameQtClient,
                             public ResourceLoaderClient
{
public:
    FrameQtClientDefault();
    virtual ~FrameQtClientDefault();

    // FrameQtClient
    virtual void setFrame(const FrameQt*);

    virtual void openURL(const KURL&);
    virtual void submitForm(const String& method, const KURL&, const FormData*);

    // ResourceLoaderClient
    virtual void receivedResponse(ResourceLoader*, PlatformResponse);
    virtual void receivedData(ResourceLoader*, const char*, int);
    virtual void receivedAllData(ResourceLoader*, PlatformData);

private:
    FrameQt* m_frame;
    bool m_beginCalled : 1;
};

}

#endif

// vim: ts=4 sw=4 et
