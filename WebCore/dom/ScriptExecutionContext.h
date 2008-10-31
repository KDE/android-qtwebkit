/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#ifndef ScriptExecutionContext_h
#define ScriptExecutionContext_h

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

    class ActiveDOMObject;
    class MessagePort;
    class KURL;
    class SecurityOrigin;
    class String;

    class ScriptExecutionContext {
    public:
        ScriptExecutionContext();
        virtual ~ScriptExecutionContext();

        virtual bool isDocument() const { return false; }
        virtual bool isWorkerContext() const { return false; }

        const KURL& url() const { return virtualURL(); }

        // Active objects are not garbage collected even if inaccessible, e.g. because their activity may result in callbacks being invoked.
        void stopActiveDOMObjects();
        void createdActiveDOMObject(ActiveDOMObject*, void* upcastPointer);
        void destroyedActiveDOMObject(ActiveDOMObject*);
        const HashMap<ActiveDOMObject*, void*>& activeDOMObjects() const { return m_activeDOMObjects; }

        // MessagePort is conceptually a kind of ActiveDOMObject, but it needs to be tracked separately for message dispatch, and for cross-heap GC support.
        void processMessagePortMessagesSoon();
        void dispatchMessagePortEvents();
        void createdMessagePort(MessagePort*);
        void destroyedMessagePort(MessagePort*);
        const HashSet<MessagePort*>& messagePorts() const { return m_messagePorts; }

        void ref() { refScriptExecutionContext(); }
        void deref() { derefScriptExecutionContext(); }

    private:
        virtual const KURL& virtualURL() const = 0;

        bool m_firedMessagePortTimer;
        HashSet<MessagePort*> m_messagePorts;

        HashMap<ActiveDOMObject*, void*> m_activeDOMObjects;

        virtual void refScriptExecutionContext() = 0;
        virtual void derefScriptExecutionContext() = 0;
    };

} // namespace WebCore


#endif // ScriptExecutionContext_h
