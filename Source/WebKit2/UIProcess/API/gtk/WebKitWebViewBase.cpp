/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitWebViewBase.h"

#include "GOwnPtrGtk.h"
#include "GtkVersioning.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NotImplemented.h"
#include "PageClientImpl.h"
#include "RefPtrCairo.h"
#include "WebContext.h"
#include "WebEventFactory.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageProxy.h"
#include <WebKit2/WKContext.h>

using namespace WebKit;
using namespace WebCore;

static gpointer webkitWebViewBaseParentClass = 0;

struct _WebKitWebViewBasePrivate {
    OwnPtr<PageClientImpl> pageClient;
    RefPtr<WebPageProxy> page;
    gboolean isPageActive;
    GtkIMContext* imContext;
    gint currentClickCount;
    IntPoint previousClickPoint;
    guint previousClickButton;
    guint32 previousClickTime;
};

G_DEFINE_TYPE(WebKitWebViewBase, webkit_web_view_base, GTK_TYPE_CONTAINER)

static void webkitWebViewBaseRealize(GtkWidget* widget)
{
    gtk_widget_set_realized(widget, TRUE);

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    GdkWindowAttr attributes;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
#ifdef GTK_API_VERSION_2
    attributes.colormap = gtk_widget_get_colormap(widget);
#endif
    attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK
        | GDK_EXPOSURE_MASK
        | GDK_BUTTON_PRESS_MASK
        | GDK_BUTTON_RELEASE_MASK
        | GDK_POINTER_MOTION_MASK
        | GDK_KEY_PRESS_MASK
        | GDK_KEY_RELEASE_MASK
        | GDK_BUTTON_MOTION_MASK
        | GDK_BUTTON1_MOTION_MASK
        | GDK_BUTTON2_MOTION_MASK
        | GDK_BUTTON3_MOTION_MASK;

    gint attributesMask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
#ifdef GTK_API_VERSION_2
    attributesMask |= GDK_WA_COLORMAP;
#endif
    GdkWindow* window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributesMask);
    gtk_widget_set_window(widget, window);
    gdk_window_set_user_data(window, widget);

#ifdef GTK_API_VERSION_2
#if GTK_CHECK_VERSION(2, 20, 0)
    gtk_widget_style_attach(widget);
#else
    widget->style = gtk_style_attach(gtk_widget_get_style(widget), window);
#endif
    gtk_style_set_background(gtk_widget_get_style(widget), window, GTK_STATE_NORMAL);
#else
    gtk_style_context_set_background(gtk_widget_get_style_context(widget), window);
#endif

    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webView->priv;
    gtk_im_context_set_client_window(priv->imContext, window);
}

static void webkitWebViewBaseContainerAdd(GtkContainer* container, GtkWidget* widget)
{
    gtk_widget_set_parent(widget, GTK_WIDGET(container));
}

static void webkitWebViewBaseFinalize(GObject* gobject)
{
    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(gobject);
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;

    if (priv->imContext) {
        g_object_unref(priv->imContext);
        priv->imContext = 0;
    }

    priv->page->close();

    delete priv;
    webkitWebViewBase->priv = 0;

    G_OBJECT_CLASS(webkit_web_view_base_parent_class)->finalize(gobject);
}

static void webkit_web_view_base_init(WebKitWebViewBase* webkitWebViewBase)
{
    WebKitWebViewBasePrivate* priv = new WebKitWebViewBasePrivate();
    webkitWebViewBase->priv = priv;

    priv->isPageActive = TRUE;

    gtk_widget_set_can_focus(GTK_WIDGET(webkitWebViewBase), TRUE);
    priv->imContext = gtk_im_multicontext_new();

    priv->currentClickCount = 0;
    priv->previousClickButton = 0;
    priv->previousClickTime = 0;

    priv->pageClient = PageClientImpl::create(GTK_WIDGET(webkitWebViewBase));
}

#ifdef GTK_API_VERSION_2
static gboolean webViewExpose(GtkWidget* widget, GdkEventExpose* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    GdkRectangle clipRect;
    gdk_region_get_clipbox(event->region, &clipRect);

    GdkWindow* window = gtk_widget_get_window(widget);
    RefPtr<cairo_t> cr = adoptRef(gdk_cairo_create(window));

    priv->page->drawingArea()->paint(clipRect, cr);

    return FALSE;
}
#else
static gboolean webViewDraw(GtkWidget* widget, cairo_t* cr)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    GdkRectangle clipRect;

    if (!gdk_cairo_get_clip_rectangle(cr, &clipRect))
        return FALSE;

    priv->page->drawingArea()->paint(clipRect, cr);

    return FALSE;
}
#endif

static void webViewSizeAllocate(GtkWidget* widget, GtkAllocation* allocation)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->size_allocate(widget, allocation);
    priv->page->drawingArea()->setSize(IntSize(allocation->width, allocation->height), IntSize());
}

static gboolean webViewFocusInEvent(GtkWidget* widget, GdkEventFocus* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (gtk_widget_is_toplevel(toplevel) && gtk_window_has_toplevel_focus(GTK_WINDOW(toplevel))) {
        gtk_im_context_focus_in(priv->imContext);
        if (!priv->isPageActive) {
            priv->isPageActive = TRUE;
            priv->page->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
        }
    }

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->focus_in_event(widget, event);
}

static gboolean webViewFocusOutEvent(GtkWidget* widget, GdkEventFocus* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    priv->isPageActive = FALSE;
    priv->page->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
    if (priv->imContext)
        gtk_im_context_focus_out(priv->imContext);

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->focus_out_event(widget, event);
}

static gboolean webViewKeyPressEvent(GtkWidget* widget, GdkEventKey* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    priv->page->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(event)));

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->key_press_event(widget, event);
}

static gboolean webViewKeyReleaseEvent(GtkWidget* widget, GdkEventKey* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (gtk_im_context_filter_keypress(priv->imContext, event))
        return TRUE;

    priv->page->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(event)));

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->key_release_event(widget, event);
}

// Copied from webkitwebview.cpp
static guint32 getEventTime(GdkEvent* event)
{
    guint32 time = gdk_event_get_time(event);
    if (time)
        return time;

    // Real events always have a non-zero time, but events synthesized
    // by the DRT do not and we must calculate a time manually. This time
    // is not calculated in the DRT, because GTK+ does not work well with
    // anything other than GDK_CURRENT_TIME on synthesized events.
    GTimeVal timeValue;
    g_get_current_time(&timeValue);
    return (timeValue.tv_sec * 1000) + (timeValue.tv_usec / 1000);
}

static gboolean webViewButtonPressEvent(GtkWidget* widget, GdkEventButton* buttonEvent)
{
    if (buttonEvent->button == 3) {
        // FIXME: [GTK] Add context menu support for Webkit2.
        // https://bugs.webkit.org/show_bug.cgi?id=54827
        notImplemented();
        return FALSE;
    }

    gtk_widget_grab_focus(widget);

    // For double and triple clicks GDK sends both a normal button press event
    // and a specific type (like GDK_2BUTTON_PRESS). If we detect a special press
    // coming up, ignore this event as it certainly generated the double or triple
    // click. The consequence of not eating this event is two DOM button press events
    // are generated.
    GOwnPtr<GdkEvent> nextEvent(gdk_event_peek());
    if (nextEvent && (nextEvent->any.type == GDK_2BUTTON_PRESS || nextEvent->any.type == GDK_3BUTTON_PRESS))
        return TRUE;

    gint doubleClickDistance = 250;
    gint doubleClickTime = 5;
    GtkSettings* settings = gtk_settings_get_for_screen(gtk_widget_get_screen(widget));
    g_object_get(settings,
                 "gtk-double-click-distance", &doubleClickDistance,
                 "gtk-double-click-time", &doubleClickTime, NULL);

    // GTK+ only counts up to triple clicks, but WebCore wants to know about
    // quadruple clicks, quintuple clicks, ad infinitum. Here, we replicate the
    // GDK logic for counting clicks.
    GdkEvent* event(reinterpret_cast<GdkEvent*>(buttonEvent));
    guint32 eventTime = getEventTime(event);
    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    if ((event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
        || ((abs(buttonEvent->x - priv->previousClickPoint.x()) < doubleClickDistance)
            && (abs(buttonEvent->y - priv->previousClickPoint.y()) < doubleClickDistance)
            && (eventTime - priv->previousClickTime < static_cast<guint>(doubleClickTime))
            && (buttonEvent->button == priv->previousClickButton)))
        priv->currentClickCount++;
    else
        priv->currentClickCount = 1;

    priv->page->handleMouseEvent(NativeWebMouseEvent(event, priv->currentClickCount));

    gdouble x, y;
    gdk_event_get_coords(event, &x, &y);
    priv->previousClickPoint = IntPoint(x, y);
    priv->previousClickButton = buttonEvent->button;
    priv->previousClickTime = eventTime;

    return FALSE;
}

static gboolean webViewButtonReleaseEvent(GtkWidget* widget, GdkEventButton* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    gtk_widget_grab_focus(widget);
    priv->page->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(event), 0 /* currentClickCount */));

    return FALSE;
}

static gboolean webViewScrollEvent(GtkWidget* widget, GdkEventScroll* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    priv->page->handleWheelEvent(WebEventFactory::createWebWheelEvent(event));

    return FALSE;
}

static gboolean webViewMotionNotifyEvent(GtkWidget* widget, GdkEventMotion* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    priv->page->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(event), 0 /* currentClickCount */));

    return FALSE;
}

static void webkit_web_view_base_class_init(WebKitWebViewBaseClass* webkitWebViewBaseClass)
{
    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(webkitWebViewBaseClass);
    widgetClass->realize = webkitWebViewBaseRealize;
#ifdef GTK_API_VERSION_2
    widgetClass->expose_event = webViewExpose;
#else
    widgetClass->draw = webViewDraw;
#endif
    widgetClass->size_allocate = webViewSizeAllocate;
    widgetClass->focus_in_event = webViewFocusInEvent;
    widgetClass->focus_out_event = webViewFocusOutEvent;
    widgetClass->key_press_event = webViewKeyPressEvent;
    widgetClass->key_release_event = webViewKeyReleaseEvent;
    widgetClass->button_press_event = webViewButtonPressEvent;
    widgetClass->button_release_event = webViewButtonReleaseEvent;
    widgetClass->scroll_event = webViewScrollEvent;
    widgetClass->motion_notify_event = webViewMotionNotifyEvent;

    GObjectClass* gobjectClass = G_OBJECT_CLASS(webkitWebViewBaseClass);
    gobjectClass->finalize = webkitWebViewBaseFinalize;

    GtkContainerClass* containerClass = GTK_CONTAINER_CLASS(webkitWebViewBaseClass);
    containerClass->add = webkitWebViewBaseContainerAdd;
}

WebKitWebViewBase* webkitWebViewBaseCreate(WebContext* context, WebPageGroup* pageGroup)
{
    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(g_object_new(WEBKIT_TYPE_WEB_VIEW_BASE, NULL));
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;

    priv->page = context->createWebPage(priv->pageClient.get(), pageGroup);
    priv->page->initializeWebPage();

    return webkitWebViewBase;
}

GtkIMContext* webkitWebViewBaseGetIMContext(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->imContext;
}

WebPageProxy* webkitWebViewBaseGetPage(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->page.get();
}
