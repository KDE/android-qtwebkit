/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WKView.h"

// C API
#import "WKAPICast.h"

// Implementation
#import "ChunkedUpdateDrawingAreaProxy.h"
#import "FindIndicator.h"
#import "FindIndicatorWindow.h"
#import "LayerBackedDrawingAreaProxy.h"
#import "Logging.h"
#import "NativeWebKeyboardEvent.h"
#import "PDFViewController.h"
#import "PageClientImpl.h"
#import "RunLoop.h"
#import "WKTextInputWindowController.h"
#import "WebContext.h"
#import "WebEventFactory.h"
#import "WebPage.h"
#import "WebPageProxy.h"
#import "WebProcessManager.h"
#import "WebProcessProxy.h"
#import "WebSystemInterface.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/ColorMac.h>
#import <WebCore/FloatRect.h>
#import <WebCore/IntRect.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/PlatformScreen.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

extern "C" {
    
    // Need to declare this attribute name because AppKit exports it but does not make it available in API or SPI headers.
    // <rdar://problem/8631468> tracks the request to make it available. This code should be removed when the bug is closed.
    
    extern NSString *NSTextInputReplacementRangeAttributeName;
    
}

using namespace WebKit;
using namespace WebCore;

@interface NSWindow (Details)
- (NSRect)_growBoxRect;
- (BOOL)_updateGrowBoxForWindowFrameChange;
@end

@interface WKViewData : NSObject {
@public
    OwnPtr<PageClientImpl> _pageClient;
    RefPtr<WebPageProxy> _page;

    // For ToolTips.
    NSToolTipTag _lastToolTipTag;
    id _trackingRectOwner;
    void* _trackingRectUserData;

#if USE(ACCELERATED_COMPOSITING)
    NSView *_layerHostingView;
#endif
    // For Menus.
    HashMap<String, RetainPtr<NSMenuItem> > _menuItemsMap;

    OwnPtr<PDFViewController> _pdfViewController;

    OwnPtr<FindIndicatorWindow> _findIndicatorWindow;
    // We keep here the event when resending it to
    // the application to distinguish the case of a new event from one 
    // that has been already sent to WebCore.
    NSEvent *_keyDownEventBeingResent;
    Vector<KeypressCommand> _commandsList;

    // The identifier of the plug-in we want to send complex text input to, or 0 if there is none.
    uint64_t _pluginComplexTextInputIdentifier;

    BOOL _isSelectionNone;
    BOOL _isSelectionEditable;
    BOOL _isSelectionInPasswordField;
    BOOL _hasMarkedText;
    Vector<CompositionUnderline> _underlines;
    unsigned _selectionStart;
    unsigned _selectionEnd;
    NSRange _selectedRange;
}
@end

@implementation WKViewData
@end

@interface NSObject (NSTextInputContextDetails)
- (BOOL)wantsToHandleMouseEvents;
- (BOOL)handleMouseEvent:(NSEvent *)event;
@end

@implementation WKView

- (id)initWithFrame:(NSRect)frame
{
    return [self initWithFrame:frame contextRef:toAPI(WebContext::sharedProcessContext())];
}

- (id)initWithFrame:(NSRect)frame contextRef:(WKContextRef)contextRef
{   
    return [self initWithFrame:frame contextRef:contextRef pageGroupRef:nil];
}

- (id)initWithFrame:(NSRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    InitWebCoreSystemInterface();
    RunLoop::initializeMainRunLoop();

    NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:frame
                                                                options:(NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect)
                                                                  owner:self
                                                               userInfo:nil];
    [self addTrackingArea:trackingArea];
    [trackingArea release];

    _data = [[WKViewData alloc] init];

    _data->_pageClient = PageClientImpl::create(self);
    _data->_page = toImpl(contextRef)->createWebPage(toImpl(pageGroupRef));
    _data->_page->setPageClient(_data->_pageClient.get());
    _data->_page->setDrawingArea(ChunkedUpdateDrawingAreaProxy::create(self, _data->_page.get()));
    _data->_page->initializeWebPage(IntSize(frame.size));
    _data->_page->setIsInWindow([self window]);

    _data->_isSelectionNone = YES;
    _data->_isSelectionEditable = NO;
    _data->_isSelectionInPasswordField = NO;
    _data->_hasMarkedText = NO;
    _data->_selectedRange = NSMakeRange(NSNotFound, 0);

    WebContext::statistics().wkViewCount++;

    return self;
}

- (void)dealloc
{
    _data->_page->close();

    [_data release];

    WebContext::statistics().wkViewCount--;

    [super dealloc];
}

- (WKPageRef)pageRef
{
    return toAPI(_data->_page.get());
}

- (void)setDrawsBackground:(BOOL)drawsBackground
{
    _data->_page->setDrawsBackground(drawsBackground);
}

- (BOOL)drawsBackground
{
    return _data->_page->drawsBackground();
}

- (void)setDrawsTransparentBackground:(BOOL)drawsTransparentBackground
{
    _data->_page->setDrawsTransparentBackground(drawsTransparentBackground);
}

- (BOOL)drawsTransparentBackground
{
    return _data->_page->drawsTransparentBackground();
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    _data->_page->setFocused(true);
    return YES;
}

- (BOOL)resignFirstResponder
{
    _data->_page->setFocused(false);
    return YES;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];

    _data->_page->drawingArea()->setSize(IntSize(size));
}

- (void)_updateWindowAndViewFrames
{
    NSWindow *window = [self window];
    ASSERT(window);
    
    NSRect windowFrameInScreenCoordinates = [window frame];
    NSRect viewFrameInWindowCoordinates = [self convertRect:[self frame] toView:nil];
    
    _data->_page->windowAndViewFramesChanged(enclosingIntRect(windowFrameInScreenCoordinates), enclosingIntRect(viewFrameInWindowCoordinates));
}

- (void)renewGState
{
    // Hide the find indicator.
    _data->_findIndicatorWindow = nullptr;

    // Update the view frame.
    if ([self window])
        [self _updateWindowAndViewFrames];

    [super renewGState];
}

typedef HashMap<SEL, String> SelectorNameMap;

// Map selectors into Editor command names.
// This is not needed for any selectors that have the same name as the Editor command.
static const SelectorNameMap* createSelectorExceptionMap()
{
    SelectorNameMap* map = new HashMap<SEL, String>;
    
    map->add(@selector(insertNewlineIgnoringFieldEditor:), "InsertNewline");
    map->add(@selector(insertParagraphSeparator:), "InsertNewline");
    map->add(@selector(insertTabIgnoringFieldEditor:), "InsertTab");
    map->add(@selector(pageDown:), "MovePageDown");
    map->add(@selector(pageDownAndModifySelection:), "MovePageDownAndModifySelection");
    map->add(@selector(pageUp:), "MovePageUp");
    map->add(@selector(pageUpAndModifySelection:), "MovePageUpAndModifySelection");
    
    return map;
}

static String commandNameForSelector(SEL selector)
{
    // Check the exception map first.
    static const SelectorNameMap* exceptionMap = createSelectorExceptionMap();
    SelectorNameMap::const_iterator it = exceptionMap->find(selector);
    if (it != exceptionMap->end())
        return it->second;
    
    // Remove the trailing colon.
    // No need to capitalize the command name since Editor command names are
    // not case sensitive.
    const char* selectorName = sel_getName(selector);
    size_t selectorNameLength = strlen(selectorName);
    if (selectorNameLength < 2 || selectorName[selectorNameLength - 1] != ':')
        return String();
    return String(selectorName, selectorNameLength - 1);
}

// Editing commands
// FIXME: we should add all the commands here as we implement them.

#define WEBCORE_COMMAND(command) - (void)command:(id)sender { _data->_page->executeEditCommand(commandNameForSelector(_cmd)); }

WEBCORE_COMMAND(copy)
WEBCORE_COMMAND(cut)
WEBCORE_COMMAND(paste)
WEBCORE_COMMAND(delete)
WEBCORE_COMMAND(pasteAsPlainText)
WEBCORE_COMMAND(selectAll)
WEBCORE_COMMAND(takeFindStringFromSelection)

#undef WEBCORE_COMMAND

// Menu items validation

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    String commandName = commandNameForSelector([item action]);
    NSMenuItem *menuItem = (NSMenuItem *)item;
    if (![menuItem isKindOfClass:[NSMenuItem class]])
        return NO; // FIXME: We need to be able to handle other user interface elements.
    
    if (_data->_menuItemsMap.find(commandName) == _data->_menuItemsMap.end()) {
        _data->_menuItemsMap.add(commandName, menuItem);
        _data->_page->validateMenuItem(commandName);
    }

    return YES;
}

// Events

// Override this so that AppKit will send us arrow keys as key down events so we can
// support them via the key bindings mechanism.
- (BOOL)_wantsKeyDownForEvent:(NSEvent *)event
{
    return YES;
}


#define EVENT_HANDLER(Selector, Type) \
    - (void)Selector:(NSEvent *)theEvent \
    { \
        Web##Type##Event webEvent = WebEventFactory::createWeb##Type##Event(theEvent, self); \
        _data->_page->handle##Type##Event(webEvent); \
    }

EVENT_HANDLER(mouseEntered, Mouse)
EVENT_HANDLER(mouseExited, Mouse)
EVENT_HANDLER(mouseMoved, Mouse)
EVENT_HANDLER(otherMouseDown, Mouse)
EVENT_HANDLER(otherMouseDragged, Mouse)
EVENT_HANDLER(otherMouseMoved, Mouse)
EVENT_HANDLER(otherMouseUp, Mouse)
EVENT_HANDLER(rightMouseDown, Mouse)
EVENT_HANDLER(rightMouseDragged, Mouse)
EVENT_HANDLER(rightMouseMoved, Mouse)
EVENT_HANDLER(rightMouseUp, Mouse)
EVENT_HANDLER(scrollWheel, Wheel)

#undef EVENT_HANDLER

#define MOUSE_EVENT_HANDLER(Selector) \
    - (void)Selector:(NSEvent *)theEvent \
    { \
        NSInputManager *currentInputManager = [NSInputManager currentInputManager]; \
        if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:theEvent]) \
            return; \
        WebMouseEvent webEvent = WebEventFactory::createWebMouseEvent(theEvent, self); \
        _data->_page->handleMouseEvent(webEvent); \
    }

MOUSE_EVENT_HANDLER(mouseDown)
MOUSE_EVENT_HANDLER(mouseDragged)
MOUSE_EVENT_HANDLER(mouseUp)

#undef MOUSE_EVENT_HANDLER

- (void)doCommandBySelector:(SEL)selector
{
    if (selector != @selector(noop:))
        _data->_commandsList.append(KeypressCommand(commandNameForSelector(selector)));
}

- (void)insertText:(id)string
{
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]]; // Otherwise, NSString
    
    LOG(TextInput, "insertText:\"%@\"", isAttributedString ? [string string] : string);
    NSString *text;
    bool isFromInputMethod = _data->_hasMarkedText;

    if (isAttributedString) {
        text = [string string];
        // We deal with the NSTextInputReplacementRangeAttributeName attribute from NSAttributedString here
        // simply because it is used by at least one Input Method -- it corresonds to the kEventParamTextInputSendReplaceRange
        // event in TSM.  This behaviour matches that of -[WebHTMLView setMarkedText:selectedRange:] when it receives an
        // NSAttributedString
        NSString *rangeString = [string attribute:NSTextInputReplacementRangeAttributeName atIndex:0 longestEffectiveRange:NULL inRange:NSMakeRange(0, [text length])];
        LOG(TextInput, "ReplacementRange: %@", rangeString);
        if (rangeString)
            isFromInputMethod = YES;
    } else
        text = string;
    
    String eventText = text;
    
    if (!isFromInputMethod)
        _data->_commandsList.append(KeypressCommand("insertText", text));
    else {
        eventText.replace(NSBackTabCharacter, NSTabCharacter); // same thing is done in KeyEventMac.mm in WebCore
        _data->_commandsList.append(KeypressCommand("insertText", eventText));
    }
}

- (BOOL)_handleStyleKeyEquivalent:(NSEvent *)event
{
    if (([event modifierFlags] & NSDeviceIndependentModifierFlagsMask) != NSCommandKeyMask)
        return NO;
    
    // Here we special case cmd+b and cmd+i but not cmd+u, for historic reason.
    // This should not be changed, since it could break some Mac applications that
    // rely on this inherent behavior.
    // See https://bugs.webkit.org/show_bug.cgi?id=24943
    
    NSString *string = [event characters];
    if ([string caseInsensitiveCompare:@"b"] == NSOrderedSame) {
        _data->_page->executeEditCommand("ToggleBold");
        return YES;
    }
    if ([string caseInsensitiveCompare:@"i"] == NSOrderedSame) {
        _data->_page->executeEditCommand("ToggleItalic");
        return YES;
    }
    
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    // There's a chance that responding to this event will run a nested event loop, and
    // fetching a new event might release the old one. Retaining and then autoreleasing
    // the current event prevents that from causing a problem inside WebKit or AppKit code.
    [[event retain] autorelease];
    
    BOOL eventWasSentToWebCore = (_data->_keyDownEventBeingResent == event);
    BOOL ret = NO;
    
    [self retain];
    
    // Pass key combos through WebCore if there is a key binding available for
    // this event. This lets web pages have a crack at intercepting key-modified keypresses.
    // But don't do it if we have already handled the event.
    // Pressing Esc results in a fake event being sent - don't pass it to WebCore.
    if (!eventWasSentToWebCore && event == [NSApp currentEvent] && self == [[self window] firstResponder]) {
        [_data->_keyDownEventBeingResent release];
        _data->_keyDownEventBeingResent = nil;
        
        _data->_page->handleKeyboardEvent(NativeWebKeyboardEvent(event, self));
        return YES;
    }
    
    ret = [self _handleStyleKeyEquivalent:event] || [super performKeyEquivalent:event];
    
    [self release];
    
    return ret;
}

- (void)_setEventBeingResent:(NSEvent *)event
{
    _data->_keyDownEventBeingResent = [event retain];
}

- (void)_selectionChanged:(BOOL)isNone isEditable:(BOOL)isContentEditable isPassword:(BOOL)isPasswordField hasMarkedText:(BOOL)hasComposition range:(NSRange)newrange
{
    _data->_isSelectionNone = isNone;
    _data->_isSelectionEditable = isContentEditable;
    _data->_isSelectionInPasswordField = isPasswordField;
    _data->_hasMarkedText = hasComposition;
    _data->_selectedRange = newrange;
}

- (Vector<KeypressCommand>&)_interceptKeyEvent:(NSEvent *)theEvent 
{
    _data->_commandsList.clear();
    // interpretKeyEvents will trigger one or more calls to doCommandBySelector or setText
    // that will populate the commandsList vector.
    [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
    return _data->_commandsList;
}

- (void)_getTextInputState:(unsigned)start selectionEnd:(unsigned)end underlines:(Vector<WebCore::CompositionUnderline>&)lines
{
    start = _data->_selectionStart;
    end = _data->_selectionEnd;
    lines = _data->_underlines;
}

- (void)keyUp:(NSEvent *)theEvent
{
    _data->_page->handleKeyboardEvent(NativeWebKeyboardEvent(theEvent, self));
}

- (void)keyDown:(NSEvent *)theEvent
{
    if (_data->_pluginComplexTextInputIdentifier) {
        // Try feeding the keyboard event directly to the plug-in.
        NSString *string = nil;
        if ([[WKTextInputWindowController sharedTextInputWindowController] interpretKeyEvent:theEvent string:&string]) {
            if (string)
                _data->_page->sendComplexTextInputToPlugin(_data->_pluginComplexTextInputIdentifier, string);
            return;
        }
    }

    _data->_underlines.clear();
    _data->_selectionStart = 0;
    _data->_selectionEnd = 0;
    // We could be receiving a key down from AppKit if we have re-sent an event
    // that maps to an action that is currently unavailable (for example a copy when
    // there is no range selection).
    // If this is the case we should ignore the key down.
    if (_data->_keyDownEventBeingResent == theEvent) {
        [_data->_keyDownEventBeingResent release];
        _data->_keyDownEventBeingResent = nil;
        [super keyDown:theEvent];
        return;
    }
    _data->_page->handleKeyboardEvent(NativeWebKeyboardEvent(theEvent, self));
}

- (NSTextInputContext *)inputContext {
    if (_data->_pluginComplexTextInputIdentifier)
        return [[WKTextInputWindowController sharedTextInputWindowController] inputContext];

    return [super inputContext];
}

- (NSRange)selectedRange
{
    if (_data->_isSelectionNone || !_data->_isSelectionEditable)
        return NSMakeRange(NSNotFound, 0);
    
    LOG(TextInput, "selectedRange -> (%u, %u)", _data->_selectedRange.location, _data->_selectedRange.length);
    return _data->_selectedRange;
}

- (BOOL)hasMarkedText
{
    LOG(TextInput, "hasMarkedText -> %u", _data-> _hasMarkedText);
    return _data->_hasMarkedText;
}

- (void)unmarkText
{
    LOG(TextInput, "unmarkText");
    
    _data->_commandsList.append(KeypressCommand("unmarkText"));
}

- (NSArray *)validAttributesForMarkedText
{
    static NSArray *validAttributes;
    if (!validAttributes) {
        validAttributes = [[NSArray alloc] initWithObjects:
                           NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName,
                           NSMarkedClauseSegmentAttributeName, NSTextInputReplacementRangeAttributeName, nil];
        // NSText also supports the following attributes, but it's
        // hard to tell which are really required for text input to
        // work well; I have not seen any input method make use of them yet.
        //     NSFontAttributeName, NSForegroundColorAttributeName,
        //     NSBackgroundColorAttributeName, NSLanguageAttributeName.
        CFRetain(validAttributes);
    }
    LOG(TextInput, "validAttributesForMarkedText -> (...)");
    return validAttributes;
}

static void extractUnderlines(NSAttributedString *string, Vector<CompositionUnderline>& result)
{
    int length = [[string string] length];
    
    int i = 0;
    while (i < length) {
        NSRange range;
        NSDictionary *attrs = [string attributesAtIndex:i longestEffectiveRange:&range inRange:NSMakeRange(i, length - i)];
        
        if (NSNumber *style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
            Color color = Color::black;
            if (NSColor *colorAttr = [attrs objectForKey:NSUnderlineColorAttributeName])
                color = colorFromNSColor([colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
            result.append(CompositionUnderline(range.location, NSMaxRange(range), color, [style intValue] > 1));
        }
        
        i = range.location + range.length;
    }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
{
    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]]; // Otherwise, NSString
    
    LOG(TextInput, "setMarkedText:\"%@\" selectedRange:(%u, %u)", isAttributedString ? [string string] : string, newSelRange.location, newSelRange.length);
    
    NSString *text = string;
    
    if (isAttributedString) {
        text = [string string];
        extractUnderlines(string, _data->_underlines);
    }
    
    _data->_commandsList.append(KeypressCommand("setMarkedText", text));
    _data->_selectionStart = newSelRange.location;
    _data->_selectionEnd = NSMaxRange(newSelRange);
}

- (NSRange)markedRange
{
    uint64_t location;
    uint64_t length;

    _data->_page->getMarkedRange(location, length);
    LOG(TextInput, "markedRange -> (%u, %u)", location, length);
    return NSMakeRange(location, length);
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)nsRange
{
    // This is not implemented for now. Need to figure out how to serialize the attributed string across processes.
    LOG(TextInput, "attributedSubstringFromRange");
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
    NSWindow *window = [self window];
    
    if (window)
        thePoint = [window convertScreenToBase:thePoint];
    thePoint = [self convertPoint:thePoint fromView:nil];  // the point is relative to the main frame 
    
    uint64_t result = _data->_page->characterIndexForPoint(IntPoint(thePoint));
    LOG(TextInput, "characterIndexForPoint:(%f, %f) -> %u", thePoint.x, thePoint.y, result);
    return result;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
{ 
    // Just to match NSTextView's behavior. Regression tests cannot detect this;
    // to reproduce, use a test application from http://bugs.webkit.org/show_bug.cgi?id=4682
    // (type something; try ranges (1, -1) and (2, -1).
    if ((theRange.location + theRange.length < theRange.location) && (theRange.location + theRange.length != 0))
        theRange.length = 0;
    
    NSRect resultRect = _data->_page->firstRectForCharacterRange(theRange.location, theRange.length);
    resultRect = [self convertRect:resultRect toView:nil];
    
    NSWindow *window = [self window];
    if (window)
        resultRect.origin = [window convertBaseToScreen:resultRect.origin];
    
    LOG(TextInput, "firstRectForCharacterRange:(%u, %u) -> (%f, %f, %f, %f)", theRange.location, theRange.length, resultRect.origin.x, resultRect.origin.y, resultRect.size.width, resultRect.size.height);
    return resultRect;
}

- (void)_updateActiveState
{
    _data->_page->setActive([[self window] isKeyWindow]);
}

- (void)_updateWindowVisibility
{
    _data->_page->updateWindowIsVisible(![[self window] isMiniaturized]);
}

- (BOOL)_ownsWindowGrowBox
{
    NSWindow* window = [self window];
    if (!window)
        return NO;

    NSView *superview = [self superview];
    if (!superview)
        return NO;

    NSRect growBoxRect = [window _growBoxRect];
    if (NSIsEmptyRect(growBoxRect))
        return NO;

    NSRect visibleRect = [self visibleRect];
    if (NSIsEmptyRect(visibleRect))
        return NO;

    NSRect visibleRectInWindowCoords = [self convertRect:visibleRect toView:nil];
    if (!NSIntersectsRect(growBoxRect, visibleRectInWindowCoords))
        return NO;

    return YES;
}

- (BOOL)_updateGrowBoxForWindowFrameChange
{
    // Temporarily enable the resize indicator to make a the _ownsWindowGrowBox calculation work.
    BOOL wasShowingIndicator = [[self window] showsResizeIndicator];
    [[self window] setShowsResizeIndicator:YES];

    BOOL ownsGrowBox = [self _ownsWindowGrowBox];
    _data->_page->setWindowResizerSize(ownsGrowBox ? enclosingIntRect([[self window] _growBoxRect]).size() : IntSize());
    
    // Once WebCore can draw the window resizer, this should read:
    // if (wasShowingIndicator)
    //     [[self window] setShowsResizeIndicator:!ownsGrowBox];
    [[self window] setShowsResizeIndicator:wasShowingIndicator];

    return ownsGrowBox;
}

- (void)addWindowObserversForWindow:(NSWindow *)window
{
    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidBecomeKey:)
                                                     name:NSWindowDidBecomeKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidResignKey:)
                                                     name:NSWindowDidResignKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidMiniaturize:) 
                                                     name:NSWindowDidMiniaturizeNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidDeminiaturize:)
                                                     name:NSWindowDidDeminiaturizeNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowFrameDidChange:)
                                                     name:NSWindowDidMoveNotification object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowFrameDidChange:) 
                                                     name:NSWindowDidResizeNotification object:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (!window)
        return;

    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidResignKeyNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidMiniaturizeNotification object:window];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidDeminiaturizeNotification object:window];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidMoveNotification object:window];
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidResizeNotification object:window];
}

static bool isViewVisible(NSView *view)
{
    if (![view window])
        return false;
    
    if ([view isHiddenOrHasHiddenAncestor])
        return false;
    
    return true;
}

- (void)_updateVisibility
{
    _data->_page->setIsInWindow([self window]);
    if (DrawingAreaProxy* area = _data->_page->drawingArea())
        area->setPageIsVisible(isViewVisible(self));
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    if (window != [self window]) {
        [self removeWindowObservers];
        [self addWindowObserversForWindow:window];
    }
}

- (void)viewDidMoveToWindow
{
    // We want to make sure to update the active state while hidden, so if the view is about to become visible, we
    // update the active state first and then make it visible. If the view is about to be hidden, we hide it first and then
    // update the active state.
    if ([self window]) {
        [self _updateActiveState];
        [self _updateVisibility];
        [self _updateWindowVisibility];
        [self _updateWindowAndViewFrames];
    } else {
        [self _updateVisibility];
        [self _updateActiveState];
    }

}

- (void)_windowDidBecomeKey:(NSNotification *)notification
{
    NSWindow *keyWindow = [notification object];
    if (keyWindow == [self window] || keyWindow == [[self window] attachedSheet])
        [self _updateActiveState];
}

- (void)_windowDidResignKey:(NSNotification *)notification
{
    NSWindow *formerKeyWindow = [notification object];
    if (formerKeyWindow == [self window] || formerKeyWindow == [[self window] attachedSheet])
        [self _updateActiveState];
}

- (void)_windowDidMiniaturize:(NSNotification *)notification
{
    [self _updateWindowVisibility];
}

- (void)_windowDidDeminiaturize:(NSNotification *)notification
{
    [self _updateWindowVisibility];
}

- (void)_windowFrameDidChange:(NSNotification *)notification
{
    [self _updateWindowAndViewFrames];
}

- (void)drawRect:(NSRect)rect
{
    if (_data->_page->isValid() && _data->_page->drawingArea()) {
        CGContextRef context = static_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]);
        _data->_page->drawingArea()->paint(IntRect(rect), context);
        _data->_page->didDraw();
    } else if (_data->_page->drawsBackground()) {
        [_data->_page->drawsTransparentBackground() ? [NSColor clearColor] : [NSColor whiteColor] set];
        NSRectFill(rect);
    }
}

- (BOOL)isOpaque
{
    return _data->_page->drawsBackground();
}

- (void)viewDidHide
{
    [self _updateVisibility];
}

- (void)viewDidUnhide
{
    [self _updateVisibility];
}

- (NSView *)hitTest:(NSPoint)point
{
    NSView *hitView = [super hitTest:point];
#if USE(ACCELERATED_COMPOSITING)
    if (hitView && _data && hitView == _data->_layerHostingView)
        hitView = self;
#endif
    return hitView;
}

- (NSInteger)conversationIdentifier
{
    return (NSInteger)self;
}

@end

@implementation WKView (Internal)

- (void)_processDidCrash
{
    [self setNeedsDisplay:YES];
}

- (void)_didRelaunchProcess
{
    _data->_page->reinitializeWebPage(IntSize([self bounds].size));

    _data->_page->setActive([[self window] isKeyWindow]);
    _data->_page->setFocused([[self window] firstResponder] == self);
    
    [self setNeedsDisplay:YES];
}

- (void)_takeFocus:(BOOL)forward
{
    if (forward)
        [[self window] selectKeyViewFollowingView:self];
    else
        [[self window] selectKeyViewPrecedingView:self];
}

- (void)_setCursor:(NSCursor *)cursor
{
    if ([NSCursor currentCursor] == cursor)
        return;
    [cursor set];
}

- (void)_setUserInterfaceItemState:(NSString *)commandName enabled:(BOOL)isEnabled state:(int)newState
{
    NSMenuItem *menuItem = _data->_menuItemsMap.take(commandName).get();
    [menuItem setState:newState];
    [menuItem setEnabled:isEnabled];
}

- (NSRect)_convertToDeviceSpace:(NSRect)rect
{
    return toDeviceSpace(rect, [self window]);
}

- (NSRect)_convertToUserSpace:(NSRect)rect
{
    return toUserSpace(rect, [self window]);
}

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_data->_trackingRectOwner == nil);
    _data->_trackingRectOwner = owner;
    _data->_trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(_data->_trackingRectOwner == nil);
    _data->_trackingRectOwner = owner;
    _data->_trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (void)_addTrackingRects:(NSRect *)rects owner:(id)owner userDataList:(void **)userDataList assumeInsideList:(BOOL *)assumeInsideList trackingNums:(NSTrackingRectTag *)trackingNums count:(int)count
{
    ASSERT(count == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(_data->_trackingRectOwner == nil);
    _data->_trackingRectOwner = owner;
    _data->_trackingRectUserData = userDataList[0];
    trackingNums[0] = TRACKING_RECT_TAG;
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    if (tag == 0)
        return;
    
    if (_data && (tag == TRACKING_RECT_TAG)) {
        _data->_trackingRectOwner = nil;
        return;
    }
    
    if (_data && (tag == _data->_lastToolTipTag)) {
        [super removeTrackingRect:tag];
        _data->_lastToolTipTag = 0;
        return;
    }
    
    // If any other tracking rect is being removed, we don't know how it was created
    // and it's possible there's a leak involved (see 3500217)
    ASSERT_NOT_REACHED();
}

- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count
{
    int i;
    for (i = 0; i < count; ++i) {
        int tag = tags[i];
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        if (_data != nil) {
            _data->_trackingRectOwner = nil;
        }
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
        userData:_data->_trackingRectUserData];
    [_data->_trackingRectOwner mouseExited:fakeEvent];
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
        userData:_data->_trackingRectUserData];
    [_data->_trackingRectOwner mouseEntered:fakeEvent];
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return nsStringFromWebCoreString(_data->_page->toolTip());
}

- (void)_toolTipChangedFrom:(NSString *)oldToolTip to:(NSString *)newToolTip
{
    if (oldToolTip)
        [self _sendToolTipMouseExited];

    if (newToolTip && [newToolTip length] > 0) {
        // See radar 3500217 for why we remove all tooltips rather than just the single one we created.
        [self removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        _data->_lastToolTipTag = [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
}

- (void)_setFindIndicator:(PassRefPtr<FindIndicator>)findIndicator fadeOut:(BOOL)fadeOut
{
    if (!findIndicator) {
        _data->_findIndicatorWindow = 0;
        return;
    }

    if (!_data->_findIndicatorWindow)
        _data->_findIndicatorWindow = FindIndicatorWindow::create(self);

    _data->_findIndicatorWindow->setFindIndicator(findIndicator, fadeOut);
}

#if USE(ACCELERATED_COMPOSITING)
- (void)_startAcceleratedCompositing:(CALayer *)rootLayer
{
    if (!_data->_layerHostingView) {
        NSView *hostingView = [[NSView alloc] initWithFrame:[self bounds]];
#if !defined(BUILDING_ON_LEOPARD)
        [hostingView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
#endif
        
        [self addSubview:hostingView];
        [hostingView release];
        _data->_layerHostingView = hostingView;
    }

    // Make a container layer, which will get sized/positioned by AppKit and CA.
    CALayer *viewLayer = [CALayer layer];

#ifndef NDEBUG
    [viewLayer setName:@"hosting layer"];
#endif

#if defined(BUILDING_ON_LEOPARD)
    // Turn off default animations.
    NSNull *nullValue = [NSNull null];
    NSDictionary *actions = [NSDictionary dictionaryWithObjectsAndKeys:
                             nullValue, @"anchorPoint",
                             nullValue, @"bounds",
                             nullValue, @"contents",
                             nullValue, @"contentsRect",
                             nullValue, @"opacity",
                             nullValue, @"position",
                             nullValue, @"sublayerTransform",
                             nullValue, @"sublayers",
                             nullValue, @"transform",
                             nil];
    [viewLayer setStyle:[NSDictionary dictionaryWithObject:actions forKey:@"actions"]];
#endif

#if !defined(BUILDING_ON_LEOPARD)
    // If we aren't in the window yet, we'll use the screen's scale factor now, and reset the scale 
    // via -viewDidMoveToWindow.
    CGFloat scaleFactor = [self window] ? [[self window] userSpaceScaleFactor] : [[NSScreen mainScreen] userSpaceScaleFactor];
    [viewLayer setTransform:CATransform3DMakeScale(scaleFactor, scaleFactor, 1)];
#endif

    [_data->_layerHostingView setLayer:viewLayer];
    [_data->_layerHostingView setWantsLayer:YES];
    
    // Parent our root layer in the container layer
    [viewLayer addSublayer:rootLayer];
}

- (void)_stopAcceleratedCompositing
{
    if (_data->_layerHostingView) {
        [_data->_layerHostingView setLayer:nil];
        [_data->_layerHostingView setWantsLayer:NO];
        [_data->_layerHostingView removeFromSuperview];
        _data->_layerHostingView = nil;
    }
}

- (void)_switchToDrawingAreaTypeIfNecessary:(DrawingAreaInfo::Type)type
{
    DrawingAreaInfo::Type existingDrawingAreaType = _data->_page->drawingArea() ? _data->_page->drawingArea()->info().type : DrawingAreaInfo::None;
    if (existingDrawingAreaType == type)
        return;

    OwnPtr<DrawingAreaProxy> newDrawingArea;
    switch (type) {
        case DrawingAreaInfo::None:
            break;
        case DrawingAreaInfo::ChunkedUpdate: {
            newDrawingArea = ChunkedUpdateDrawingAreaProxy::create(self, _data->_page.get());
            break;
        }
        case DrawingAreaInfo::LayerBacked: {
            newDrawingArea = LayerBackedDrawingAreaProxy::create(self, _data->_page.get());
            break;
        }
    }

    newDrawingArea->setSize(IntSize([self frame].size));

    _data->_page->drawingArea()->detachCompositingContext();
    _data->_page->setDrawingArea(newDrawingArea.release());
}

- (void)_pageDidEnterAcceleratedCompositing
{
    [self _switchToDrawingAreaTypeIfNecessary:DrawingAreaInfo::LayerBacked];
}

- (void)_pageDidLeaveAcceleratedCompositing
{
    // FIXME: we may want to avoid flipping back to the non-layer-backed drawing area until the next page load, to avoid thrashing.
    [self _switchToDrawingAreaTypeIfNecessary:DrawingAreaInfo::ChunkedUpdate];
}
#endif // USE(ACCELERATED_COMPOSITING)

- (void)_setComplexTextInputEnabled:(BOOL)complexTextInputEnabled pluginComplexTextInputIdentifier:(uint64_t)pluginComplexTextInputIdentifier
{
    BOOL inputSourceChanged = _data->_pluginComplexTextInputIdentifier;

    if (complexTextInputEnabled) {
        // Check if we're already allowing text input for this plug-in.
        if (pluginComplexTextInputIdentifier == _data->_pluginComplexTextInputIdentifier)
            return;

        _data->_pluginComplexTextInputIdentifier = pluginComplexTextInputIdentifier;

    } else {
        // Check if we got a request to disable complex text input for a plug-in that is not the current plug-in.
        if (pluginComplexTextInputIdentifier != _data->_pluginComplexTextInputIdentifier)
            return;

        _data->_pluginComplexTextInputIdentifier = 0;
    }

    if (inputSourceChanged) {
        // Inform the out of line window that the input source changed.
        [[WKTextInputWindowController sharedTextInputWindowController] keyboardInputSourceChanged];
    }
}

- (void)_setPageHasCustomRepresentation:(BOOL)pageHasCustomRepresentation
{
    _data->_pdfViewController = nullptr;

    if (pageHasCustomRepresentation)
        _data->_pdfViewController = PDFViewController::create(self);
}

- (void)_didFinishLoadingDataForCustomRepresentation:(const CoreIPC::DataReference&)dataReference
{
    ASSERT(_data->_pdfViewController);

    _data->_pdfViewController->setPDFDocumentData(_data->_page->mainFrame()->mimeType(), dataReference);
}

@end
