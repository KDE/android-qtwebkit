/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef csshelper_h
#define csshelper_h

namespace WebCore {

    class String;

    /*
     * mostly just removes the url("...") brace
     */
    String parseURL(const String& url);

    // We always assume 96 CSS pixels in a CSS inch. This is the cold hard truth of the Web.
    // At high DPI, we may scale a CSS pixel, but the ratio of the CSS pixel to the so-called
    // "absolute" CSS length units like inch and pt is always fixed and never changes.
    const double cssPixelsPerInch = 96.0;
} // namespace WebCore

#endif // csshelper_h
