/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#ifndef AsyncScriptRunner_h
#define AsyncScriptRunner_h

#include "CachedResourceHandle.h"
#include "Timer.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CachedScript;
class Document;
class PendingScript;
class ScriptElement;
    
class AsyncScriptRunner : public Noncopyable {
public:
    static PassOwnPtr<AsyncScriptRunner> create(Document* document) { return new AsyncScriptRunner(document); }
    ~AsyncScriptRunner();

    void executeScriptSoon(ScriptElement*, CachedResourceHandle<CachedScript>);
    bool hasPendingScripts() const { return !m_scriptsToExecuteSoon.isEmpty(); }
    void suspend();
    void resume();

private:
    AsyncScriptRunner(Document*);

    void timerFired(Timer<AsyncScriptRunner>*);

    Document* m_document;
    Vector<PendingScript> m_scriptsToExecuteSoon; // http://www.whatwg.org/specs/web-apps/current-work/#set-of-scripts-that-will-execute-as-soon-as-possible
    Timer<AsyncScriptRunner> m_timer;
};

}

#endif
