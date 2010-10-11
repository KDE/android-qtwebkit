/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#if ENABLE(SVG)
#include "SVGPolylineElement.h"

#include "SVGPointList.h"

namespace WebCore {

inline SVGPolylineElement::SVGPolylineElement(const QualifiedName& tagName, Document* document)
    : SVGPolyElement(tagName, document)
{
}

PassRefPtr<SVGPolylineElement> SVGPolylineElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGPolylineElement(tagName, document));
}

void SVGPolylineElement::toPathData(Path& path) const
{
    ASSERT(path.isEmpty());

    int len = points()->numberOfItems();
    if (len < 1)
        return;

    ExceptionCode ec = 0;
    path.moveTo(points()->getItem(0, ec));

    for (int i = 1; i < len; ++i)
        path.addLineTo(points()->getItem(i, ec));
}

}

#endif // ENABLE(SVG)
