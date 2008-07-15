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

#include "config.h"
#include "Profiler.h"

#include "ExecState.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "Profile.h"
#include "ProfileGenerator.h"
#include "ProfileNode.h"
#include <stdio.h>

namespace KJS {

static const char* GlobalCodeExecution = "(program)";
static const char* AnonymousFunction = "(anonymous function)";

static CallIdentifier createCallIdentifier(JSObject*);
static CallIdentifier createCallIdentifier(const UString& sourceURL, int startingLineNumber);
static CallIdentifier createCallIdentifierFromFunctionImp(JSFunction*);

Profiler* Profiler::s_sharedProfiler = 0;
Profiler* Profiler::s_sharedEnabledProfilerReference = 0;

Profiler* Profiler::profiler()
{
    if (!s_sharedProfiler)
        s_sharedProfiler = new Profiler();
    return s_sharedProfiler;
}   

void Profiler::startProfiling(ExecState* exec, const UString& title, ProfilerClient* client)
{
    ASSERT_ARG(exec, exec);

    // Check if we currently have a Profile for this global ExecState and title.
    // If so return early and don't create a new Profile.
    ExecState* globalExec = exec->lexicalGlobalObject()->globalExec();
    for (size_t i = 0; i < m_currentProfiles.size(); ++i) {
        ProfileGenerator* profileGenerator = m_currentProfiles[i].get();
        if (!profileGenerator->stoppedProfiling() && profileGenerator->originatingGlobalExec() == globalExec && profileGenerator->title() == title)
            return;
    }

    s_sharedEnabledProfilerReference = this;
    RefPtr<ProfileGenerator> profileGenerator = ProfileGenerator::create(title, globalExec, exec->lexicalGlobalObject()->profileGroup(), client);
    m_currentProfiles.append(profileGenerator);
}

void Profiler::stopProfiling(ExecState* exec, const UString& title)
{
    ExecState* globalExec = exec->lexicalGlobalObject()->globalExec();
    for (ptrdiff_t i = m_currentProfiles.size() - 1; i >= 0; --i) {
        ProfileGenerator* profileGenerator = m_currentProfiles[i].get();
        if (!profileGenerator->stoppedProfiling() && profileGenerator->originatingGlobalExec() == globalExec && (title.isNull() || profileGenerator->title() == title))
            m_currentProfiles[i]->stopProfiling();
    }
}

void Profiler::didFinishAllExecution(ExecState* exec)
{
    ExecState* globalExec = exec->lexicalGlobalObject()->globalExec();
    for (ptrdiff_t i = m_currentProfiles.size() - 1; i >= 0; --i) {
        ProfileGenerator* profileGenerator = m_currentProfiles[i].get();
        if (profileGenerator->originatingGlobalExec() == globalExec && profileGenerator->didFinishAllExecution()) {
            PassRefPtr<ProfileGenerator> prpProfileGenerator = m_currentProfiles[i].release();
            m_currentProfiles.remove(i);

            if (!m_currentProfiles.size())
                s_sharedEnabledProfilerReference = 0;

            if (ProfilerClient* client = prpProfileGenerator->client())
                client->finishedProfiling(prpProfileGenerator->profile());
        }
    }
}

static inline void dispatchFunctionToProfiles(const Vector<RefPtr<ProfileGenerator> >& profiles, ProfileGenerator::ProfileFunction function, const CallIdentifier& callIdentifier, unsigned currentProfileTargetGroup)
{
    for (size_t i = 0; i < profiles.size(); ++i) {
        if (profiles[i]->profileGroup() == currentProfileTargetGroup)
            (profiles[i].get()->*function)(callIdentifier);
    }
}

void Profiler::willExecute(ExecState* exec, JSObject* calledFunction)
{
    ASSERT(!m_currentProfiles.isEmpty());

    dispatchFunctionToProfiles(m_currentProfiles, &ProfileGenerator::willExecute, createCallIdentifier(calledFunction), exec->lexicalGlobalObject()->profileGroup());
}

void Profiler::willExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    ASSERT(!m_currentProfiles.isEmpty());

    CallIdentifier callIdentifier = createCallIdentifier(sourceURL, startingLineNumber);

    dispatchFunctionToProfiles(m_currentProfiles, &ProfileGenerator::willExecute, callIdentifier, exec->lexicalGlobalObject()->profileGroup());
}

void Profiler::didExecute(ExecState* exec, JSObject* calledFunction)
{
    ASSERT(!m_currentProfiles.isEmpty());

    dispatchFunctionToProfiles(m_currentProfiles, &ProfileGenerator::didExecute, createCallIdentifier(calledFunction), exec->lexicalGlobalObject()->profileGroup());
}

void Profiler::didExecute(ExecState* exec, const UString& sourceURL, int startingLineNumber)
{
    ASSERT(!m_currentProfiles.isEmpty());

    dispatchFunctionToProfiles(m_currentProfiles, &ProfileGenerator::didExecute, createCallIdentifier(sourceURL, startingLineNumber), exec->lexicalGlobalObject()->profileGroup());
}

CallIdentifier createCallIdentifier(JSObject* calledFunction)
{
    if (calledFunction->inherits(&JSFunction::info))
        return createCallIdentifierFromFunctionImp(static_cast<JSFunction*>(calledFunction));
    if (calledFunction->inherits(&InternalFunction::info))
        return CallIdentifier(static_cast<InternalFunction*>(calledFunction)->functionName().ustring(), "", 0);

    UString name = "(" + calledFunction->className() + " object)";
    return CallIdentifier(name, 0, 0);
}

CallIdentifier createCallIdentifier(const UString& sourceURL, int startingLineNumber)
{
    return CallIdentifier(GlobalCodeExecution, sourceURL, startingLineNumber);
}

CallIdentifier createCallIdentifierFromFunctionImp(JSFunction* functionImp)
{
    UString name = functionImp->functionName().ustring();
    if (name.isEmpty())
        name = AnonymousFunction;

    return CallIdentifier(name, functionImp->m_body->sourceURL(), functionImp->m_body->lineNo());
}

} // namespace KJS
