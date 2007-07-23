/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
 * 3.  Neither the name of Apple, Inc. ("Apple") nor the names of
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

#ifndef WEBKIT_GTK_NETWORK_REQUEST_H
#define WEBKIT_GTK_NETWORK_REQUEST_H

#include <glib-object.h>

#include "webkitgtkdefines.h"

G_BEGIN_DECLS

#define WEBKIT_GTK_TYPE_NETWORK_REQUEST            (webkit_gtk_network_request_get_type())
#define WEBKIT_GTK_NETWORK_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_GTK_TYPE_NETWORK_REQUEST, WebKitGtkNetworkRequest))
#define WEBKIT_GTK_NETWORK_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_GTK_TYPE_NETWORK_REQUEST, WebKitGtkNetworkRequestClass))
#define WEBKIT_GTK_IS_NETWORK_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_GTK_TYPE_NETWORK_REQUEST))
#define WEBKIT_GTK_IS_NETWORK_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_GTK_TYPE_NETWORK_REQUEST))
#define WEBKIT_GTK_NETWORK_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_GTK_TYPE_NETWORK_REQUEST, WebKitGtkNetworkRequestClass))


typedef struct _WebKitGtkNetworkRequest WebKitGtkNetworkRequest;
typedef struct _WebKitGtkNetworkRequestClass WebKitGtkNetworkRequestClass;

struct _WebKitGtkNetworkRequest {
    GObject parent;
};

struct _WebKitGtkNetworkRequestClass {
    GObject parent;
};

WEBKIT_GTK_API GType
webkit_gtk_network_request_get_type (void);

WEBKIT_GTK_API GObject*
webkit_gtk_network_request_new (void);

G_END_DECLS

#endif
