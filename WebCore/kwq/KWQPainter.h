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

#ifndef QPAINTER_H_
#define QPAINTER_H_

#include "KWQNamespace.h"
#include "KWQPaintDevice.h"
#include "KWQColor.h"
#include "KWQPen.h"
#include "KWQBrush.h"
#include "KWQRect.h"
#include "KWQRegion.h"
#include "KWQPointArray.h"
#include "KWQString.h"
#include "KWQFontMetrics.h"

class QFont;
class QPixmap;
class QWidget;
class QPainterPrivate;

class QPainter : public Qt {
public:
    typedef enum { RTL, LTR } TextDirection;

    QPainter();
    QPainter(bool forPrinting);
    ~QPainter();

    QPaintDevice *device() const;
    
    const QFont &font() const;
    void setFont(const QFont &);
    QFontMetrics fontMetrics() const;
    
    const QPen &pen() const;
    void setPen(const QPen &);
    void setPen(PenStyle);
    void setPen(QRgb);
    
    const QBrush &QPainter::brush() const;
    void setBrush(const QBrush &);
    void setBrush(BrushStyle);
    void setBrush(QRgb);

    QRect xForm(const QRect &) const;

    void save();
    void restore();
    
    void drawRect(int, int, int, int);
    void drawLine(int, int, int, int);
    void drawLineSegments(const QPointArray &, int index=0, int nlines=-1);
    void drawEllipse(int, int, int, int);
    void drawArc(int, int, int, int, int, int);
    void drawPolyline(const QPointArray &, int index=0, int npoints=-1);
    void drawPolygon(const QPointArray &, bool winding=FALSE, int index=0, int npoints=-1);
    void drawConvexPolygon(const QPointArray &);

    void fillRect(int, int, int, int, const QBrush &);

    void drawPixmap(const QPoint &, const QPixmap &);
    void drawPixmap(const QPoint &, const QPixmap &, const QRect &);
    void drawPixmap( int x, int y, const QPixmap &,
			    int sx=0, int sy=0, int sw=-1, int sh=-1 );
    void drawTiledPixmap(int, int, int, int, const QPixmap &, int sx=0, int sy=0);

    void addClip(const QRect &);

    RasterOp rasterOp() const;
    void setRasterOp(RasterOp);

    void drawText(int x, int y, int, int, int alignmentFlags, const QString &);
    void drawHighlightForText(int x, int y, const QChar *, int length, int from, int to, int toAdd,
                  const QColor& backgroundColor, QPainter::TextDirection d,
                  int letterSpacing, int wordSpacing, bool smallCaps);
    void drawText(int x, int y, const QChar *, int length, int from, int to, int toAdd,
                  const QColor& backgroundColor, QPainter::TextDirection d,
                  int letterSpacing, int wordSpacing, bool smallCaps);
    void drawLineForText(int x, int y, int yOffset, int width);

    QColor selectedTextBackgroundColor() const;
    void setUsesInactiveTextBackgroundColor(bool u) { _usesInactiveTextBackgroundColor = u; }

    bool paintingDisabled() const;
    void setPaintingDisabled(bool);
        
    void beginTransparencyLayer(float opacity);
    void endTransparencyLayer();

    void setShadow(int x, int y, int blur, const QColor& color);
    void clearShadow();
    
private:
    // no copying or assignment
    QPainter(const QPainter &);
    QPainter &operator=(const QPainter &);

    void _setColorFromBrush();
    void _setColorFromPen();

    // A fillRect designed to work around buggy behavior in NSRectFill.
    void _fillRect(float x, float y, float w, float h, const QColor& color);
    
    void _drawPoints(const QPointArray &_points, bool winding, int index, int _npoints, bool fill);

    void _updateRenderer(NSString **families);

    QPainterPrivate *data;
    bool _isForPrinting;
    bool _usesInactiveTextBackgroundColor;
};

#endif
