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

#ifndef NPJSObjectWrapperMap_h
#define NPJSObjectWrapperMap_h

#include <wtf/HashMap.h>

struct NPObject;

namespace JSC {
    class JSObject;
}

namespace WebKit {

class NPJSObject;
class PluginView;

// A per plug-in map of NPObjects that wrap JavaScript objects.

class NPJSObjectMap {
public:
    explicit NPJSObjectMap(PluginView*);

    // Returns an NPObject that wraps the given JavaScript object. If there is already an NPObject that wraps this JSObject, it will
    // retain it and return it.
    NPObject* getOrCreateObject(JSC::JSObject*);

    void invalidate();

private:
    friend class NPJSObject;
    PluginView* m_pluginView;

    HashMap<JSC::JSObject*, NPJSObject*> m_objects;
};

} // namespace WebKit

#endif // NPJSObjectWrapperMap_h
