//
//  WebDynamicScrollBarsView.m
//  WebKit
//
//  Created by John Sullivan on Tue Jan 22 2002.
//  Copyright (c) 2001, 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebDynamicScrollBarsView.h>

#import <WebKit/WebDocument.h>

@implementation WebDynamicScrollBarsView

- (id)initWithFrame:(NSRect)frameRect
{
    [super initWithFrame:frameRect];

    hScroll = WebCoreScrollBarAuto;
    vScroll = WebCoreScrollBarAuto;

    return self;
}

- (void)setSuppressLayout: (BOOL)flag;
{
    suppressLayout = flag;
}

- (void)setScrollBarsSuppressed:(BOOL)suppressed repaintOnUnsuppress:(BOOL)repaint
{
    suppressScrollers = suppressed;
    if (suppressed || repaint) {
        [[self verticalScroller] setNeedsDisplay: !suppressed];
        [[self horizontalScroller] setNeedsDisplay: !suppressed];
    }
}

- (void)updateScrollers
{
    // We need to do the work below twice in the case where a scroll bar disappears,
    // making the second layout have a wider width than the first. Doing it more than
    // twice would indicate some kind of infinite loop, so we do it at most twice.
    // It's quite efficient to do this work twice in the normal case, so we don't bother
    // trying to figure out of the second pass is needed or not.
    
    int pass;
    BOOL hasVerticalScroller = [self hasVerticalScroller];
    BOOL hasHorizontalScroller = [self hasHorizontalScroller];
    BOOL oldHasVertical = hasVerticalScroller;
    BOOL oldHasHorizontal = hasHorizontalScroller;

    for (pass = 0; pass < 2; pass++) {
        BOOL scrollsVertically;
        BOOL scrollsHorizontally;

        if (!suppressLayout && !suppressScrollers &&
            (hScroll == WebCoreScrollBarAuto || vScroll == WebCoreScrollBarAuto))
        {
            // Do a layout if pending, before checking if scrollbars are needed.
            // This fixes 2969367, although may introduce a slowdown in live resize performance.
            NSView *documentView = [self documentView];
            if ((hasVerticalScroller != oldHasVertical ||
                hasHorizontalScroller != oldHasHorizontal || [documentView inLiveResize]) && [documentView conformsToProtocol:@protocol(WebDocumentView)]) {
                [(id <WebDocumentView>)documentView setNeedsLayout: YES];
                [(id <WebDocumentView>)documentView layout];
            }
            
            NSSize documentSize = [documentView frame].size;
            NSSize frameSize = [self frame].size;
            
            scrollsVertically = (vScroll == WebCoreScrollBarAlwaysOn) ||
                (vScroll == WebCoreScrollBarAuto && documentSize.height > frameSize.height);
            if (scrollsVertically)
                scrollsHorizontally = (hScroll == WebCoreScrollBarAlwaysOn) ||
                    (hScroll == WebCoreScrollBarAuto && documentSize.width + [NSScroller scrollerWidth] > frameSize.width);
            else {
                scrollsHorizontally = (hScroll == WebCoreScrollBarAlwaysOn) ||
                    (hScroll == WebCoreScrollBarAuto && documentSize.width > frameSize.width);
                if (scrollsHorizontally)
                    scrollsVertically = (vScroll == WebCoreScrollBarAlwaysOn) ||
                        (vScroll == WebCoreScrollBarAuto && documentSize.height + [NSScroller scrollerWidth] > frameSize.height);
            }
        }
        else {
            scrollsHorizontally = (hScroll == WebCoreScrollBarAuto) ? hasHorizontalScroller : (hScroll == WebCoreScrollBarAlwaysOn);
            scrollsVertically = (vScroll == WebCoreScrollBarAuto) ? hasVerticalScroller : (vScroll == WebCoreScrollBarAlwaysOn);
        }
        
        if (hasVerticalScroller != scrollsVertically) {
            [self setHasVerticalScroller:scrollsVertically];
            hasVerticalScroller = scrollsVertically;
        }

        if (hasHorizontalScroller != scrollsHorizontally) {
            [self setHasHorizontalScroller:scrollsHorizontally];
            hasHorizontalScroller = scrollsHorizontally;
        }
    }

    if (suppressScrollers) {
        [[self verticalScroller] setNeedsDisplay: NO];
        [[self horizontalScroller] setNeedsDisplay: NO];
    }
}

// Make the horizontal and vertical scroll bars come and go as needed.
- (void)reflectScrolledClipView:(NSClipView *)clipView
{
    if (clipView == [self contentView]) {
        // FIXME: This hack here prevents infinite recursion that takes place when we
        // gyrate between having a vertical scroller and not having one. A reproducible
        // case is clicking on the "the Policy Routing text" link at
        // http://www.linuxpowered.com/archive/howto/Net-HOWTO-8.html.
        // The underlying cause is some problem in the NSText machinery, but I was not
        // able to pin it down.
        static BOOL inUpdateScrollers;
        if (!inUpdateScrollers && [[NSGraphicsContext currentContext] isDrawingToScreen]) {
            inUpdateScrollers = YES;
            [self updateScrollers];
            inUpdateScrollers = NO;
        }
    }
    [super reflectScrolledClipView:clipView];

    // Validate the scrollers if they're being suppressed.
    if (suppressScrollers) {
        [[self verticalScroller] setNeedsDisplay: NO];
        [[self horizontalScroller] setNeedsDisplay: NO];
    }
}

- (void)setAllowsScrolling:(BOOL)flag
{
    hScroll = vScroll = (flag ? WebCoreScrollBarAuto : WebCoreScrollBarAlwaysOff);
    [self updateScrollers];
}

- (BOOL)allowsScrolling
{
    return hScroll != WebCoreScrollBarAlwaysOff && vScroll != WebCoreScrollBarAlwaysOff;
}

- (void)setAllowsHorizontalScrolling:(BOOL)flag
{
    hScroll = (flag ? WebCoreScrollBarAuto : WebCoreScrollBarAlwaysOff);
    [self updateScrollers];
}

- (void)setAllowsVerticalScrolling:(BOOL)flag
{
    vScroll = (flag ? WebCoreScrollBarAuto : WebCoreScrollBarAlwaysOff);
    [self updateScrollers];
}

- (BOOL)allowsHorizontalScrolling
{
    return hScroll != WebCoreScrollBarAlwaysOff;
}

- (BOOL)allowsVerticalScrolling
{
    return vScroll != WebCoreScrollBarAlwaysOff;
}

-(WebCoreScrollBarMode)horizontalScrollingMode
{
    return hScroll;
}

-(WebCoreScrollBarMode)verticalScrollingMode
{
    return vScroll;
}

- (void)setHorizontalScrollingMode:(WebCoreScrollBarMode)mode
{
    if (mode == hScroll)
        return;
    hScroll = mode;
    [self updateScrollers];
}

- (void)setVerticalScrollingMode:(WebCoreScrollBarMode)mode
{
    if (mode == vScroll)
        return;
    vScroll = mode;
    [self updateScrollers];
}

- (void)setScrollingMode:(WebCoreScrollBarMode)mode
{
    if (mode == vScroll && mode == hScroll)
        return;
    vScroll = hScroll = mode;
    [self updateScrollers];
}
@end
