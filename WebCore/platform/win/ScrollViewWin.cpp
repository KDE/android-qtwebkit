/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "Chrome.h"
#include "ChromeClient.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Scrollbar.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "RenderTheme.h"
#include "Scrollbar.h"
#include "ScrollbarClient.h"
#include "ScrollbarTheme.h"
#include "Settings.h"
#include <algorithm>
#include <winsock2.h>
#include <windows.h>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate : public ScrollbarClient {
public:
    ScrollViewPrivate(ScrollView* view)
        : m_view(view)
        , m_inUpdateScrollbars(false)
        , m_panScrollIconPoint(0,0)
        , m_drawPanScrollIcon(false)
    {
    }

    ~ScrollViewPrivate()
    {
        setHasHorizontalScrollbar(false);
        setHasVerticalScrollbar(false);
    }

    void setHasHorizontalScrollbar(bool hasBar);
    void setHasVerticalScrollbar(bool hasBar);

    virtual void valueChanged(Scrollbar*);
    virtual IntRect windowClipRect() const;
    virtual bool isActive() const;

    void scrollBackingStore(const IntSize& scrollDelta);


    ScrollView* m_view;
    bool m_inUpdateScrollbars;
    HRGN m_dirtyRegion;
    IntPoint m_panScrollIconPoint;
    bool m_drawPanScrollIcon;
};

const int panIconSizeLength = 20;

void ScrollView::ScrollViewPrivate::setHasHorizontalScrollbar(bool hasBar)
{
    if (hasBar && !m_view->m_horizontalScrollbar) {
        m_view->m_horizontalScrollbar = Scrollbar::createNativeScrollbar(this, HorizontalScrollbar, RegularScrollbar);
        m_view->addChild(m_view->m_horizontalScrollbar.get());
    } else if (!hasBar && m_view->m_horizontalScrollbar) {
        m_view->removeChild(m_view->m_horizontalScrollbar.get());
        m_view->m_horizontalScrollbar = 0;
    }
}

void ScrollView::ScrollViewPrivate::setHasVerticalScrollbar(bool hasBar)
{
    if (hasBar && !m_view->m_verticalScrollbar) {
        m_view->m_verticalScrollbar = Scrollbar::createNativeScrollbar(this, VerticalScrollbar, RegularScrollbar);
        m_view->addChild(m_view->m_verticalScrollbar.get());
    } else if (!hasBar && m_view->m_verticalScrollbar) {
        m_view->removeChild(m_view->m_verticalScrollbar.get());
        m_view->m_verticalScrollbar = 0;
    }
}

void ScrollView::ScrollViewPrivate::valueChanged(Scrollbar* bar)
{
    // Figure out if we really moved.
    IntSize newOffset = m_view->m_scrollOffset;
    if (bar) {
        if (bar == m_view->m_horizontalScrollbar)
            newOffset.setWidth(bar->value());
        else if (bar == m_view->m_verticalScrollbar)
            newOffset.setHeight(bar->value());
    }
    IntSize scrollDelta = newOffset - m_view->m_scrollOffset;
    if (scrollDelta == IntSize())
        return;
    m_view->m_scrollOffset = newOffset;

    if (m_view->scrollbarsSuppressed())
        return;

    static_cast<FrameView*>(m_view)->frame()->sendScrollEvent();
    scrollBackingStore(scrollDelta);
}

void ScrollView::ScrollViewPrivate::scrollBackingStore(const IntSize& scrollDelta)
{
    // Since scrolling is double buffered, we will be blitting the scroll view's intersection
    // with the clip rect every time to keep it smooth.
    HWND containingWindowHandle = m_view->containingWindow();
    IntRect clipRect = m_view->windowClipRect();
    IntRect scrollViewRect = m_view->convertToContainingWindow(IntRect(0, 0, m_view->visibleWidth(), m_view->visibleHeight()));
    IntRect updateRect = clipRect;
    updateRect.intersect(scrollViewRect);
    RECT r = updateRect;
    ::InvalidateRect(containingWindowHandle, &r, false);

    if (m_drawPanScrollIcon) {
        int panIconDirtySquareSizeLength = 2 * (panIconSizeLength + max(abs(scrollDelta.width()), abs(scrollDelta.height()))); // We only want to repaint what's necessary
        IntPoint panIconDirtySquareLocation = IntPoint(m_panScrollIconPoint.x() - (panIconDirtySquareSizeLength / 2), m_panScrollIconPoint.y() - (panIconDirtySquareSizeLength / 2));
        IntRect panScrollIconDirtyRect = IntRect(panIconDirtySquareLocation , IntSize(panIconDirtySquareSizeLength, panIconDirtySquareSizeLength));
        
        m_view->updateWindowRect(panScrollIconDirtyRect);
    }

    if (m_view->canBlitOnScroll()) // The main frame can just blit the WebView window
       // FIXME: Find a way to blit subframes without blitting overlapping content
       m_view->scrollBackingStore(-scrollDelta.width(), -scrollDelta.height(), scrollViewRect, clipRect);
    else  {
       // We need to go ahead and repaint the entire backing store.  Do it now before moving the
       // plugins.
       m_view->addToDirtyRegion(updateRect);
       m_view->updateBackingStore();
    }

    // This call will move child HWNDs (plugins) and invalidate them as well.
    m_view->frameRectsChanged();

    // Now update the window (which should do nothing but a blit of the backing store's updateRect and so should
    // be very fast).
    ::UpdateWindow(containingWindowHandle);
}

IntRect ScrollView::ScrollViewPrivate::windowClipRect() const
{
    return static_cast<const FrameView*>(m_view)->windowClipRect(false);
}

bool ScrollView::ScrollViewPrivate::isActive() const
{
    Page* page = static_cast<const FrameView*>(m_view)->frame()->page();
    return page && page->focusController()->isActive();
}

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate(this))
{
    init();
}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::platformAddChild(Widget*)
{
}

void ScrollView::platformRemoveChild(Widget*)
{
}

void ScrollView::updateContents(const IntRect& rect, bool now)
{
    if (rect.isEmpty())
        return;

    IntPoint windowPoint = contentsToWindow(rect.location());
    IntRect containingWindowRect = rect;
    containingWindowRect.setLocation(windowPoint);

    updateWindowRect(containingWindowRect, now);
}

void ScrollView::updateWindowRect(const IntRect& rect, bool now)
{
    RECT containingWindowRectWin = rect;
    HWND containingWindowHandle = containingWindow();

    ::InvalidateRect(containingWindowHandle, &containingWindowRectWin, false);
      
    // Cache the dirty spot.
    addToDirtyRegion(rect);

    if (now)
        ::UpdateWindow(containingWindowHandle);
}

void ScrollView::update()
{
    ::UpdateWindow(containingWindow());
}

void ScrollView::setScrollPosition(const IntPoint& scrollPoint)
{
    IntPoint newScrollPosition = scrollPoint.shrunkTo(maximumScrollPosition());
    newScrollPosition.clampNegativeToZero();

    if (newScrollPosition == scrollPosition())
        return;

    updateScrollbars(IntSize(newScrollPosition.x(), newScrollPosition.y()));
}

void ScrollView::setFrameRect(const IntRect& newGeometry)
{
    IntRect oldGeometry = frameRect();
    Widget::setFrameRect(newGeometry);

    if (newGeometry == oldGeometry)
        return;

    if (newGeometry.width() != oldGeometry.width() || newGeometry.height() != oldGeometry.height()) {
        updateScrollbars(m_scrollOffset);
        static_cast<FrameView*>(this)->setNeedsLayout();
    }

    frameRectsChanged();
}

void ScrollView::updateScrollbars(const IntSize& desiredOffset)
{
    // Don't allow re-entrancy into this function.
    if (m_data->m_inUpdateScrollbars)
        return;

    // FIXME: This code is here so we don't have to fork FrameView.h/.cpp.
    // In the end, FrameView should just merge with ScrollView.
    if (static_cast<const FrameView*>(this)->frame()->prohibitsScrolling())
        return;

    m_data->m_inUpdateScrollbars = true;

    bool hasVerticalScrollbar = m_verticalScrollbar;
    bool hasHorizontalScrollbar = m_horizontalScrollbar;
    bool oldHasVertical = hasVerticalScrollbar;
    bool oldHasHorizontal = hasHorizontalScrollbar;
    ScrollbarMode hScroll = m_horizontalScrollbarMode;
    ScrollbarMode vScroll = m_verticalScrollbarMode;

    const int scrollbarThickness = ScrollbarTheme::nativeTheme()->scrollbarThickness();

    for (int pass = 0; pass < 2; pass++) {
        bool scrollsVertically;
        bool scrollsHorizontally;

        if (!m_scrollbarsSuppressed && (hScroll == ScrollbarAuto || vScroll == ScrollbarAuto)) {
            // Do a layout if pending before checking if scrollbars are needed.
            if (hasVerticalScrollbar != oldHasVertical || hasHorizontalScrollbar != oldHasHorizontal)
                static_cast<FrameView*>(this)->layout();

            scrollsVertically = (vScroll == ScrollbarAlwaysOn) || (vScroll == ScrollbarAuto && contentsHeight() > height());
            if (scrollsVertically)
                scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) || (hScroll == ScrollbarAuto && contentsWidth() + scrollbarThickness > width());
            else {
                scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) || (hScroll == ScrollbarAuto && contentsWidth() > width());
                if (scrollsHorizontally)
                    scrollsVertically = (vScroll == ScrollbarAlwaysOn) || (vScroll == ScrollbarAuto && contentsHeight() + scrollbarThickness > height());
            }
        }
        else {
            scrollsHorizontally = (hScroll == ScrollbarAuto) ? hasHorizontalScrollbar : (hScroll == ScrollbarAlwaysOn);
            scrollsVertically = (vScroll == ScrollbarAuto) ? hasVerticalScrollbar : (vScroll == ScrollbarAlwaysOn);
        }
        
        if (hasVerticalScrollbar != scrollsVertically) {
            m_data->setHasVerticalScrollbar(scrollsVertically);
            hasVerticalScrollbar = scrollsVertically;
        }

        if (hasHorizontalScrollbar != scrollsHorizontally) {
            m_data->setHasHorizontalScrollbar(scrollsHorizontally);
            hasHorizontalScrollbar = scrollsHorizontally;
        }
    }
    
    // Set up the range (and page step/line step).
    IntSize maxScrollPosition(contentsWidth() - visibleWidth(), contentsHeight() - visibleHeight());
    IntSize scroll = desiredOffset.shrunkTo(maxScrollPosition);
    scroll.clampNegativeToZero();
 
    if (m_horizontalScrollbar) {
        int clientWidth = visibleWidth();
        m_horizontalScrollbar->setEnabled(contentsWidth() > clientWidth);
        int pageStep = (clientWidth - cAmountToKeepWhenPaging);
        if (pageStep < 0) pageStep = clientWidth;
        IntRect oldRect(m_horizontalScrollbar->frameRect());
        IntRect hBarRect = IntRect(0,
                                   height() - m_horizontalScrollbar->height(),
                                   width() - (m_verticalScrollbar ? m_verticalScrollbar->width() : 0),
                                   m_horizontalScrollbar->height());
        m_horizontalScrollbar->setFrameRect(hBarRect);
        if (!m_scrollbarsSuppressed && oldRect != m_horizontalScrollbar->frameRect())
            m_horizontalScrollbar->invalidate();

        if (m_scrollbarsSuppressed)
            m_horizontalScrollbar->setSuppressInvalidation(true);
        m_horizontalScrollbar->setSteps(cScrollbarPixelsPerLineStep, pageStep);
        m_horizontalScrollbar->setProportion(clientWidth, contentsWidth());
        m_horizontalScrollbar->setValue(scroll.width());
        if (m_scrollbarsSuppressed)
            m_horizontalScrollbar->setSuppressInvalidation(false); 
    } 

    if (m_verticalScrollbar) {
        int clientHeight = visibleHeight();
        m_verticalScrollbar->setEnabled(contentsHeight() > clientHeight);
        int pageStep = (clientHeight - cAmountToKeepWhenPaging);
        if (pageStep < 0) pageStep = clientHeight;
        IntRect oldRect(m_verticalScrollbar->frameRect());
        IntRect vBarRect = IntRect(width() - m_verticalScrollbar->width(), 
                                   0,
                                   m_verticalScrollbar->width(),
                                   height() - (m_horizontalScrollbar ? m_horizontalScrollbar->height() : 0));
        m_verticalScrollbar->setFrameRect(vBarRect);
        if (!m_scrollbarsSuppressed && oldRect != m_verticalScrollbar->frameRect())
            m_verticalScrollbar->invalidate();

        if (m_scrollbarsSuppressed)
            m_verticalScrollbar->setSuppressInvalidation(true);
        m_verticalScrollbar->setSteps(cScrollbarPixelsPerLineStep, pageStep);
        m_verticalScrollbar->setProportion(clientHeight, contentsHeight());
        m_verticalScrollbar->setValue(scroll.height());
        if (m_scrollbarsSuppressed)
            m_verticalScrollbar->setSuppressInvalidation(false);
    }

    if (oldHasVertical != (m_verticalScrollbar != 0) || oldHasHorizontal != (m_horizontalScrollbar != 0))
        frameRectsChanged();

    // See if our offset has changed in a situation where we might not have scrollbars.
    // This can happen when editing a body with overflow:hidden and scrolling to reveal selection.
    // It can also happen when maximizing a window that has scrollbars (but the new maximized result
    // does not).
    IntSize scrollDelta = scroll - m_scrollOffset;
    if (scrollDelta != IntSize()) {
       m_scrollOffset = scroll;
       m_data->scrollBackingStore(scrollDelta);
    }

    m_data->m_inUpdateScrollbars = false;
}

void ScrollView::printPanScrollIcon(const IntPoint& iconPosition)
{
    m_data->m_drawPanScrollIcon = true;    
    m_data->m_panScrollIconPoint = IntPoint(iconPosition.x() - panIconSizeLength / 2 , iconPosition.y() - panIconSizeLength / 2) ;

    updateWindowRect(IntRect(m_data->m_panScrollIconPoint, IntSize(panIconSizeLength,panIconSizeLength)), true);    
}

void ScrollView::removePanScrollIcon()
{
    m_data->m_drawPanScrollIcon = false; 

    updateWindowRect(IntRect(m_data->m_panScrollIconPoint, IntSize(panIconSizeLength, panIconSizeLength)), true);
}

void ScrollView::paint(GraphicsContext* context, const IntRect& rect)
{
    // FIXME: This code is here so we don't have to fork FrameView.h/.cpp.
    // In the end, FrameView should just merge with ScrollView.
    ASSERT(isFrameView());

    if (context->paintingDisabled() && !context->updatingControlTints())
        return;

    IntRect documentDirtyRect = rect;
    documentDirtyRect.intersect(frameRect());

    context->save();

    context->translate(x(), y());
    documentDirtyRect.move(-x(), -y());

    context->translate(-scrollX(), -scrollY());
    documentDirtyRect.move(scrollX(), scrollY());

    context->clip(visibleContentRect());

    const FrameView* frameView = static_cast<const FrameView*>(this);
    frameView->frame()->paint(context, documentDirtyRect);

    context->restore();

    // Now paint the scrollbars.
    if (!m_scrollbarsSuppressed && (m_horizontalScrollbar || m_verticalScrollbar)) {
        context->save();
        IntRect scrollViewDirtyRect = rect;
        scrollViewDirtyRect.intersect(frameRect());
        context->translate(x(), y());
        scrollViewDirtyRect.move(-x(), -y());
        if (m_horizontalScrollbar)
            m_horizontalScrollbar->paint(context, scrollViewDirtyRect);
        if (m_verticalScrollbar)
            m_verticalScrollbar->paint(context, scrollViewDirtyRect);

        // Fill the scroll corner with white.
        IntRect hCorner;
        if (m_horizontalScrollbar && width() - m_horizontalScrollbar->width() > 0) {
            hCorner = IntRect(m_horizontalScrollbar->width(),
                              height() - m_horizontalScrollbar->height(),
                              width() - m_horizontalScrollbar->width(),
                              m_horizontalScrollbar->height());
            if (hCorner.intersects(scrollViewDirtyRect)) {
                Page* page = frameView->frame() ? frameView->frame()->page() : 0;
                if (page && page->settings()->shouldPaintCustomScrollbars()) {
                    if (!page->chrome()->client()->paintCustomScrollCorner(context, hCorner))
                        context->fillRect(hCorner, Color::white);
                }
            }
        }

        if (m_verticalScrollbar && height() - m_verticalScrollbar->height() > 0) {
            IntRect vCorner(width() - m_verticalScrollbar->width(),
                            m_verticalScrollbar->height(),
                            m_verticalScrollbar->width(),
                            height() - m_verticalScrollbar->height());
            if (vCorner != hCorner && vCorner.intersects(scrollViewDirtyRect)) {
                Page* page = frameView->frame() ? frameView->frame()->page() : 0;
                if (page && page->settings()->shouldPaintCustomScrollbars()) {
                    if (!page->chrome()->client()->paintCustomScrollCorner(context, vCorner))
                        context->fillRect(vCorner, Color::white);
                }
            }
        }

        context->restore();
    }

    //Paint the panScroll Icon
    static RefPtr<Image> panScrollIcon;
    if (m_data->m_drawPanScrollIcon) {
        if (!panScrollIcon)
            panScrollIcon = Image::loadPlatformResource("panIcon");
        context->drawImage(panScrollIcon.get(), m_data->m_panScrollIconPoint);
    }
}

void ScrollView::themeChanged()
{
    ScrollbarTheme::nativeTheme()->themeChanged();
    theme()->themeChanged();
    invalidate();
}

bool ScrollView::scroll(ScrollDirection direction, ScrollGranularity granularity)
{
    if (direction == ScrollUp || direction == ScrollDown) {
        if (m_verticalScrollbar)
            return m_verticalScrollbar->scroll(direction, granularity);
    } else {
        if (m_horizontalScrollbar)
            return m_horizontalScrollbar->scroll(direction, granularity);
    }
    return false;
}

void ScrollView::setParentVisible(bool visible)
{
    if (isParentVisible() == visible)
        return;
    
    Widget::setParentVisible(visible);

    if (isVisible()) {
        HashSet<Widget*>::iterator end = m_children.end();
        for (HashSet<Widget*>::iterator it = m_children.begin(); it != end; ++it)
            (*it)->setParentVisible(visible);
    }
}

void ScrollView::show()
{
    if (!isSelfVisible()) {
        setSelfVisible(true);
        if (isParentVisible()) {
            HashSet<Widget*>::iterator end = m_children.end();
            for (HashSet<Widget*>::iterator it = m_children.begin(); it != end; ++it)
                (*it)->setParentVisible(true);
        }
    }

    Widget::show();
}

void ScrollView::hide()
{
    if (isSelfVisible()) {
        if (isParentVisible()) {
            HashSet<Widget*>::iterator end = m_children.end();
            for (HashSet<Widget*>::iterator it = m_children.begin(); it != end; ++it)
                (*it)->setParentVisible(false);
        }
        setSelfVisible(false);
    }

    Widget::hide();
}

void ScrollView::addToDirtyRegion(const IntRect& containingWindowRect)
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->addToDirtyRegion(containingWindowRect);
}

void ScrollView::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->scrollBackingStore(dx, dy, scrollViewRect, clipRect);
}

void ScrollView::updateBackingStore()
{
    ASSERT(isFrameView());
    const FrameView* frameView = static_cast<const FrameView*>(this);
    Page* page = frameView->frame() ? frameView->frame()->page() : 0;
    if (!page)
        return;
    page->chrome()->updateBackingStore();
}

bool ScrollView::isOffscreen() const
{
    return false;
}

IntRect ScrollView::contentsToScreen(const IntRect& rect) const
{
    IntRect result(contentsToWindow(rect));
    POINT topLeft = {0, 0};

    // Find the top left corner of the Widget's containing window in screen coords,
    // and adjust the result rect's position by this amount.
    ::ClientToScreen(containingWindow(), &topLeft);
    result.move(topLeft.x, topLeft.y);

    return result;
}

IntPoint ScrollView::screenToContents(const IntPoint& point) const
{
    POINT result = point;

    ::ScreenToClient(containingWindow(), &result);

    return windowToContents(result);
}

} // namespace WebCore
