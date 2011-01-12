/*
* Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "InspectorInstrumentation.h"

#if ENABLE(INSPECTOR)

#include "DOMWindow.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "EventContext.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorController.h"
#include "InspectorDOMAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorResourceAgent.h"
#include "InspectorTimelineAgent.h"
#include "XMLHttpRequest.h"
#include <wtf/text/CString.h>

namespace WebCore {

static const char* const domNativeBreakpointType = "DOM";
static const char* const eventListenerNativeBreakpointType = "EventListener";
static const char* const xhrNativeBreakpointType = "XHR";

static const char* const listenerEventCategoryType = "listener";
static const char* const instrumentationEventCategoryType = "instrumentation";

static const char* const setTimerEventName = "setTimer";
static const char* const clearTimerEventName = "clearTimer";
static const char* const timerFiredEventName = "timerFired";

int InspectorInstrumentation::s_frontendCounter = 0;

static bool eventHasListeners(const AtomicString& eventType, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors)
{
    if (window && window->hasEventListeners(eventType))
        return true;

    if (node->hasEventListeners(eventType))
        return true;

    for (size_t i = 0; i < ancestors.size(); i++) {
        Node* ancestor = ancestors[i].node();
        if (ancestor->hasEventListeners(eventType))
            return true;
    }

    return false;
}

void InspectorInstrumentation::willInsertDOMNodeImpl(InspectorController* inspectorController, Node* node, Node* parent)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;
    InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get();
    if (!domAgent)
        return;
    RefPtr<InspectorObject> eventData = InspectorObject::create();
    if (domAgent->shouldBreakOnNodeInsertion(node, parent, eventData)) {
        eventData->setString("breakpointType", domNativeBreakpointType);
        debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData);
    }
#endif
}

void InspectorInstrumentation::didInsertDOMNodeImpl(InspectorController* inspectorController, Node* node)
{
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->didInsertDOMNode(node);
}

void InspectorInstrumentation::willRemoveDOMNodeImpl(InspectorController* inspectorController, Node* node)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;
    InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get();
    if (!domAgent)
        return;
    RefPtr<InspectorObject> eventData = InspectorObject::create();
    if (domAgent->shouldBreakOnNodeRemoval(node, eventData)) {
        eventData->setString("breakpointType", domNativeBreakpointType);
        debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData);
    }
#endif
}

void InspectorInstrumentation::didRemoveDOMNodeImpl(InspectorController* inspectorController, Node* node)
{
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->didRemoveDOMNode(node);
}

void InspectorInstrumentation::willModifyDOMAttrImpl(InspectorController* inspectorController, Element* element)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;
    InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get();
    if (!domAgent)
        return;
    RefPtr<InspectorObject> eventData = InspectorObject::create();
    if (domAgent->shouldBreakOnAttributeModification(element, eventData)) {
        eventData->setString("breakpointType", domNativeBreakpointType);
        debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData);
    }
#endif
}

void InspectorInstrumentation::didModifyDOMAttrImpl(InspectorController* inspectorController, Element* element)
{
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->didModifyDOMAttr(element);
}

void InspectorInstrumentation::characterDataModifiedImpl(InspectorController* inspectorController, CharacterData* characterData)
{
    if (InspectorDOMAgent* domAgent = inspectorController->m_domAgent.get())
        domAgent->characterDataModified(characterData);
}

void InspectorInstrumentation::willSendXMLHttpRequestImpl(InspectorController* inspectorController, const String& url)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;

    String breakpointURL;
    if (!inspectorController->hasXHRBreakpoint(url, &breakpointURL))
        return;

    RefPtr<InspectorObject> eventData = InspectorObject::create();
    eventData->setString("breakpointType", xhrNativeBreakpointType);
    eventData->setString("breakpointURL", breakpointURL);
    eventData->setString("url", url);
    debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData);
#endif
}

void InspectorInstrumentation::didScheduleResourceRequestImpl(InspectorController* inspectorController, const String& url)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController))
        timelineAgent->didScheduleResourceRequest(url);
}

void InspectorInstrumentation::didInstallTimerImpl(InspectorController* inspectorController, int timerId, int timeout, bool singleShot)
{
    pauseOnNativeEventIfNeeded(inspectorController, instrumentationEventCategoryType, setTimerEventName, true);
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController))
        timelineAgent->didInstallTimer(timerId, timeout, singleShot);
}

void InspectorInstrumentation::didRemoveTimerImpl(InspectorController* inspectorController, int timerId)
{
    pauseOnNativeEventIfNeeded(inspectorController, instrumentationEventCategoryType, clearTimerEventName, true);
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController))
        timelineAgent->didRemoveTimer(timerId);
}


InspectorInstrumentationCookie InspectorInstrumentation::willCallFunctionImpl(InspectorController* inspectorController, const String& scriptName, int scriptLine)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willCallFunction(scriptName, scriptLine);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didCallFunctionImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didCallFunction();
}

InspectorInstrumentationCookie InspectorInstrumentation::willChangeXHRReadyStateImpl(InspectorController* inspectorController, XMLHttpRequest* request)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent && request->hasEventListeners(eventNames().readystatechangeEvent)) {
        timelineAgent->willChangeXHRReadyState(request->url().string(), request->readyState());
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didChangeXHRReadyStateImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didChangeXHRReadyState();
}

InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventImpl(InspectorController* inspectorController, const Event& event, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors)
{
    pauseOnNativeEventIfNeeded(inspectorController, listenerEventCategoryType, event.type(), false);

    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent && eventHasListeners(event.type(), window, node, ancestors)) {
        timelineAgent->willDispatchEvent(event);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didDispatchEventImpl(const InspectorInstrumentationCookie& cookie)
{
    cancelPauseOnNativeEvent(cookie.first);

    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didDispatchEvent();
}

InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventOnWindowImpl(InspectorController* inspectorController, const Event& event, DOMWindow* window)
{
    pauseOnNativeEventIfNeeded(inspectorController, listenerEventCategoryType, event.type(), false);

    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent && window->hasEventListeners(event.type())) {
        timelineAgent->willDispatchEvent(event);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie& cookie)
{
    cancelPauseOnNativeEvent(cookie.first);

    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didDispatchEvent();
}

InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScriptImpl(InspectorController* inspectorController, const String& url, int lineNumber)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willEvaluateScript(url, lineNumber);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didEvaluateScriptImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didEvaluateScript();
}

InspectorInstrumentationCookie InspectorInstrumentation::willFireTimerImpl(InspectorController* inspectorController, int timerId)
{
    pauseOnNativeEventIfNeeded(inspectorController, instrumentationEventCategoryType, timerFiredEventName, false);

    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willFireTimer(timerId);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didFireTimerImpl(const InspectorInstrumentationCookie& cookie)
{
    cancelPauseOnNativeEvent(cookie.first);

    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didFireTimer();
}

InspectorInstrumentationCookie InspectorInstrumentation::willLayoutImpl(InspectorController* inspectorController)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willLayout();
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didLayoutImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didLayout();
}

InspectorInstrumentationCookie InspectorInstrumentation::willLoadXHRImpl(InspectorController* inspectorController, XMLHttpRequest* request)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent && request->hasEventListeners(eventNames().loadEvent)) {
        timelineAgent->willLoadXHR(request->url());
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didLoadXHRImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didLoadXHR();
}

InspectorInstrumentationCookie InspectorInstrumentation::willPaintImpl(InspectorController* inspectorController, const IntRect& rect)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willPaint(rect);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didPaintImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didPaint();
}

InspectorInstrumentationCookie InspectorInstrumentation::willRecalculateStyleImpl(InspectorController* inspectorController)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willRecalculateStyle();
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didRecalculateStyleImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didRecalculateStyle();
}

void InspectorInstrumentation::identifierForInitialRequestImpl(InspectorController* ic, unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    if (!ic->enabled())
        return;
    ic->ensureSettingsLoaded();

    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->identifierForInitialRequest(identifier, request.url(), loader, ic->isMainResourceLoader(loader, request.url()));
}

void InspectorInstrumentation::willSendRequestImpl(InspectorController* ic, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ic->willSendRequest(request);
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(ic))
        timelineAgent->willSendResourceRequest(identifier, request);
    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->willSendRequest(identifier, request, redirectResponse);
}

void InspectorInstrumentation::markResourceAsCachedImpl(InspectorController* ic, unsigned long identifier)
{
    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->markResourceAsCached(identifier);
}

void InspectorInstrumentation::didLoadResourceFromMemoryCacheImpl(InspectorController* ic, DocumentLoader* loader, const CachedResource* cachedResource)
{
    if (!ic->enabled())
        return;
    ic->ensureSettingsLoaded();
    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->didLoadResourceFromMemoryCache(loader, cachedResource);
}

InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceDataImpl(InspectorController* inspectorController, unsigned long identifier)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willReceiveResourceData(identifier);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didReceiveResourceDataImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didReceiveResourceData();
}

InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceResponseImpl(InspectorController* inspectorController, unsigned long identifier, const ResourceResponse& response)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willReceiveResourceResponse(identifier, response);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didReceiveResourceResponseImpl(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response)
{
    InspectorController* ic = cookie.first;
    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->didReceiveResponse(identifier, loader, response);
    // FIXME(52282): move this to console agent.
    ic->didReceiveResponse(identifier, response);
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didReceiveResourceResponse();
}

void InspectorInstrumentation::didReceiveContentLengthImpl(InspectorController* ic, unsigned long identifier, int lengthReceived)
{
    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->didReceiveContentLength(identifier, lengthReceived);
}

void InspectorInstrumentation::didFinishLoadingImpl(InspectorController* ic, unsigned long identifier, double finishTime)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(ic))
        timelineAgent->didFinishLoadingResource(identifier, false, finishTime);
    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->didFinishLoading(identifier, finishTime);
}

void InspectorInstrumentation::didFailLoadingImpl(InspectorController* ic, unsigned long identifier, const ResourceError& error)
{
    // FIXME(52282): move this to console agent.
    ic->didFailLoading(identifier, error);
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(ic))
        timelineAgent->didFinishLoadingResource(identifier, true, 0);
    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->didFailLoading(identifier, error);
}

void InspectorInstrumentation::resourceRetrievedByXMLHttpRequestImpl(InspectorController* ic, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
    ic->resourceRetrievedByXMLHttpRequest(url, sendURL, sendLineNumber);
    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->setInitialContent(identifier, sourceString, "XHR");
}

void InspectorInstrumentation::scriptImportedImpl(InspectorController* ic, unsigned long identifier, const String& sourceString)
{
    if (InspectorResourceAgent* resourceAgent = retrieveResourceAgent(ic))
        resourceAgent->setInitialContent(identifier, sourceString, "Script");
}

InspectorInstrumentationCookie InspectorInstrumentation::willWriteHTMLImpl(InspectorController* inspectorController, unsigned int length, unsigned int startLine)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(inspectorController);
    if (timelineAgent) {
        timelineAgent->willWriteHTML(length, startLine);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(inspectorController, timelineAgentId);
}

void InspectorInstrumentation::didWriteHTMLImpl(const InspectorInstrumentationCookie& cookie, unsigned int endLine)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didWriteHTML(endLine);
}

#if ENABLE(WEB_SOCKETS)
void InspectorInstrumentation::didCreateWebSocketImpl(InspectorController* inspectorController, unsigned long identifier, const KURL& requestURL, const KURL& documentURL)
{
    inspectorController->didCreateWebSocket(identifier, requestURL, documentURL);
}

void InspectorInstrumentation::willSendWebSocketHandshakeRequestImpl(InspectorController* inspectorController, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    inspectorController->willSendWebSocketHandshakeRequest(identifier, request);
}

void InspectorInstrumentation::didReceiveWebSocketHandshakeResponseImpl(InspectorController* inspectorController, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    inspectorController->didReceiveWebSocketHandshakeResponse(identifier, response);
}

void InspectorInstrumentation::didCloseWebSocketImpl(InspectorController* inspectorController, unsigned long identifier)
{
    inspectorController->didCloseWebSocket(identifier);
}
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void InspectorInstrumentation::networkStateChangedImpl(InspectorController* ic)
{
    if (InspectorApplicationCacheAgent* applicationCacheAgent = ic->applicationCacheAgent())
        applicationCacheAgent->networkStateChanged();
}

void InspectorInstrumentation::updateApplicationCacheStatusImpl(InspectorController* ic, Frame* frame)
{
    if (InspectorApplicationCacheAgent* applicationCacheAgent = ic->applicationCacheAgent())
        applicationCacheAgent->updateApplicationCacheStatus(frame);
}
#endif

bool InspectorInstrumentation::hasFrontend(InspectorController* inspectorController)
{
    return inspectorController->hasFrontend();
}

void InspectorInstrumentation::pauseOnNativeEventIfNeeded(InspectorController* inspectorController, const String& categoryType, const String& eventName, bool synchronous)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get();
    if (!debuggerAgent)
        return;
    String fullEventName = String::format("%s:%s", categoryType.utf8().data(), eventName.utf8().data());
    if (!inspectorController->hasEventListenerBreakpoint(fullEventName))
        return;
    RefPtr<InspectorObject> eventData = InspectorObject::create();
    eventData->setString("breakpointType", eventListenerNativeBreakpointType);
    eventData->setString("eventName", fullEventName);
    if (synchronous)
        debuggerAgent->breakProgram(NativeBreakpointDebuggerEventType, eventData);
    else
        debuggerAgent->schedulePauseOnNextStatement(NativeBreakpointDebuggerEventType, eventData);
#endif
}

void InspectorInstrumentation::cancelPauseOnNativeEvent(InspectorController* inspectorController)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent* debuggerAgent = inspectorController->m_debuggerAgent.get())
        debuggerAgent->cancelPauseOnNextStatement();
#endif
}

InspectorTimelineAgent* InspectorInstrumentation::retrieveTimelineAgent(InspectorController* inspectorController)
{
    return inspectorController->m_timelineAgent.get();
}

InspectorTimelineAgent* InspectorInstrumentation::retrieveTimelineAgent(const InspectorInstrumentationCookie& cookie)
{
    InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie.first);
    if (timelineAgent && timelineAgent->id() == cookie.second)
        return timelineAgent;
    return 0;
}

InspectorResourceAgent* InspectorInstrumentation::retrieveResourceAgent(InspectorController* ic)
{
    return ic->m_resourceAgent.get();
}

} // namespace WebCore

#endif // !ENABLE(INSPECTOR)
