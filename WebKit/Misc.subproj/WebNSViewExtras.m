/*
        WebNSViewExtras.m
        Copyright (c) 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNSViewExtras.h>

#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewInternal.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebNSImageExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSURLExtras.h>

#import <Foundation/NSString_NSURLExtras.h>
#import <Foundation/NSURL_NSURLExtras.h>
#import <Foundation/NSURLFileTypeMappings.h>

#define WebDragStartHysteresisX			5.0
#define WebDragStartHysteresisY			5.0
#define WebMaxDragImageSize 			NSMakeSize(400, 400)
#define WebMaxOriginalImageArea			(1500 * 1500)
#define WebDragIconRightInset			7.0
#define WebDragIconBottomInset			3.0

#ifdef DEBUG_VIEWS
@interface NSObject (Foo)
- (void*)_renderFramePart;
- (id)_frameForView: (id)aView;
@end
#endif

@implementation NSView (WebExtras)

- (NSView *)_web_superviewOfClass:(Class)class stoppingAtClass:(Class)limitClass
{
    NSView *view = self;
    while ((view = [view superview]) != nil) {
        if ([view isKindOfClass:class]) {
            return view;
        } else if (limitClass && [view isKindOfClass:limitClass]) {
            break;
        }
    }

    return nil;
}

- (NSView *)_web_superviewOfClass:(Class)class
{
    return [self _web_superviewOfClass:class stoppingAtClass:nil];
}

- (WebFrameView *)_web_parentWebFrameView
{
    WebFrameView *view = (WebFrameView *)[[[self superview] superview] superview];
    if ([view isKindOfClass: [WebFrameView class]])
        return view;
    return nil;
}

- (WebView *)_web_parentWebView
{
    return [[self _web_parentWebFrameView] _webView];
}

/* Determine whether a mouse down should turn into a drag; started as copy of NSTableView code */
- (BOOL)_web_dragShouldBeginFromMouseDown:(NSEvent *)mouseDownEvent
                           withExpiration:(NSDate *)expiration
                              xHysteresis:(float)xHysteresis
                              yHysteresis:(float)yHysteresis
{
    NSEvent *nextEvent, *firstEvent, *dragEvent, *mouseUp;
    BOOL dragIt;

    if ([mouseDownEvent type] != NSLeftMouseDown) {
        return NO;
    }

    nextEvent = nil;
    firstEvent = nil;
    dragEvent = nil;
    mouseUp = nil;
    dragIt = NO;

    while ((nextEvent = [[self window] nextEventMatchingMask:(NSLeftMouseUpMask | NSLeftMouseDraggedMask)
                                                   untilDate:expiration
                                                      inMode:NSEventTrackingRunLoopMode
                                                     dequeue:YES]) != nil) {
        if (firstEvent == nil) {
            firstEvent = nextEvent;
        }

        if ([nextEvent type] == NSLeftMouseDragged) {
            float deltax = ABS([nextEvent locationInWindow].x - [mouseDownEvent locationInWindow].x);
            float deltay = ABS([nextEvent locationInWindow].y - [mouseDownEvent locationInWindow].y);
            dragEvent = nextEvent;

            if (deltax >= xHysteresis) {
                dragIt = YES;
                break;
            }

            if (deltay >= yHysteresis) {
                dragIt = YES;
                break;
            }
        } else if ([nextEvent type] == NSLeftMouseUp) {
            mouseUp = nextEvent;
            break;
        }
    }

    // Since we've been dequeuing the events (If we don't, we'll never see the mouse up...),
    // we need to push some of the events back on.  It makes sense to put the first and last
    // drag events and the mouse up if there was one.
    if (mouseUp != nil) {
        [NSApp postEvent:mouseUp atStart:YES];
    }
    if (dragEvent != nil) {
        [NSApp postEvent:dragEvent atStart:YES];
    }
    if (firstEvent != mouseUp && firstEvent != dragEvent) {
        [NSApp postEvent:firstEvent atStart:YES];
    }

    return dragIt;
}

- (BOOL)_web_dragShouldBeginFromMouseDown:(NSEvent *)mouseDownEvent
                           withExpiration:(NSDate *)expiration
{
    return [self _web_dragShouldBeginFromMouseDown:mouseDownEvent
                                    withExpiration:expiration
                                       xHysteresis:WebDragStartHysteresisX
                                       yHysteresis:WebDragStartHysteresisY];
}


- (NSDragOperation)_web_dragOperationForDraggingInfo:(id <NSDraggingInfo>)sender
{
    if (![NSApp modalWindow] && 
        ![[self window] attachedSheet] &&
        [sender draggingSource] != self &&
        [[sender draggingPasteboard] _web_bestURL]) {
        return NSDragOperationCopy;
    } else {
        return NSDragOperationNone;
    }
}

#ifdef DEBUG_VIEWS
- (void)_web_printViewHierarchy: (int)level
{
    NSArray *subviews;
    int _level = level, i;
    NSRect f;
    NSView *subview;
    void *rfp = 0;
    
    subviews = [self subviews];
    _level = level;
    while (_level-- > 0)
        printf (" ");
    f = [self frame];
    
    if ([self respondsToSelector: @selector(_webView)]){
        id aWebView = [self _webView];
        id aFrame = [aWebView _frameForView: self];
        rfp = [aFrame _renderFramePart];
    }
    
    printf ("%s renderFramePart %p (%f,%f) w %f, h %f\n", [[[self class] className] cString], rfp, f.origin.x, f.origin.y, f.size.width, f.size.height);
    for (i = 0; i < (int)[subviews count]; i++){
        subview = [subviews objectAtIndex: i];
        [subview _web_printViewHierarchy: level + 1];
    }
}
#endif

- (void)_web_dragImage:(WebImageRenderer *)wir
                  rect:(NSRect)rect
                 event:(NSEvent *)event
            pasteboard:(NSPasteboard *)pasteboard 
                source:(id)source
                offset:(NSPoint *)dragImageOffset
{
    NSPoint mouseDownPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    NSImage *dragImage;
    NSPoint origin;
    NSImage *image;
    
    image = [wir image];
    if ([image size].height * [image size].width <= WebMaxOriginalImageArea) {
        NSSize originalSize = rect.size;
        origin = rect.origin;
        
        dragImage = [[image copy] autorelease];
        [dragImage setScalesWhenResized:YES];
        [dragImage setSize:originalSize];
        
        [dragImage _web_scaleToMaxSize:WebMaxDragImageSize];
        NSSize newSize = [dragImage size];
        
        [dragImage _web_dissolveToFraction:WebDragImageAlpha];
        
        // Properly orient the drag image and orient it differently if it's smaller than the original
        origin.x = mouseDownPoint.x - (((mouseDownPoint.x - origin.x) / originalSize.width) * newSize.width);
        origin.y = origin.y + originalSize.height;
        origin.y = mouseDownPoint.y - (((mouseDownPoint.y - origin.y) / originalSize.height) * newSize.height);
    } else {
        NSString *extension = [[NSURLFileTypeMappings sharedMappings] preferredExtensionForMIMEType:[wir MIMEType]];
        if (extension == nil) {
            extension = @"";
        }
        dragImage = [[NSWorkspace sharedWorkspace] iconForFileType:extension];
        NSSize offset = NSMakeSize([dragImage size].width - WebDragIconRightInset, -WebDragIconBottomInset);
        origin = NSMakePoint(mouseDownPoint.x - offset.width, mouseDownPoint.y - offset.height);
    }

    // This is the offset from the lower left corner of the image to the mouse location.  Because we
    // are a flipped view the calculation of Y is inverted.
    if (dragImageOffset) {
        dragImageOffset->x = mouseDownPoint.x - origin.x;
        dragImageOffset->y = origin.y - mouseDownPoint.y;
    }
    
    // Per kwebster, offset arg is ignored
    [self dragImage:dragImage at:origin offset:NSZeroSize event:event pasteboard:pasteboard source:source slideBack:YES];
}

- (BOOL)_web_firstResponderIsSelfOrDescendantView
{
    NSResponder *responder = [[self window] firstResponder];
    return (responder && 
           (responder == self || 
           ([responder isKindOfClass:[NSView class]] && [(NSView *)responder isDescendantOf:self])));
}

- (BOOL)_web_firstResponderCausesFocusDisplay
{
    return [self _web_firstResponderIsSelfOrDescendantView] || [[self window] firstResponder] == [self _web_parentWebFrameView];
}

@end

@implementation NSView (WebDocumentViewExtras)

- (WebView *)_webView
{
    // We used to use the view hierarchy exclusively here, but that won't work
    // right when the first viewDidMoveToSuperview call is done, and this wil.
    return [[self _frame] webView];
}

- (WebFrame *)_frame
{
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    return [webFrameView webFrame];
}

// Required so view can access the part's selection.
- (WebBridge *)_bridge
{
    return [[self _frame] _bridge];
}

- (WebDataSource *)_dataSource
{
    return [[self _frame] dataSource];
}

@end
