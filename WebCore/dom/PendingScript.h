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

#ifndef PendingScript_h
#define PendingScript_h

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

class CachedScript;
class Element;

// A container for an external script which may be loaded and executed.
//
// A CachedResourceHandle alone does not prevent the underlying CachedResource
// from purging its data buffer. This class holds a dummy client open for its
// lifetime in order to guarantee that the data buffer will not be purged.
class PendingScript : public CachedResourceClient {
public:
    PendingScript()
        : m_startingLineNumber(0)
        , m_watchingForLoad(false)
    {
    }

    PendingScript(const PendingScript& other)
        : CachedResourceClient(other)
        , m_startingLineNumber(other.m_startingLineNumber)
        , m_watchingForLoad(other.m_watchingForLoad)
        , m_element(other.m_element)
    {
        setCachedScript(other.cachedScript());
    }

    ~PendingScript();

    PendingScript& operator=(const PendingScript& other)
    {
        if (this == &other)
            return *this;

        m_startingLineNumber = other.m_startingLineNumber;
        m_watchingForLoad = other.m_watchingForLoad;
        m_element = other.m_element;
        setCachedScript(other.cachedScript());

        return *this;
    }

    // FIXME: No setter means this is never set to anything other than 0.
    // This is either unnecessary or incorrect.
    int startingLineNumber() const { return m_startingLineNumber; }

    bool watchingForLoad() const { return m_watchingForLoad; }
    void setWatchingForLoad(bool b) { m_watchingForLoad = b; }

    Element* element() const { return m_element.get(); }
    void adoptElement(Element* element) { m_element = element; }
    PassRefPtr<Element> releaseElementAndClear();

    CachedScript* cachedScript() const;
    void setCachedScript(CachedScript*);

    virtual void notifyFinished(CachedResource*);

private:
    int m_startingLineNumber; // Only used for inline script tags.
    bool m_watchingForLoad;
    RefPtr<Element> m_element;
    CachedResourceHandle<CachedScript> m_cachedScript;
};

}

#endif
