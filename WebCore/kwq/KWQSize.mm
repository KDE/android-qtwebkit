/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "KWQSize.h"

QSize::QSize() : w(-1), h(-1)
{
}

QSize::QSize(int width, int height) : w(width), h(height)
{
}

QSize::QSize(const NSSize &s) : w((int)s.width), h((int)s.height)
{
}

bool QSize::isValid() const
{
    return w >= 0 && h >= 0;
}

QSize QSize::expandedTo(const QSize &o) const
{
    return QSize(w > o.w ? w : o.w, h > o.h ? h : o.h);
}

QSize::operator NSSize() const
{
    return NSMakeSize(w, h);
}

QSize operator+(const QSize &a, const QSize &b)
{
    return QSize(a.w + b.w, a.h + b.h);
}

bool operator==(const QSize &a, const QSize &b)
{
    return a.w == b.w && a.h == b.h;
}

bool operator!=(const QSize &a, const QSize &b)
{
    return a.w != b.w || a.h != b.h;
}

#ifdef _KWQ_IOSTREAM_
std::ostream &operator<<(std::ostream &o, const QSize &s)
{
    return o << "QSize: [w: " << s.width() << "; h: " << s.height() << "]";
}
#endif
