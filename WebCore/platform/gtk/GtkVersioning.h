/*
 * Copyright (C) 2010 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef GtkVersioning_h
#define GtkVersioning_h

#include <gtk/gtk.h>

// Macros to avoid deprecation checking churn
#if !GTK_CHECK_VERSION(2, 21, 2)
#define gdk_visual_get_depth(visual) (visual)->depth
#define gdk_visual_get_bits_per_rgb(visual) (visual)->bits_per_rgb
#define gdk_drag_context_get_selected_action(context) (context)->action
#endif // GTK_CHECK_VERSION(2, 21, 2)

#if !GTK_CHECK_VERSION(2, 20, 0)
#define gtk_widget_set_realized(widget, FALSE) GTK_WIDGET_UNSET_FLAGS((widget), GTK_REALIZED)
#define gtk_widget_set_realized(widget, TRUE) GTK_WIDGET_SET_FLAGS((widget), GTK_REALIZED)
#endif // GTK_CHECK_VERSION(2, 20, 0)

#if !GTK_CHECK_VERSION(2, 19, 0)
#define gtk_widget_is_toplevel(widget) GTK_WIDGET_TOPLEVEL(widget)
#define gtk_widget_get_realized(widget) GTK_WIDGET_REALIZED(widget)
#define gtk_widget_get_has_window(widget) !GTK_WIDGET_NO_WINDOW(widget)
#define gtk_widget_get_can_focus(widget) GTK_WIDGET_CAN_FOCUS(widget)
#define gtk_widget_is_sensitive(widget) GTK_WIDGET_IS_SENSITIVE(widget)
#endif // GTK_CHECK_VERSION(2, 19, 0)

#if !GTK_CHECK_VERSION(2, 18, 0)
#define gtk_widget_set_visible(widget, FALSE) GTK_WIDGET_UNSET_FLAGS((widget), GTK_VISIBLE)
#define gtk_widget_set_visible(widget, TRUE) GTK_WIDGET_SET_FLAGS((widget), GTK_VISIBLE)
#define gtk_widget_set_window(widget, new_window) (widget)->window = (new_window)
#define gtk_widget_set_can_focus(widget, FALSE) GTK_WIDGET_UNSET_FLAGS((widget), GTK_CAN_FOCUS)
#define gtk_widget_set_can_focus(widget, TRUE) GTK_WIDGET_SET_FLAGS((widget), GTK_CAN_FOCUS)
#endif // GTK_CHECK_VERSION(2, 18, 0)

#if !GTK_CHECK_VERSION(2, 14, 0)
#define gtk_widget_get_window(widget) (widget)->window
#define gtk_adjustment_get_value(adj) (adj)->value
#define gtk_dialog_get_content_area(dialog) (dialog)->vbox
#endif // GTK_CHECK_VERSION(2, 14, 0)

#endif // GtkVersioning_h
