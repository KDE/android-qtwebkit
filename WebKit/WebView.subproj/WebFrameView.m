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

#import <Foundation/NSDictionary_NSURLExtras.h>
#import <Foundation/NSURLRequest.h>

enum {
    SpaceKey = 0x0020
};

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
                if (![self allowsScrolling]) {
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
