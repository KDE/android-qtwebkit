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

#ifndef QPOINT_H_
#define QPOINT_H_

#include "KWQDef.h"

#ifdef _KWQ_IOSTREAM_
#include <iosfwd>
#endif

#include "KWQMemArray.h"

typedef struct _NSPoint NSPoint;

class QPoint {
public:
    QPoint();
    QPoint(int, int);
    explicit QPoint(const NSPoint &); // don't do this implicitly since it's lossy

    int x() const { return xCoord; }
    int y() const { return yCoord; }

    int manhattanLength() const;
    
    friend QPoint operator+(const QPoint &, const QPoint &);
    friend QPoint operator-(const QPoint &, const QPoint &);
    
    operator NSPoint() const;

private:
    int xCoord;
    int yCoord;
};

class QPointArray : public QMemArray<QPoint> {
public:
    QPointArray() { }
    QPointArray(int size) : QMemArray<QPoint>(size) { }

    QPointArray(int, const int *);

    void setPoint(uint, int, int);
#if 0
    // FIXME: Workaround for Radar 2921061.
    bool setPoints(int, int, int, ...);
#else
    bool setPoints(int, int, int, int, int, int, int, int, int);
#endif
    bool setPoints( int nPoints, const int *points );
    
#ifdef _KWQ_IOSTREAM_
    friend std::ostream &operator<<(std::ostream &, const QPoint &);
#endif

};

#endif
