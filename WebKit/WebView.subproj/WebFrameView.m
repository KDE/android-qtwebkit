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
#import <WebKit/WebFrameViewInternal.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebGraphicsBridge.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageView.h>
#import <WebKit/WebKeyGenerator.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
// Assume we'll only ever compile this on Panther or greater, so 
// MAC_OS_X_VERSION_10_3 is guaranteed to be defined.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
#import <WebKit/WebPDFView.h>
#endif
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebTextView.h>
#import <WebKit/WebViewFactory.h>
#import <WebKit/WebViewInternal.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebAssertions.h>

#import <Foundation/NSDictionary_NSURLExtras.h>
#import <Foundation/NSURLRequest.h>

@interface NSClipView (AppKitSecretsIKnow)
- (BOOL)_scrollTo:(const NSPoint *)newOrigin; // need the boolean result from this method
@end

enum {
    SpaceKey = 0x0020
};

@interface WebFrameView (WebFrameViewFileInternal)
- (float)_verticalKeyboardScrollDistance;
- (void)_tile;
- (BOOL)_shouldDrawBorder;
@end

@interface WebFrameViewPrivate : NSObject
{
@public
    WebView *webView;
    WebDynamicScrollBarsView *frameScrollView;
    
    // These margin values are used to temporarily hold the margins of a frame until
    // we have the appropriate document view type.
    int marginWidth;
    int marginHeight;
    
    NSArray *draggingTypes;
    
    BOOL hasBorder;
}
@end

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

@implementation WebFrameView (WebFrameViewFileInternal)

- (float)_verticalKeyboardScrollDistance
{
    // verticalLineScroll is quite small, to make scrolling from the scroll bar
    // arrows relatively smooth. But this seemed too small for scrolling with
    // the arrow keys, so we bump up the number here. Cheating? Perhaps.
    return [[self _scrollView] verticalLineScroll] * 4;
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

- (void)_tile
{
    NSRect scrollViewFrame = [self bounds];
    // The border drawn by WebFrameView is 1 pixel on the left and right,
    // two pixels on top and bottom.  Shrink the scroll view to accomodate
    // the border.
    if ([self _shouldDrawBorder]) {
        scrollViewFrame = NSInsetRect (scrollViewFrame, 1, 2);
    }
    [_private->frameScrollView setFrame:scrollViewFrame];
}

@end

@implementation WebFrameView (WebInternal)

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
    Class viewClass = [[self class] _viewClassForMIMEType:[[dataSource response] MIMEType]];
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
    // this can be called by [super dealloc] when cleaning up the keyview loop,
    // after _private has been nilled out.
    if (_private == nil) {
        return nil;
    }
    return _private->frameScrollView;
}

- (NSClipView *)_contentView
{
    return [[self _scrollView] contentView];
}

- (float)_verticalPageScrollDistance
{
    float overlap = [self _verticalKeyboardScrollDistance];
    float height = [[self _contentView] bounds].size.height;
    return (height < overlap) ? height / 2 : height - overlap;
}

static NSMutableDictionary *viewTypes;

+ (NSMutableDictionary *)_viewTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission
{
    static BOOL addedImageTypes;

    if (!viewTypes) {
        viewTypes = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
            [WebHTMLView class], @"text/html",
	    [WebHTMLView class], @"text/xml",
	    [WebHTMLView class], @"text/xsl",
	    [WebHTMLView class], @"application/xml",
	    [WebHTMLView class], @"application/xhtml+xml",
            [WebHTMLView class], @"application/rss+xml",
            [WebHTMLView class], @"application/atom+xml",
            [WebHTMLView class], @"application/x-webarchive",
            [WebTextView class], @"text/",
            [WebTextView class], @"application/x-javascript",
// Assume we'll only ever compile this on Panther or greater, so 
// MAC_OS_X_VERSION_10_3 is guranateed to be defined.
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
            [WebPDFView class], @"application/pdf",
#endif
            nil];
    }

    if (!addedImageTypes && !allowImageTypeOmission) {
        NSEnumerator *enumerator = [[WebImageView supportedImageMIMETypes] objectEnumerator];
        ASSERT(enumerator != nil);
        NSString *mime;
        while ((mime = [enumerator nextObject]) != nil) {
            // Don't clobber previously-registered view classes.
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
    Class viewClass;
    return [WebView _viewClass:&viewClass andRepresentationClass:nil forMIMEType:MIMEType] ? viewClass : nil;
}

- (void)_setHasBorder:(BOOL)hasBorder
{
    if (_private->hasBorder == hasBorder) {
        return;
    }
    _private->hasBorder = hasBorder;
    [self _tile];
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
    [WebGraphicsBridge createSharedBridge];
    [WebKeyGenerator createSharedGenerator];
    
    _private = [[WebFrameViewPrivate alloc] init];

    WebDynamicScrollBarsView *scrollView  = [[WebDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,frame.size.width,frame.size.height)];
    _private->frameScrollView = scrollView;
    [scrollView setContentView: [[[WebClipView alloc] initWithFrame:[scrollView bounds]] autorelease]];
    [scrollView setDrawsBackground: NO];
    [scrollView setHasVerticalScroller: NO];
    [scrollView setHasHorizontalScroller: NO];
    [scrollView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: scrollView];
    // don't call our overridden version here; we need to make the standard NSView link between us
    // and our subview so that previousKeyView and previousValidKeyView work as expected. This works
    // together with our becomeFirstResponder and setNextKeyView overrides.
    [super setNextKeyView:scrollView];
    
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

- (void)finalize 
{
    --WebFrameViewCount;

    _private = nil;

    [super finalize];
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

- (BOOL)acceptsFirstResponder
{
    // We always accept first responder; this matches OS X 10.2 WebKit
    // behavior (see 3469791).
    return YES;
}

- (BOOL)becomeFirstResponder
{
    // This works together with setNextKeyView to splice the WebFrameView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebView has similar code.
    
    // If the scrollView won't accept first-responderness now, then we just become
    // the first responder ourself like a normal view. This lets us be the first 
    // responder in cases where no page has yet been loaded (see 3469791).
    if ([[self _scrollView] acceptsFirstResponder]) {
        NSWindow *window = [self window];
        if ([window keyViewSelectionDirection] == NSSelectingPrevious) {
            NSView *previousValidKeyView = [self previousValidKeyView];
            if ((previousValidKeyView != self) && (previousValidKeyView != [self _scrollView])) {
                [window makeFirstResponder:previousValidKeyView];
            }
        } else {
            [window makeFirstResponder:[self _scrollView]];
        }
    }    
    
    return YES;
}

- (void)setNextKeyView:(NSView *)aView
{
    // This works together with becomeFirstResponder to splice the WebFrameView into
    // the key loop similar to the way NSScrollView does this. Note that
    // WebView has very similar code.
    if ([self _scrollView] != nil) {
        [[self _scrollView] setNextKeyView:aView];
    } else {
        [super setNextKeyView:aView];
    }
}

- (BOOL)isOpaque
{
    return [[self _webView] drawsBackground];
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
    if (!NSEqualSizes(size, [self frame].size) && [[[self webFrame] webView] drawsBackground]) {
        [[self _scrollView] setDrawsBackground:YES];
    }
    [super setFrameSize:size];
    [self _tile];
}

- (void)scrollToBeginningOfDocument:(id)sender
{
    [[self _contentView] scrollPoint:[[[self _scrollView] documentView] frame].origin];
}

- (void)scrollToEndOfDocument:(id)sender
{
    NSRect frame = [[[self _scrollView] documentView] frame];
    [[self _contentView] scrollPoint:NSMakePoint(frame.origin.x, NSMaxY(frame))];
}

- (void)_goBack
{
    [_private->webView goBack];
}

- (void)_goForward
{
    [_private->webView goForward];
}

- (BOOL)_scrollVerticallyBy: (float)delta
{
    // This method uses the secret method _scrollTo on NSClipView.
    // It does that because it needs to know definitively whether scrolling was
    // done or not to help implement the "scroll parent if we are at the limit" feature.
    // In the presence of smooth scrolling, there's no easy way to tell if the method
    // did any scrolling or not with the public API.
    NSPoint point = [[self _contentView] bounds].origin;
    point.y += delta;
    return [[self _contentView] _scrollTo:&point];
}

- (void)_scrollHorizontallyBy: (float)delta
{
    NSPoint point = [[self _contentView] bounds].origin;
    point.x += delta;
    [[self _contentView] scrollPoint: point];
}

- (float)_horizontalKeyboardScrollDistance
{
    // horizontalLineScroll is quite small, to make scrolling from the scroll bar
    // arrows relatively smooth. But this seemed too small for scrolling with
    // the arrow keys, so we bump up the number here. Cheating? Perhaps.
    return [[self _scrollView] horizontalLineScroll] * 4;
}

- (float)_horizontalPageScrollDistance
{
    float overlap = [self _horizontalKeyboardScrollDistance];
    float width = [[self _contentView] bounds].size.width;
    return (width < overlap) ? width / 2 : width - overlap;
}

- (BOOL)_pageVertically:(BOOL)up
{
    float delta = [self _verticalPageScrollDistance];
    return [self _scrollVerticallyBy:up ? -delta : delta];
}

- (void)_pageHorizontally: (BOOL)left
{
    float delta = [self _horizontalPageScrollDistance];
    [self _scrollHorizontallyBy:left ? -delta : delta];
}

- (void)_scrollLineVertically: (BOOL)up
{
    float delta = [self _verticalKeyboardScrollDistance];
    [self _scrollVerticallyBy:up ? -delta : delta];
}

- (void)_scrollLineHorizontally: (BOOL)left
{
    float delta = [self _horizontalKeyboardScrollDistance];
    [self _scrollHorizontallyBy:left ? -delta : delta];
}

- (void)scrollPageUp:(id)sender
{
    if (![self _pageVertically:YES]) {
        // If we were already at the top, tell the next responder to scroll if it can.
        [[self nextResponder] tryToPerform:@selector(scrollPageUp:) with:sender];
    }
}

- (void)scrollPageDown:(id)sender
{
    if (![self _pageVertically:NO]) {
        // If we were already at the bottom, tell the next responder to scroll if it can.
        [[self nextResponder] tryToPerform:@selector(scrollPageDown:) with:sender];
    }
}

- (void)scrollLineUp:(id)sender
{
    [self _scrollLineVertically:YES];
}

- (void)scrollLineDown:(id)sender
{
    [self _scrollLineVertically:NO];
}

- (BOOL)_firstResponderIsControl
{
    return [[[self window] firstResponder] isKindOfClass:[NSControl class]];
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
                [self scrollToBeginningOfDocument:nil];
                callSuper = NO;
                break;
            case NSEndFunctionKey:
                if (![self allowsScrolling]) {
                    callSuper = YES;
                    break;
                }
                [self scrollToEndOfDocument:nil];
                callSuper = NO;
                break;
            case NSUpArrowFunctionKey:
                // We don't handle shifted or control-arrow keys here, so let super have a chance.
                if ([event modifierFlags] & (NSShiftKeyMask | NSControlKeyMask)) {
                    callSuper = YES;
                    break;
                }
                if (![self allowsScrolling] ||
                    [[[self window] firstResponder] isKindOfClass:[NSPopUpButton class]]) {
                    // Let arrow keys go through to pop up buttons
                    // <rdar://problem/3455910>: hitting up or down arrows when focus is on a 
                    // pop-up menu should pop the menu
                    callSuper = YES;
                    break;
                }
                if ([event modifierFlags] & NSCommandKeyMask) {
                    [self scrollToBeginningOfDocument:nil];
                } else if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self scrollPageUp:nil];
                } else {
                    [self scrollLineUp:nil];
                }
                callSuper = NO;
                break;
            case NSDownArrowFunctionKey:
                // We don't handle shifted or control-arrow keys here, so let super have a chance.
                if ([event modifierFlags] & (NSShiftKeyMask | NSControlKeyMask)) {
                    callSuper = YES;
                    break;
                }
                if (![self allowsScrolling] ||
                    [[[self window] firstResponder] isKindOfClass:[NSPopUpButton class]]) {
                    // Let arrow keys go through to pop up buttons
                    // <rdar://problem/3455910>: hitting up or down arrows when focus is on a 
                    // pop-up menu should pop the menu
                    callSuper = YES;
                    break;
                }
                if ([event modifierFlags] & NSCommandKeyMask) {
                    [self scrollToEndOfDocument:nil];
                } else if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self scrollPageDown:nil];
                } else {
                    [self scrollLineDown:nil];
                }
                callSuper = NO;
                break;
            case NSLeftArrowFunctionKey:
                // We don't handle shifted or control-arrow keys here, so let super have a chance.
                if ([event modifierFlags] & (NSShiftKeyMask | NSControlKeyMask)) {
                    callSuper = YES;
                    break;
                }
                // Check back/forward related keys.
                if ([event modifierFlags] & NSCommandKeyMask) {
                    if (!maintainsBackForwardList) {
                        callSuper = YES;
                        break;
                    }
                    [self _goBack];
                } else {
                    // Now check scrolling related keys.
                    if (![self allowsScrolling]) {
                        callSuper = YES;
                        break;
                    }

                    if ([event modifierFlags] & NSAlternateKeyMask) {
                        [self _pageHorizontally:YES];
                    } else {
                        [self _scrollLineHorizontally:YES];
                    }
                }
                callSuper = NO;
                break;
            case NSRightArrowFunctionKey:
                // We don't handle shifted or control-arrow keys here, so let super have a chance.
                if ([event modifierFlags] & (NSShiftKeyMask | NSControlKeyMask)) {
                    callSuper = YES;
                    break;
                }
                // Check back/forward related keys.
                if ([event modifierFlags] & NSCommandKeyMask) {
                    if (!maintainsBackForwardList) {
                        callSuper = YES;
                        break;
                    }
                    [self _goForward];
                } else {
                    // Now check scrolling related keys.
                    if (![self allowsScrolling]) {
                        callSuper = YES;
                        break;
                    }

                    if ([event modifierFlags] & NSAlternateKeyMask) {
                        [self _pageHorizontally:NO];
                    } else {
                        [self _scrollLineHorizontally:NO];
                    }
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

@end
