/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "KWQSignal.h"

namespace khtml {
    class CachedObject;
    class DocLoader;
    class Loader;
    class Request;
}

namespace KIO {
    class TransferJob;
}

bool KWQServeRequest(khtml::Loader *, khtml::Request *, KIO::TransferJob *);
void KWQCheckCacheObjectStatus(khtml::DocLoader *, khtml::CachedObject *);
bool KWQCheckIfReloading(khtml::DocLoader *loader);
void KWQRetainResponse(void *response);
void KWQReleaseResponse(void *response);
void *KWQResponseMIMEType(void *response);
int KWQNumberOfPendingOrLoadingRequests(khtml::DocLoader *dl);

class KWQLoader
{
public:
    KWQLoader(khtml::Loader *);
    KWQSignal _requestStarted;
    KWQSignal _requestDone;
    KWQSignal _requestFailed;
};
