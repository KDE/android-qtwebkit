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

#include "config.h"

#include "webkitnetworkrequest.h"
#include "webkitprivate.h"

using namespace WebKit;
using namespace WebCore;

extern "C" {

G_DEFINE_TYPE(WebKitNetworkRequest, webkit_network_request, G_TYPE_OBJECT);

static void webkit_network_request_finalize(GObject* object)
{
    WebKitNetworkRequestPrivate* requestPrivate = WEBKIT_NETWORK_REQUEST_GET_PRIVATE(object);

    g_free(requestPrivate->uri);

    G_OBJECT_CLASS(webkit_network_request_parent_class)->finalize(object);
}

static void webkit_network_request_class_init(WebKitNetworkRequestClass* requestClass)
{
    g_type_class_add_private(requestClass, sizeof(WebKitNetworkRequestPrivate));

    G_OBJECT_CLASS(requestClass)->finalize = webkit_network_request_finalize;
}

static void webkit_network_request_init(WebKitNetworkRequest* request)
{
}

WebKitNetworkRequest* webkit_network_request_new(const gchar* uri)
{
    WebKitNetworkRequest* request = WEBKIT_NETWORK_REQUEST(g_object_new(WEBKIT_TYPE_NETWORK_REQUEST, NULL));
    WebKitNetworkRequestPrivate* requestPrivate = WEBKIT_NETWORK_REQUEST_GET_PRIVATE(request);

    requestPrivate->uri = g_strdup(uri);

    return request;
}

void webkit_network_request_set_uri(WebKitNetworkRequest* request, const gchar* uri)
{
    WebKitNetworkRequestPrivate* requestPrivate;

    g_return_if_fail(WEBKIT_IS_NETWORK_REQUEST(request));

    requestPrivate = WEBKIT_NETWORK_REQUEST_GET_PRIVATE(request);

    g_free(requestPrivate->uri);
    requestPrivate->uri = g_strdup(uri);
}

const gchar* webkit_network_request_get_uri(WebKitNetworkRequest* request)
{
    WebKitNetworkRequestPrivate* requestPrivate;

    g_return_val_if_fail(WEBKIT_IS_NETWORK_REQUEST(request), NULL);

    requestPrivate = WEBKIT_NETWORK_REQUEST_GET_PRIVATE(request);

    return requestPrivate->uri;
}

}
