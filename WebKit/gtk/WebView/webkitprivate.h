/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef WEBKIT_PRIVATE_H
#define WEBKIT_PRIVATE_H

/*
 * Internal class. This class knows the shared secret of WebKitWebFrame,
 * WebKitNetworkRequest and WebKitWebView.
 * They are using WebCore which musn't be exposed to the outer world.
 */

#include "webkitdefines.h"
#include "webkitsettings.h"
#include "webkitwebview.h"
#include "webkitwebframe.h"
#include "webkitnetworkrequest.h"

#include "Settings.h"
#include "Page.h"
#include "Frame.h"
#include "FrameLoaderClient.h"

namespace WebKit {
    void apply(WebKitSettings*,WebCore::Settings*);
    WebKitSettings* create(WebCore::Settings*);
    WebKitWebView* getViewFromFrame(WebKitWebFrame*);

    WebCore::Frame* core(WebKitWebFrame*);
    WebKitWebFrame* kit(WebCore::Frame*);
    WebCore::Page* core(WebKitWebView*);
    WebKitWebView* kit(WebCore::Page*);
}

extern "C" {
    void webkit_init();

    #define WEBKIT_WEB_VIEW_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_VIEW, WebKitWebViewPrivate))
    typedef struct _WebKitWebViewPrivate WebKitWebViewPrivate;
    struct _WebKitWebViewPrivate {
        WebCore::Page* corePage;
        WebCore::Settings* settings;

        WebKitWebFrame* mainFrame;
        WebCore::String applicationNameForUserAgent;
        WebCore::String* userAgent;

        HashSet<GtkWidget*> children;
        bool editable;
        GtkIMContext* imContext;

        GtkTargetList* copy_target_list;
        GtkTargetList* paste_target_list;
    };

    #define WEBKIT_WEB_FRAME_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_WEB_FRAME, WebKitWebFramePrivate))
    typedef struct _WebKitWebFramePrivate WebKitWebFramePrivate;
    struct _WebKitWebFramePrivate {
        WebCore::Frame* frame;
        WebCore::FrameLoaderClient* client;
        WebKitWebView* webView;

        gchar* name;
        gchar* title;
        gchar* uri;
    };

    #define WEBKIT_NETWORK_REQUEST_GET_PRIVATE(obj)    (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_NETWORK_REQUEST, WebKitNetworkRequestPrivate))
    typedef struct _WebKitNetworkRequestPrivate WebKitNetworkRequestPrivate;
    struct _WebKitNetworkRequestPrivate {
        gchar* uri;
    };

    WebKitWebFrame* webkit_web_frame_init_with_web_view(WebKitWebView*, WebCore::HTMLFrameOwnerElement*);

    // TODO: Move these to webkitwebframe.h once these functions are fully
    // implemented and their API has been discussed.

    WEBKIT_API GSList*
    webkit_web_frame_get_children (WebKitWebFrame* frame);

    WEBKIT_API gchar*
    webkit_web_frame_get_inner_text (WebKitWebFrame* frame);

    WEBKIT_API void
    webkit_web_frame_print (WebKitWebFrame* frame);

    WEBKIT_API gchar*
    webkit_web_view_get_selected_text (WebKitWebView* web_view);
}

#endif
