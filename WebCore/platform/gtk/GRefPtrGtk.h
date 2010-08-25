/*
 *  Copyright (C) 2008 Collabora Ltd.
 *  Copyright (C) 2009 Martin Robinson
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

#ifndef GRefPtrGtk_h
#define GRefPtrGtk_h

#include "GRefPtr.h"

typedef struct _GtkTargetList GtkTargetList;
typedef struct _GdkCursor GdkCursor;

namespace WTF {

template <> GtkTargetList* refPlatformPtr(GtkTargetList* ptr);
template <> void derefPlatformPtr(GtkTargetList* ptr);

template <> GdkCursor* refPlatformPtr(GdkCursor* ptr);
template <> void derefPlatformPtr(GdkCursor* ptr);

}

#endif
