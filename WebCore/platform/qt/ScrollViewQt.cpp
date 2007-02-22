/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
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

#include "config.h"
#include "ScrollView.h"
#include "FrameView.h"
#include "FloatRect.h"
#include "IntPoint.h"
#include "PlatformMouseEvent.h"

#include "Frame.h"

#include <QAbstractScrollArea>
#include <QDebug>
#include <QScrollBar>

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {

ScrollView::ScrollView()
    : m_area(0), m_allowsScrolling(true),
      m_width(0), m_height(0)
{
}

ScrollView::~ScrollView()
{
}

void ScrollView::setScrollArea(QAbstractScrollArea* area)
{
    m_area = area;
    Widget::setQWidget(m_area);
}

void ScrollView::updateContents(const IntRect& updateRect, bool now)
{
    //Update is fine for both now=true/false cases
    if (m_area && m_area->viewport() && !updateRect.isEmpty()) {
        IntRect realCoords(updateRect.x() - scrollOffset().width(),
                           updateRect.y() - scrollOffset().height(),
                           updateRect.width(), updateRect.height());
        m_area->viewport()->update(realCoords);
    }
}

int ScrollView::visibleWidth() const
{
    if (!m_area)
        return 0;

    int scrollBarAdjustments = 0;
    if (m_area->verticalScrollBar()->isVisible())
        scrollBarAdjustments = m_area->verticalScrollBar()->width();
    return m_area->maximumViewportSize().width() - scrollBarAdjustments;
}

int ScrollView::visibleHeight() const
{
    if (!m_area)
        return 0;

    int scrollBarAdjustments = 0;
    if (m_area->horizontalScrollBar()->isVisible())
        scrollBarAdjustments = m_area->horizontalScrollBar()->height();
    return m_area->maximumViewportSize().height()-scrollBarAdjustments;
}

FloatRect ScrollView::visibleContentRect() const
{
    if (!m_area)
        return FloatRect();

    return FloatRect(m_area->horizontalScrollBar()->value(),
                     m_area->verticalScrollBar()->value(),
                     visibleWidth(),
                     visibleHeight());
}

void ScrollView::setContentsPos(int newX, int newY)
{
    if (!m_area)
        return;
    m_area->horizontalScrollBar()->setValue(newX);
    m_area->verticalScrollBar()->setValue(newY);
}

void ScrollView::resizeContents(int w, int h)
{
    m_width = w; m_height = h;
    QAbstractSlider* hbar = m_area->horizontalScrollBar();
    QAbstractSlider* vbar = m_area->verticalScrollBar();

    const QSize viewportSize = m_area->viewport()->size();
    QSize docSize = QSize(contentsWidth(), contentsHeight());

    hbar->setRange(0, docSize.width() - viewportSize.width());
    hbar->setPageStep(viewportSize.width());

    vbar->setRange(0, docSize.height() - viewportSize.height());
    vbar->setPageStep(viewportSize.height());
}

int ScrollView::contentsX() const
{
    if (!m_area)
        return 0;
    return m_area->horizontalScrollBar()->value();
}

int ScrollView::contentsY() const
{
    if (!m_area)
        return 0;
    return m_area->verticalScrollBar()->value();
}

int ScrollView::contentsWidth() const
{
    return m_width;
}

int ScrollView::contentsHeight() const
{
    return m_height;
}


IntPoint ScrollView::contentsToWindow(const IntPoint& point) const
{
    QAbstractSlider* hbar = m_area->horizontalScrollBar();
    QAbstractSlider* vbar = m_area->verticalScrollBar();

    return IntPoint(point.x() - hbar->value(), point.y() - vbar->value());
}

IntPoint ScrollView::windowToContents(const IntPoint& point) const
{
    QAbstractSlider* hbar = m_area->horizontalScrollBar();
    QAbstractSlider* vbar = m_area->verticalScrollBar();

    return IntPoint(point.x() + hbar->value(), point.y() + vbar->value());
}

IntSize ScrollView::scrollOffset() const
{
    if (!m_area)
        return IntSize();
    return IntSize(m_area->horizontalScrollBar()->value(), m_area->verticalScrollBar()->value());
}

void ScrollView::scrollBy(int dx, int dy)
{
    if (!m_area)
        return;
    m_area->horizontalScrollBar()->setValue(m_area->horizontalScrollBar()->value() + dx);
    m_area->verticalScrollBar()->setValue(m_area->verticalScrollBar()->value() + dy);
}

ScrollbarMode ScrollView::hScrollbarMode() const
{
    if (!m_area)
        return ScrollbarAuto;
    switch (m_area->horizontalScrollBarPolicy())
    {
        case Qt::ScrollBarAsNeeded:
            return ScrollbarAuto;
        case Qt::ScrollBarAlwaysOff:
            return ScrollbarAlwaysOff;
        case Qt::ScrollBarAlwaysOn:
            return ScrollbarAlwaysOn;
    }

    return ScrollbarAuto;
}

ScrollbarMode ScrollView::vScrollbarMode() const
{
    if (!m_area)
        return ScrollbarAuto;
    switch (m_area->verticalScrollBarPolicy())
    {
        case Qt::ScrollBarAsNeeded:
            return ScrollbarAuto;
        case Qt::ScrollBarAlwaysOff:
            return ScrollbarAlwaysOff;
        case Qt::ScrollBarAlwaysOn:
            return ScrollbarAlwaysOn;
    }

    return ScrollbarAuto;
}

void ScrollView::suppressScrollbars(bool suppressed, bool /* repaintOnSuppress */)
{
    setScrollbarsMode(suppressed ? ScrollbarAlwaysOff : ScrollbarAuto);
}

void ScrollView::setHScrollbarMode(ScrollbarMode newMode)
{
    if (!m_area || !m_allowsScrolling)
        return;
    switch (newMode)
    {
        case ScrollbarAuto:
            m_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            break;
        case ScrollbarAlwaysOff:
            m_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            break;
        case ScrollbarAlwaysOn:
            m_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            break;
    }
}

void ScrollView::setVScrollbarMode(ScrollbarMode newMode)
{
    if (!m_area || !m_allowsScrolling)
        return;
    switch (newMode)
    {
        case ScrollbarAuto:
            m_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            break;
        case ScrollbarAlwaysOff:
            m_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            break;
        case ScrollbarAlwaysOn:
            m_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            break;
    }
}

void ScrollView::setScrollbarsMode(ScrollbarMode newMode)
{
    setHScrollbarMode(newMode);
    setVScrollbarMode(newMode);
}

void ScrollView::setStaticBackground(bool flag)
{
    if (flag) {
        QPalette pal = m_area->viewport()->palette();
        pal.setBrush(QPalette::Background, QBrush(QColor(Qt::transparent)));
        m_area->viewport()->setPalette(pal);
        m_area->viewport()->setAutoFillBackground(false);
    } else {
        m_area->viewport()->setAutoFillBackground(true);
    }
}

void ScrollView::addChild(Widget* child)
{
    QWidget* w = child->qwidget();
    w->setParent(m_area->viewport());
}

void ScrollView::removeChild(Widget* child)
{
    child->hide();
}

void ScrollView::scrollPointRecursively(int x, int y)
{
    x = (x < 0) ? 0 : x;
    y = (y < 0) ? 0 : y;

    m_area->horizontalScrollBar()->setValue(x);
    m_area->verticalScrollBar()->setValue(y);
}

bool ScrollView::inWindow() const
{
    return true;
}

void ScrollView::wheelEvent(PlatformWheelEvent&)
{
    //we don't do absolutely anything here - we handled it already in ScrollViewCanvasQt
    // internally in Qt
}

PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent)
{
    // Probably don't care about this.
#if 0
    // Not so sure: frames with scrollbars have the wrong mouse cursor over
    // the scrollbar.  Is this why?  FIXME
    if (m_area->horizontalScrollBar()->geometry().contains(mouseEvent.pos())) {
        return m_area->horizontalScrollBar();
    }
    if (m_area->verticalScrollBar()->geometry().contains(mouseEvent.pos())) {
        return m_area->verticalScrollBar();
    }
#endif
    return 0;
}

void ScrollView::setAllowsScrolling(bool allows)
{
    if (!allows)
        suppressScrollbars(true);
    m_allowsScrolling = allows;
}

}

// vim: ts=4 sw=4 et
