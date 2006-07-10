/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "WebCoreTextField.h"

#import "DOMInternal.h"
#import "Element.h"
#import "FrameMac.h"
#import "HTMLNames.h"
#import "Settings.h"
#import "TextField.h"
#import "WebCoreFrameBridge.h"
#import "WidgetClient.h"
#import <wtf/Assertions.h>

using namespace WebCore;
using namespace HTMLNames;

@interface NSString (WebCoreTextField)
- (int)_webcore_numComposedCharacterSequences;
- (NSString *)_webcore_truncateToNumComposedCharacterSequences:(int)num;
@end

@interface NSTextField (WebCoreTextField)
- (NSTextView *)_webcore_currentEditor;
@end

@interface NSCell (WebCoreTextField)
- (NSMutableDictionary *)_textAttributes;
@end

@interface NSSearchFieldCell (WebCoreTextField)
- (void)_addStringToRecentSearches:(NSString *)string;
@end

@interface NSTextView (WebCoreTextField)
- (void)setWantsNotificationForMarkedText:(BOOL)wantsNotification;
@end

// The two cell subclasses allow us to tell when we get focus without an editor subclass,
// and override the base writing direction.
@interface WebCoreSecureTextFieldCell : NSSecureTextFieldCell
@end
@interface WebCoreSearchFieldCell : NSSearchFieldCell
@end

// WebCoreTextFieldFormatter enforces a maximum length.
@interface WebCoreTextFieldFormatter : NSFormatter
{
    int maxLength;
}
- (void)setMaximumLength:(int)len;
- (int)maximumLength;
@end

@interface WebCoreTextFieldController (WebCoreInternal)
- (id)initWithTextField:(NSTextField *)f TextField:(TextField *)w;
- (Widget *)widget;
- (void)textChanged;
- (void)setInDrawingMachinery:(BOOL)inDrawing;
- (BOOL)textView:(NSTextView *)view shouldDrawInsertionPointInRect:(NSRect)rect color:(NSColor *)color turnedOn:(BOOL)drawInsteadOfErase;
- (BOOL)textView:(NSTextView *)view shouldHandleEvent:(NSEvent *)event;
- (void)textView:(NSTextView *)view didHandleEvent:(NSEvent *)event;
- (BOOL)textView:(NSTextView *)view shouldChangeTextInRange:(NSRange)range replacementString:(NSString *)string;
- (void)textViewDidChangeSelection:(NSNotification *)notification;
- (void)updateTextAttributes:(NSMutableDictionary *)attributes;
- (NSString *)preprocessString:(NSString *)string;
- (void)setHasFocus:(BOOL)hasFocus;
@end

@implementation WebCoreTextFieldController

- (id)initWithTextField:(NSTextField *)f TextField:(TextField *)w
{
    self = [self init];
    if (!self)
        return nil;

    // This is initialization that's shared by all types of text fields.
    widget = w;
    field = f;
    formatter = [[WebCoreTextFieldFormatter alloc] init];
    lastSelectedRange.location = NSNotFound;
    [[field cell] setScrollable:YES];
    [field setFormatter:formatter];
    [field setDelegate:self];
    
    if (widget->type() == TextField::Search) {
        [field setTarget:self];
        [field setAction:@selector(action:)];
    }
    
    return self;
}

- (void)detachQLineEdit
{
    widget = 0;
}

- (void)action:(id)sender
{
    if (widget && widget->client())
        widget->client()->valueChanged(widget);
    if (widget && widget->client())
        widget->client()->performSearch(widget);
}

- (void)dealloc
{
    [formatter release];
    [super dealloc];
}

- (Widget*)widget
{
    return widget;
}

- (void)setMaximumLength:(int)len
{
    NSString *oldValue = [self string];
    if ([oldValue _webcore_numComposedCharacterSequences] > len) {
        [field setStringValue:[oldValue _webcore_truncateToNumComposedCharacterSequences:len]];
    }
    [formatter setMaximumLength:len];
}

- (int)maximumLength
{
    return [formatter maximumLength];
}

- (BOOL)edited
{
    return edited;
}

- (void)setEdited:(BOOL)ed
{
    edited = ed;
}

static DOMHTMLInputElement* inputElement(TextField* widget)
{
    if (!widget)
        return nil;
    WidgetClient* client = widget->client();
    if (!client)
        return nil;
    Element* element = client->element(widget);
    if (!element || !element->hasTagName(inputTag))
        return nil;
    return (DOMHTMLInputElement*)[DOMElement _elementWith:element];
}

- (void)controlTextDidBeginEditing:(NSNotification *)notification
{
    if (!widget)
        return;
    
    [[field _webcore_currentEditor] setWantsNotificationForMarkedText:YES];

    if (DOMHTMLInputElement* input = inputElement(widget))
        [FrameMac::bridgeForWidget(widget) textFieldDidBeginEditing:input];
}

- (void)controlTextDidEndEditing:(NSNotification *)notification
{
    if (!widget)
        return;
    
    if (DOMHTMLInputElement* input = inputElement(widget))
        [FrameMac::bridgeForWidget(widget) textFieldDidEndEditing:input];
    
    if (widget && widget->client() && [[[notification userInfo] objectForKey:@"NSTextMovement"] intValue] == NSReturnTextMovement)
        widget->client()->returnPressed(widget);
}

- (void)controlTextDidChange:(NSNotification *)notification
{
    if (!widget)
        return;
    
    if (FrameMac::handleKeyboardOptionTabInView(field))
        return;
    
    if (![[field _webcore_currentEditor] hasMarkedText])
        if (DOMHTMLInputElement* input = inputElement(widget))
            [FrameMac::bridgeForWidget(widget) textDidChangeInTextField:input];
    
    edited = YES;
    [self textChanged];
}

- (BOOL)control:(NSControl *)control textShouldBeginEditing:(NSText *)fieldEditor
{
    if (!widget)
        return NO;    
    
    // In WebHTMLView, we set a clip. This is not typical to do in an
    // NSView, and while correct for any one invocation of drawRect:,
    // it causes some bad problems if that clip is cached between calls.
    // The cached graphics state, which some views keep around, does
    // cache the clip in this undesirable way. Consequently, we want to 
    // turn off this caching for all views contained in a WebHTMLView that
    // would otherwise do it. Here we turn it off for the editor (NSTextView)
    // used for text fields in forms and the clip view it's embedded in.
    // See bug 3457875 and 3310943 for more context.
    [fieldEditor releaseGState];
    [[fieldEditor superview] releaseGState];
    
    return YES;
}

- (BOOL)control:(NSControl *)control textShouldEndEditing:(NSText *)fieldEditor
{
    if (!widget)
        return NO;

    return YES;
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector
{
    if (!widget)
        return NO;

    if (DOMHTMLInputElement* input = inputElement(widget))
        return [FrameMac::bridgeForWidget(widget) textField:input doCommandBySelector:commandSelector];

    return NO;
}

- (void)textChanged
{
    if (widget && widget->client())
        widget->client()->valueChanged(widget);
}

- (void)setInDrawingMachinery:(BOOL)inDrawing
{
    inDrawingMachinery = inDrawing;
}

// Use the "needs display" mechanism to do all insertion point drawing in the web view.
- (BOOL)textView:(NSTextView *)view shouldDrawInsertionPointInRect:(NSRect)rect color:(NSColor *)color turnedOn:(BOOL)drawInsteadOfErase
{
    // We only need to take control of the cases where we are being asked to draw by something
    // outside the normal display machinery, and when we are being asked to draw the insertion
    // point, not erase it.
    if (inDrawingMachinery || !drawInsteadOfErase)
        return YES;

    // NSTextView's insertion-point drawing code sets the rect width to 1.
    // So we do the same thing, to affect exactly the same rectangle.
    rect.size.width = 1;

    // Call through to the setNeedsDisplayInRect implementation in NSView.
    // If we call the one in NSTextView through the normal method dispatch
    // we will reenter the caret blinking code and end up with a nasty crash
    // (see Radar 3250608).
    SEL selector = @selector(setNeedsDisplayInRect:);
    typedef void (*IMPWithNSRect)(id, SEL, NSRect);
    IMPWithNSRect implementation = (IMPWithNSRect)[NSView instanceMethodForSelector:selector];
    implementation(view, selector, rect);

    return NO;
}

- (BOOL)textView:(NSTextView *)view shouldHandleEvent:(NSEvent *)event
{
    if (!widget)
        return YES;
    
    NSEventType type = [event type];
    if ((type == NSKeyDown || type == NSKeyUp) && ![[NSInputManager currentInputManager] hasMarkedText]) {
        WebCoreFrameBridge *bridge = FrameMac::bridgeForWidget(widget);

        Widget::setDeferFirstResponderChanges(true);

        BOOL intercepted = NO;
        if (DOMHTMLInputElement* input = inputElement(widget))
            intercepted = [bridge textField:input shouldHandleEvent:event];
        if (!intercepted)
            intercepted = [bridge interceptKeyEvent:event toView:view];

        // Always intercept key up events because we don't want them
        // passed along the responder chain. This is arguably a bug in
        // NSTextView; see Radar 3507083.
        if (type == NSKeyUp)
            intercepted = YES;

        if (intercepted || !widget) {
            Widget::setDeferFirstResponderChanges(false);
            return NO;
        }
    }

    return YES;
}

- (void)textView:(NSTextView *)view didHandleEvent:(NSEvent *)event
{
    Widget::setDeferFirstResponderChanges(false);

    if (!widget)
        return;

    if ([event type] == NSLeftMouseUp) {
        widget->sendConsumedMouseUp();
        if (widget && widget->client())
            widget->client()->clicked(widget);
    }
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction
{
    if (baseWritingDirection != direction) {
        baseWritingDirection = direction;
        [field setNeedsDisplay:YES];
    }
}

- (NSWritingDirection)baseWritingDirection
{
    return baseWritingDirection;
}

- (NSRange)selectedRange
{
    NSText *editor = [field _webcore_currentEditor];
    return editor ? [editor selectedRange] : lastSelectedRange;
}

- (void)setSelectedRange:(NSRange)range
{
    // Range check just in case the saved range has gotten out of sync.
    // Even though we don't see this in testing, we really don't want
    // an exception in this case, so we protect ourselves.
    NSText *editor = [field _webcore_currentEditor];
    if (editor) { // if we have no focus, we don't have a current editor
        unsigned len = [[editor string] length];
        if (NSMaxRange(range) > len) {
            if (range.location > len) {
                range.location = len;
                range.length = 0;
            } else {
                range.length = len - range.location;
            }
        }
        [editor setSelectedRange:range];
    } else {
        // set the lastSavedRange, so it will be used when given focus
        unsigned len = [[field stringValue] length];
        if (NSMaxRange(range) > len) {
            if (range.location > len) {
                range.location = len;
                range.length = 0;
            } else {
                range.length = len - range.location;
            }
        }
        lastSelectedRange = range;
    }
}

- (BOOL)hasSelection
{
    return [self selectedRange].length > 0;
}

- (void)setHasFocus:(BOOL)nowHasFocus
{
    if (!widget || nowHasFocus == hasFocus)
        return;

    hasFocus = nowHasFocus;
    hasFocusAndSelectionSet = NO;
    
    if (nowHasFocus) {
        // Select all the text if we are tabbing in, but otherwise preserve/remember
        // the selection from last time we had focus (to match WinIE).
        if ([[field window] keyViewSelectionDirection] != NSDirectSelection)
            lastSelectedRange.location = NSNotFound;
        
        if (lastSelectedRange.location != NSNotFound)
            [self setSelectedRange:lastSelectedRange];
        
        hasFocusAndSelectionSet = YES;

        if (widget && widget->client() && !FrameMac::currentEventIsMouseDownInWidget(widget))
            widget->client()->scrollToVisible(widget);
        if (widget && widget->client()) {
            widget->client()->focusIn(widget);
            [FrameMac::bridgeForWidget(widget) formControlIsBecomingFirstResponder:field];
        }
        
        // Sending the onFocus event above, may have resulted in a blur() - if this
        // happens when tabbing from another text field, then endEditing: and
        // controlTextDidEndEditing: will never be called. The bad side effects of this 
        // include the fact that our idea of the focus state will be wrong;
        // and the text field will think it's still editing, so it will continue to draw
        // the focus ring. So we call endEditing: manually if we detect this inconsistency,
        // and the correct our internal impression of the focus state.
        if ([field _webcore_currentEditor] == nil && [field currentEditor] != nil) {
            [[field cell] endEditing:[field currentEditor]];
            [self setHasFocus:NO];
        }
    } else {
        lastSelectedRange = [self selectedRange];
        
        if (widget && widget->client()) {
            widget->client()->focusOut(widget);
            [FrameMac::bridgeForWidget(widget) formControlIsResigningFirstResponder:field];
        }
    }
}

- (void)updateTextAttributes:(NSMutableDictionary *)attributes
{
    NSParagraphStyle *style = [attributes objectForKey:NSParagraphStyleAttributeName];
    ASSERT(style != nil);
    if ([style baseWritingDirection] != baseWritingDirection) {
        NSMutableParagraphStyle *mutableStyle = [style mutableCopy];
        [mutableStyle setBaseWritingDirection:baseWritingDirection];
        [attributes setObject:mutableStyle forKey:NSParagraphStyleAttributeName];
        [mutableStyle release];
    }
}

- (NSString *)string
{
    // Calling stringValue can have a side effect of ending International inline input.
    // So don't call it unless there's no editor.
    NSText *editor = [field _webcore_currentEditor];
    if (editor == nil) {
        return [field stringValue];
    }
    return [editor string];
}

- (BOOL)textView:(NSTextView *)textView shouldChangeTextInRange:(NSRange)range replacementString:(NSString *)string
{
    if (string == nil)
        return YES;
    NSRange newline = [string rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"\r\n"]];
    if (newline.location == NSNotFound) {
        return YES;
    }
    NSString *truncatedString = [string substringToIndex:newline.location];
    if ([textView shouldChangeTextInRange:range replacementString:truncatedString]) {
        [textView replaceCharactersInRange:range withString:truncatedString];
        [textView didChangeText];
    }
    return NO;
}

- (NSString *)preprocessString:(NSString *)string
{
    NSString *result = string;
    NSRange newline = [result rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"\r\n"]];
    if (newline.location != NSNotFound)
        result = [result substringToIndex:newline.location];
    return [result _webcore_truncateToNumComposedCharacterSequences:[formatter maximumLength]];
}

- (void)textViewDidChangeSelection:(NSNotification *)notification
{
    if (widget && widget->client() && hasFocusAndSelectionSet)
        widget->client()->selectionChanged(widget);
}

@end

@implementation WebCoreSecureTextField

+ (Class)cellClass
{
    return [WebCoreSecureTextFieldCell class];
}

- (id)initWithQLineEdit:(TextField *)w 
{
    self = [self init];
    if (!self)
        return nil;
    controller = [[WebCoreTextFieldController alloc] initWithTextField:self TextField:w];
    return self;
}

- (void)dealloc
{
    [controller release];
    [super dealloc];
}

- (WebCoreTextFieldController *)controller
{
    return controller;
}

- (Widget *)widget
{
    return [controller widget];
}

- (void)setStringValue:(NSString *)string
{
    [super setStringValue:[controller preprocessString:string]];
    [controller textChanged];
}

- (NSView *)nextKeyView
{
    if (!inNextValidKeyView)
        return [super nextKeyView];
    Widget* widget = [controller widget];
    if (!widget)
        return [super nextKeyView];
    return FrameMac::nextKeyViewForWidget(widget, SelectingNext);
}

- (NSView *)previousKeyView
{
    if (!inNextValidKeyView)
        return [super previousKeyView];
    Widget* widget = [controller widget];
    if (!widget)
        return [super previousKeyView];
    return FrameMac::nextKeyViewForWidget(widget, SelectingPrevious);
}

- (NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (BOOL)acceptsFirstResponder
{
    return [self isEnabled];
}

- (void)display
{
    // This is a workaround for Radar 2753974.
    // Also, in the web page context, it's never OK to just display.
    [self setNeedsDisplay:YES];
}

// This is the only one of the display family of calls that we use, and the way we do
// displaying in WebCore means this is called on this NSView explicitly, so this catches
// all cases where we are inside the normal display machinery. (Used only by the insertion
// point method below.)
- (void)displayRectIgnoringOpacity:(NSRect)rect
{
    [controller setInDrawingMachinery:YES];
    [super displayRectIgnoringOpacity:rect];
    [controller setInDrawingMachinery:NO];
}

- (BOOL)textView:(NSTextView *)view shouldDrawInsertionPointInRect:(NSRect)rect color:(NSColor *)color turnedOn:(BOOL)drawInsteadOfErase
{
    return [controller textView:view shouldDrawInsertionPointInRect:rect color:color turnedOn:drawInsteadOfErase];
}

- (BOOL)textView:(NSTextView *)view shouldHandleEvent:(NSEvent *)event
{
    return [controller textView:view shouldHandleEvent:event];
}

- (void)textView:(NSTextView *)view didHandleEvent:(NSEvent *)event
{
    [controller textView:view didHandleEvent:event];
}

- (BOOL)textView:(NSTextView *)view shouldChangeTextInRange:(NSRange)range replacementString:(NSString *)string
{
    return [controller textView:view shouldChangeTextInRange:range replacementString:string]
        && [super textView:view shouldChangeTextInRange:range replacementString:string];
}

- (void)textViewDidChangeSelection:(NSNotification *)notification
{
    [super textViewDidChangeSelection:notification];
    [controller textViewDidChangeSelection:notification];
}

// These next two methods are the workaround for bug 3024443.
// Basically, setFrameSize ends up calling an inappropriate selectText, so we just ignore
// calls to selectText while setFrameSize is running.

- (void)selectText:(id)sender
{
    if (sender == self && inSetFrameSize)
        return;

    // Don't call the NSSecureTextField's selectText if the field is already first responder.
    // If we do, we'll end up deactivating and then reactivating, which will send
    // unwanted onBlur events and wreak havoc in other ways as well by setting the focus
    // back to the window.
    NSText *editor = [self _webcore_currentEditor];
    if (editor) {
        [editor setSelectedRange:NSMakeRange(0, [[editor string] length])];
        return;
    }

    [super selectText:sender];
}

- (void)setFrameSize:(NSSize)size
{
    inSetFrameSize = YES;
    [super setFrameSize:size];
    inSetFrameSize = NO;
}

- (void)textDidEndEditing:(NSNotification *)notification
{
    [controller setHasFocus:NO];
    [super textDidEndEditing:notification];

    // When tabbing from one secure text field to another, the super
    // call above will change the focus, and then turn off bullet mode
    // for the secure field, leaving the plain text showing. As a
    // workaround for this AppKit bug, we detect this circumstance
    // (changing from one secure field to another) and set selectable
    // to YES, and then back to whatever it was - this has the side
    // effect of turning on bullet mode. This is also helpful when
    // we end editing once, but we've already started editing
    // again on the same text field. (On Panther we only did this when
    // advancing to a *different* secure text field.)

    NSTextView *textObject = [notification object];
    id delegate = [textObject delegate];
    if ([delegate isKindOfClass:[NSSecureTextField class]]) {
        BOOL oldSelectable = [textObject isSelectable];
        [textObject setSelectable:YES];
        [textObject setSelectable:oldSelectable];
    }
}

@end

@implementation WebCoreSecureTextFieldCell

- (void)editWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate event:(NSEvent *)event
{
    [super editWithFrame:frame inView:view editor:editor delegate:delegate event:event];
    ASSERT([delegate isKindOfClass:[WebCoreSecureTextField class]]);
    [[(WebCoreSecureTextField *)delegate controller] setHasFocus:YES];
}

- (void)selectWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate start:(int)start length:(int)length
{
    [super selectWithFrame:frame inView:view editor:editor delegate:delegate start:start length:length];
    ASSERT([delegate isKindOfClass:[WebCoreSecureTextField class]]);
    [[(WebCoreSecureTextField *)delegate controller] setHasFocus:YES];
}

- (NSMutableDictionary *)_textAttributes
{
    ASSERT([[self controlView] isKindOfClass:[WebCoreSecureTextField class]]);
    NSMutableDictionary* attributes = [super _textAttributes];
    [[(WebCoreSecureTextField*)[self controlView] controller] updateTextAttributes:attributes];
    return attributes;
}

// Ignore the per-application typesetter setting and instead always use the latest behavior for
// text fields in web pages. This fixes the "text fields too tall" problem.
- (NSTypesetterBehavior)_typesetterBehavior
{
    return NSTypesetterLatestBehavior;
}

@end

@implementation WebCoreSearchField

+ (Class)cellClass
{
    return [WebCoreSearchFieldCell class];
}

- (id)initWithQLineEdit:(TextField *)w 
{
    self = [self init];
    if (!self)
        return nil;
    controller = [[WebCoreTextFieldController alloc] initWithTextField:self TextField:w];
    return self;
}

- (void)dealloc
{
    [controller release];
    [super dealloc];
}

- (WebCoreTextFieldController *)controller
{
    return controller;
}

- (Widget *)widget
{
    return [controller widget];
}

- (void)selectText:(id)sender
{
    // Don't call the NSTextField's selectText if the field is already first responder.
    // If we do, we'll end up deactivating and then reactivating, which will send
    // unwanted onBlur events.
    NSText *editor = [self currentEditor];
    if (editor) {
        [editor setSelectedRange:NSMakeRange(0, [[editor string] length])];
        return;
    }
    
    [super selectText:sender];
}

- (void)setStringValue:(NSString *)string
{
    [super setStringValue:[controller preprocessString:string]];
    [controller textChanged];
}

- (NSView *)nextKeyView
{
    if (!inNextValidKeyView)
        return [super nextKeyView];
    Widget* widget = [controller widget];
    if (!widget)
        return [super nextKeyView];
    return FrameMac::nextKeyViewForWidget(widget, SelectingNext);
}

- (NSView *)previousKeyView
{
    if (!inNextValidKeyView)
        return [super previousKeyView];
    Widget* widget = [controller widget];
    if (!widget)
        return [super previousKeyView];
    return FrameMac::nextKeyViewForWidget(widget, SelectingPrevious);
}

- (NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (BOOL)acceptsFirstResponder
{
    return [self isEnabled];
}

- (void)display
{
    // This is a workaround for Radar 2753974.
    // Also, in the web page context, it's never OK to just display.
    [self setNeedsDisplay:YES];
}

// This is the only one of the display family of calls that we use, and the way we do
// displaying in WebCore means this is called on this NSView explicitly, so this catches
// all cases where we are inside the normal display machinery. (Used only by the insertion
// point method below.)
- (void)displayRectIgnoringOpacity:(NSRect)rect
{
    [controller setInDrawingMachinery:YES];
    [super displayRectIgnoringOpacity:rect];
    [controller setInDrawingMachinery:NO];
}

- (BOOL)textView:(NSTextView *)view shouldDrawInsertionPointInRect:(NSRect)rect color:(NSColor *)color turnedOn:(BOOL)drawInsteadOfErase
{
    return [controller textView:view shouldDrawInsertionPointInRect:rect color:color turnedOn:drawInsteadOfErase];
}

- (BOOL)textView:(NSTextView *)view shouldHandleEvent:(NSEvent *)event
{
    return [controller textView:view shouldHandleEvent:event];
}

- (void)textView:(NSTextView *)view didHandleEvent:(NSEvent *)event
{
    [controller textView:view didHandleEvent:event];
}

- (BOOL)textView:(NSTextView *)view shouldChangeTextInRange:(NSRange)range replacementString:(NSString *)string
{
    return [controller textView:view shouldChangeTextInRange:range replacementString:string]
        && [super textView:view shouldChangeTextInRange:range replacementString:string];
}

- (void)textViewDidChangeSelection:(NSNotification *)notification
{
    [super textViewDidChangeSelection:notification];
    [controller textViewDidChangeSelection:notification];
}

- (void)textDidEndEditing:(NSNotification *)notification
{
    [controller setHasFocus:NO];
    [super textDidEndEditing:notification];
}

@end

@implementation WebCoreSearchFieldCell

- (void)editWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate event:(NSEvent *)event
{
    [super editWithFrame:frame inView:view editor:editor delegate:delegate event:event];
    ASSERT([delegate isKindOfClass:[WebCoreSearchField class]]);
    [[(WebCoreSearchField *)delegate controller] setHasFocus:YES];
}

- (void)selectWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate start:(int)start length:(int)length
{
    [super selectWithFrame:frame inView:view editor:editor delegate:delegate start:start length:length];
    ASSERT([delegate isKindOfClass:[WebCoreSearchField class]]);
    [[(WebCoreSearchField *)delegate controller] setHasFocus:YES];
}

- (void)_addStringToRecentSearches:(NSString *)string
{
    ASSERT([[self controlView] isKindOfClass:[WebCoreSearchField class]]);
    Frame *frame = Frame::frameForWidget([(WebCoreSearchField*)[self controlView] widget]);
    if (frame && frame->settings()->privateBrowsingEnabled())
        return;

    [super _addStringToRecentSearches:string];
}

- (NSMutableDictionary *)_textAttributes
{
    ASSERT([[self controlView] isKindOfClass:[WebCoreSearchField class]]);
    NSMutableDictionary* attributes = [super _textAttributes];
    [[(WebCoreSearchField*)[self controlView] controller] updateTextAttributes:attributes];
    return attributes;
}

// Ignore the per-application typesetter setting and instead always use the latest behavior for
// text fields in web pages. This fixes the "text fields too tall" problem.
- (NSTypesetterBehavior)_typesetterBehavior
{
    return NSTypesetterLatestBehavior;
}

@end

@implementation WebCoreTextFieldFormatter

- init
{
    self = [super init];
    if (!self)
        return nil;
    maxLength = INT_MAX;
    return self;
}

- (void)setMaximumLength:(int)len
{
    maxLength = len;
}

- (int)maximumLength
{
    return maxLength;
}

- (NSString *)stringForObjectValue:(id)object
{
    return (NSString *)object;
}

- (BOOL)getObjectValue:(id *)object forString:(NSString *)string errorDescription:(NSString **)error
{
    *object = string;
    return YES;
}

- (BOOL)isPartialStringValid:(NSString **)partialStringPtr proposedSelectedRange:(NSRangePointer)proposedSelectedRangePtr
    originalString:(NSString *)originalString originalSelectedRange:(NSRange)originalSelectedRange errorDescription:(NSString **)errorDescription
{
    NSString *p = *partialStringPtr;

    int length = [p _webcore_numComposedCharacterSequences];
    if (length <= maxLength) {
        return YES;
    }

    int composedSequencesToRemove = length - maxLength;
    int removeRangeEnd = proposedSelectedRangePtr->location;
    int removeRangeStart = removeRangeEnd;
    while (composedSequencesToRemove > 0 && removeRangeStart != 0) {
        removeRangeStart = [p rangeOfComposedCharacterSequenceAtIndex:removeRangeStart - 1].location;
        --composedSequencesToRemove;
    }

    if (removeRangeStart != 0) {
        *partialStringPtr = [[p substringToIndex:removeRangeStart] stringByAppendingString:[p substringFromIndex:removeRangeEnd]];
        proposedSelectedRangePtr->location = removeRangeStart;
    }

    return NO;
}

- (NSAttributedString *)attributedStringForObjectValue:(id)anObject withDefaultAttributes:(NSDictionary *)attributes
{
    return nil;
}

@end

@implementation NSString (WebCoreTextField)

- (int)_webcore_numComposedCharacterSequences
{
    unsigned i = 0;
    unsigned l = [self length];
    int num = 0;
    while (i < l) {
        i = NSMaxRange([self rangeOfComposedCharacterSequenceAtIndex:i]);
        ++num;
    }
    return num;
}

- (NSString *)_webcore_truncateToNumComposedCharacterSequences:(int)num
{
    unsigned i = 0;
    unsigned l = [self length];
    if (l == 0) {
        return self;
    }
    for (int j = 0; j < num; j++) {
        i = NSMaxRange([self rangeOfComposedCharacterSequenceAtIndex:i]);
        if (i >= l) {
            return self;
        }
    }
    return [self substringToIndex:i];
}

@end

@implementation NSTextField (WebCoreTextField)

// The currentEditor method does not work for secure text fields.
// This works around that limitation.
- (NSTextView *)_webcore_currentEditor
{
    NSResponder *firstResponder = [[self window] firstResponder];
    if ([firstResponder isKindOfClass:[NSTextView class]]) {
        NSTextView *editor = (NSTextView *)firstResponder;
        id delegate = [editor delegate];
        if (delegate == self)
            return editor;
    }
    return nil;
}

@end
