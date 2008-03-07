/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JavaScriptDebugServer_h
#define JavaScriptDebugServer_h

#include <kjs/debugger.h>

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

    class Frame;
    class Page;
    class JavaScriptDebugListener;

    class JavaScriptDebugServer : KJS::Debugger {
    public:
        static JavaScriptDebugServer& shared();

        void addListener(JavaScriptDebugListener*);
        void removeListener(JavaScriptDebugListener*);

        void pageCreated(Page*);

        typedef HashSet<JavaScriptDebugListener*> ListenerSet;

    private:
        JavaScriptDebugServer();
        ~JavaScriptDebugServer();

        typedef void (JavaScriptDebugListener::*JavaScriptExecutionCallback)(KJS::ExecState*, int sourceID, int lineNumber);
        void dispatchFunctionToListeners(JavaScriptExecutionCallback, KJS::ExecState*, int sourceID, int lineNumber);

        virtual bool sourceParsed(KJS::ExecState*, int sourceID, const KJS::UString& sourceURL, const KJS::UString& source, int startingLineNumber, int errorLine, const KJS::UString& errorMsg);
        virtual bool callEvent(KJS::ExecState*, int sourceID, int lineNumber, KJS::JSObject* function, const KJS::List& args);
        virtual bool atStatement(KJS::ExecState*, int sourceID, int firstLine, int lastLine);
        virtual bool returnEvent(KJS::ExecState*, int sourceID, int lineNumber, KJS::JSObject* function);
        virtual bool exception(KJS::ExecState*, int sourceID, int lineNumber, KJS::JSValue* exception);

        ListenerSet m_listeners;
        bool m_callingListeners;
    };

} // namespace WebCore

#endif // JavaScriptDebugServer_h
