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

#ifndef QFONTMETRICS_H_
#define QFONTMETRICS_H_

#include "KWQRect.h"
#include "KWQSize.h"
#include "KWQString.h"
#include "KWQFont.h"
#include "KWQFontInfo.h"

class QFontMetricsPrivate;

class QFontMetrics {
public:
    QFontMetrics();
    QFontMetrics(const QFont &);
    ~QFontMetrics();

    QFontMetrics(const QFontMetrics &);
    QFontMetrics &operator=(const QFontMetrics &);

    const QFont &font() const;
    void setFont(const QFont &);
    
    int ascent() const;
    int descent() const;
    int height() const;
    int lineSpacing() const;
    float xHeight() const;
    
    int width(QChar) const;
    int width(char) const;
    int width(const QString &, int len=-1) const;
    int charWidth(const QString &, int pos) const;
    int width(const QChar *, int len) const;
    float floatWidth(const QChar *, int slen, int pos, int len,
                     int letterSpacing, int wordSpacing, bool smallCaps) const;
    float floatCharacterWidths(const QChar *, int slen, int pos, int len, int toAdd, float *buffer,
                               int letterSpacing, int wordSpacing, bool smallCaps) const;

    QRect boundingRect(const QString &, int len=-1) const;
    QRect boundingRect(int, int, int, int, int, const QString &) const;

    QSize size(int, const QString &) const;

    int rightBearing(QChar) const;
    int leftBearing(QChar) const;
    int baselineOffset() const { return ascent(); }
    
private:
    KWQRefPtr<QFontMetricsPrivate> data;

};

#endif

