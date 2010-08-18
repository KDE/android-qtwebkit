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

WebInspector.ExtensionServer = function()
{
    this._clientObjects = {};
    this._handlers = {};
    this._subscribers = {};
    this._status = new WebInspector.ExtensionStatus();

    this._registerHandler("subscribe", this._onSubscribe.bind(this));
    this._registerHandler("unsubscribe", this._onUnsubscribe.bind(this));
    this._registerHandler("getResources", this._onGetResources.bind(this));
    this._registerHandler("createPanel", this._onCreatePanel.bind(this));
    this._registerHandler("createSidebarPane", this._onCreateSidebar.bind(this));
    this._registerHandler("log", this._onLog.bind(this)); 
    this._registerHandler("evaluateOnInspectedPage", this._onEvaluateOnInspectedPage.bind(this));
    this._registerHandler("setSidebarHeight", this._onSetSidebarHeight.bind(this));
    this._registerHandler("setSidebarExpanded", this._onSetSidebarExpansion.bind(this));

    window.addEventListener("message", this._onWindowMessage.bind(this), false);
}

WebInspector.ExtensionServer.prototype = {
    notifyPanelShown: function(panelName)
    {
        this._postNotification("panel-shown-" + panelName);
    },

    notifyObjectSelected: function(panelId, objectType, objectId)
    {
        this._postNotification("panel-objectSelected-" + panelId, objectType, objectId);
    },

    notifyResourceFinished: function(resource)
    {
        this._postNotification("resource-finished", this._convertResource(resource));
    },

    notifySearchAction: function(panelId, action, searchString)
    {
        this._postNotification("panel-search-" + panelId, action, searchString);
    },

    notifyInspectedPageLoaded: function()
    {
        this._postNotification("inspectedPageLoaded");
    },

    notifyInspectedURLChanged: function()
    {
        this._postNotification("inspectedURLChanged");
    },

    notifyInspectorReset: function()
    {
        this._postNotification("reset");
    },

    _convertResource: function(resource)
    {
        return {
            id: resource.identifier,
            type: resource.type,
            har: (new WebInspector.HAREntry(resource)).build(),
        };
    },

    _postNotification: function(type, details)
    {
        var subscribers = this._subscribers[type];
        if (!subscribers)
            return;
        var message = {
            command: "notify-" + type,
            arguments: Array.prototype.slice.call(arguments, 1) 
        };
        for (var i = 0; i < subscribers.length; ++i)
            subscribers[i].postMessage(message);
    },

    _onSubscribe: function(message, port)
    {
        var subscribers = this._subscribers[message.type];
        if (subscribers)
            subscribers.push(port);
        else
            this._subscribers[message.type] = [ port ];
    },

    _onUnsubscribe: function(message, port)
    {
        var subscribers = this._subscribers[message.type];
        if (!subscribers)
            return;
        subscribers.remove(port);
        if (!subscribers.length)
            delete this._subscribers[message.type];
    },

    _onCreatePanel: function(message, port)
    {
        var id = message.id;
        // The ids are generated on the client API side and must be unique, so the check below
        // shouldn't be hit unless someone is bypassing the API.
        if (id in this._clientObjects || id in WebInspector.panels)
            return this._status.E_EXISTS(id);
        var panel = new WebInspector.ExtensionPanel(id, message.label, message.icon);
        this._clientObjects[id] = panel;

        var toolbarElement = document.getElementById("toolbar");
        var lastToolbarItem = WebInspector.panelOrder[WebInspector.panelOrder.length - 1].toolbarItem;
        WebInspector.addPanelToolbarIcon(toolbarElement, panel, lastToolbarItem);
        WebInspector.panels[id] = panel;
        var iframe = this._createClientIframe(panel.element, message.url);
        iframe.style.height = "100%";
        return this._status.OK();
    },

    _onCreateSidebar: function(message, port)
    {
        var panel = WebInspector.panels[message.panel];
        if (!panel)
            return this._status.E_NOTFOUND(message.panel);
        if (!panel.sidebarElement || !panel.sidebarPanes)
            return this._status.E_NOTSUPPORTED();
        var id = message.id;
        var sidebar = new WebInspector.SidebarPane(message.title);
        this._clientObjects[id] = sidebar;
        panel.sidebarPanes[id] = sidebar;
        panel.sidebarElement.appendChild(sidebar.element);
        this._createClientIframe(sidebar.bodyElement, message.url);
        return this._status.OK();
    },

    _createClientIframe: function(parent, url, requestId, port)
    {
        var iframe = document.createElement("iframe");
        iframe.src = url;
        iframe.style.width = "100%";
        parent.appendChild(iframe);
        return iframe;
    },

    _onSetSidebarHeight: function(message)
    {
        var sidebar = this._clientObjects[message.id];
        if (!sidebar)
            return this._status.E_NOTFOUND(message.id);
        sidebar.bodyElement.firstChild.style.height = message.height;
    },

    _onSetSidebarExpansion: function(message)
    {
        var sidebar = this._clientObjects[message.id];
        if (!sidebar)
            return this._status.E_NOTFOUND(message.id);
        if (message.expanded)
            sidebar.expand();
        else
            sidebar.collapse();
    },

    _onLog: function(message)
    {
        WebInspector.log(message.message);
    },

    _onEvaluateOnInspectedPage: function(message, port)
    {
        var escapedMessage = escape(message.expression);
        function callback(resultPayload)
        {
            var resultObject = WebInspector.RemoteObject.fromPayload(resultPayload);
            var result = {};
            if (resultObject.isError())
                result.isException = true;
            result.value = resultObject.description;
            this._dispatchCallback(message.requestId, port, result);
        }
        InjectedScriptAccess.getDefault().evaluate("(function() { var a = window.eval(unescape(\"" + escapedMessage + "\")); return JSON.stringify(a); })();", "", callback.bind(this));
    },

    _onRevealAndSelect: function(message)
    {
        if (message.panelId === "resources" && type === "resource")
            return this._onRevealAndSelectResource(message);
        else
            return this._status.E_NOTSUPPORTED(message.panelId, message.type);
    },

    _onRevealAndSelectResource: function(message)
    {
        var id = message.id;
        var resource = null;

        resource = typeof id === "number" ? WebInspector.resources[id] : WebInspector.resourceForURL(id);
        if (!resource)
            return this._status.E_NOTFOUND(typeof id + ": " + id);
        WebInspector.panels.resources.showResource(resource, message.line);
        WebInspector.showPanel("resources");
    },

    _dispatchCallback: function(requestId, port, result)
    {
        port.postMessage({ command: "callback", requestId: requestId, result: result });
    },

    _onGetResources: function(request)
    {
        function resourceWrapper(id)
        {
            return WebInspector.extensionServer._convertResource(WebInspector.resources[id]);
        }

        var response;
        if (request.id)
            response = WebInspector.resources[request.id] ? resourceWrapper(request.id) : this._status.E_NOTFOUND(request.id);
        else
            response = Object.properties(WebInspector.resources).map(resourceWrapper);
        return response;
    },

    initExtensions: function()
    {
        InspectorExtensionRegistry.getExtensionsAsync();
    },

    _addExtensions: function(extensions)
    {
        InspectorFrontendHost.setExtensionAPI("(" + injectedExtensionAPI.toString() + ")"); // See ExtensionAPI.js for details.
        for (var i = 0; i < extensions.length; ++i) {
            var extension = extensions[i];
            try {
                if (!extension.startPage)
                    return;
                var iframe = document.createElement("iframe");
                iframe.src = extension.startPage;
                iframe.style.display = "none";
                document.body.appendChild(iframe);
            } catch (e) {
                console.error("Failed to initialize extension " + extension.startPage + ":" + e);
            }
        }
    },

    _onWindowMessage: function(event)
    {
        if (event.data !== "registerExtension")
            return;
        var port = event.ports[0];
        port.addEventListener("message", this._onmessage.bind(this), false);
        port.start();
    },

    _onmessage: function(event)
    {
        var request = event.data;
        var result;

        if (request.command in this._handlers)
            result = this._handlers[request.command](request, event.target);
        else
            result = this._status.E_NOTSUPPORTED(request.command);

        if (result && request.requestId)
            this._dispatchCallback(request.requestId, event.target, result);
    },

    _registerHandler: function(command, callback)
    {
        this._handlers[command] = callback;
    }
}

WebInspector.ExtensionServer._statuses = 
{
    OK: "",
    E_NOTFOUND: "Object not found (%s)",
    E_NOTSUPPORTED: "Object does not support requested operation (%s)",
    E_EXISTS: "Object already exists (%s)"
}

WebInspector.ExtensionStatus = function()
{
    function makeStatus(code)
    {
        var description = WebInspector.ExtensionServer._statuses[code] || code;
        var details = Array.prototype.slice.call(arguments, 1);
        var status = { code: code, description: description, details: details };
        if (code !== "OK") {
            status.isError = true;
            console.log("Extension server error: " + String.vsprintf(description, details));
        }
        return status; 
    }
    for (status in WebInspector.ExtensionServer._statuses)
        this[status] = makeStatus.bind(null, status);
}

WebInspector.addExtensions = function(extensions)
{
    WebInspector.extensionServer._addExtensions(extensions);
}

WebInspector.extensionServer = new WebInspector.ExtensionServer();
