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

#import "KWQScrollView.h"

#import "KWQExceptions.h"
#import "KWQLogging.h"
#import "KWQNSViewExtras.h"
#import "WebCoreFrameView.h"

/*
    This class implementation does NOT actually emulate the Qt QScrollView.
    It does provide an implementation that khtml will use to interact with
    WebKit's WebFrameView documentView and our NSScrollView subclass.

    QScrollView's view is a NSScrollView (or subclass of NSScrollView)
    in most cases. That scrollview is a subview of an
    WebCoreFrameView. The WebCoreFrameView's documentView will also be
    the scroll view's documentView.
    
    The WebCoreFrameView's size is the frame size.  The WebCoreFrameView's documentView
    corresponds to the frame content size.  The scrollview itself is autosized to the
    WebCoreFrameView's size (see QWidget::resize).
*/

@interface NSView (KWQExtensions)
- (BOOL)_KWQ_isScrollView;
@end

@implementation NSView (KWQExtensions)

- (BOOL)_KWQ_isScrollView
{
    return [self isKindOfClass:[NSScrollView class]];
}

@end

QWidget* QScrollView::viewport() const
{
    return const_cast<QScrollView *>(this);
}

int QScrollView::visibleWidth() const
{
    NSScrollView *view = (NSScrollView *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) {
        return (int)[view documentVisibleRect].size.width;
    } else {
        return (int)[view bounds].size.width;
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

int QScrollView::visibleHeight() const
{
    NSScrollView *view = (NSScrollView *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) {
        return (int)[view documentVisibleRect].size.height;
    } else {
        return (int)[view bounds].size.height;
    }
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return 0;
}

int QScrollView::contentsWidth() const
{
    NSView *docView, *view = getView();
    docView = getDocumentView();

    KWQ_BLOCK_EXCEPTIONS;
    if (docView) {
        return (int)[docView bounds].size.width;
    } else {
	return (int)[view bounds].size.width;
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

int QScrollView::contentsHeight() const
{
    NSView *docView, *view = getView();
    docView = getDocumentView();

    KWQ_BLOCK_EXCEPTIONS;
    if (docView) {
        return (int)[docView bounds].size.height;
    } else {
	return (int)[view bounds].size.height;
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

int QScrollView::contentsX() const
{
    NSView *view = getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) {
        NSScrollView *sview = view;
        return (int)[sview documentVisibleRect].origin.x;
    } else {
        return (int)[view visibleRect].origin.x;
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

int QScrollView::contentsY() const
{
    NSView *view = getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view _KWQ_isScrollView]) {
        NSScrollView *sview = view;
        return (int)[sview documentVisibleRect].origin.y;
    } else {
        return (int)[view visibleRect].origin.y;
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

int QScrollView::childX(QWidget* w)
{
    return w->x();
}

int QScrollView::childY(QWidget* w)
{
    return w->y();
}

void QScrollView::scrollBy(int dx, int dy)
{
    setContentsPos(contentsX() + dx, contentsY() + dy);
}

void QScrollView::setContentsPos(int x, int y)
{
    x = (x < 0) ? 0 : x;
    y = (y < 0) ? 0 : y;
    NSPoint p =  NSMakePoint(x,y);

    KWQ_BLOCK_EXCEPTIONS;
    NSView *docView;
    NSView *view = getView();    
    docView = getDocumentView();
    if (docView)
        view = docView;
        
    [view scrollPoint:p];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::setVScrollBarMode(ScrollBarMode vMode)
{
    NSView* view = getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setVerticalScrollingMode: (WebCoreScrollBarMode)vMode];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::setHScrollBarMode(ScrollBarMode hMode)
{
    NSView* view = getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setHorizontalScrollingMode: (WebCoreScrollBarMode)hMode];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::setScrollBarsMode(ScrollBarMode mode)
{
    NSView* view = getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setScrollingMode: (WebCoreScrollBarMode)mode];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

QScrollView::ScrollBarMode
QScrollView::vScrollBarMode() const
{
    NSView* view = getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        return (ScrollBarMode)[frameView verticalScrollingMode];
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return Auto;
}

QScrollView::ScrollBarMode
QScrollView::hScrollBarMode() const
{
    NSView* view = getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        return (ScrollBarMode)[frameView horizontalScrollingMode];
    }
    KWQ_UNBLOCK_EXCEPTIONS;

    return Auto;
}

bool QScrollView::hasVerticalScrollBar() const
{
    NSScrollView *view = (NSScrollView *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        return  [view hasVerticalScroller];
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

bool QScrollView::hasHorizontalScrollBar() const
{
    NSScrollView *view = (NSScrollView *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        return [view hasHorizontalScroller];
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void QScrollView::suppressScrollBars(bool suppressed,  bool repaintOnUnsuppress)
{
    NSView* view = getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view conformsToProtocol:@protocol(WebCoreFrameView)]) {
        NSView<WebCoreFrameView>* frameView = (NSView<WebCoreFrameView>*)view;
        [frameView setScrollBarsSuppressed: suppressed
                       repaintOnUnsuppress: repaintOnUnsuppress];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::addChild(QWidget* child, int x, int y)
{
    ASSERT(child != this);
    
    child->move(x, y);

    KWQ_BLOCK_EXCEPTIONS;

    NSView * thisView;
    NSView *thisDocView, *subview;
    
    thisView = getView();
    thisDocView = getDocumentView();
    if (thisDocView)
        thisView = thisDocView;

    subview = child->getOuterView();
    ASSERT(subview != thisView);

    if ([subview superview] != thisView) {
	[subview removeFromSuperview];
	
	LOG(Frames, "Adding %p %@ at (%d,%d) w %d h %d\n", subview,
	    [(id)[subview class] className], x, y, (int)[subview frame].size.width, (int)[subview frame].size.height);
	
	[thisView addSubview:subview];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::removeChild(QWidget* child)
{
    KWQ_BLOCK_EXCEPTIONS;
    [child->getOuterView() removeFromSuperview];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::resizeContents(int w, int h)
{
    KWQ_BLOCK_EXCEPTIONS;
    int _w = w;
    int _h = h;

    LOG(Frames, "%p %@ at w %d h %d\n", getView(), [(id)[getView() class] className], w, h);
    NSView *view = getView();
    if ([view _KWQ_isScrollView]){
        view = getDocumentView();
        
        LOG(Frames, "%p %@ at w %d h %d\n", view, [(id)[view class] className], w, h);
        if (_w < 0)
            _w = 0;
        if (_h < 0)
            _h = 0;

        [view setFrameSize: NSMakeSize (_w,_h)];
    } else {
        resize (_w, _h);
    }
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::updateContents(int x, int y, int w, int h, bool now)
{
    updateContents(QRect(x, y, w, h), now);
}

void QScrollView::updateContents(const QRect &rect, bool now)
{
    KWQ_BLOCK_EXCEPTIONS;
    NSView * view = getView();

    if ([view _KWQ_isScrollView])
        view = getDocumentView();

    if (now)
        [view displayRect: rect];
    else
        [view setNeedsDisplayInRect:rect];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::repaintContents(int x, int y, int w, int h, bool erase)
{
    LOG(Frames, "%p %@ at (%d,%d) w %d h %d\n", getView(), [(id)[getView() class] className], x, y, w, h);
}

QPoint QScrollView::contentsToViewport(const QPoint &p)
{
    int vx, vy;
    contentsToViewport(p.x(), p.y(), vx, vy);
    return QPoint(vx, vy);
}

void QScrollView::contentsToViewport(int x, int y, int& vx, int& vy)
{
    KWQ_BLOCK_EXCEPTIONS;

    NSView *docView;
    NSView *view = getView();    
     
    docView = getDocumentView();
    if (docView)
        view = docView;
    
    NSPoint np = [view convertPoint: NSMakePoint (x, y) toView: nil];
    vx = (int)np.x;
    vy = (int)np.y;
    
    return;

    KWQ_UNBLOCK_EXCEPTIONS;
    
    vx = 0;
    vy = 0;
}

void QScrollView::viewportToContents(int vx, int vy, int& x, int& y)
{
    KWQ_BLOCK_EXCEPTIONS;

    NSView *docView;
    NSView *view = getView();    

    docView = getDocumentView();
    if (docView)
        view = docView;
        
    NSPoint np = [view convertPoint: NSMakePoint (vx, vy) fromView: nil];
    x = (int)np.x;
    y = (int)np.y;

    return;

    KWQ_UNBLOCK_EXCEPTIONS;

    x = 0;
    y = 0;
}

void QScrollView::setStaticBackground(bool b)
{
    NSScrollView *view = (NSScrollView *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    if ([view _KWQ_isScrollView])
        [[view contentView] setCopiesOnScroll: !b];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::resizeEvent(QResizeEvent *)
{
}

void QScrollView::setContentsPosRecursive(int x, int y)
{
    KWQ_BLOCK_EXCEPTIONS;
    [getDocumentView() _KWQ_scrollPointRecursive:NSMakePoint(x, y)];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::ensureVisible(int x, int y)
{
    // Note that the definition of ensureVisible in trolltech documentation says:
    // "Scrolls the content so that the point (x, y) is visible with at least 
    // 50-pixel margins (if possible, otherwise centered).", which is
    // not what we're doing here.
    KWQ_BLOCK_EXCEPTIONS;
    [getDocumentView() scrollRectToVisible:NSMakeRect(x, y, 0, 0)];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::ensureVisible(int x, int y, int xmargin, int ymargin)
{
    // Note that the definition of ensureVisible in trolltech documentation says:
    // "Scrolls the content so that the point (x, y) is visible with at least the 
    // xmargin and ymargin margins (if possible, otherwise centered).", which is
    // not what we're doing here.
    KWQ_BLOCK_EXCEPTIONS;
    [getDocumentView() scrollRectToVisible:NSMakeRect(x, y, xmargin, ymargin)];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QScrollView::ensureRectVisibleCentered(const QRect &rect)
{
    KWQ_BLOCK_EXCEPTIONS;
    [getDocumentView() _KWQ_scrollRectToVisible:NSMakeRect(rect.x(), rect.y(), rect.width(), rect.height())];
    KWQ_UNBLOCK_EXCEPTIONS;
}

NSView *QScrollView::getDocumentView() const
{
    id view = getView();

    KWQ_BLOCK_EXCEPTIONS;
    if ([view respondsToSelector:@selector(documentView)]) 
	return [view documentView];
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return nil;
}
