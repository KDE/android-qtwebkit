/*	
    WebHTMLView.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHTMLView.h>

#import <WebKit/DOM.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebClipView.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDocumentInternal.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebHTMLRepresentationPrivate.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import "WebNSPrintOperationExtras.h"
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebTextRenderer.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebUIDelegatePrivate.h>
#import <WebKit/WebUnicode.h>
#import <WebKit/WebViewPrivate.h>

#import <AppKit/NSGraphicsContextPrivate.h>
#import <AppKit/NSResponder_Private.h>
#import <CoreGraphics/CGContextGState.h>

#import <AppKit/NSAccessibility.h>

#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNSEventExtras.h>
#import <WebKit/WebNSImageExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebStringTruncator.h>

#import <Foundation/NSFileManager_NSURLExtras.h>
#import <Foundation/NSURL_NSURLExtras.h>

// The link drag hysteresis is much larger than the others because there
// needs to be enough space to cancel the link press without starting a link drag,
// and because dragging links is rare.
#define LinkDragHysteresis              40.0
#define ImageDragHysteresis  		5.0
#define TextDragHysteresis  		3.0
#define TextDragDelay			0.15

// By imaging to a width a little wider than the available pixels,
// thin pages will be scaled down a little, matching the way they
// print in IE and Camino. This lets them use fewer sheets than they
// would otherwise, which is presumably why other browsers do this.
// Wide pages will be scaled down more than this.
#define PrintingMinimumShrinkFactor     1.25

// This number determines how small we are willing to reduce the page content
// in order to accommodate the widest line. If the page would have to be
// reduced smaller to make the widest line fit, we just clip instead (this
// behavior matches MacIE and Mozilla, at least)
#define PrintingMaximumShrinkFactor     2.0

#define AUTOSCROLL_INTERVAL             0.1

#define DRAG_LABEL_BORDER_X		4.0
#define DRAG_LABEL_BORDER_Y		2.0
#define DRAG_LABEL_RADIUS		5.0
#define DRAG_LABEL_BORDER_Y_OFFSET              2.0

#define MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP	120.0
#define MAX_DRAG_LABEL_WIDTH                    320.0

#define DRAG_LINK_LABEL_FONT_SIZE   11.0
#define DRAG_LINK_URL_FONT_SIZE   10.0

static BOOL forceRealHitTest = NO;

@interface WebHTMLView (WebHTMLViewPrivate)
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize;
- (void)_updateTextSizeMultiplier;
- (float)_calculatePrintHeight;
- (float)_scaleFactorForPrintOperation:(NSPrintOperation *)printOperation;
@end

// Any non-zero value will do, but using somethign recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE


@interface NSView (AppKitSecretsIKnowAbout)
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect;
- (NSRect)_dirtyRect;
- (void)_setDrawsOwnDescendants:(BOOL)drawsOwnDescendants;
@end

@interface NSView (WebHTMLViewPrivate)
- (void)_web_setPrintingModeRecursive;
- (void)_web_clearPrintingModeRecursive;
- (void)_web_layoutIfNeededRecursive:(NSRect)rect testDirtyRect:(bool)testDirtyRect;
@end

@interface NSMutableDictionary (WebHTMLViewPrivate)
- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key;
@end

@implementation WebHTMLViewPrivate

- (void)dealloc
{
    ASSERT(autoscrollTimer == nil);
    ASSERT(autoscrollTriggerEvent == nil);
    
    [pluginController destroyAllPlugins];
    
    [mouseDownEvent release];
    [draggingImageURL release];
    [pluginController release];
    [toolTip release];
    
    [super dealloc];
}

@end


@implementation WebHTMLView (WebPrivate)

- (void)_reset
{
    [WebImageRenderer stopAnimationsInView:self];
}

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

+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[flagsChangedEvent window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[flagsChangedEvent modifierFlags]
        timestamp:[flagsChangedEvent timestamp]
        windowNumber:[flagsChangedEvent windowNumber]
        context:[flagsChangedEvent context]
        eventNumber:0 clickCount:0 pressure:0];

    // Pretend it's a mouse move.
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSMouseMovedNotification object:self
        userInfo:[NSDictionary dictionaryWithObject:fakeEvent forKey:@"NSEvent"]];
}

- (void)_updateMouseoverWithFakeEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    
    [self _updateMouseoverWithEvent:fakeEvent];
}

- (void)_frameOrBoundsChanged
{
    if (!NSEqualSizes(_private->lastLayoutSize, [(NSClipView *)[self superview] documentVisibleRect].size)) {
        [self setNeedsLayout:YES];
        [self setNeedsDisplay:YES];
    }

    SEL selector = @selector(_updateMouseoverWithFakeEvent);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:selector object:nil];
    [self performSelector:selector withObject:nil afterDelay:0];
}

- (NSDictionary *)_elementAtPoint:(NSPoint)point
{
    NSDictionary *elementInfoWC = [[self _bridge] elementAtPoint:point];
    NSMutableDictionary *elementInfo = [elementInfoWC mutableCopy];

    // Convert URL strings to NSURLs
    [elementInfo _web_setObjectIfNotNil:[NSURL _web_URLWithDataAsString:[elementInfoWC objectForKey:WebElementLinkURLKey]] forKey:WebElementLinkURLKey];
    [elementInfo _web_setObjectIfNotNil:[NSURL _web_URLWithDataAsString:[elementInfoWC objectForKey:WebElementImageURLKey]] forKey:WebElementImageURLKey];
    
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    ASSERT(webFrameView);
    WebFrame *webFrame = [webFrameView webFrame];
    
    if (webFrame) {
        NSString *frameName = [elementInfoWC objectForKey:WebElementLinkTargetFrameKey];
        if ([frameName length] == 0) {
            [elementInfo setObject:webFrame forKey:WebElementLinkTargetFrameKey];
        } else {
            WebFrame *wf = [webFrame findFrameNamed:frameName];
            if (wf != nil)
                [elementInfo setObject:wf forKey:WebElementLinkTargetFrameKey];
            else
                [elementInfo removeObjectForKey:WebElementLinkTargetFrameKey];
        }
    
        [elementInfo setObject:webFrame forKey:WebElementFrameKey];
    }
    
    return [elementInfo autorelease];
}

- (void)_setAsideSubviews
{
    ASSERT(!_private->subviewsSetAside);
    ASSERT(_private->savedSubviews == nil);
    _private->savedSubviews = _subviews;
    _subviews = nil;
    _private->subviewsSetAside = YES;
 }
 
 - (void)_restoreSubviews
 {
    ASSERT(_private->subviewsSetAside);
    ASSERT(_subviews == nil);
    _subviews = _private->savedSubviews;
    _private->savedSubviews = nil;
    _private->subviewsSetAside = NO;
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];
    if (wasInPrintingMode != isPrinting) {
        if (isPrinting) {
            [self _web_setPrintingModeRecursive];
        } else {
            [self _web_clearPrintingModeRecursive];
        }
    }

    [self _web_layoutIfNeededRecursive: rect testDirtyRect:YES];

    [self _setAsideSubviews];
    [super _recursiveDisplayRectIfNeededIgnoringOpacity:rect isVisibleRect:isVisibleRect
        rectIsVisibleRectForView:visibleView topView:topView];
    [self _restoreSubviews];

    if (wasInPrintingMode != isPrinting) {
        if (wasInPrintingMode) {
            [self _web_setPrintingModeRecursive];
        } else {
            [self _web_clearPrintingModeRecursive];
        }
    }
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect
{
    BOOL needToSetAsideSubviews = !_private->subviewsSetAside;

    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];

    if (needToSetAsideSubviews) {
        // This helps when we print as part of a larger print process.
        // If the WebHTMLView itself is what we're printing, then we will never have to do this.
        if (wasInPrintingMode != isPrinting) {
            if (isPrinting) {
                [self _web_setPrintingModeRecursive];
            } else {
                [self _web_clearPrintingModeRecursive];
            }
        }

        [self _web_layoutIfNeededRecursive: visRect testDirtyRect:NO];

        [self _setAsideSubviews];
    }
    
    [super _recursiveDisplayAllDirtyWithLockFocus:needsLockFocus visRect:visRect];
    
    if (needToSetAsideSubviews) {
        if (wasInPrintingMode != isPrinting) {
            if (wasInPrintingMode) {
                [self _web_setPrintingModeRecursive];
            } else {
                [self _web_clearPrintingModeRecursive];
            }
        }

        [self _restoreSubviews];
    }
}

- (BOOL)_insideAnotherHTMLView
{
    NSView *view = self;
    while ((view = [view superview])) {
        if ([view isKindOfClass:[WebHTMLView class]]) {
            return YES;
        }
    }
    return NO;
}

- (void)scrollPoint:(NSPoint)point
{
    // Since we can't subclass NSTextView to do what we want, we have to second guess it here.
    // If we get called during the handling of a key down event, we assume the call came from
    // NSTextView, and ignore it and use our own code to decide how to page up and page down
    // We are smarter about how far to scroll, and we have "superview scrolling" logic.
    NSEvent *event = [[self window] currentEvent];
    if ([event type] == NSKeyDown) {
        const unichar pageUp = NSPageUpFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageUp length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageUp:) with:nil];
            return;
        }
        const unichar pageDown = NSPageDownFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageDown length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageDown:) with:nil];
            return;
        }
    }
    
    [super scrollPoint:point];
}

- (NSView *)hitTest:(NSPoint)point
{
    // WebHTMLView objects handle all left mouse clicks for objects inside them.
    // That does not include left mouse clicks with the control key held down.
    BOOL captureHitsOnSubviews;
    if (forceRealHitTest) {
        captureHitsOnSubviews = NO;
    } else {
        NSEvent *event = [[self window] currentEvent];
        captureHitsOnSubviews = [event type] == NSLeftMouseDown && ([event modifierFlags] & NSControlKeyMask) == 0;
    }
    if (!captureHitsOnSubviews) {
        return [super hitTest:point];
    }
    if ([[self superview] mouse:point inRect:[self frame]]) {
        return self;
    }
    return nil;
}

static WebHTMLView *lastHitView = nil;

- (void)_clearLastHitViewIfSelf
{
    if (lastHitView == self) {
	lastHitView = nil;
    }
}

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == TRACKING_RECT_TAG);
    return [self addTrackingRect:rect owner:owner userData:data assumeInside:assumeInside];
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    ASSERT(tag == TRACKING_RECT_TAG);
    if (_private != nil) {
        ASSERT(_private->trackingRectOwner != nil);
        _private->trackingRectOwner = nil;
    }
}

- (void)_sendToolTipMouseExited
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseExited:fakeEvent];
}

- (void)_sendToolTipMouseEntered
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseEntered:fakeEvent];
}

- (void)_setToolTip:(NSString *)string
{
    NSString *toolTip = [string length] == 0 ? nil : string;
    NSString *oldToolTip = _private->toolTip;
    if ((toolTip == nil || oldToolTip == nil) ? toolTip == oldToolTip : [toolTip isEqualToString:oldToolTip]) {
        return;
    }
    if (oldToolTip) {
        [self _sendToolTipMouseExited];
        [oldToolTip release];
    }
    _private->toolTip = [toolTip copy];
    if (toolTip) {
        [self removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return [[_private->toolTip copy] autorelease];
}

- (void)_updateMouseoverWithEvent:(NSEvent *)event
{
    WebHTMLView *view = nil;
    if ([event window] == [self window]) {
        forceRealHitTest = YES;
        NSView *hitView = [[[self window] contentView] hitTest:[event locationInWindow]];
        forceRealHitTest = NO;
        while (hitView) {
            if ([hitView isKindOfClass:[WebHTMLView class]]) {
                view = (WebHTMLView *)hitView;
                break;
            }
            hitView = [hitView superview];
        }
    }

    if (lastHitView != view && lastHitView != nil) {
	// If we are moving out of a view (or frame), let's pretend the mouse moved
	// all the way out of that view. But we have to account for scrolling, because
	// khtml doesn't understand our clipping.
	NSRect visibleRect = [[[[lastHitView _frame] frameView] _scrollView] documentVisibleRect];
	float yScroll = visibleRect.origin.y;
	float xScroll = visibleRect.origin.x;

	event = [NSEvent mouseEventWithType:NSMouseMoved
			 location:NSMakePoint(-1 - xScroll, -1 - yScroll )
			 modifierFlags:[[NSApp currentEvent] modifierFlags]
			 timestamp:[NSDate timeIntervalSinceReferenceDate]
			 windowNumber:[[self window] windowNumber]
			 context:[[NSApp currentEvent] context]
			 eventNumber:0 clickCount:0 pressure:0];
	[[lastHitView _bridge] mouseMoved:event];
    }

    lastHitView = view;
    
    NSDictionary *element;
    if (view == nil) {
        element = nil;
    } else {
        [[view _bridge] mouseMoved:event];

        NSPoint point = [view convertPoint:[event locationInWindow] fromView:nil];
        element = [view _elementAtPoint:point];
    }

    // Have the web view send a message to the delegate so it can do status bar display.
    [[self _webView] _mouseDidMoveOverElement:element modifierFlags:[event modifierFlags]];

    // Set a tool tip; it won't show up right away but will if the user pauses.
    [self _setToolTip:[element objectForKey:WebCoreElementTitleKey]];
}

+ (NSArray *)_selectionPasteboardTypes
{
    return [NSArray arrayWithObjects:WebArchivePboardType, NSHTMLPboardType, NSRTFPboardType, NSRTFDPboardType, NSStringPboardType, nil];
}

- (WebArchive *)_selectedWebArchive:(NSString **)markupString
{
    NSArray *subresourceURLStrings;
    WebHTMLRepresentation *rep = [[self _dataSource] representation];
    *markupString = [[self _bridge] markupStringFromRange:[[self _bridge] selectedDOMRange] subresourceURLStrings:&subresourceURLStrings];
    return [rep _webArchiveWithMarkupString:*markupString subresourceURLStrings:subresourceURLStrings];
}

- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard
{
    [pasteboard declareTypes:[[self class] _selectionPasteboardTypes] owner:nil];

    // Put HTML on the pasteboard.
    NSString *markupString;
    WebArchive *webArchive = [self _selectedWebArchive:&markupString];
    [pasteboard setString:markupString forType:NSHTMLPboardType];
    [pasteboard setData:[webArchive dataRepresentation] forType:WebArchivePboardType];
    
    // Put attributed string on the pasteboard (RTF format).
    NSAttributedString *attributedString = [self selectedAttributedString];
    NSRange range = NSMakeRange(0, [attributedString length]);
    NSData *attributedData = [attributedString RTFFromRange:range documentAttributes:nil];
    [pasteboard setData:attributedData forType:NSRTFPboardType];

    attributedData = [attributedString RTFDFromRange:range documentAttributes:nil];
    [pasteboard setData:attributedData forType:NSRTFDPboardType];
    
    // Put plain string on the pasteboard.
    // Map &nbsp; to a plain old space because this is better for source code, other browsers do it,
    // and because HTML forces you to do this any time you want two spaces in a row.
    NSMutableString *s = [[self selectedString] mutableCopy];
    const unichar NonBreakingSpaceCharacter = 0xA0;
    NSString *NonBreakingSpaceString = [NSString stringWithCharacters:&NonBreakingSpaceCharacter length:1];
    [s replaceOccurrencesOfString:NonBreakingSpaceString withString:@" " options:0 range:NSMakeRange(0, [s length])];
    [pasteboard setString:s forType:NSStringPboardType];
    [s release];
}

- (BOOL)_haveSelection
{
    return [[self _bridge] haveSelection];
}

- (BOOL)_canDelete
{
    return [self _haveSelection] && [[self _bridge] isSelectionEditable];
}

- (BOOL)_canPaste
{
    return [[self _bridge] isSelectionEditable];
}

- (void)_pasteMarkupFromPasteboard:(NSPasteboard *)pasteboard
{
    NSArray *types = [pasteboard types];
    NSString *markupString = nil;
	
    if ([types containsObject:WebArchivePboardType]) {
        WebArchive *webArchive = [[WebArchive alloc] initWithData:[pasteboard dataForType:WebArchivePboardType]];
        WebResource *mainResource = [webArchive mainResource];
        if (mainResource) {
            markupString = [[[NSString alloc] initWithData:[mainResource data] encoding:NSUTF8StringEncoding] autorelease];
            [[self _dataSource] addSubresources:[webArchive subresources]];
        }
        [webArchive release];
    }
    
    if (!markupString && [types containsObject:NSHTMLPboardType]) {
        markupString = [pasteboard stringForType:NSHTMLPboardType];
    }
    if (!markupString && [types containsObject:NSStringPboardType]) {
        markupString = [pasteboard stringForType:NSStringPboardType];
    }
    if (markupString) {
        [[self _bridge] pasteMarkupString:markupString];
    }
}

-(NSImage *)_dragImageForLinkElement:(NSDictionary *)element
{
    NSURL *linkURL = [element objectForKey: WebElementLinkURLKey];

    BOOL drawURLString = YES;
    BOOL clipURLString = NO, clipLabelString = NO;
    
    NSString *label = [element objectForKey: WebElementLinkLabelKey];
    NSString *urlString = [linkURL _web_userVisibleString];
    
    if (!label) {
	drawURLString = NO;
	label = urlString;
    }
    
    NSFont *labelFont = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:DRAG_LINK_LABEL_FONT_SIZE]
                                                   toHaveTrait:NSBoldFontMask];
    NSFont *urlFont = [NSFont systemFontOfSize: DRAG_LINK_URL_FONT_SIZE];
    NSSize labelSize;
    labelSize.width = [label _web_widthWithFont: labelFont];
    labelSize.height = [labelFont ascender] - [labelFont descender];
    if (labelSize.width > MAX_DRAG_LABEL_WIDTH){
        labelSize.width = MAX_DRAG_LABEL_WIDTH;
        clipLabelString = YES;
    }
    
    NSSize imageSize, urlStringSize;
    imageSize.width += labelSize.width + DRAG_LABEL_BORDER_X * 2;
    imageSize.height += labelSize.height + DRAG_LABEL_BORDER_Y * 2;
    if (drawURLString) {
	urlStringSize.width = [urlString _web_widthWithFont: urlFont];
        urlStringSize.height = [urlFont ascender] - [urlFont descender];
	imageSize.height += urlStringSize.height;
	if (urlStringSize.width > MAX_DRAG_LABEL_WIDTH) {
	    imageSize.width = MAX(MAX_DRAG_LABEL_WIDTH + DRAG_LABEL_BORDER_X * 2, MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP);
	    clipURLString = YES;
	} else {
	    imageSize.width = MAX(labelSize.width + DRAG_LABEL_BORDER_X * 2, urlStringSize.width + DRAG_LABEL_BORDER_X * 2);
	}
    }
    NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
    [dragImage lockFocus];
    
    [[NSColor colorWithCalibratedRed: 0.7 green: 0.7 blue: 0.7 alpha: 0.8] set];
    
    // Drag a rectangle with rounded corners/
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0,0, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0,imageSize.height - DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2, imageSize.height - DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2,0, DRAG_LABEL_RADIUS * 2, DRAG_LABEL_RADIUS * 2)];
    
    [path appendBezierPathWithRect: NSMakeRect(DRAG_LABEL_RADIUS, 0, imageSize.width - DRAG_LABEL_RADIUS * 2, imageSize.height)];
    [path appendBezierPathWithRect: NSMakeRect(0, DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 10, imageSize.height - 2 * DRAG_LABEL_RADIUS)];
    [path appendBezierPathWithRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS - 20,DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 20, imageSize.height - 2 * DRAG_LABEL_RADIUS)];
    [path fill];
        
    NSColor *topColor = [NSColor colorWithCalibratedWhite:0.0 alpha:0.75];
    NSColor *bottomColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.5];
    if (drawURLString) {
	if (clipURLString)
	    urlString = [WebStringTruncator centerTruncateString: urlString toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2) withFont:urlFont];

        [urlString _web_drawDoubledAtPoint:NSMakePoint(DRAG_LABEL_BORDER_X, DRAG_LABEL_BORDER_Y - [urlFont descender]) 
             withTopColor:topColor bottomColor:bottomColor font:urlFont];
    }

    if (clipLabelString)
	    label = [WebStringTruncator rightTruncateString: label toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2) withFont:labelFont];
    [label _web_drawDoubledAtPoint:NSMakePoint (DRAG_LABEL_BORDER_X, imageSize.height - DRAG_LABEL_BORDER_Y_OFFSET - [labelFont pointSize])
             withTopColor:topColor bottomColor:bottomColor font:labelFont];
    
    [dragImage unlockFocus];
    
    return dragImage;
}

- (void)_handleMouseDragged:(NSEvent *)event
{
    NSPoint mouseDownPoint = [self convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    NSDictionary *element = [self _elementAtPoint:mouseDownPoint];

    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    NSURL *imageURL = [element objectForKey:WebElementImageURLKey];
    BOOL isSelected = [[element objectForKey:WebElementIsSelectedKey] boolValue];

    [_private->draggingImageURL release];
    _private->draggingImageURL = nil;

    // We must have started over something draggable:
    ASSERT((imageURL && [[WebPreferences standardPreferences] loadsImagesAutomatically]) ||
           (!imageURL && linkURL) || isSelected); 

    NSPoint mouseDraggedPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    float deltaX = ABS(mouseDraggedPoint.x - mouseDownPoint.x);
    float deltaY = ABS(mouseDraggedPoint.y - mouseDownPoint.y);
    
    // Drag hysteresis hasn't been met yet but we don't want to do other drag actions like selection.
    if ((imageURL && deltaX < ImageDragHysteresis && deltaY < ImageDragHysteresis) ||
        (linkURL && deltaX < LinkDragHysteresis && deltaY < LinkDragHysteresis) ||
        (isSelected && deltaX < TextDragHysteresis && deltaY < TextDragHysteresis)) {
        return;
    }
    
    if (imageURL) {
        _private->draggingImageURL = [imageURL retain];
        WebImageRenderer *image = [element objectForKey:WebElementImageKey];
        ASSERT([image isKindOfClass:[WebImageRenderer class]]);
        NSFileWrapper *fileWrapper = [[self _dataSource] _fileWrapperForURL:_private->draggingImageURL];
        ASSERT(fileWrapper);
        [self _web_dragImage:image
                 fileWrapper:fileWrapper
                        rect:[[element objectForKey:WebElementImageRectKey] rectValue]
                         URL:linkURL ? linkURL : imageURL
                       title:[element objectForKey:WebElementImageAltStringKey]
                  HTMLString:[[element objectForKey:WebCoreElementDOMNodeKey] HTMLString]
                       event:_private->mouseDownEvent];
        
    } else if (linkURL) {
        NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
        NSString *label = [element objectForKey:WebElementLinkLabelKey];
        [pasteboard _web_writeURL:linkURL andTitle:label withOwner:self];
        NSImage *dragImage = [self _dragImageForLinkElement:element];
        NSSize offset = NSMakeSize([dragImage size].width / 2, -DRAG_LABEL_BORDER_Y);
        [self dragImage:dragImage
                     at:NSMakePoint(mouseDraggedPoint.x - offset.width, mouseDraggedPoint.y - offset.height)
                 offset:offset
                  event:event
             pasteboard:pasteboard
                 source:self
              slideBack:NO];
        
    } else if (isSelected) {
        NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
        [self _writeSelectionToPasteboard:pasteboard];
        NSImage *selectionImage = [[self _bridge] selectionImage];
        [selectionImage _web_dissolveToFraction:WebDragImageAlpha];
        NSRect visibleSelectionRect = [[self _bridge] visibleSelectionRect];
        [self dragImage:selectionImage
                     at:NSMakePoint(NSMinX(visibleSelectionRect), NSMaxY(visibleSelectionRect))
                 offset:NSMakeSize(mouseDraggedPoint.x - mouseDownPoint.x, mouseDraggedPoint.y - mouseDownPoint.y)
                  event:_private->mouseDownEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else {
        ERROR("Attempt to drag unknown element");
    }
}

- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event
{
    [self autoscroll:event];
    [self _startAutoscrollTimer:event];
}

- (BOOL)_mayStartDragWithMouseDragged:(NSEvent *)mouseDraggedEvent
{
    NSPoint mouseDownPoint = [self convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    NSDictionary *mouseDownElement = [self _elementAtPoint:mouseDownPoint];

    NSURL *imageURL = [mouseDownElement objectForKey: WebElementImageURLKey];

    if ((imageURL && [[WebPreferences standardPreferences] loadsImagesAutomatically]) ||
        (!imageURL && [mouseDownElement objectForKey: WebElementLinkURLKey]) ||
        ([[mouseDownElement objectForKey:WebElementIsSelectedKey] boolValue] &&
         ([mouseDraggedEvent timestamp] - [_private->mouseDownEvent timestamp]) > TextDragDelay)) {
        return YES;
    }

    return NO;
}

- (WebPluginController *)_pluginController
{
    return _private->pluginController;
}

- (void)_web_setPrintingModeRecursive
{
    [self _setPrinting:YES minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    [super _web_setPrintingModeRecursive];
}

- (void)_web_clearPrintingModeRecursive
{
    [self _setPrinting:NO minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    [super _web_clearPrintingModeRecursive];
}

- (void)_web_layoutIfNeededRecursive:(NSRect)displayRect testDirtyRect:(bool)testDirtyRect
{
    ASSERT(!_private->subviewsSetAside);
    displayRect = NSIntersectionRect(displayRect, [self bounds]);

    if (!testDirtyRect || [self needsDisplay]) {
        if (testDirtyRect) {
            NSRect dirtyRect = [self _dirtyRect];
            displayRect = NSIntersectionRect(displayRect, dirtyRect);
        }
        if (!NSIsEmptyRect(displayRect)) {
            if ([[self _bridge] needsLayout])
                _private->needsLayout = YES;
            if (_private->needsToApplyStyles || _private->needsLayout)
                [self layout];
        }
    }

    [super _web_layoutIfNeededRecursive: displayRect testDirtyRect: NO];
}

- (NSRect)_selectionRect
{
    return [[self _bridge] selectionRect];
}

- (void)_startAutoscrollTimer: (NSEvent *)triggerEvent
{
    if (_private->autoscrollTimer == nil) {
        _private->autoscrollTimer = [[NSTimer scheduledTimerWithTimeInterval:AUTOSCROLL_INTERVAL
            target:self selector:@selector(_autoscroll) userInfo:nil repeats:YES] retain];
        _private->autoscrollTriggerEvent = [triggerEvent retain];
    }
}

- (void)_stopAutoscrollTimer
{
    NSTimer *timer = _private->autoscrollTimer;
    _private->autoscrollTimer = nil;
    [_private->autoscrollTriggerEvent release];
    _private->autoscrollTriggerEvent = nil;
    [timer invalidate];
    [timer release];
}


- (void)_autoscroll
{
    int isStillDown;
    
    // Guarantee that the autoscroll timer is invalidated, even if we don't receive
    // a mouse up event.
    PSstilldown([_private->autoscrollTriggerEvent eventNumber], &isStillDown);
    if (!isStillDown){
        [self _stopAutoscrollTimer];
        return;
    }

    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    [self mouseDragged:fakeEvent];
}

@end

@implementation NSView (WebHTMLViewPrivate)

- (void)_web_setPrintingModeRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_setPrintingModeRecursive)];
}

- (void)_web_clearPrintingModeRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_clearPrintingModeRecursive)];
}

- (void)_web_layoutIfNeededRecursive: (NSRect)rect testDirtyRect:(bool)testDirtyRect
{
    unsigned index, count;
    for (index = 0, count = [_subviews count]; index < count; index++) {
        NSView *subview = [_subviews objectAtIndex:index];
        NSRect dirtiedSubviewRect = [subview convertRect: rect fromView: self];
        [subview _web_layoutIfNeededRecursive: dirtiedSubviewRect testDirtyRect:testDirtyRect];
    }
}

@end

@implementation NSMutableDictionary (WebHTMLViewPrivate)

- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key
{
    if (object == nil) {
        [self removeObjectForKey:key];
    } else {
        [self setObject:object forKey:key];
    }
}

@end

// The following is a workaround for
// <rdar://problem/3429631> window stops getting mouse moved events after first tooltip appears
// The trick is to define a category on NSToolTipPanel that implements setAcceptsMouseMovedEvents:.
// Since the category will be searched before the real class, we'll prevent the flag from being
// set on the tool tip panel.

@interface NSToolTipPanel : NSPanel
@end

@interface NSToolTipPanel (WebHTMLViewPrivate)
@end

@implementation NSToolTipPanel (WebHTMLViewPrivate)

- (void)setAcceptsMouseMovedEvents:(BOOL)flag
{
    // Do nothing, preventing the tool tip panel from trying to accept mouse-moved events.
}

@end


@interface WebHTMLView (TextSizing) <_web_WebDocumentTextSizing>
@end

@interface NSArray (WebHTMLView)
- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object;
@end

@implementation WebHTMLView

+ (void)initialize
{
    WebKitInitializeUnicode();
    [NSApp registerServicesMenuSendTypes:[[self class] _selectionPasteboardTypes] returnTypes:nil];
}

- (id)initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    
    // Make all drawing go through us instead of subviews.
    // The bulk of the code to handle this is in WebHTMLViewPrivate.m.
    if (NSAppKitVersionNumber >= 711) {
        [self _setDrawsOwnDescendants:YES];
    }
    
    _private = [[WebHTMLViewPrivate alloc] init];

    _private->pluginController = [[WebPluginController alloc] initWithHTMLView:self];
    _private->needsLayout = YES;
	
    [self registerForDraggedTypes:[NSArray arrayWithObjects:WebArchivePboardType, NSHTMLPboardType, NSStringPboardType, nil]];

    return self;
}

- (void)dealloc
{
    [self _clearLastHitViewIfSelf];
    [self _reset];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_private release];
    _private = nil;
    [super dealloc];
}

- (IBAction)takeFindStringFromSelection:(id)sender
{
    if (![self _haveSelection]) {
        NSBeep();
        return;
    }

    [NSPasteboard _web_setFindPasteboardString:[self selectedString] withOwner:self];
}

- (void)copy:(id)sender
{
    [self _writeSelectionToPasteboard:[NSPasteboard generalPasteboard]];
}

- (void)cut:(id)sender
{   
	[self copy:sender];
	[[self _bridge] deleteSelection];
}

- (void)delete:(id)sender
{
	[[self _bridge] deleteSelection];
}

- (void)paste:(id)sender
{
    [self _pasteMarkupFromPasteboard:[NSPasteboard generalPasteboard]];
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    [self _writeSelectionToPasteboard:pasteboard];
    return YES;
}

- (void)selectAll:(id)sender
{
    [self selectAll];
}

- (void)jumpToSelection: sender
{
    [[self _bridge] jumpToSelection];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item 
{
    SEL action = [item action];
    
	if (action == @selector(cut:)) {
        return [self _canDelete];
    } else if (action == @selector(copy:)) {
        return [self _haveSelection];
    } else if (action == @selector(delete:)) {
        return [self _canDelete];
    } else if (action == @selector(paste:)) {
        return [self _canPaste];
    }else if (action == @selector(takeFindStringFromSelection:)) {
        return [self _haveSelection];
    }else if (action == @selector(jumpToSelection:)) {
        return [self _haveSelection];
	}
    
    return YES;
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    if (sendType && ([[[self class] _selectionPasteboardTypes] containsObject:sendType]) && [self _haveSelection]){
        return self;
    }

    return [super validRequestorForSendType:sendType returnType:returnType];
}

- (BOOL)acceptsFirstResponder
{
    // Don't accept first responder when we first click on this view.
    // We have to pass the event down through WebCore first to be sure we don't hit a subview.
    // Do accept first responder at any other time, for example from keyboard events,
    // or from calls back from WebCore once we begin mouse-down event handling.
    NSEvent *event = [NSApp currentEvent];
    if ([event type] == NSLeftMouseDown && event != _private->mouseDownEvent && 
        NSPointInRect([event locationInWindow], [self convertRect:[self visibleRect] toView:nil])) {
        return NO;
    }
    return YES;
}

- (void)updateTextBackgroundColor
{
    NSWindow *window = [self window];
    BOOL shouldUseInactiveTextBackgroundColor = !([window isKeyWindow] && [window firstResponder] == self);
    WebBridge *bridge = [self _bridge];
    if ([bridge usesInactiveTextBackgroundColor] != shouldUseInactiveTextBackgroundColor) {
        [bridge setUsesInactiveTextBackgroundColor:shouldUseInactiveTextBackgroundColor];
        [self setNeedsDisplayInRect:[bridge visibleSelectionRect]];
    }
}

- (void)addMouseMovedObserver
{
    if ([[self window] isKeyWindow] && ![self _insideAnotherHTMLView]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mouseMovedNotification:)
            name:NSMouseMovedNotification object:nil];
        [self _frameOrBoundsChanged];
    }
}

- (void)removeMouseMovedObserver
{
    [[self _webView] _mouseDidMoveOverElement:nil modifierFlags:0];
    [[NSNotificationCenter defaultCenter] removeObserver:self
        name:NSMouseMovedNotification object:nil];
}

- (void)updateShowsFirstResponder
{
    [[self _bridge] setShowsFirstResponder:[[self window] isKeyWindow]];
}

- (void)addSuperviewObservers
{
    // We watch the bounds of our superview, so that we can do a layout when the size
    // of the superview changes. This is different from other scrollable things that don't
    // need this kind of thing because their layout doesn't change.
    
    // We need to pay attention to both height and width because our "layout" has to change
    // to extend the background the full height of the space and because some elements have
    // sizes that are based on the total size of the view.
    
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewBoundsDidChangeNotification object:superview];
    }
}

- (void)removeSuperviewObservers
{
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewBoundsDidChangeNotification object:superview];
    }
}

- (void)addWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidBecomeKey:)
            name:NSWindowDidBecomeKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidResignKey:)
            name:NSWindowDidResignKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowWillClose:)
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidBecomeKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidResignKeyNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    [self removeSuperviewObservers];
}

- (void)viewDidMoveToSuperview
{
    // Do this here in case the text size multiplier changed when a non-HTML
    // view was installed.
    if ([self superview] != nil) {
        [self _updateTextSizeMultiplier];
        [self addSuperviewObservers];
    }
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (_private){
        // FIXME: Some of these calls may not work because this view may be already removed from it's superview.
        [self removeMouseMovedObserver];
        [self removeWindowObservers];
        [self removeSuperviewObservers];
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];
    
        [[self _pluginController] stopAllPlugins];
    }
}

- (void)viewDidMoveToWindow
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (_private) {
        [self _stopAutoscrollTimer];
        if ([self window]) {
            [self addWindowObservers];
            [self addSuperviewObservers];
            [self addMouseMovedObserver];
            [self updateTextBackgroundColor];
    
            [[self _pluginController] startAllPlugins];
    
            _private->inWindow = YES;
        } else {
            // Reset when we are moved out of a window after being moved into one.
            // Without this check, we reset ourselves before we even start.
            // This is only needed because viewDidMoveToWindow is called even when
            // the window is not changing (bug in AppKit).
            if (_private->inWindow) {
                [self _reset];
                _private->inWindow = NO;
            }
        }
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewWillMoveToHostWindow:) withObject:hostWindow];
}

- (void)viewDidMoveToHostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewDidMoveToHostWindow) withObject:nil];
}


- (void)addSubview:(NSView *)view
{
    [super addSubview:view];

    if ([view conformsToProtocol:@protocol(WebPlugin)]) {
        [[self _pluginController] addPlugin:view];
    }
}

- (void)reapplyStyles
{
    if (!_private->needsToApplyStyles) {
        return;
    }
    
#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [[self _bridge] reapplyStylesForDeviceType:
        _private->printing ? WebCoreDevicePrinter : WebCoreDeviceScreen];
    
#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s apply style seconds = %f", [self URL], thisTime);
#endif

    _private->needsToApplyStyles = NO;
}

// Do a layout, but set up a new fixed width for the purposes of doing printing layout.
// minPageWidth==0 implies a non-printing layout
- (void)layoutToMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)adjustViewSize
{
    [self reapplyStyles];
    
    // Ensure that we will receive mouse move events.  Is this the best place to put this?
    [[self window] setAcceptsMouseMovedEvents: YES];
    [[self window] _setShouldPostEventNotifications: YES];

    if (!_private->needsLayout) {
        return;
    }

#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    LOG(View, "%@ doing layout", self);

    if (minPageWidth > 0.0) {
        [[self _bridge] forceLayoutWithMinimumPageWidth:minPageWidth maximumPageWidth:maxPageWidth adjustingViewSize:adjustViewSize];
    } else {
        [[self _bridge] forceLayoutAdjustingViewSize:adjustViewSize];
    }
    _private->needsLayout = NO;
    
    if (!_private->printing) {
	// get size of the containing dynamic scrollview, so
	// appearance and disappearance of scrollbars will not show up
	// as a size change
	NSSize newLayoutFrameSize = [[[self superview] superview] frame].size;
	if (_private->laidOutAtLeastOnce && !NSEqualSizes(_private->lastLayoutFrameSize, newLayoutFrameSize)) {
	    [[self _bridge] sendResizeEvent];
	}
	_private->laidOutAtLeastOnce = YES;
	_private->lastLayoutSize = [(NSClipView *)[self superview] documentVisibleRect].size;
        _private->lastLayoutFrameSize = newLayoutFrameSize;
    }

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s layout seconds = %f", [self URL], thisTime);
#endif
}

- (void)layout
{
    [self layoutToMinimumPageWidth:0.0 maximumPageWidth:0.0 adjustingViewSize:NO];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    if ([[self _bridge] sendContextMenuEvent:event]) {
        return nil;
    }
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSDictionary *element = [self _elementAtPoint:point];
    return [[self _webView] _menuForElement:element];
}

// Search from the end of the currently selected location, or from the beginning of the
// document if nothing is selected.
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag;
{
    return [[self _bridge] searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag];
}

- (NSString *)string
{
    return [[self attributedString] string];
}

- (NSAttributedString *)attributedString
{
    WebBridge *b = [self _bridge];
    return [b attributedStringFrom:[b DOMDocument]
                       startOffset:0
                                to:nil
                         endOffset:0];
}

- (NSString *)selectedString
{
    return [[self _bridge] selectedString];
}

// Get an attributed string that represents the current selection.
- (NSAttributedString *)selectedAttributedString
{
    return [[self _bridge] selectedAttributedString];
}

- (void)selectAll
{
    [[self _bridge] selectAll];
}

// Remove the selection.
- (void)deselectAll
{
    [[self _bridge] deselectAll];
}

- (void)deselectText
{
    [[self _bridge] deselectText];
}

- (BOOL)isOpaque
{
    return YES;
}

- (void)setNeedsDisplay:(BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    [super setNeedsDisplay: flag];
}

- (void)setNeedsLayout: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsLayout = flag;
}


- (void)setNeedsToApplyStyles: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsToApplyStyles = flag;
}

- (void)drawRect:(NSRect)rect
{
    LOG(View, "%@ drawing", self);
    
    BOOL subviewsWereSetAside = _private->subviewsSetAside;
    if (subviewsWereSetAside) {
        [self _restoreSubviews];
    }

#ifdef _KWQ_TIMING
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [NSGraphicsContext saveGraphicsState];
    NSRectClip(rect);
        
    ASSERT([[self superview] isKindOfClass:[WebClipView class]]);

    [(WebClipView *)[self superview] setAdditionalClip:rect];

    NS_DURING {
        WebTextRendererFactory *textRendererFactoryIfCoalescing = nil;
        if ([WebTextRenderer shouldBufferTextDrawing] && [NSView focusView]) {
            textRendererFactoryIfCoalescing = [WebTextRendererFactory sharedFactory];
            [textRendererFactoryIfCoalescing startCoalesceTextDrawing];
        }

        //double start = CFAbsoluteTimeGetCurrent();
        [[self _bridge] drawRect:rect];
        //LOG(Timing, "draw time %e", CFAbsoluteTimeGetCurrent() - start);

        if (textRendererFactoryIfCoalescing != nil) {
            [textRendererFactoryIfCoalescing endCoalesceTextDrawing];
        }

        [(WebClipView *)[self superview] resetAdditionalClip];

        [NSGraphicsContext restoreGraphicsState];
    } NS_HANDLER {
        [(WebClipView *)[self superview] resetAdditionalClip];
        [NSGraphicsContext restoreGraphicsState];
        ERROR("Exception caught while drawing: %@", localException);
        [localException raise];
    } NS_ENDHANDLER

#ifdef DEBUG_LAYOUT
    NSRect vframe = [self frame];
    [[NSColor blackColor] set];
    NSBezierPath *path;
    path = [NSBezierPath bezierPath];
    [path setLineWidth:(float)0.1];
    [path moveToPoint:NSMakePoint(0, 0)];
    [path lineToPoint:NSMakePoint(vframe.size.width, vframe.size.height)];
    [path closePath];
    [path stroke];
    path = [NSBezierPath bezierPath];
    [path setLineWidth:(float)0.1];
    [path moveToPoint:NSMakePoint(0, vframe.size.height)];
    [path lineToPoint:NSMakePoint(vframe.size.width, 0)];
    [path closePath];
    [path stroke];
#endif

#ifdef _KWQ_TIMING
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s draw seconds = %f", widget->part()->baseURL().URL().latin1(), thisTime);
#endif

    if (subviewsWereSetAside) {
        [self _setAsideSubviews];
    }
}

// Turn off the additional clip while computing our visibleRect.
- (NSRect)visibleRect
{
    if (!([[self superview] isKindOfClass:[WebClipView class]]))
        return [super visibleRect];
        
    WebClipView *clipView = (WebClipView *)[self superview];

    BOOL hasAdditionalClip = [clipView hasAdditionalClip];
    if (!hasAdditionalClip) {
        return [super visibleRect];
    }
    
    NSRect additionalClip = [clipView additionalClip];
    [clipView resetAdditionalClip];
    NSRect visibleRect = [super visibleRect];
    [clipView setAdditionalClip:additionalClip];
    return visibleRect;
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [self addMouseMovedObserver];
    [self updateTextBackgroundColor];
    [self updateShowsFirstResponder];
}

- (void)windowDidResignKey: (NSNotification *)notification
{
    ASSERT([notification object] == [self window]);
    [self removeMouseMovedObserver];
    [self updateTextBackgroundColor];
    [self updateShowsFirstResponder];
}

- (void)windowWillClose:(NSNotification *)notification
{
    [[self _pluginController] destroyAllPlugins];
}

- (BOOL)_isSelectionEvent:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    return [[[self _elementAtPoint:point] objectForKey:WebElementIsSelectedKey] boolValue];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return [self _isSelectionEvent:event];
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event
{
    return [self _isSelectionEvent:event];
}

- (void)mouseDown:(NSEvent *)event
{
    // If the web page handles the context menu event and menuForEvent: returns nil, we'll get control click events here.
    // We don't want to pass them along to KHTML a second time.
    if ([event modifierFlags] & NSControlKeyMask) {
        return;
    }
    
    _private->ignoringMouseDraggedEvents = NO;
    
    // Record the mouse down position so we can determine drag hysteresis.
    [_private->mouseDownEvent release];
    _private->mouseDownEvent = [event retain];

    // Don't do any mouseover while the mouse is down.
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];

    // Let KHTML get a chance to deal with the event. This will call back to us
    // to start the autoscroll timer if appropriate.
    [[self _bridge] mouseDown:event];
}

- (void)dragImage:(NSImage *)dragImage
               at:(NSPoint)at
           offset:(NSSize)offset
            event:(NSEvent *)event
       pasteboard:(NSPasteboard *)pasteboard
           source:(id)source
        slideBack:(BOOL)slideBack
{   
    _private->isDragging = YES;
    
    [self _stopAutoscrollTimer];

    // Don't allow drags to be accepted by this WebFrameView.
    [[self _webView] unregisterDraggedTypes];
    
    // Retain this view during the drag because it may be released before the drag ends.
    [self retain];

    [super dragImage:dragImage at:at offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

- (void)mouseDragged:(NSEvent *)event
{
    if (!_private->ignoringMouseDraggedEvents) {
        [[self _bridge] mouseDragged:event];
    }
}

- (unsigned)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
    return (NSDragOperationGeneric|NSDragOperationCopy);
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    _private->isDragging = NO;
    
    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    _private->ignoringMouseDraggedEvents = YES;
    
    // Once the dragging machinery kicks in, we no longer get mouse drags or the up event.
    // khtml expects to get balanced down/up's, so we must fake up a mouseup.
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                            location:[[self window] convertScreenToBase:aPoint]
                                       modifierFlags:[[NSApp currentEvent] modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[[self window] windowNumber]
                                             context:[[NSApp currentEvent] context]
                                         eventNumber:0 clickCount:0 pressure:0];
    [self mouseUp:fakeEvent];	    // This will also update the mouseover state.

    // Reregister for drag types because they were unregistered before the drag.
    [[self _webView] _registerDraggedTypes];
    
    // Balance the previous retain from when the drag started.
    [self release];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    ASSERT(_private->draggingImageURL);
    
    NSFileWrapper *wrapper = [[self _dataSource] _fileWrapperForURL:_private->draggingImageURL];
    ASSERT(wrapper);    
    
    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[wrapper preferredFilename]];
    path = [[NSFileManager defaultManager] _web_pathWithUniqueFilenameForPath:path];
    if (![wrapper writeToFile:path atomically:NO updateFilenames:YES]) {
        ERROR("Failed to create image file via -[NSFileWrapper writeToFile:atomically:updateFilenames:]");
    }
    
    return [NSArray arrayWithObject:[path lastPathComponent]];
}

- (NSDragOperation)_dragOperationForDraggingInfo:(id <NSDraggingInfo>)sender
{
    NSPoint point = [self convertPoint:[sender draggingLocation] fromView:nil];
    NSDictionary *element = [self _elementAtPoint:point];
    if ([[element objectForKey:WebElementIsEditableKey] boolValue] && [[self _bridge] moveCaretToPoint:point]) {
        if (_private->isDragging) {
            if ([[element objectForKey:WebElementIsSelectedKey] boolValue]) {
                return NSDragOperationMove;
            }
        } else {
            return NSDragOperationCopy;
        }
    }
    return NSDragOperationNone;
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    // If we're not handling the drag, forward this message to the WebView since it may want to handle it.
    NSDragOperation dragOperation = [self _dragOperationForDraggingInfo:sender];
    return dragOperation != NSDragOperationNone ? dragOperation : [[self _webView] draggingEntered:sender];
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
    // If we're not handling the drag, forward this message to the WebView since it may want to handle it.
    NSDragOperation dragOperation = [self _dragOperationForDraggingInfo:sender];
    return dragOperation != NSDragOperationNone ? dragOperation : [[self _webView] draggingUpdated:sender];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    if ([self _dragOperationForDraggingInfo:sender] != NSDragOperationNone) {
        // FIXME: We should delete the original selection if we're doing a move.
        [self _pasteMarkupFromPasteboard:[sender draggingPasteboard]];
    } else {
        // Since we're not handling the drag, forward this message to the WebView since it may want to handle it.
        [[self _webView] concludeDragOperation:sender];
    }
}

- (void)mouseUp:(NSEvent *)event
{
    [self _stopAutoscrollTimer];
    [[self _bridge] mouseUp:event];
    [self _updateMouseoverWithFakeEvent];
}

- (void)mouseMovedNotification:(NSNotification *)notification
{
    [self _updateMouseoverWithEvent:[[notification userInfo] objectForKey:@"NSEvent"]];
}

- (BOOL)supportsTextEncoding
{
    return YES;
}

- (NSView *)nextKeyView
{
    return (_private && _private->inNextValidKeyView && ![[self _bridge] inNextKeyViewOutsideWebFrameViews])
        ? [[self _bridge] nextKeyView]
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
    return (_private && _private->inNextValidKeyView)
        ? [[self _bridge] previousKeyView]
        : [super previousKeyView];
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

- (BOOL)becomeFirstResponder
{
    NSView *view = nil;
    if (![[self _webView] _isPerformingProgrammaticFocus]) {
	switch ([[self window] keyViewSelectionDirection]) {
	case NSDirectSelection:
	    break;
	case NSSelectingNext:
	    view = [[self _bridge] nextKeyViewInsideWebFrameViews];
	    break;
	case NSSelectingPrevious:
	    view = [[self _bridge] previousKeyViewInsideWebFrameViews];
	    break;
	}
    }
    if (view) {
        [[self window] makeFirstResponder:view];
    }
    [self updateTextBackgroundColor];
    return YES;
}

// This approach could be relaxed when dealing with 3228554
- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        if ([[self _webView] _isPerformingProgrammaticFocus]) {
            [self deselectText];
        }
        else {
            [self deselectAll];
        }
        [self updateTextBackgroundColor];
    }
    return resign;
}

//------------------------------------------------------------------------------------
// WebDocumentView protocol
//------------------------------------------------------------------------------------
- (void)setDataSource:(WebDataSource *)dataSource 
{
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

// Does setNeedsDisplay:NO as a side effect when printing is ending.
// pageWidth != 0 implies we will relayout to a new width
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize
{
    WebFrame *frame = [self _frame];
    NSArray *subframes = [frame childFrames];
    unsigned n = [subframes count];
    unsigned i;
    for (i = 0; i != n; ++i) {
        WebFrame *subframe = [subframes objectAtIndex:i];
        WebFrameView *frameView = [subframe frameView];
        if ([[subframe dataSource] _isDocumentHTML]) {
            [(WebHTMLView *)[frameView documentView] _setPrinting:printing minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:adjustViewSize];
        }
    }

    if (printing != _private->printing) {
        [_private->pageRects release];
        _private->pageRects = nil;
        _private->printing = printing;
        [self setNeedsToApplyStyles:YES];
        [self setNeedsLayout:YES];
        [self layoutToMinimumPageWidth:minPageWidth maximumPageWidth:maxPageWidth adjustingViewSize:adjustViewSize];
        if (printing) {
            [[self _webView] _adjustPrintingMarginsForHeaderAndFooter];
        } else {
            // Can't do this when starting printing or nested printing won't work, see 3491427.
            [self setNeedsDisplay:NO];
        }
    }
}

// This is needed for the case where the webview is embedded in the view that's being printed.
// It shouldn't be called when the webview is being printed directly.
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    if (!wasInPrintingMode) {
        [self _setPrinting:YES minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    }
    
    [[self _bridge] adjustPageHeightNew:newBottom top:oldTop bottom:oldBottom limit:bottomLimit];
    
    if (!wasInPrintingMode) {
        [self _setPrinting:NO minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:NO];
    }
}

- (float)_availablePaperWidthForPrintOperation:(NSPrintOperation *)printOperation
{
    NSPrintInfo *printInfo = [printOperation printInfo];
    return [printInfo paperSize].width - [printInfo leftMargin] - [printInfo rightMargin];
}

- (float)_scaleFactorForPrintOperation:(NSPrintOperation *)printOperation
{
    float viewWidth = NSWidth([self bounds]);
    if (viewWidth < 1) {
        ERROR("%@ has no width when printing", self);
        return 1.0;
    }
	
    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    float maxShrinkToFitScaleFactor = 1/PrintingMaximumShrinkFactor;
    float shrinkToFitScaleFactor = [self _availablePaperWidthForPrintOperation:printOperation]/viewWidth;
    return userScaleFactor * MAX(maxShrinkToFitScaleFactor, shrinkToFitScaleFactor);
}

// FIXME 3491344: This is a secret AppKit-internal method that we need to override in order
// to get our shrink-to-fit to work with a custom pagination scheme. We can do this better
// if AppKit makes it SPI/API.
- (float)_provideTotalScaleFactorForPrintOperation:(NSPrintOperation *)printOperation 
{
    return [self _scaleFactorForPrintOperation:printOperation];
}

// Return the number of pages available for printing
- (BOOL)knowsPageRange:(NSRangePointer)range {
    // Must do this explicit display here, because otherwise the view might redisplay while the print
    // sheet was up, using printer fonts (and looking different).
    [self displayIfNeeded];
    [[self window] setAutodisplay:NO];
    
    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the paper size
    float minLayoutWidth = 0.0;
    float maxLayoutWidth = 0.0;
    if (![[self _bridge] isFrameSet]) {
        float paperWidth = [self _availablePaperWidthForPrintOperation:[NSPrintOperation currentOperation]];
        minLayoutWidth = paperWidth*PrintingMinimumShrinkFactor;
        maxLayoutWidth = paperWidth*PrintingMaximumShrinkFactor;
    }
    [self _setPrinting:YES minimumPageWidth:minLayoutWidth maximumPageWidth:maxLayoutWidth adjustViewSize:YES];	// will relayout
    
    // There is a theoretical chance that someone could do some drawing between here and endDocument,
    // if something caused setNeedsDisplay after this point. If so, it's not a big tragedy, because
    // you'd simply see the printer fonts on screen. As of this writing, this does not happen with Safari.

    range->location = 1;
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    float totalScaleFactor = [self _scaleFactorForPrintOperation:printOperation];
    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    [_private->pageRects release];
    _private->pageRects = [[[self _bridge] computePageRectsWithPrintWidth:NSWidth([self bounds])/userScaleFactor
                                                             printHeight:[self _calculatePrintHeight]/totalScaleFactor] retain];
    range->length = [_private->pageRects count];
    return YES;
}

// Return the drawing rectangle for a particular page number
- (NSRect)rectForPage:(int)page {
    return [[_private->pageRects objectAtIndex: (page-1)] rectValue];
}

// Calculate the vertical size of the view that fits on a single page
- (float)_calculatePrintHeight {
    // Obtain the print info object for the current operation
    NSPrintInfo *pi = [[NSPrintOperation currentOperation] printInfo];
    
    // Calculate the page height in points
    NSSize paperSize = [pi paperSize];
    return paperSize.height - [pi topMargin] - [pi bottomMargin];
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));    
    [[self _webView] _drawHeaderAndFooter];
}

- (void)endDocument
{
    [super endDocument];
    // Note sadly at this point [NSGraphicsContext currentContextDrawingToScreen] is still NO 
    [self _setPrinting:NO minimumPageWidth:0.0 maximumPageWidth:0.0 adjustViewSize:YES];
    [[self window] setAutodisplay:YES];
}

- (void)_updateTextSizeMultiplier
{
    [[self _bridge] setTextSizeMultiplier:[[self _webView] textSizeMultiplier]];    
}

- (void)keyDown:(NSEvent *)event
{
    BOOL intercepted = [[self _bridge] interceptKeyEvent:event toView:self];
    if (!intercepted || [event _web_isTabKeyEvent]) {
	[super keyDown:event];
    }
}

- (void)keyUp:(NSEvent *)event
{
    if (![[self _bridge] interceptKeyEvent:event toView:self]) {
	[super keyUp:event];
    }
}

- (id)accessibilityAttributeValue:(NSString*)attributeName
{
    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        id accTree = [[self _bridge] accessibilityTree];
        if (accTree)
            return [NSArray arrayWithObject: accTree];
        return nil;
    }
    return [super accessibilityAttributeValue:attributeName];
}

- (id)accessibilityHitTest:(NSPoint)point
{
    id accTree = [[self _bridge] accessibilityTree];
    if (accTree) {
        NSPoint windowCoord = [[self window] convertScreenToBase: point];
        return [accTree accessibilityHitTest: [self convertPoint:windowCoord fromView:nil]];
    }
    else
        return self;
}
@end

@implementation WebHTMLView (TextSizing)

- (void)_web_textSizeMultiplierChanged
{
    [self _updateTextSizeMultiplier];
}

@end

@implementation NSArray (WebHTMLView)

- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object
{
    NSEnumerator *enumerator = [self objectEnumerator];
    WebNetscapePluginEmbeddedView *view;
    while ((view = [enumerator nextObject]) != nil) {
        if ([view isKindOfClass:[WebNetscapePluginEmbeddedView class]]) {
            [view performSelector:selector withObject:object];
        }
    }
}

@end
