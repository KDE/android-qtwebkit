/*
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef ScrollView_h
#define ScrollView_h

#include "IntRect.h"
#include "ScrollTypes.h"
#include "Widget.h"

#include <wtf/HashSet.h>

#if PLATFORM(MAC) && defined __OBJC__
@protocol WebCoreFrameScrollView;
#endif

#if PLATFORM(GTK)
typedef struct _GtkAdjustment GtkAdjustment;
#endif

#if PLATFORM(WIN)
typedef struct HRGN__* HRGN;
#endif

#if PLATFORM(WX)
class wxScrollWinEvent;
#endif

// DANGER WILL ROBINSON! THIS FILE IS UNDERGOING HEAVY REFACTORING.
// Everything is changing!
// Port authors should wait until this refactoring is complete before attempting to implement this interface.
namespace WebCore {

class PlatformWheelEvent;
class Scrollbar;

class ScrollView : public Widget {
public:
    ScrollView();
    ~ScrollView();

    // Methods for child manipulation and inspection.
    const HashSet<Widget*>* children() const { return &m_children; }
    void addChild(Widget*);
    void removeChild(Widget*);

    // If the scroll view does not use a native widget, then it will have cross-platform Scrollbars.  These methods
    // can be used to obtain those scrollbars.
    Scrollbar* horizontalScrollbar() const;
    Scrollbar* verticalScrollbar() const;

    // Whether or not a scroll view will blit visible contents when it is scrolled.  Blitting is disabled in situations
    // where it would cause rendering glitches (such as with fixed backgrounds or when the view is partially transparent).
    void setCanBlitOnScroll(bool);
    bool canBlitOnScroll() const { return m_canBlitOnScroll; }

    // The visible content rect has a location that is the scrolled offset of the document. The width and height are the viewport width
    // and height.  By default the scrollbars themselves are excluded from this rectangle, but an optional boolean argument allows them to be
    // included.
    IntRect visibleContentRect(bool includeScrollbars = false) const;
    int visibleWidth() const { return visibleContentRect().width(); }
    int visibleHeight() const { return visibleContentRect().height(); }
    
    // Methods for getting/setting the size of the document contained inside the ScrollView (as an IntSize or as individual width and height
    // values).
    IntSize contentsSize() const;
    int contentsWidth() const { return contentsSize().width(); }
    int contentsHeight() const { return contentsSize().height(); }
    void setContentsSize(const IntSize&);
   
    // Methods for querying the current scrolled position (both as a point, a size, or as individual X and Y values).
    IntPoint scrollPosition() const { return visibleContentRect().location(); }
    IntSize scrollOffset() const { return visibleContentRect().location() - IntPoint(); } // Gets the scrolled position as an IntSize. Convenient for adding to other sizes.
    IntPoint maximumScrollPosition() const; // The maximum position we can be scrolled to.
    int scrollX() const { return scrollPosition().x(); }
    int scrollY() const { return scrollPosition().y(); }
    
    // Methods for scrolling the view.  setScrollPosition is the only method that really scrolls the view.  The other two methods are helper functions
    // that ultimately end up calling setScrollPosition.
    void setScrollPosition(const IntPoint&);
    void scrollBy(const IntSize& s) { return setScrollPosition(scrollPosition() + s); }
    void scrollRectIntoViewRecursively(const IntRect&);

    virtual void setVScrollbarMode(ScrollbarMode);
    virtual void setHScrollbarMode(ScrollbarMode);

    // Set the mode for both scrollbars at once.
    virtual void setScrollbarsMode(ScrollbarMode);

    // This gives us a means of blocking painting on our scrollbars until the first layout has occurred.
    void suppressScrollbars(bool suppressed, bool repaintOnUnsuppress = false);

    ScrollbarMode vScrollbarMode() const;
    ScrollbarMode hScrollbarMode() const;

    bool isScrollable();
     
    // Event coordinates are assumed to be in the coordinate space of a window that contains
    // the entire widget hierarchy. It is up to the platform to decide what the precise definition
    // of containing window is. (For example on Mac it is the containing NSWindow.)
    IntPoint windowToContents(const IntPoint&) const;
    IntPoint contentsToWindow(const IntPoint&) const;
    IntRect windowToContents(const IntRect&) const;
    IntRect contentsToWindow(const IntRect&) const;

    bool inWindow() const;
    virtual bool shouldUpdateWhileOffscreen() const = 0;

    // For platforms that need to hit test scrollbars from within the engine's event handlers (like Win32).
    Scrollbar* scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent);

    // This method exists for scrollviews that need to handle wheel events manually.
    // On Mac the underlying NSScrollView just does the scrolling, but on other platforms
    // (like Windows), we need this method in order to do the scroll ourselves.
    void wheelEvent(PlatformWheelEvent&);

    bool scroll(ScrollDirection, ScrollGranularity);
    
    IntPoint convertChildToSelf(const Widget* child, const IntPoint& point) const
    {
        IntPoint newPoint = point;
        if (!isScrollViewScrollbar(child))
            newPoint = point - scrollOffset();
        newPoint.move(child->x(), child->y());
        return newPoint;
    }

    IntPoint convertSelfToChild(const Widget* child, const IntPoint& point) const
    {
        IntPoint newPoint = point;
        if (!isScrollViewScrollbar(child))
            newPoint = point + scrollOffset();
        newPoint.move(-child->x(), -child->y());
        return newPoint;
    }
    
    bool isScrollViewScrollbar(const Widget*) const;

#if HAVE(ACCESSIBILITY)
    IntRect contentsToScreen(const IntRect&) const;
    IntPoint screenToContents(const IntPoint&) const;
#endif

protected:
    void updateContents(const IntRect&, bool now = false);
    void updateWindowRect(const IntRect&, bool now = false);
public:
    void update();

private:
    HashSet<Widget*> m_children;
    bool m_canBlitOnScroll;
    IntSize m_scrollOffset; // FIXME: Would rather store this as a position, but we will wait to make this change until more code is shared.
    IntSize m_contentsSize;

    void init();
    
    void platformAddChild(Widget*);
    void platformRemoveChild(Widget*);
    void platformSetCanBlitOnScroll();
    IntRect platformVisibleContentRect(bool includeScrollbars) const;
    IntSize platformContentsSize() const;
    void platformSetContentsSize();

#if PLATFORM(MAC) && defined __OBJC__
public:
    NSView* documentView() const;

private:
    NSScrollView<WebCoreFrameScrollView>* scrollView() const;
#endif

#if !PLATFORM(MAC)
private:
    IntSize maximumScroll() const;
#endif

    class ScrollViewPrivate;
    ScrollViewPrivate* m_data;

    friend class ScrollViewPrivate; // FIXME: Temporary.
    
#if !PLATFORM(MAC) && !PLATFORM(WX)
public:
    virtual void paint(GraphicsContext*, const IntRect&);

    virtual void geometryChanged() const;
    virtual void setFrameGeometry(const IntRect&);

    void addToDirtyRegion(const IntRect&);
    void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect);
    void updateBackingStore();

private:
    void updateScrollbars(const IntSize& desiredOffset);
#else
    void updateScrollbars(const IntSize& desiredOffset) {} // FIXME: Temporary.
#endif

#if PLATFORM(WIN) || PLATFORM(QT) || PLATFORM(MAC)
public:
    IntRect windowResizerRect();
    bool resizerOverlapsContent() const;
    void adjustOverlappingScrollbarCount(int overlapDelta);

private:
    virtual void setParent(ScrollView*);
#else
public:
    IntRect windowResizerRect() { return IntRect(); }
    bool resizerOverlapsContent() const { return false; }
    void adjustOverlappingScrollbarCount(int overlapDelta) {}
#endif

#if PLATFORM(WIN)
public:
    virtual void themeChanged();

    void printPanScrollIcon(const IntPoint&);
    void removePanScrollIcon();

    virtual void show();
    virtual void hide();
    virtual void setParentVisible(bool);

    void setAllowsScrolling(bool);
    bool allowsScrolling() const;
#endif

#if PLATFORM(QT)
private:
    void incrementNativeWidgetCount();
    void decrementNativeWidgetCount();
    bool hasNativeWidgets() const;
    void invalidateScrollbars();
#endif

#if PLATFORM(GTK)
public:
    void setGtkAdjustments(GtkAdjustment* hadj, GtkAdjustment* vadj);
#endif

#if PLATFORM(WX)
public:
    virtual void setPlatformWidget(wxWindow*);

private:
    void adjustScrollbars(int x = -1, int y = -1, bool refresh = true);
#endif

}; // class ScrollView

#if !PLATFORM(MAC)

// On Mac only, because of flipped NSWindow y-coordinates, we have to have a special implementation.
// Other platforms can just implement these helper methods using the corresponding point conversion methods.

inline IntRect ScrollView::contentsToWindow(const IntRect& rect) const
{
    return IntRect(contentsToWindow(rect.location()), rect.size());
}

inline IntRect ScrollView::windowToContents(const IntRect& rect) const
{
    return IntRect(windowToContents(rect.location()), rect.size());
}

#endif

} // namespace WebCore

#endif // ScrollView_h
