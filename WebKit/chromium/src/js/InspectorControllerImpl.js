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

/**
 * @fileoverview DevTools' implementation of the InspectorController API.
 */

if (!this.devtools)
    devtools = {};

devtools.InspectorBackendImpl = function()
{
    WebInspector.InspectorBackendStub.call(this);
    this.installInspectorControllerDelegate_("addInspectedNode");
    this.installInspectorControllerDelegate_("addScriptToEvaluateOnLoad");
    this.installInspectorControllerDelegate_("changeTagName");
    this.installInspectorControllerDelegate_("clearConsoleMessages");
    this.installInspectorControllerDelegate_("copyNode");
    this.installInspectorControllerDelegate_("deleteCookie");
    this.installInspectorControllerDelegate_("didEvaluateForTestInFrontend");
    this.installInspectorControllerDelegate_("disableMonitoringXHR");
    this.installInspectorControllerDelegate_("disableResourceTracking");
    this.installInspectorControllerDelegate_("disableSearchingForNode");
    this.installInspectorControllerDelegate_("disableTimeline");
    this.installInspectorControllerDelegate_("enableMonitoringXHR");
    this.installInspectorControllerDelegate_("enableResourceTracking");
    this.installInspectorControllerDelegate_("enableSearchingForNode");
    this.installInspectorControllerDelegate_("enableTimeline");
    this.installInspectorControllerDelegate_("getChildNodes");
    this.installInspectorControllerDelegate_("getCookies");
    this.installInspectorControllerDelegate_("getDatabaseTableNames");
    this.installInspectorControllerDelegate_("getDOMStorageEntries");
    this.installInspectorControllerDelegate_("getEventListenersForNode");
    this.installInspectorControllerDelegate_("getOuterHTML");
    this.installInspectorControllerDelegate_("getProfile");
    this.installInspectorControllerDelegate_("getProfileHeaders");
    this.installInspectorControllerDelegate_("removeProfile");
    this.installInspectorControllerDelegate_("clearProfiles");
    this.installInspectorControllerDelegate_("getResourceContent");
    this.installInspectorControllerDelegate_("highlightDOMNode");
    this.installInspectorControllerDelegate_("hideDOMNodeHighlight");
    this.installInspectorControllerDelegate_("performSearch");
    this.installInspectorControllerDelegate_("pushNodeByPathToFrontend");
    this.installInspectorControllerDelegate_("releaseWrapperObjectGroup");
    this.installInspectorControllerDelegate_("removeAllScriptsToEvaluateOnLoad");
    this.installInspectorControllerDelegate_("reloadPage");
    this.installInspectorControllerDelegate_("removeAttribute");
    this.installInspectorControllerDelegate_("removeDOMStorageItem");
    this.installInspectorControllerDelegate_("removeNode");
    this.installInspectorControllerDelegate_("saveApplicationSettings");
    this.installInspectorControllerDelegate_("saveSessionSettings");
    this.installInspectorControllerDelegate_("searchCanceled");
    this.installInspectorControllerDelegate_("setAttribute");
    this.installInspectorControllerDelegate_("setDOMStorageItem");
    this.installInspectorControllerDelegate_("setInjectedScriptSource");
    this.installInspectorControllerDelegate_("setOuterHTML");
    this.installInspectorControllerDelegate_("setTextNodeValue");
    this.installInspectorControllerDelegate_("startProfiling");
    this.installInspectorControllerDelegate_("startTimelineProfiler");
    this.installInspectorControllerDelegate_("stopProfiling");
    this.installInspectorControllerDelegate_("stopTimelineProfiler");
    this.installInspectorControllerDelegate_("storeLastActivePanel");
    this.installInspectorControllerDelegate_("takeHeapSnapshot");

    this.installInspectorControllerDelegate_("getAllStyles");
    this.installInspectorControllerDelegate_("getStyles");
    this.installInspectorControllerDelegate_("getComputedStyle");
    this.installInspectorControllerDelegate_("getInlineStyle");
    this.installInspectorControllerDelegate_("getStyleSheet");
    this.installInspectorControllerDelegate_("getRuleRangesForStyleSheetId");
    this.installInspectorControllerDelegate_("applyStyleText");
    this.installInspectorControllerDelegate_("setStyleText");
    this.installInspectorControllerDelegate_("setStyleProperty");
    this.installInspectorControllerDelegate_("toggleStyleEnabled");
    this.installInspectorControllerDelegate_("setRuleSelector");
    this.installInspectorControllerDelegate_("addRule");

    if (window.v8ScriptDebugServerEnabled) {
    this.installInspectorControllerDelegate_("disableDebugger");
    this.installInspectorControllerDelegate_("editScriptSource");
    this.installInspectorControllerDelegate_("getScriptSource");
    this.installInspectorControllerDelegate_("enableDebugger");
    this.installInspectorControllerDelegate_("setBreakpoint");
    this.installInspectorControllerDelegate_("removeBreakpoint");
    this.installInspectorControllerDelegate_("activateBreakpoints");
    this.installInspectorControllerDelegate_("deactivateBreakpoints");
    this.installInspectorControllerDelegate_("resumeDebugger");
    this.installInspectorControllerDelegate_("stepIntoStatementInDebugger");
    this.installInspectorControllerDelegate_("stepOutOfFunctionInDebugger");
    this.installInspectorControllerDelegate_("stepOverStatementInDebugger");
    this.installInspectorControllerDelegate_("setPauseOnExceptionsState");
    }
};
devtools.InspectorBackendImpl.prototype.__proto__ = WebInspector.InspectorBackendStub.prototype;


if (!window.v8ScriptDebugServerEnabled) {

devtools.InspectorBackendImpl.prototype.setBreakpoint = function(sourceID, line, enabled, condition)
{
    this.removeBreakpoint(sourceID, line);
    devtools.tools.getDebuggerAgent().addBreakpoint(sourceID, line, enabled, condition);
};


devtools.InspectorBackendImpl.prototype.removeBreakpoint = function(sourceID, line)
{
    devtools.tools.getDebuggerAgent().removeBreakpoint(sourceID, line);
};


devtools.InspectorBackendImpl.prototype.editScriptSource = function(callID, sourceID, newContent)
{
    devtools.tools.getDebuggerAgent().editScriptSource(sourceID, newContent, function(success, newBodyOrErrorMessage, callFrames) {
        WebInspector.didEditScriptSource(callID, success, newBodyOrErrorMessage, callFrames);
    });
};


devtools.InspectorBackendImpl.prototype.getScriptSource = function(callID, sourceID)
{
    devtools.tools.getDebuggerAgent().resolveScriptSource(
        sourceID,
        function(source) {
             WebInspector.didGetScriptSource(callID, source);
        });
};


devtools.InspectorBackendImpl.prototype.activateBreakpoints = function()
{
    devtools.tools.getDebuggerAgent().setBreakpointsActivated(true);
};


devtools.InspectorBackendImpl.prototype.deactivateBreakpoints = function()
{
    devtools.tools.getDebuggerAgent().setBreakpointsActivated(false);
};


devtools.InspectorBackendImpl.prototype.pauseInDebugger = function()
{
    devtools.tools.getDebuggerAgent().pauseExecution();
};


devtools.InspectorBackendImpl.prototype.resumeDebugger = function()
{
    devtools.tools.getDebuggerAgent().resumeExecution();
};


devtools.InspectorBackendImpl.prototype.stepIntoStatementInDebugger = function()
{
    devtools.tools.getDebuggerAgent().stepIntoStatement();
};


devtools.InspectorBackendImpl.prototype.stepOutOfFunctionInDebugger = function()
{
    devtools.tools.getDebuggerAgent().stepOutOfFunction();
};


devtools.InspectorBackendImpl.prototype.stepOverStatementInDebugger = function()
{
    devtools.tools.getDebuggerAgent().stepOverStatement();
};

/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.setPauseOnExceptionsState = function(state)
{
    this._setPauseOnExceptionsState = state;
    // TODO(yurys): support all three states. See http://crbug.com/32877
    var enabled = (state !== WebInspector.ScriptsPanel.PauseOnExceptionsState.DontPauseOnExceptions);
    WebInspector.updatePauseOnExceptionsState(enabled ? WebInspector.ScriptsPanel.PauseOnExceptionsState.PauseOnUncaughtExceptions : WebInspector.ScriptsPanel.PauseOnExceptionsState.DontPauseOnExceptions);
    devtools.tools.getDebuggerAgent().setPauseOnExceptions(enabled);
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.pauseOnExceptions = function()
{
    return devtools.tools.getDebuggerAgent().pauseOnExceptions();
};


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.setPauseOnExceptions = function(value)
{
    return devtools.tools.getDebuggerAgent().setPauseOnExceptions(value);
};

} else {

devtools.InspectorBackendImpl.prototype.pauseInDebugger = function()
{
    RemoteDebuggerCommandExecutor.DebuggerPauseScript();
};

}


/**
 * @override
 */
devtools.InspectorBackendImpl.prototype.dispatchOnInjectedScript = function(callId, injectedScriptId, methodName, argsString, async)
{
    // Encode injectedScriptId into callId
    if (typeof injectedScriptId !== "number")
        injectedScriptId = 0;
    RemoteToolsAgent.dispatchOnInjectedScript(callId, injectedScriptId, methodName, argsString, async);
};


/**
 * Installs delegating handler into the inspector controller.
 * @param {string} methodName Method to install delegating handler for.
 */
devtools.InspectorBackendImpl.prototype.installInspectorControllerDelegate_ = function(methodName)
{
    this[methodName] = this.callInspectorController_.bind(this, methodName);
};


/**
 * Bound function with the installInjectedScriptDelegate_ actual
 * implementation.
 */
devtools.InspectorBackendImpl.prototype.callInspectorController_ = function(methodName, var_arg)
{
    var args = Array.prototype.slice.call(arguments, 1);
    RemoteToolsAgent.dispatchOnInspectorController(WebInspector.Callback.wrap(function(){}), methodName, JSON.stringify(args));
};


InspectorBackend = new devtools.InspectorBackendImpl();
