/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8XMLHttpRequestUtilities.h"

#include <v8.h>

#include "V8CustomBinding.h"
#include "V8Proxy.h"

#include <wtf/Assertions.h>

namespace WebCore {

// Use an array to hold dependents. It works like a ref-counted scheme.
// A value can be added more than once to the xmlHttpRequest object.
void createHiddenXHRDependency(v8::Local<v8::Object> xmlHttpRequest, v8::Local<v8::Value> value)
{
    ASSERT(V8Proxy::GetDOMWrapperType(xmlHttpRequest) == V8ClassIndex::XMLHTTPREQUEST || V8Proxy::GetDOMWrapperType(xmlHttpRequest) == V8ClassIndex::XMLHTTPREQUESTUPLOAD);
    v8::Local<v8::Value> cache = xmlHttpRequest->GetInternalField(V8Custom::kXMLHttpRequestCacheIndex);
    if (cache->IsNull() || cache->IsUndefined()) {
        cache = v8::Array::New();
        xmlHttpRequest->SetInternalField(V8Custom::kXMLHttpRequestCacheIndex, cache);
    }

    v8::Local<v8::Array> cacheArray = v8::Local<v8::Array>::Cast(cache);
    cacheArray->Set(v8::Integer::New(cacheArray->Length()), value);
}

void removeHiddenXHRDependency(v8::Local<v8::Object> xmlHttpRequest, v8::Local<v8::Value> value)
{
    ASSERT(V8Proxy::GetDOMWrapperType(xmlHttpRequest) == V8ClassIndex::XMLHTTPREQUEST || V8Proxy::GetDOMWrapperType(xmlHttpRequest) == V8ClassIndex::XMLHTTPREQUESTUPLOAD);
    v8::Local<v8::Value> cache = xmlHttpRequest->GetInternalField(V8Custom::kXMLHttpRequestCacheIndex);
    ASSERT(cache->IsArray());
    v8::Local<v8::Array> cacheArray = v8::Local<v8::Array>::Cast(cache);
    for (int i = cacheArray->Length() - 1; i >= 0; --i) {
        v8::Local<v8::Value> cached = cacheArray->Get(v8::Integer::New(i));
        if (cached->StrictEquals(value)) {
            cacheArray->Delete(i);
            return;
        }
    }

    // We should only get here if we try to remove an event listener that was never added.
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
