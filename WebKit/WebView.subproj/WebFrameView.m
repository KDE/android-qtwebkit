/*	WebFrameView.m
	Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebFrameView.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebClipView.h>
#import <WebKit/WebCookieAdapter.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageView.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebTextView.h>
#import <WebKit/WebViewFactory.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebAssertions.h>

#import <Foundation/NSDictionary_NSURLExtras.h>
#import <Foundation/NSURLRequest.h>

enum {
    SpaceKey = 0x0020
};

@implementation WebFrameViewPrivate

- init
{
    [super init];
    
    marginWidth = -1;
    marginHeight = -1;
    
    return self;
}

- (void)dealloc
{
    [frameScrollView release];
    [draggingTypes release];
    [super dealloc];
}

@end

@implementation WebFrameView (WebPrivate)

// Note that the WebVew is not retained.
- (WebView *)_webView
{
    return _private->webView;
}


- (void)_setMarginWidth: (int)w
{
    _private->marginWidth = w;
}

- (int)_marginWidth
{
    return _private->marginWidth;
}

- (void)_setMarginHeight: (int)h
{
    _private->marginHeight = h;
}

- (int)_marginHeight
{
    return _private->marginHeight;
}

- (void)_setDocumentView:(NSView *)view
{
    WebDynamicScrollBarsView *sv = (WebDynamicScrollBarsView *)[self _scrollView];
    
    [sv setSuppressLayout: YES];
    
    // Always start out with arrow.  New docView can then change as needed, but doesn't have to
    // clean up after the previous docView.  Also TextView will set cursor when added to view
    // tree, so must do this before setDocumentView:.
    [sv setDocumentCursor:[NSCursor arrowCursor]];

    [sv setDocumentView: view];
    [sv setSuppressLayout: NO];
}

-(NSView <WebDocumentView> *)_makeDocumentViewForDataSource:(WebDataSource *)dataSource
{
    NSString *MIMEType = [[dataSource response] MIMEType];
    
    Class viewClass = [[self class] _viewClassForMIMEType:MIMEType];
    NSView <WebDocumentView> *documentView = viewClass ? [[viewClass alloc] init] : nil;
    [self _setDocumentView:documentView];
    [documentView release];
    
    return documentView;
}

- (void)_setWebView:(WebView *)webView
{
    // Not retained because the WebView owns the WebFrame, which owns the WebFrameView.
    _private->webView = webView;    
}

- (NSScrollView *)_scrollView
{
    return _private->frameScrollView;
}


- (NSClipView *)_contentView
{
    return [[self _scrollView] contentView];
}

- (void)_scrollVerticallyBy: (float)delta
{
    NSPoint point = [[self _contentView] bounds].origin;
    point.y += delta;
    [[self _contentView] scrollPoint: point];
}

- (void)_scrollHorizontallyBy: (float)delta
{
    NSPoint point = [[self _contentView] bounds].origin;
    point.x += delta;
    [[self _contentView] scrollPoint: point];
}

- (float)_verticalKeyboardScrollAmount
{
    // verticalLineScroll is quite small, to make scrolling from the scroll bar
    // arrows relatively smooth. But this seemed too small for scrolling with
    // the arrow keys, so we bump up the number here. Cheating? Perhaps.
    return [[self _scrollView] verticalLineScroll] * 4;
}

- (float)_horizontalKeyboardScrollAmount
{
    // verticalLineScroll is quite small, to make scrolling from the scroll bar
    // arrows relatively smooth. But this seemed too small for scrolling with
    // the arrow keys, so we bump up the number here. Cheating? Perhaps.
    return [[self _scrollView] horizontalLineScroll] * 4;
}

- (void)_pageVertically:(BOOL)up
{
    float pageOverlap = [self _verticalKeyboardScrollAmount];
    float delta = [[self _contentView] bounds].size.height;
    
    delta = (delta < pageOverlap) ? delta / 2.0 : delta - pageOverlap;

    if (up) {
        delta = -delta;
    }

    [self _scrollVerticallyBy: delta];
}

- (void)_pageHorizontally: (BOOL)left
{
    float pageOverlap = [self _horizontalKeyboardScrollAmount];
    float delta = [[self _contentView] bounds].size.width;
    
    delta = (delta < pageOverlap) ? delta / 2.0 : delta - pageOverlap;

    if (left) {
        delta = -delta;
    }

    [self _scrollHorizontallyBy: delta];
}

- (void)_scrollLineVertically: (BOOL)up
{
    float delta = [self _verticalKeyboardScrollAmount];

    if (up) {
        delta = -delta;
    }

    [self _scrollVerticallyBy: delta];
}

- (void)_scrollLineHorizontally: (BOOL)left
{
    float delta = [self _horizontalKeyboardScrollAmount];

    if (left) {
        delta = -delta;
    }

    [self _scrollHorizontallyBy: delta];
}

- (void)scrollPageUp:(id)sender
{
    // After hitting the top, tell our parent to scroll
    float oldY = [[self _contentView] bounds].origin.y;
    [self _pageVertically:YES];
    if (oldY == [[self _contentView] bounds].origin.y) {
        [[self nextResponder] tryToPerform:@selector(scrollPageUp:) with:nil];
    }
}

- (void)scrollPageDown:(id)sender
{
    // After hitting the bottom, tell our parent to scroll
    float oldY = [[self _contentView] bounds].origin.y;
    [self _pageVertically:NO];
    if (oldY == [[self _contentView] bounds].origin.y) {
        [[self nextResponder] tryToPerform:@selector(scrollPageDown:) with:nil];
    }
}

- (void)_pageLeft
{
    [self _pageHorizontally:YES];
}

- (void)_pageRight
{
    [self _pageHorizontally:NO];
}

- (void)_scrollToTopLeft
{
    [[self _contentView] scrollPoint: NSMakePoint(0, 0)];
}

- (void)_scrollToBottomLeft
{
    [[self _contentView] scrollPoint: NSMakePoint(0, [[[self _scrollView] documentView] bounds].size.height)];
}

- (void)scrollLineUp:(id)sender
{
    [self _scrollLineVertically: YES];
}

- (void)scrollLineDown:(id)sender
{
    [self _scrollLineVertically: NO];
}

- (void)_lineLeft
{
    [self _scrollLineHorizontally: YES];
}

- (void)_lineRight
{
    [self _scrollLineHorizontally: NO];
}

static NSMutableDictionary *viewTypes;

+ (NSMutableDictionary *)_viewTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission
{
    static BOOL addedImageTypes;

    if (!viewTypes) {
        viewTypes = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
            [WebHTMLView class], @"text/html",
	    [WebHTMLView class], @"text/xml",
	    [WebHTMLView class], @"application/xml",
	    [WebHTMLView class], @"application/xhtml+xml",
            [WebTextView class], @"text/",
            [WebTextView class], @"application/x-javascript",
            nil];
    }

    if (!addedImageTypes && !allowImageTypeOmission) {
        NSEnumerator *enumerator = [[WebImageView supportedImageMIMETypes] objectEnumerator];
        ASSERT(enumerator != nil);
        NSString *mime;
        while ((mime = [enumerator nextObject]) != nil) {
            // Don't clobber previously-registered user image types
            if ([viewTypes objectForKey:mime] == nil) {
                [viewTypes setObject:[WebImageView class] forKey:mime];
            }
        }
        addedImageTypes = YES;
    }
    
    return viewTypes;
}

+ (BOOL)_canShowMIMETypeAsHTML:(NSString *)MIMEType
{
    return ([viewTypes objectForKey:MIMEType] == [WebHTMLView class]);
}


+ (Class)_viewClassForMIMEType:(NSString *)MIMEType
{
    // Getting the image types is slow, so don't do it until we have to.
    Class c = [[self _viewTypesAllowImageTypeOmission:YES] _web_objectForMIMEType:MIMEType];
    if (c == nil) {
        c = [[self _viewTypesAllowImageTypeOmission:NO] _web_objectForMIMEType:MIMEType];
    }
    return c;
}

- (void)_goBack
{
    [_private->webView goBack];
}

- (void)_goForward
{
    [_private->webView goForward];
}

- (BOOL)_isMainFrame
{
    return [_private->webView mainFrame] == [self webFrame];
}

- (void)_tile
{
    NSRect scrollViewFrame = [self bounds];
    // The border drawn by WebFrameView is 1 pixel on the left and right,
    // two pixels on top and bottom.  Shrink the scroll view to accomodate
    // the border.
    if ([self _shouldDrawBorder]){
        scrollViewFrame = NSInsetRect (scrollViewFrame, 1, 2);
    }
    [_private->frameScrollView setFrame:scrollViewFrame];
}

- (void)_drawBorder
{
    if ([self _shouldDrawBorder]){
        NSRect vRect = [self frame];
            
        // Left, black
        [[NSColor blackColor] set];
        NSRectFill(NSMakeRect(0,0,1,vRect.size.height));
        
        // Top, light gray, black
        [[NSColor lightGrayColor] set];
        NSRectFill(NSMakeRect(0,0,vRect.size.width,1));
        [[NSColor whiteColor] set];
        NSRectFill(NSMakeRect(1,1,vRect.size.width-2,1));
        
        // Right, light gray
        [[NSColor lightGrayColor] set];
        NSRectFill(NSMakeRect(vRect.size.width-1,1,1,vRect.size.height-2));
        
        // Bottom, light gray, white
        [[NSColor blackColor] set];
        NSRectFill(NSMakeRect(0,vRect.size.height-1,vRect.size.width,1));
        [[NSColor lightGrayColor] set];
        NSRectFill(NSMakeRect(1,vRect.size.height-2,vRect.size.width-2,1));
    }
}

- (BOOL)_shouldDrawBorder
{
    if (!_private->hasBorder)
        return NO;
        
    // Only draw a border for frames that request a border and the frame does
    // not contain a frameset.  Additionally we should (post-panther) not draw
    // a border (left, right, top or bottom) if the frame edge abutts the window frame.
    NSView *docV = [self documentView];
    if ([docV isKindOfClass:[WebHTMLView class]]){
        if ([[(WebHTMLView *)docV _bridge] isFrameSet]){
            return NO;
        }
    }
    return YES;
}

- (void)_setHasBorder:(BOOL)hasBorder
{
    if (_private->hasBorder == hasBorder) {
        return;
    }
    _private->hasBorder = hasBorder;
    [self _tile];
}

- (BOOL)_firstResponderIsControl
{
    return [[[self window] firstResponder] isKindOfClass:[NSControl class]];
}

@end

@implementation WebFrameView

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];
 
    [WebViewFactory createSharedFactory];
    [WebTextRendererFactory createSharedFactory];
    [WebImageRendererFactory createSharedFactory];
    [WebCookieAdapter createSharedAdapter];
    
    _private = [[WebFrameViewPrivate alloc] init];

    WebDynamicScrollBarsView *scrollView  = [[WebDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,frame.size.width,frame.size.height)];
    _private->frameScrollView = scrollView;
    [scrollView setContentView: [[[WebClipView alloc] initWithFrame:[scrollView bounds]] autorelease]];
    [scrollView setDrawsBackground: NO];
    [scrollView setHasVerticalScroller: NO];
    [scrollView setHasHorizontalScroller: NO];
    [scrollView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: scrollView];
    
    ++WebFrameViewCount;
    
    return self;
}

- (void)dealloc 
{
    --WebFrameViewCount;
    
    [_private release];
    _private = nil;
    
    [super dealloc];
}

- (WebFrame *)webFrame
{
    return [[self _webView] _frameForView: self]; 
}


- (void)setAllowsScrolling: (BOOL)flag
{
    [(WebDynamicScrollBarsView *)[self _scrollView] setAllowsScrolling: flag];
}

- (BOOL)allowsScrolling
{
    return [(WebDynamicScrollBarsView *)[self _scrollView] allowsScrolling];
}

- documentView
{
    return [[self _scrollView] documentView];
}

-(BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    if ([self documentView]) {
        [[self window] makeFirstResponder:[self documentView]];
    }
    return YES;
}

- (BOOL)isOpaque
{
    return YES;
}

- (void)drawRect:(NSRect)rect
{
    if ([self documentView] == nil) {
        // Need to paint ourselves if there's no documentView to do it instead.
        [[NSColor whiteColor] set];
        NSRectFill(rect);
    } else {
#ifndef NDEBUG
        if ([[self _scrollView] drawsBackground]) {
            [[NSColor cyanColor] set];
            NSRectFill(rect);
        }
#endif
    }
    
    [self _drawBorder];
}

- (void)setFrameSize:(NSSize)size
{
    if (!NSEqualSizes(size, [self frame].size)) {
        [[self _scrollView] setDrawsBackground:YES];
    }
    [super setFrameSize:size];
    [self _tile];
}

- (void)keyDown:(NSEvent *)event
{
    NSString *characters = [event characters];
    int index, count;
    BOOL callSuper = YES;
    BOOL maintainsBackForwardList = [[[self webFrame] webView] backForwardList] == nil ? NO : YES;

    count = [characters length];
    for (index = 0; index < count; ++index) {
        switch ([characters characterAtIndex:index]) {
            case NSDeleteCharacter:
                if (!maintainsBackForwardList) {
                    callSuper = YES;
                    break;
                }
                // This odd behavior matches some existing browsers,
                // including Windows IE
                if ([event modifierFlags] & NSShiftKeyMask) {
                    [self _goForward];
                } else {
                    [self _goBack];
                }
                callSuper = NO;
                break;
            case SpaceKey:
                // Checking for a control will allow events to percolate 
                // correctly when the focus is on a form control and we
                // are in full keyboard access mode.
                if (![self allowsScrolling] || [self _firstResponderIsControl]) {
                    callSuper = YES;
                    break;
                }
                if ([event modifierFlags] & NSShiftKeyMask) {
                    [self scrollPageUp:nil];
                } else {
                    [self scrollPageDown:nil];
                }
                callSuper = NO;
                break;
            case NSPageUpFunctionKey:
                if (![self allowsScrolling]) {
                    callSuper = YES;
                    break;
                }
                [self scrollPageUp:nil];
                callSuper = NO;
                break;
            case NSPageDownFunctionKey:
                if (![self allowsScrolling]) {
                    callSuper = YES;
                    break;
                }
                [self scrollPageDown:nil];
                callSuper = NO;
                break;
            case NSHomeFunctionKey:
                if (![self allowsScrolling]) {
                    callSuper = YES;
                    break;
                }
                [self _scrollToTopLeft];
                callSuper = NO;
                break;
            case NSEndFunctionKey:
                if (![self allowsScrolling]) {
                    callSuper = YES;
                    break;
                }
                [self _scrollToBottomLeft];
                callSuper = NO;
                break;
            case NSUpArrowFunctionKey:
                if (![self allowsScrolling]) {
                    callSuper = YES;
                    break;
                }
                if ([event modifierFlags] & NSCommandKeyMask) {
                    [self _scrollToTopLeft];
                } else if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self scrollPageUp:nil];
                } else {
                    [self scrollLineUp:nil];
                }
                callSuper = NO;
                break;
            case NSDownArrowFunctionKey:
                if (![self allowsScrolling]) {
                    callSuper = YES;
                    break;
                }
                if ([event modifierFlags] & NSCommandKeyMask) {
                    [self _scrollToBottomLeft];
                } else if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self scrollPageDown:nil];
                } else {
                    [self scrollLineDown:nil];
                }
                callSuper = NO;
                break;
            case NSLeftArrowFunctionKey:
                // Check back/forward related keys.
                if ([event modifierFlags] & NSCommandKeyMask) {
                    if (!maintainsBackForwardList) {
                        callSuper = YES;
                        break;
                    }
                    [self _goBack];
                } 
                
                // Now check scrolling related keys.
                if (![self allowsScrolling]) {
                    callSuper = YES;
                    break;
                }

                if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self _pageLeft];
                } else {
                    [self _lineLeft];
                }
                callSuper = NO;
                break;
            case NSRightArrowFunctionKey:
                // Check back/forward related keys.
                if ([event modifierFlags] & NSCommandKeyMask) {
                    if (!maintainsBackForwardList) {
                        callSuper = YES;
                        break;
                    }
                    [self _goForward];
                } 
                
                // Now check scrolling related keys.
                if (![self allowsScrolling]) {
                    callSuper = YES;
                    break;
                }

                if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self _pageRight];
                } else {
                    [self _lineRight];
                }
                callSuper = NO;
                break;
        }
    }
    
    if (callSuper) {
        [super keyDown:event];
    } else {
        // if we did something useful, get the cursor out of the way
        [NSCursor setHiddenUntilMouseMoves:YES];
    }
}

- (NSView *)nextKeyView
{
    if (_private != nil && _private->inNextValidKeyView) {
        WebFrame *webFrame = [self webFrame];
        WebView *webView = [[self webFrame] webView];
        if (webFrame == [webView mainFrame]) {
            return [webView nextKeyView];
        }
    }
    return [super nextKeyView];
}

- (NSView *)previousKeyView
{
    if (_private != nil && _private->inNextValidKeyView) {
        WebFrame *webFrame = [self webFrame];
        WebView *webView = [[self webFrame] webView];
        if (webFrame == [webView mainFrame]) {
            return [webView previousKeyView];
        }
    }
    return [super previousKeyView];
}

- (NSView *)nextValidKeyView
{
    _private->inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    _private->inNextValidKeyView = NO;
    return view;
}

- (NSView *)previousValidKeyView
{
    _private->inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    _private->inNextValidKeyView = NO;
    return view;
}

@end
