/*	
        WebBaseNetscapePluginView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBaseNetscapePluginView.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h> 
#import <WebKit/WebFrameView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNetscapePluginStream.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <Foundation/NSData_NSURLExtras.h>
#import <Foundation/NSString_NSURLExtras.h>
#import <Foundation/NSURL_NSURLExtras.h>
#import <Foundation/NSURLRequestPrivate.h>

#import <AppKit/NSEvent_Private.h>
#import <Carbon/Carbon.h>
#import <HIToolbox/TextServicesPriv.h>
#import <QD/QuickdrawPriv.h>

// This is not yet in QuickdrawPriv.h, although it's supposed to be.
void CallDrawingNotifications(CGrafPtr port, Rect *mayDrawIntoThisRect, int drawingType);

// Send null events 50 times a second when active, so plug-ins like Flash get high frame rates.
#define NullEventIntervalActive 	0.02
#define NullEventIntervalNotActive	0.25

static WebBaseNetscapePluginView *currentPluginView = nil;

typedef struct {
    GrafPtr oldPort;
    Point oldOrigin;
    RgnHandle oldClipRegion;
    RgnHandle oldVisibleRegion;
    RgnHandle clipRegion;
    BOOL forUpdate;
} PortState;

@interface WebPluginRequest : NSObject
{
    NSURLRequest *_request;
    NSString *_frameName;
    void *_notifyData;
}

- (id)initWithRequest:(NSURLRequest *)request frameName:(NSString *)frameName notifyData:(void *)notifyData;

- (NSURLRequest *)request;
- (NSString *)frameName;
- (void *)notifyData;

@end

@interface NSData (WebPluginDataExtras)
- (BOOL)_web_startsWithBlankLine;
- (unsigned)_web_locationAfterFirstBlankLine;
@end

static OSStatus TSMEventHandler(EventHandlerCallRef inHandlerRef, EventRef inEvent, void *pluginView);

@implementation WebBaseNetscapePluginView

#pragma mark EVENTS

+ (void)getCarbonEvent:(EventRecord *)carbonEvent
{
    carbonEvent->what = nullEvent;
    carbonEvent->message = 0;
    carbonEvent->when = TickCount();
    GetGlobalMouse(&carbonEvent->where);
    carbonEvent->modifiers = GetCurrentKeyModifiers();
    if (!Button())
        carbonEvent->modifiers |= btnState;
}

- (void)getCarbonEvent:(EventRecord *)carbonEvent
{
    [[self class] getCarbonEvent:carbonEvent];
}

- (EventModifiers)modifiersForEvent:(NSEvent *)event
{
    EventModifiers modifiers;
    unsigned int modifierFlags = [event modifierFlags];
    NSEventType eventType = [event type];
    
    modifiers = 0;
    
    if (eventType != NSLeftMouseDown && eventType != NSRightMouseDown)
        modifiers |= btnState;
    
    if (modifierFlags & NSCommandKeyMask)
        modifiers |= cmdKey;
    
    if (modifierFlags & NSShiftKeyMask)
        modifiers |= shiftKey;

    if (modifierFlags & NSAlphaShiftKeyMask)
        modifiers |= alphaLock;

    if (modifierFlags & NSAlternateKeyMask)
        modifiers |= optionKey;

    if (modifierFlags & NSControlKeyMask || eventType == NSRightMouseDown)
        modifiers |= controlKey;
    
    return modifiers;
}

- (void)getCarbonEvent:(EventRecord *)carbonEvent withEvent:(NSEvent *)cocoaEvent
{
    if ([cocoaEvent _eventRef] && ConvertEventRefToEventRecord([cocoaEvent _eventRef], carbonEvent)) {
        return;
    }
    
    NSPoint where = [[cocoaEvent window] convertBaseToScreen:[cocoaEvent locationInWindow]];
        
    carbonEvent->what = nullEvent;
    carbonEvent->message = 0;
    carbonEvent->when = (UInt32)([cocoaEvent timestamp] * 60); // seconds to ticks
    carbonEvent->where.h = (short)where.x;
    carbonEvent->where.v = (short)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - where.y);
    carbonEvent->modifiers = [self modifiersForEvent:cocoaEvent];
}

- (PortState)saveAndSetPortStateForUpdate:(BOOL)forUpdate
{
    ASSERT([self currentWindow]);
    
    WindowRef windowRef = [[self currentWindow] windowRef];
    CGrafPtr port = GetWindowPort(windowRef);

    Rect portBounds;
    GetPortBounds(port, &portBounds);

    // Use AppKit to convert view coordinates to NSWindow coordinates.

    NSRect boundsInWindow = [self convertRect:[self bounds] toView:nil];
    NSRect visibleRectInWindow = [self convertRect:[self visibleRect] toView:nil];
    
    // Flip Y to convert NSWindow coordinates to top-left-based window coordinates.

    float borderViewHeight = [[self currentWindow] frame].size.height;
    boundsInWindow.origin.y = borderViewHeight - NSMaxY(boundsInWindow);
    visibleRectInWindow.origin.y = borderViewHeight - NSMaxY(visibleRectInWindow);
    
    // Look at the Carbon port to convert top-left-based window coordinates into top-left-based content coordinates.

    PixMap *pix = *GetPortPixMap(port);
    boundsInWindow.origin.x += pix->bounds.left - portBounds.left;
    boundsInWindow.origin.y += pix->bounds.top - portBounds.top;
    visibleRectInWindow.origin.x += pix->bounds.left - portBounds.left;
    visibleRectInWindow.origin.y += pix->bounds.top - portBounds.top;
    
    // Set up NS_Port.
    
    nPort.port = port;
    nPort.portx = (int32)-boundsInWindow.origin.x;
    nPort.porty = (int32)-boundsInWindow.origin.y;
    
    // Set up NPWindow.
    
    window.window = &nPort;
    
    window.x = (int32)boundsInWindow.origin.x; 
    window.y = (int32)boundsInWindow.origin.y;
    window.width = NSWidth(boundsInWindow);
    window.height = NSHeight(boundsInWindow);

    window.clipRect.top = (uint16)visibleRectInWindow.origin.y;
    window.clipRect.left = (uint16)visibleRectInWindow.origin.x;
    window.clipRect.bottom = (uint16)(visibleRectInWindow.origin.y + visibleRectInWindow.size.height);
    window.clipRect.right = (uint16)(visibleRectInWindow.origin.x + visibleRectInWindow.size.width);

    // Clip out the plug-in when it's not really in a window or off screen or has no height or width.
    // The "big negative number" technique is how WebCore expresses off-screen widgets.
    NSWindow *realWindow = [self window];
    if (window.width <= 0 || window.height <= 0 || window.x < -100000 || realWindow == nil || [realWindow isMiniaturized] || [NSApp isHidden]) {
        // The following code tries to give plug-ins the same size they will eventually have.
        // The specifiedWidth and specifiedHeight variables are used to predict the size that
        // WebCore will eventually resize us to.

        // The QuickTime plug-in has problems if you give it a width or height of 0.
        // Since other plug-ins also might have the same sort of trouble, we make sure
        // to always give plug-ins a size other than 0,0.

        if (window.width <= 0) {
            window.width = specifiedWidth > 0 ? specifiedWidth : 100;
        }
        if (window.height <= 0) {
            window.height = specifiedHeight > 0 ? specifiedHeight : 100;
        }

        window.clipRect.bottom = window.clipRect.top;
        window.clipRect.left = window.clipRect.right;
    }

    window.type = NPWindowTypeWindow;
    
    // Save the port state.

    PortState portState;
    
    GetPort(&portState.oldPort);    

    portState.oldOrigin.h = portBounds.left;
    portState.oldOrigin.v = portBounds.top;

    portState.oldClipRegion = NewRgn();
    GetPortClipRegion(port, portState.oldClipRegion);
    
    portState.oldVisibleRegion = NewRgn();
    GetPortVisibleRegion(port, portState.oldVisibleRegion);
    
    RgnHandle clipRegion = NewRgn();
    portState.clipRegion = clipRegion;
    
    MacSetRectRgn(clipRegion,
        window.clipRect.left + nPort.portx, window.clipRect.top + nPort.porty,
        window.clipRect.right + nPort.portx, window.clipRect.bottom + nPort.porty);
    
    portState.forUpdate = forUpdate;
    
    // Switch to the port and set it up.

    SetPort(port);

    PenNormal();
    ForeColor(blackColor);
    BackColor(whiteColor);
    
    SetOrigin(nPort.portx, nPort.porty);

    SetPortClipRegion(nPort.port, clipRegion);

    if (forUpdate) {
        // AppKit may have tried to help us by doing a BeginUpdate.
        // But the invalid region at that level didn't include AppKit's notion of what was not valid.
        // We reset the port's visible region to counteract what BeginUpdate did.
        SetPortVisibleRegion(nPort.port, clipRegion);

        // Some plugins do their own BeginUpdate/EndUpdate.
        // For those, we must make sure that the update region contains the area we want to draw.
        InvalWindowRgn(windowRef, clipRegion);
    }
    
    return portState;
}

- (PortState)saveAndSetPortState
{
    return [self saveAndSetPortStateForUpdate:NO];
}

- (void)restorePortState:(PortState)portState
{
    ASSERT([self currentWindow]);
    
    WindowRef windowRef = [[self currentWindow] windowRef];
    CGrafPtr port = GetWindowPort(windowRef);

    if (portState.forUpdate) {
        ValidWindowRgn(windowRef, portState.clipRegion);
    }
    
    SetOrigin(portState.oldOrigin.h, portState.oldOrigin.v);

    SetPortClipRegion(port, portState.oldClipRegion);
    if (portState.forUpdate) {
        SetPortVisibleRegion(port, portState.oldVisibleRegion);
    }

    DisposeRgn(portState.oldClipRegion);
    DisposeRgn(portState.oldVisibleRegion);
    DisposeRgn(portState.clipRegion);

    SetPort(portState.oldPort);
}

- (BOOL)sendEvent:(EventRecord *)event
{
    ASSERT([self window]);

    if (!isStarted) {
        return NO;
    }

    ASSERT(NPP_HandleEvent);
    
    // Make sure we don't call NPP_HandleEvent while we're inside NPP_SetWindow.
    // We probably don't want more general reentrancy protection; we are really
    // protecting only against this one case, which actually comes up when
    // you first install the SVG viewer plug-in.
    if (inSetWindow) {
        return NO;
    }

    BOOL defers = [[self webView] defersCallbacks];
    if (!defers) {
        [[self webView] setDefersCallbacks:YES];
    }

    PortState portState = [self saveAndSetPortStateForUpdate:event->what == updateEvt];

#ifndef NDEBUG
    // Draw green to help debug.
    // If we see any green we know something's wrong.
    if (event->what == updateEvt) {
        ForeColor(greenColor);
        const Rect bigRect = { -10000, -10000, 10000, 10000 };
        PaintRect(&bigRect);
        ForeColor(blackColor);
    }
#endif
    
    // Temporarily retain self in case the plug-in view is released while sending an event. 
    [self retain];

    BOOL acceptedEvent = NPP_HandleEvent(instance, event);

    if ([self currentWindow]) {
        [self restorePortState:portState];
    }

    if (!defers) {
        [[self webView] setDefersCallbacks:NO];
    }
    
    [self release];
    
    return acceptedEvent;
}

- (void)sendActivateEvent:(BOOL)activate
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = activateEvt;
    WindowRef windowRef = [[self window] windowRef];
    event.message = (UInt32)windowRef;
    if (activate) {
        event.modifiers |= activeFlag;
    }
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(activateEvent): %d  isActive: %d", acceptedEvent, activate);
}

- (BOOL)sendUpdateEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = updateEvt;
    WindowRef windowRef = [[self window] windowRef];
    event.message = (UInt32)windowRef;

    BOOL acceptedEvent = [self sendEvent:&event];

    LOG(PluginEvents, "NPP_HandleEvent(updateEvt): %d", acceptedEvent);

    return acceptedEvent;
}

-(void)sendNullEvent
{
    EventRecord event;

    [self getCarbonEvent:&event];

    // Plug-in should not react to cursor position when not active or when a menu is down.
    MenuTrackingData trackingData;
    OSStatus error = GetMenuTrackingData(NULL, &trackingData);

    // Plug-in should not react to cursor position when the actual window is not key.
    if (![[self window] isKeyWindow] || (error == noErr && trackingData.menu)) {
        // FIXME: Does passing a v and h of -1 really prevent it from reacting to the cursor position?
        event.where.v = -1;
        event.where.h = -1;
    }

    [self sendEvent:&event];
}

- (void)stopNullEvents
{
    [nullEventTimer invalidate];
    [nullEventTimer release];
    nullEventTimer = nil;
}

- (void)restartNullEvents
{
    ASSERT([self window]);
    
    if (nullEventTimer) {
        [self stopNullEvents];
    }
    
    if ([[self window] isMiniaturized]) {
        return;
    }

    NSTimeInterval interval;

    // Send null events less frequently when the actual window is not key.
    if ([[self window] isKeyWindow]) {
        interval = NullEventIntervalActive;
    } else {
        interval = NullEventIntervalNotActive;
    }
    
    nullEventTimer = [[NSTimer scheduledTimerWithTimeInterval:interval
                                                       target:self
                                                     selector:@selector(sendNullEvent)
                                                     userInfo:nil
                                                      repeats:YES] retain];
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)installKeyEventHandler
{
    static const EventTypeSpec sTSMEvents[] =
    {
    { kEventClassTextInput, kEventTextInputUnicodeForKeyEvent }
    };
    
    if (!keyEventHandler) {
        InstallEventHandler(GetWindowEventTarget([[self window] windowRef]),
                            NewEventHandlerUPP(TSMEventHandler),
                            GetEventTypeCount(sTSMEvents),
                            sTSMEvents,
                            self,
                            &keyEventHandler);
    }
}

- (void)removeKeyEventHandler
{
    if (keyEventHandler) {
        RemoveEventHandler(keyEventHandler);
        keyEventHandler = NULL;
    }
}

- (BOOL)becomeFirstResponder
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = getFocusEvent;
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(getFocusEvent): %d", acceptedEvent);
    
    [self installKeyEventHandler];
        
    return YES;
}

- (BOOL)resignFirstResponder
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = loseFocusEvent;
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(loseFocusEvent): %d", acceptedEvent);
    
    [self removeKeyEventHandler];
    
    return YES;
}

// AppKit doesn't call mouseDown or mouseUp on right-click. Simulate control-click
// mouseDown and mouseUp so plug-ins get the right-click event as they do in Carbon (3125743).
- (void)rightMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

- (void)mouseDown:(NSEvent *)theEvent
{
    EventRecord event;

    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = mouseDown;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(mouseDown): %d pt.v=%d, pt.h=%d", acceptedEvent, event.where.v, event.where.h);
}

- (void)mouseUp:(NSEvent *)theEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = mouseUp;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(mouseUp): %d pt.v=%d, pt.h=%d", acceptedEvent, event.where.v, event.where.h);
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(mouseEntered): %d", acceptedEvent);
}

- (void)mouseExited:(NSEvent *)theEvent
{
    EventRecord event;
        
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(mouseExited): %d", acceptedEvent);
    
    // Set cursor back to arrow cursor.
    [[NSCursor arrowCursor] set];
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    // Do nothing so that other responders don't respond to the drag that initiated in this view.
}

- (void)keyUp:(NSEvent *)theEvent
{
    TSMProcessRawKeyEvent([theEvent _eventRef]);
}

- (void)keyDown:(NSEvent *)theEvent
{
    TSMProcessRawKeyEvent([theEvent _eventRef]);
}

static OSStatus TSMEventHandler(EventHandlerCallRef inHandlerRef, EventRef inEvent, void *pluginView)
{
    EventRef rawKeyEventRef;
    OSStatus status = GetEventParameter(inEvent, kEventParamTextInputSendKeyboardEvent, typeEventRef, NULL, sizeof(EventRef), NULL, &rawKeyEventRef);
    if (status != noErr) {
        ERROR("GetEventParameter failed with error: %d", status);
        return noErr;
    }
    
    // Two-pass read to allocate/extract Mac charCodes
    UInt32 numBytes;    
    status = GetEventParameter(rawKeyEventRef, kEventParamKeyMacCharCodes, typeChar, NULL, 0, &numBytes, NULL);
    if (status != noErr) {
        ERROR("GetEventParameter failed with error: %d", status);
        return noErr;
    }
    char *buffer = malloc(numBytes);
    status = GetEventParameter(rawKeyEventRef, kEventParamKeyMacCharCodes, typeChar, NULL, numBytes, NULL, buffer);
    if (status != noErr) {
        ERROR("GetEventParameter failed with error: %d", status);
        free(buffer);
        return noErr;
    }
    
    EventRef cloneEvent = CopyEvent(rawKeyEventRef);
    unsigned i;
    for (i = 0; i < numBytes; i++) {
        status = SetEventParameter(cloneEvent, kEventParamKeyMacCharCodes, typeChar, 1 /* one char code */, &buffer[i]);
        if (status != noErr) {
            ERROR("SetEventParameter failed with error: %d", status);
            free(buffer);
            return noErr;
        }
        
        EventRecord eventRec;
        if (ConvertEventRefToEventRecord(cloneEvent, &eventRec)) {
            BOOL acceptedEvent;
            acceptedEvent = [(WebBaseNetscapePluginView *)pluginView sendEvent:&eventRec];
            
            LOG(PluginEvents, "NPP_HandleEvent(keyDown): %d charCode:%c keyCode:%lu",
                acceptedEvent, (char) (eventRec.message & charCodeMask), (eventRec.message & keyCodeMask));
            
            // We originally thought that if the plug-in didn't accept this event,
            // we should pass it along so that keyboard scrolling, for example, will work.
            // In practice, this is not a good idea, because plug-ins tend to eat the event but return false.
            // MacIE handles each key event twice because of this, but we will emulate the other browsers instead.
        }
    }
    ReleaseEvent(cloneEvent);
    
    free(buffer);
    return noErr;
}

#pragma mark WEB_NETSCAPE_PLUGIN

- (BOOL)isNewWindowEqualToOldWindow
{
    if (window.x != lastSetWindow.x) {
        return NO;
    }
    if (window.y != lastSetWindow.y) {
        return NO;
    }
    if (window.width != lastSetWindow.width) {
        return NO;
    }
    if (window.height != lastSetWindow.height) {
        return NO;
    }
    if (window.clipRect.top != lastSetWindow.clipRect.top) {
        return NO;
    }
    if (window.clipRect.left != lastSetWindow.clipRect.left) {
        return NO;
    }
    if (window.clipRect.bottom  != lastSetWindow.clipRect.bottom ) {
        return NO;
    }
    if (window.clipRect.right != lastSetWindow.clipRect.right) {
        return NO;
    }
    if (window.type != lastSetWindow.type) {
        return NO;
    }
    if (nPort.portx != lastSetPort.portx) {
        return NO;
    }
    if (nPort.porty != lastSetPort.porty) {
        return NO;
    }
    if (nPort.port != lastSetPort.port) {
        return NO;
    }
    
    return YES;
}

- (void)setWindow
{
    if (!isStarted) {
        return;
    }
    
    PortState portState = [self saveAndSetPortState];

    if (![self isNewWindowEqualToOldWindow]) {        
        // Make sure we don't call NPP_HandleEvent while we're inside NPP_SetWindow.
        // We probably don't want more general reentrancy protection; we are really
        // protecting only against this one case, which actually comes up when
        // you first install the SVG viewer plug-in.
        NPError npErr;
        ASSERT(!inSetWindow);
        
        inSetWindow = YES;
        npErr = NPP_SetWindow(instance, &window);
        inSetWindow = NO;

        LOG(Plugins, "NPP_SetWindow: %d, port=0x%08x, window.x:%d window.y:%d",
            npErr, (int)nPort.port, (int)window.x, (int)window.y);

        lastSetWindow = window;
        lastSetPort = nPort;
    }

    [self restorePortState:portState];
}

- (void)removeTrackingRect
{
    if (trackingTag) {
        [self removeTrackingRect:trackingTag];
        trackingTag = 0;

        // Must release the window to balance the retain in resetTrackingRect.
        // But must do it after setting trackingTag to 0 so we don't re-enter.
        [[self window] release];
    }
}

- (void)resetTrackingRect
{
    [self removeTrackingRect];
    if (isStarted) {
        // Must retain the window so that removeTrackingRect can work after the window is closed.
        [[self window] retain];
        trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
    }
}

+ (void)setCurrentPluginView:(WebBaseNetscapePluginView *)view
{
    currentPluginView = view;
}

+ (WebBaseNetscapePluginView *)currentPluginView
{
    return currentPluginView;
}

- (BOOL)canStart
{
    return YES;
}

- (void)didStart
{
    // Do nothing. Overridden by subclasses.
}

- (void)addWindowObservers
{
    ASSERT([self window]);

    NSWindow *theWindow = [self window];
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    NSView *view;
    for (view = self; view; view = [view superview]) {
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:)
                                   name:NSViewFrameDidChangeNotification object:view];
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:)
                                   name:NSViewBoundsDidChangeNotification object:view];
    }
    [notificationCenter addObserver:self selector:@selector(windowWillClose:)
                               name:NSWindowWillCloseNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowBecameKey:)
                               name:NSWindowDidBecomeKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowResignedKey:)
                               name:NSWindowDidResignKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowDidMiniaturize:)
                               name:NSWindowDidMiniaturizeNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowDidDeminiaturize:)
                               name:NSWindowDidDeminiaturizeNotification object:theWindow];
}

- (void)removeWindowObservers
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:NSViewFrameDidChangeNotification     object:nil];
    [notificationCenter removeObserver:self name:NSViewBoundsDidChangeNotification    object:nil];
    [notificationCenter removeObserver:self name:NSWindowWillCloseNotification        object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidBecomeKeyNotification     object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidResignKeyNotification     object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidMiniaturizeNotification   object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidDeminiaturizeNotification object:nil];
}

- (BOOL)start
{
    ASSERT([self currentWindow]);
    
    if (isStarted) {
        return YES;
    }

    if (![[WebPreferences standardPreferences] arePlugInsEnabled] || ![self canStart]) {
        return NO;
    }

    ASSERT(NPP_New);

    [[self class] setCurrentPluginView:self];
    NPError npErr = NPP_New((char *)[MIMEType cString], instance, mode, argsCount, cAttributes, cValues, NULL);
    [[self class] setCurrentPluginView:nil];
    
    LOG(Plugins, "NPP_New: %d", npErr);
    if (npErr != NPERR_NO_ERROR) {
        ERROR("NPP_New failed with error: %d", npErr);
        return NO;
    }

    isStarted = YES;
        
    [self setWindow];

    if ([self window]) {
        [self addWindowObservers];
        if ([[self window] isKeyWindow]) {
            [self sendActivateEvent:YES];
        }
        [self restartNullEvents];
    }

    [self resetTrackingRect];
    
    [self didStart];
    
    return YES;
}

- (void)stop
{
    [self removeTrackingRect];

    if (!isStarted) {
        return;
    }
    
    isStarted = NO;
    
    // Stop any active streams
    [streams makeObjectsPerformSelector:@selector(stop)];
    
    // Stop the null events
    [self stopNullEvents];

    // Set cursor back to arrow cursor
    [[NSCursor arrowCursor] set];
    
    // Stop notifications and callbacks.
    [self removeWindowObservers];
    [NSObject cancelPreviousPerformRequestsWithTarget:self];

    // Setting the window type to 0 ensures that NPP_SetWindow will be called if the plug-in is restarted.
    lastSetWindow.type = 0;
    
    NPError npErr;
    npErr = NPP_Destroy(instance, NULL);
    LOG(Plugins, "NPP_Destroy: %d", npErr);

    instance->pdata = NULL;
    
    // We usually remove the key event handler in resignFirstResponder but it is possible that resignFirstResponder 
    // may never get called so we can't completely rely on it.
    [self removeKeyEventHandler];
}

- (BOOL)isStarted
{
    return isStarted;
}

- (WebDataSource *)dataSource
{
    // Do nothing. Overridden by subclasses.
    return nil;
}

- (WebFrame *)webFrame
{
    return [[self dataSource] webFrame];
}

- (WebView *)webView
{
    return [[self webFrame] webView];
}

- (NSWindow *)currentWindow
{
    return [self window] ? [self window] : [[self webView] hostWindow];
}

- (NPP)pluginPointer
{
    return instance;
}

- (WebNetscapePluginPackage *)plugin
{
    return plugin;
}

- (void)setPlugin:(WebNetscapePluginPackage *)thePlugin;
{
    [thePlugin retain];
    [plugin release];
    plugin = thePlugin;

    NPP_New = 		[plugin NPP_New];
    NPP_Destroy = 	[plugin NPP_Destroy];
    NPP_SetWindow = 	[plugin NPP_SetWindow];
    NPP_NewStream = 	[plugin NPP_NewStream];
    NPP_WriteReady = 	[plugin NPP_WriteReady];
    NPP_Write = 	[plugin NPP_Write];
    NPP_StreamAsFile = 	[plugin NPP_StreamAsFile];
    NPP_DestroyStream = [plugin NPP_DestroyStream];
    NPP_HandleEvent = 	[plugin NPP_HandleEvent];
    NPP_URLNotify = 	[plugin NPP_URLNotify];
    NPP_GetValue = 	[plugin NPP_GetValue];
    NPP_SetValue = 	[plugin NPP_SetValue];
    NPP_Print = 	[plugin NPP_Print];
}

- (void)setMIMEType:(NSString *)theMIMEType
{
    NSString *type = [theMIMEType copy];
    [MIMEType release];
    MIMEType = type;
}

- (void)setBaseURL:(NSURL *)theBaseURL
{
    [theBaseURL retain];
    [baseURL release];
    baseURL = theBaseURL;
}

- (void)setAttributeKeys:(NSArray *)keys andValues:(NSArray *)values;
{
    ASSERT([keys count] == [values count]);
    
    // Convert the attributes to 2 C string arrays.
    // These arrays are passed to NPP_New, but the strings need to be
    // modifiable and live the entire life of the plugin.

    // The Java plug-in requires the first argument to be the base URL
    if ([MIMEType isEqualToString:@"application/x-java-applet"]) {
        cAttributes = (char **)malloc(([keys count] + 1) * sizeof(char *));
        cValues = (char **)malloc(([values count] + 1) * sizeof(char *));
        cAttributes[0] = strdup("DOCBASE");
        cValues[0] = strdup([baseURL _web_URLCString]);
        argsCount++;
    } else {
        cAttributes = (char **)malloc([keys count] * sizeof(char *));
        cValues = (char **)malloc([values count] * sizeof(char *));
    }

    unsigned i;
    unsigned count = [keys count];
    for (i = 0; i < count; i++) {
        NSString *key = [keys objectAtIndex:i];
        NSString *value = [values objectAtIndex:i];
        if ([key _web_isCaseInsensitiveEqualToString:@"height"]) {
            specifiedHeight = [value intValue];
        } else if ([key _web_isCaseInsensitiveEqualToString:@"width"]) {
            specifiedWidth = [value intValue];
        }
        cAttributes[argsCount] = strdup([key UTF8String]);
        cValues[argsCount] = strdup([value UTF8String]);
        LOG(Plugins, "%@ = %@", key, value);
        argsCount++;
    }
}

- (void)setMode:(int)theMode
{
    mode = theMode;
}

#pragma mark NSVIEW

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];

    instance = &instanceStruct;
    instance->ndata = self;

    streams = [[NSMutableArray alloc] init];
    streamNotifications = [[NSMutableDictionary alloc] init];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(preferencesHaveChanged:)
                                                 name:WebPreferencesChangedNotification
                                               object:nil];

    return self;
}

-(void)dealloc
{
    unsigned i;

    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [self stop];

    for (i = 0; i < argsCount; i++) {
        free(cAttributes[i]);
        free(cValues[i]);
    }
    [plugin release];
    [streams release];
    [MIMEType release];
    [baseURL release];
    [streamNotifications release];
    free(cAttributes);
    free(cValues);
    [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
    if (!isStarted) {
        return;
    }
    
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        [self sendUpdateEvent];
    } else {
        // Printing 2862383
    }
}

- (BOOL)isFlipped
{
    return YES;
}

-(void)tellQuickTimeToChill
{
    // Make a call to the secret QuickDraw API that makes QuickTime calm down.
    WindowRef windowRef = [[self window] windowRef];
    if (!windowRef) {
        return;
    }
    CGrafPtr port = GetWindowPort(windowRef);
    Rect bounds;
    GetPortBounds(port, &bounds);
    CallDrawingNotifications(port, &bounds, kBitsProc);
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
    [self tellQuickTimeToChill];

    // We must remove the tracking rect before we move to the new window.
    // Once we move to the new window, it will be too late.
    [self removeTrackingRect];
    [self removeWindowObservers];

    if (!newWindow) {
        if ([[self webView] hostWindow]) {
            // View will be moved out of the actual window but it still has a host window.
            [self stopNullEvents];
        } else {
            // View will have no associated windows.
            [self stop];
        }
    }
}

- (void)viewDidMoveToWindow
{
    [self resetTrackingRect];
    
    if ([self window]) {
        // View moved to an actual window. Start it if not already started.
        [self start];
        [self restartNullEvents];
        [self addWindowObservers];
    } else if ([[self webView] hostWindow]) {
        // View moved out of an actual window, but still has a host window.
        // Call setWindow to explicitly "clip out" the plug-in from sight.
        // FIXME: It would be nice to do this where we call stopNullEvents in viewWillMoveToWindow.
        [self setWindow];
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    if (!hostWindow && ![self window]) {
        // View will have no associated windows.
        [self stop];
    }
}

- (void)viewDidMoveToHostWindow
{
    if ([[self webView] hostWindow]) {
        // View now has an associated window. Start it if not already started.
        [self start];
    }
}

#pragma mark NOTIFICATIONS

-(void)viewHasMoved:(NSNotification *)notification
{
    [self tellQuickTimeToChill];
    [self setWindow];
    [self resetTrackingRect];
}

-(void)windowWillClose:(NSNotification *)notification
{
    [self stop];
}

-(void)windowBecameKey:(NSNotification *)notification
{
    [self sendActivateEvent:YES];
    [self setNeedsDisplay:YES];
    [self restartNullEvents];
}

-(void)windowResignedKey:(NSNotification *)notification
{
    [self sendActivateEvent:NO];
    [self setNeedsDisplay:YES];
    [self restartNullEvents];
}

-(void)windowDidMiniaturize:(NSNotification *)notification
{
    [self stopNullEvents];
}

-(void)windowDidDeminiaturize:(NSNotification *)notification
{
    [self restartNullEvents];
}

- (void)preferencesHaveChanged:(NSNotification *)notification
{
    WebPreferences *preferences = [[self webView] preferences];
    BOOL arePlugInsEnabled = [preferences arePlugInsEnabled];
    
    if ([notification object] == preferences && isStarted != arePlugInsEnabled) {
        if (arePlugInsEnabled) {
            if ([self currentWindow]) {
                [self start];
            }
        } else {
            [self stop];
            [self setNeedsDisplay:YES];
        }
    }
}

- (void)frameStateChanged:(NSNotification *)notification
{
    WebFrame *frame = [notification object];
    NSURL *URL = [[[frame dataSource] request] URL];
    NSValue *notifyDataValue = [streamNotifications objectForKey:URL];
    if (!notifyDataValue) {
        return;
    }
    
    void *notifyData = [notifyDataValue pointerValue];
    WebFrameState frameState = [[[notification userInfo] objectForKey:WebCurrentFrameState] intValue];
    if (frameState == WebFrameStateComplete) {
        if (isStarted) {
            NPP_URLNotify(instance, [URL _web_URLCString], NPRES_DONE, notifyData);
        }
        [streamNotifications removeObjectForKey:URL];
    }

    //FIXME: Need to send other NPReasons
}

@end

@implementation WebBaseNetscapePluginView (WebNPPCallbacks)

- (NSMutableURLRequest *)requestWithURLCString:(const char *)URLCString
{
    if (!URLCString) {
        return nil;
    }
    
    NSString *string = (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, URLCString, kCFStringEncodingWindowsLatin1);
    NSString *URLString = [string _web_stringByStrippingReturnCharacters];
    [string release];
    
    NSURL *URL = [NSURL _web_URLWithDataAsString:URLString relativeToURL:baseURL];
    

    if (!URL) {
        return nil;
    }
    
    return [NSMutableURLRequest requestWithURL:URL];
}

- (void)evaluateJavaScriptPluginRequest:(WebPluginRequest *)JSPluginRequest
{
    // FIXME: Is this isStarted check needed here? evaluateJavaScriptPluginRequest should not be called
    // if we are stopped since this method is called after a delay and we call 
    // cancelPreviousPerformRequestsWithTarget inside of stop.
    if (!isStarted) {
        return;
    }
    
    NSURL *URL = [[JSPluginRequest request] URL];
    NSString *JSString = [URL _web_scriptIfJavaScriptURL];
    ASSERT(JSString);
    
    NSString *result = [[[self webFrame] _bridge] stringByEvaluatingJavaScriptFromString:JSString];
    
    // Don't continue if stringByEvaluatingJavaScriptFromString caused the plug-in to stop.
    if (!isStarted) {
        return;
    }
    
    void *notifyData = [JSPluginRequest notifyData];
    
    if ([JSPluginRequest frameName] != nil) {
        // FIXME: If the result is a string, we probably want to put that string into the frame, just
        // like we do in KHTMLPartBrowserExtension::openURLRequest.
        if (notifyData) {
            NPP_URLNotify(instance, [URL _web_URLCString], NPRES_DONE, notifyData);
        }
    } else {
        NSData *JSData = nil;
        
        if ([result length] > 0) {
            JSData = [result dataUsingEncoding:NSUTF8StringEncoding];
        }
        
        WebBaseNetscapePluginStream *stream = [[WebBaseNetscapePluginStream alloc] init];
        [stream setPluginPointer:instance];
        [stream setNotifyData:notifyData];
        [stream startStreamWithURL:URL
             expectedContentLength:[JSData length]
                  lastModifiedDate:nil
                          MIMEType:@"text/plain"];
        [stream receivedData:JSData];
        [stream finishedLoadingWithData:JSData];
        [stream release];
    }
}

- (void)loadPluginRequest:(WebPluginRequest *)pluginRequest
{
    NSURLRequest *request = [pluginRequest request];
    NSString *frameName = [pluginRequest frameName];
    void *notifyData = [pluginRequest notifyData];
    WebFrame *frame = nil;
    
    NSURL *URL = [request URL];
    NSString *JSString = [URL _web_scriptIfJavaScriptURL];
    
    ASSERT(frameName || JSString);
    
    if (frameName) {
        // FIXME - need to get rid of this window creation which
        // bypasses normal targeted link handling
        frame = [[self webFrame] findFrameNamed:frameName];
    
        if (frame == nil) {
            WebView *newWebView = nil;
            WebView *currentWebView = [self webView];
            id wd = [currentWebView UIDelegate];
            if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)]) {
                newWebView = [wd webView:currentWebView createWebViewWithRequest:nil];
            } else {
                newWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:currentWebView createWebViewWithRequest:nil];
            }
            
            [newWebView _setTopLevelFrameName:frameName];
            [[newWebView _UIDelegateForwarder] webViewShow:newWebView];
            frame = [newWebView mainFrame];
        }
    }

    if (JSString) {
        ASSERT(frame == nil || [self webFrame] == frame);
        [self evaluateJavaScriptPluginRequest:pluginRequest];
    } else {
        [frame loadRequest:request];
        if (notifyData) {
            // FIXME: How do we notify about failures? It seems this will only notify about success.
        
            // FIXME: This will overwrite any previous notification for the same URL.
            // It might be better to keep track of these per frame.
            [streamNotifications setObject:[NSValue valueWithPointer:notifyData] forKey:URL];
            
            // FIXME: We add this same observer to a frame multiple times. Is that OK?
            // FIXME: This observer doesn't get removed until the plugin stops, so we could
            // end up with lots and lots of these.
            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(frameStateChanged:)
                                                         name:WebFrameStateChangedNotification
                                                       object:frame];
        }
    }
}

- (NPError)loadRequest:(NSMutableURLRequest *)request inTarget:(const char *)cTarget withNotifyData:(void *)notifyData
{
    NSURL *URL = [request URL];

    if (!URL) {
        return NPERR_INVALID_URL;
    }
    
    NSString *JSString = [URL _web_scriptIfJavaScriptURL];
    if (JSString != nil && cTarget == NULL && mode == NP_FULL) {
        // Don't allow a JavaScript request from a standalone plug-in that is self-targetted
        // because this can cause the user to be redirected to a blank page (3424039).
        return NPERR_INVALID_PARAM;
    }
        
    if (cTarget || JSString) {
        // Make when targetting a frame or evaluating a JS string, perform the request after a delay because we don't
        // want to potentially kill the plug-in inside of its URL request.
        NSString *target = nil;
        if (cTarget) {
            // Find the frame given the target string.
            target = (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, cTarget, kCFStringEncodingWindowsLatin1);
        }
        
        WebFrame *frame = [self webFrame];
        if (JSString != nil && target != nil && [frame findFrameNamed:target] != frame) {
            // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
            return NPERR_INVALID_PARAM;
        }
        
        [request setHTTPReferrer:[[[[[self webFrame] dataSource] request] URL] _web_originalDataAsString]];
        WebPluginRequest *pluginRequest = [[WebPluginRequest alloc] initWithRequest:request frameName:target notifyData:notifyData];
        [self performSelector:@selector(loadPluginRequest:) withObject:pluginRequest afterDelay:0];
        [pluginRequest release];
        [target release];
    } else {
        WebNetscapePluginStream *stream = [[WebNetscapePluginStream alloc]
            initWithRequest:request pluginPointer:instance notifyData:notifyData];
        if (!stream) {
            return NPERR_INVALID_URL;
        }
        [streams addObject:stream];
        [stream start];
        [stream release];
    }
    
    return NPERR_NO_ERROR;
}

-(NPError)getURLNotify:(const char *)URLCString target:(const char *)cTarget notifyData:(void *)notifyData
{
    LOG(Plugins, "NPN_GetURLNotify: %s target: %s", URLCString, cTarget);

    NSMutableURLRequest *request = [self requestWithURLCString:URLCString];
    return [self loadRequest:request inTarget:cTarget withNotifyData:notifyData];
}

-(NPError)getURL:(const char *)URLCString target:(const char *)cTarget
{
    LOG(Plugins, "NPN_GetURL: %s target: %s", URLCString, cTarget);

    NSMutableURLRequest *request = [self requestWithURLCString:URLCString];
    return [self loadRequest:request inTarget:cTarget withNotifyData:NULL];
}

- (NPError)_postURLNotify:(const char *)URLCString
                   target:(const char *)target
                      len:(UInt32)len
                      buf:(const char *)buf
                     file:(NPBool)file
               notifyData:(void *)notifyData
             allowHeaders:(BOOL)allowHeaders
{
    if (!URLCString || !len || !buf) {
        return NPERR_INVALID_PARAM;
    }
    
    NSData *postData = nil;

    if (file) {
        // If we're posting a file, buf is either a file URL or a path to the file.
        NSString *bufString = (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, buf, kCFStringEncodingWindowsLatin1);
        if (!bufString) {
            return NPERR_INVALID_PARAM;
        }
        NSURL *fileURL = [NSURL _web_URLWithDataAsString:bufString];
        NSString *path;
        if ([fileURL isFileURL]) {
            path = [fileURL path];
        } else {
            path = bufString;
        }
        postData = [NSData dataWithContentsOfFile:[path _web_fixedCarbonPOSIXPath]];
        [bufString release];
        if (!postData) {
            return NPERR_FILE_NOT_FOUND;
        }
    } else {
        postData = [NSData dataWithBytes:buf length:len];
    }

    if ([postData length] == 0) {
        return NPERR_INVALID_PARAM;
    }

    NSMutableURLRequest *request = [self requestWithURLCString:URLCString];
    [request setHTTPMethod:@"POST"];
    
    if (allowHeaders) {
        if ([postData _web_startsWithBlankLine]) {
            postData = [postData subdataWithRange:NSMakeRange(1, [postData length] - 1)];
        } else {
            unsigned location = [postData _web_locationAfterFirstBlankLine];
            if (location != NSNotFound) {
                // If the blank line is somewhere in the middle of postData, everything before is the header.
                NSData *headerData = [postData subdataWithRange:NSMakeRange(0, location)];
                NSMutableDictionary *header = [headerData _web_parseRFC822HeaderFields];
		unsigned dataLength = [postData length] - location;

		// Sometimes plugins like to set Content-Length themselves when they post,
		// but WebFoundation does not like that. So we will remove the header
		// and instead truncate the data to the requested length.
		NSString *contentLength = [header objectForKey:@"Content-Length"];

		if (contentLength != nil) {
		    dataLength = MIN((unsigned)[contentLength intValue], dataLength);
		}
		[header removeObjectForKey:@"Content-Length"];

                if ([header count] > 0) {
                    [request setAllHTTPHeaderFields:header];
                }
                // Everything after the blank line is the actual content of the POST.
                postData = [postData subdataWithRange:NSMakeRange(location, dataLength)];

            }
        }
        if ([postData length] == 0) {
            return NPERR_INVALID_PARAM;
        }
    }

    // Plug-ins expect to receive uncached data when doing a POST (3347134).
    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    [request setHTTPBody:postData];
    
    return [self loadRequest:request inTarget:target withNotifyData:notifyData];
}

- (NPError)postURLNotify:(const char *)URLCString
                  target:(const char *)target
                     len:(UInt32)len
                     buf:(const char *)buf
                    file:(NPBool)file
              notifyData:(void *)notifyData
{
    LOG(Plugins, "NPN_PostURLNotify: %s", URLCString);
    return [self _postURLNotify:URLCString target:target len:len buf:buf file:file notifyData:notifyData allowHeaders:YES];
}

-(NPError)postURL:(const char *)URLCString
           target:(const char *)target
              len:(UInt32)len
              buf:(const char *)buf
             file:(NPBool)file
{
    LOG(Plugins, "NPN_PostURL: %s", URLCString);        
    // As documented, only allow headers to be specified via NPP_PostURL when using a file.
    return [self _postURLNotify:URLCString target:target len:len buf:buf file:file notifyData:NULL allowHeaders:file];
}

-(NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream
{
    LOG(Plugins, "NPN_NewStream");
    return NPERR_GENERIC_ERROR;
}

-(NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer
{
    LOG(Plugins, "NPN_Write");
    return NPERR_GENERIC_ERROR;
}

-(NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason
{
    LOG(Plugins, "NPN_DestroyStream");
    if (!stream->ndata) {
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    [(WebBaseNetscapePluginStream *)stream->ndata cancelWithReason:reason];
    return NPERR_NO_ERROR;
}

- (const char *)userAgent
{
    return [[[self webView] userAgentForURL:baseURL] lossyCString];
}

-(void)status:(const char *)message
{    
    if (!message) {
        ERROR("NPN_Status passed a NULL status message");
        return;
    }

    NSString *status = (NSString *)CFStringCreateWithCString(NULL, message, kCFStringEncodingWindowsLatin1);
    LOG(Plugins, "NPN_Status: %@", status);
    WebView *wv = [self webView];
    [[wv _UIDelegateForwarder] webView:wv setStatusText:status];
    [status release];
}

-(void)invalidateRect:(NPRect *)invalidRect
{
    LOG(Plugins, "NPN_InvalidateRect");
    [self setNeedsDisplayInRect:NSMakeRect(invalidRect->left, invalidRect->top,
        (float)invalidRect->right - invalidRect->left, (float)invalidRect->bottom - invalidRect->top)];
}

-(void)invalidateRegion:(NPRegion)invalidRegion
{
    LOG(Plugins, "NPN_InvalidateRegion");
    Rect invalidRect;
    GetRegionBounds(invalidRegion, &invalidRect);
    [self setNeedsDisplayInRect:NSMakeRect(invalidRect.left, invalidRect.top,
        (float)invalidRect.right - invalidRect.left, (float)invalidRect.bottom - invalidRect.top)];
}

-(void)forceRedraw
{
    LOG(Plugins, "forceRedraw");
    [self setNeedsDisplay:YES];
    [[self window] displayIfNeeded];
}

@end

@implementation WebPluginRequest

- (id)initWithRequest:(NSURLRequest *)request frameName:(NSString *)frameName notifyData:(void *)notifyData
{
    [super init];
    _request = [request retain];
    _frameName = [frameName retain];
    _notifyData = notifyData;
    return self;
}

- (void)dealloc
{
    [_request release];
    [_frameName release];
    [super dealloc];
}

- (NSURLRequest *)request
{
    return _request;
}

- (NSString *)frameName
{
    return _frameName;
}

- (void *)notifyData
{
    return _notifyData;
}

@end

@implementation NSData (PluginExtras)

- (BOOL)_web_startsWithBlankLine
{
    return [self length] > 0 && ((const char *)[self bytes])[0] == '\n';
}

- (unsigned)_web_locationAfterFirstBlankLine
{
    const char *bytes = (const char *)[self bytes];
    unsigned length = [self length];

    unsigned i;
    for (i = 0; i < length - 2; i++) {
        if (bytes[i] == '\n' && (i == 0 || bytes[i+1] == '\n')) {
            i++;
            while (i < length - 2 && bytes[i] == '\n') {
                i++;
            }
            return i;
        }
    }
    
    return NSNotFound;
}

@end

