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

#ifndef QREGEXP_H_
#define QREGEXP_H_

#include "KWQString.h"

#include "KWQRefPtr.h"

class QRegExp {
public:
    QRegExp();
    QRegExp(const QString &, bool caseSensitive = false, bool glob = false);
    QRegExp(const char *);
    ~QRegExp();

    QRegExp(const QRegExp &);    
    QRegExp &operator=(const QRegExp &);

    QString pattern() const;
    int match(const QString &, int startFrom = 0, int *matchLength = 0, bool treatStartAsStartOfInput = true) const;

    int search(const QString &, int startFrom = 0) const;
    int searchRev(const QString &, int startFrom = -1) const;

    int pos(int n = 0);
    int matchedLength() const;
    
private:
    class KWQRegExpPrivate;    
    KWQRefPtr<KWQRegExpPrivate> d;
};

#endif
