/*
 *  Copyright (C) 2010 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "PlatformRefPtrCairo.h"

#include <cairo.h>

namespace WTF {

template <> cairo_t* refPlatformPtr(cairo_t* ptr)
{
    if (ptr)
        cairo_reference(ptr);
    return ptr;
}

template <> void derefPlatformPtr(cairo_t* ptr)
{
    if (ptr)
        cairo_destroy(ptr);
}

template <> cairo_surface_t* refPlatformPtr(cairo_surface_t* ptr)
{
    if (ptr)
        cairo_surface_reference(ptr);
    return ptr;
}

template <> void derefPlatformPtr(cairo_surface_t* ptr)
{
    if (ptr)
        cairo_surface_destroy(ptr);
}

}
