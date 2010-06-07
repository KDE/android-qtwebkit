/*
 * Copyright (C) 2010 Collabora Ltd.
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
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "GOwnPtrCairo.h"

#if defined(USE_FREETYPE)
#include <cairo-ft.h>
#include <fontconfig/fcfreetype.h>
#endif

namespace WTF {

#if defined(USE_FREETYPE)
template <> void freeOwnedGPtr<FcPattern>(FcPattern* ptr)
{
    if (ptr)
        FcPatternDestroy(ptr);
}

template <> void freeOwnedGPtr<FcObjectSet>(FcObjectSet* ptr)
{
    if (ptr)
        FcObjectSetDestroy(ptr);
}

template <> void freeOwnedGPtr<FcFontSet>(FcFontSet* ptr)
{
    if (ptr)
        FcFontSetDestroy(ptr);
}
#endif

} // namespace WTF
