/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import "KWQTextField.h"

#import "KWQAssertions.h"
#import "KWQKHTMLPart.h"
#import "KWQLineEdit.h"
#import "KWQNSViewExtras.h"
#import "KWQView.h"
#import "WebCoreBridge.h"

@interface NSString (KWQTextField)
- (int)_KWQ_numComposedCharacterSequences;
- (NSString *)_KWQ_truncateToNumComposedCharacterSequences:(int)num;
@end

@interface NSTextField (KWQTextField)
- (NSText *)_KWQ_currentEditor;
@end

@interface NSCell (KWQTextFieldKnowsAppKitSecrets)
- (NSMutableDictionary *)_textAttributes;
@end

// The three cell subclasses allow us to tell when we get focus without an editor subclass,
// and override the base writing direction.
@interface KWQTextFieldCell : NSTextFieldCell
@end
@interface KWQSecureTextFieldCell : NSSecureTextFieldCell
@end
@interface KWQSearchFieldCell : NSSearchFieldCell
@end

// KWQTextFieldFormatter enforces a maximum length.
@interface KWQTextFieldFormatter : NSFormatter
{
    int maxLength;
}
- (void)setMaximumLength:(int)len;
- (int)maximumLength;
@end

@interface KWQTextFieldController (KWQInternal)
- (id)initWithTextField:(NSTextField *)f QLineEdit:(QLineEdit *)w;
- (QWidget *)widget;
- (void)textChanged;
- (void)setInDrawingMachinery:(BOOL)inDrawing;
- (BOOL)textView:(NSTextView *)view shouldDrawInsertionPointInRect:(NSRect)rect color:(NSColor *)color turnedOn:(BOOL)drawInsteadOfErase;
- (BOOL)textView:(NSTextView *)view shouldHandleEvent:(NSEvent *)event;
- (void)textView:(NSTextView *)view didHandleEvent:(NSEvent *)event;
- (BOOL)textView:(NSTextView *)view shouldChangeTextInRange:(NSRange)range replacementString:(NSString *)string;
- (void)updateTextAttributes:(NSMutableDictionary *)attributes;
- (NSString *)preprocessString:(NSString *)string;
@end

@implementation KWQTextFieldController

- (id)initWithTextField:(NSTextField *)f QLineEdit:(QLineEdit *)w
{
    [self init];

    // This is initialization that's shared by all types of text fields.
    widget = w;
    field = f;
    formatter = [[KWQTextFieldFormatter alloc] init];
    lastSelectedRange.location = NSNotFound;
    [[field cell] setScrollable:YES];
    [field setFormatter:formatter];
    [field setDelegate:self];
    
    if (widget->type() == QLineEdit::Search) {
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
    if (!widget)
	return;
    widget->textChanged();
    if (!widget)
        return;
    widget->performSearch();
}

- (void)dealloc
{
    [formatter release];
    [super dealloc];
}

- (QWidget*)widget
{
    return widget;
}

- (void)setMaximumLength:(int)len
{
    NSString *oldValue = [self string];
    if ([oldValue _KWQ_numComposedCharacterSequences] > len) {
        [field setStringValue:[oldValue _KWQ_truncateToNumComposedCharacterSequences:len]];
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

- (void)controlTextDidBeginEditing:(NSNotification *)notification
{
    if (!widget)
	return;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge controlTextDidBeginEditing:notification];
}

- (void)controlTextDidEndEditing:(NSNotification *)notification
{
    if (!widget)
	return;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge controlTextDidEndEditing:notification];
    
    [self setHasFocus:NO];

    if (widget && [[[notification userInfo] objectForKey:@"NSTextMovement"] intValue] == NSReturnTextMovement)
        widget->returnPressed();
}

- (void)controlTextDidChange:(NSNotification *)notification
{
    if (!widget)
	return;
    
    if (KWQKHTMLPart::handleKeyboardOptionTabInView(field))
        return;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge controlTextDidChange:notification];
    
    edited = YES;
    if (widget) {
        widget->textChanged();
    }
}

- (BOOL)control:(NSControl *)control textShouldBeginEditing:(NSText *)fieldEditor
{
    if (!widget)
        return NO;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    
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
    
    return [bridge control:control textShouldBeginEditing:fieldEditor];
}

- (BOOL)control:(NSControl *)control textShouldEndEditing:(NSText *)fieldEditor
{
    if (!widget)
	return NO;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control textShouldEndEditing:fieldEditor];
}

- (BOOL)control:(NSControl *)control didFailToFormatString:(NSString *)string errorDescription:(NSString *)error
{
    if (!widget)
	return NO;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control didFailToFormatString:string errorDescription:error];
}

- (void)control:(NSControl *)control didFailToValidatePartialString:(NSString *)string errorDescription:(NSString *)error
{
    if (!widget)
	return;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    [bridge control:control didFailToValidatePartialString:string errorDescription:error];
}

- (BOOL)control:(NSControl *)control isValidObject:(id)obj
{
    if (!widget)
	return NO;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control isValidObject:obj];
}

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector
{
    if (!widget)
	return NO;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    return [bridge control:control textView:textView doCommandBySelector:commandSelector];
}

- (void)textChanged
{
    if (widget)
        widget->textChanged();
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
        WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);

        QWidget::setDeferFirstResponderChanges(true);

        BOOL intercepted = [bridge control:field textView:view shouldHandleEvent:event];
        if (!intercepted) {
            intercepted = [bridge interceptKeyEvent:event toView:view];
        }

        // Always intercept key up events because we don't want them
        // passed along the responder chain. This is arguably a bug in
        // NSTextView; see Radar 3507083.
        if (type == NSKeyUp) {
            intercepted = YES;
        }

        if (intercepted || !widget) {
            QWidget::setDeferFirstResponderChanges(false);
            return NO;
        }
    }

    return YES;
}

- (void)textView:(NSTextView *)view didHandleEvent:(NSEvent *)event
{
    QWidget::setDeferFirstResponderChanges(false);

    if (!widget)
	return;

    if ([event type] == NSLeftMouseUp) {
        widget->sendConsumedMouseUp();
        if (widget) {
            widget->clicked();
        }
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
    NSText *editor = [field _KWQ_currentEditor];
    return editor ? [editor selectedRange] : NSMakeRange(NSNotFound, 0);
}

- (void)setSelectedRange:(NSRange)range
{
    // Range check just in case the saved range has gotten out of sync.
    // Even though we don't see this in testing, we really don't want
    // an exception in this case, so we protect ourselves.
    NSText *editor = [field _KWQ_currentEditor];    
    if (NSMaxRange(range) <= [[editor string] length]) {
        [editor setSelectedRange:range];
    }
}

- (void)setHasFocus:(BOOL)nowHasFocus
{
    if (!widget || nowHasFocus == hasFocus)
	return;

    hasFocus = nowHasFocus;
    
    if (nowHasFocus) {
        // Select all the text if we are tabbing in, but otherwise preserve/remember
        // the selection from last time we had focus (to match WinIE).
        if ([[field window] keyViewSelectionDirection] != NSDirectSelection)
            lastSelectedRange.location = NSNotFound;
        
        if (lastSelectedRange.location != NSNotFound)
            [self setSelectedRange:lastSelectedRange];
        
        if (!KWQKHTMLPart::currentEventIsMouseDownInWidget(widget))
            [field _KWQ_scrollFrameToVisible];
        
        if (widget) {
            QFocusEvent event(QEvent::FocusIn);
            const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);
        }
        
	// Sending the onFocus event above, may have resulted in a blur() - if this
	// happens when tabbing from another text field, then endEditing: and
	// controlTextDidEndEditing: will never be called. The bad side effects of this 
	// include the fact that our idea of the focus state will be wrong;
	// and the text field will think it's still editing, so it will continue to draw
	// the focus ring. So we call endEditing: manually if we detect this inconsistency,
	// and the correct our internal impression of the focus state.
	if ([field _KWQ_currentEditor] == nil && [field currentEditor] != nil) {
	    [[field cell] endEditing:[field currentEditor]];
	    [self setHasFocus:NO];
	}
    } else {
        lastSelectedRange = [self selectedRange];
        
        if (widget) {
            QFocusEvent event(QEvent::FocusOut);
            const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);
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
#if BUILDING_ON_PANTHER
    // On Panther, the secure text field's editor does not contain the real
    // string, so we must always call stringValue on the field. We'll live
    // with the side effect of ending International inline input for these
    // password fields on Panther only, since it's fixed in Tiger.
    if ([field isKindOfClass:[NSSecureTextField class]]) {
	    return [field stringValue];
    }
#endif
    // Calling stringValue can have a side effect of ending International inline input.
    // So don't call it unless there's no editor.
    NSText *editor = [field _KWQ_currentEditor];
    if (editor == nil) {
        return [field stringValue];
    }
    return [editor string];
}

- (BOOL)textView:(NSTextView *)textView shouldChangeTextInRange:(NSRange)range replacementString:(NSString *)string
{
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
    return [result _KWQ_truncateToNumComposedCharacterSequences:[formatter maximumLength]];
}

@end

@implementation KWQTextField

+ (Class)cellClass
{
    return [KWQTextFieldCell class];
}

- (id)initWithQLineEdit:(QLineEdit *)w 
{
    [self init];
    controller = [[KWQTextFieldController alloc] initWithTextField:self QLineEdit:w];
    return self;
}

- (void)dealloc
{
    [controller release];
    [super dealloc];
}

- (KWQTextFieldController *)controller
{
    return controller;
}

- (QWidget *)widget
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
    QWidget* widget = [controller widget];
    if (!widget)
	return [super nextKeyView];
    return KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingNext);
}

- (NSView *)previousKeyView
{
    if (!inNextValidKeyView)
	return [super previousKeyView];
    QWidget* widget = [controller widget];
    if (!widget)
	return [super previousKeyView];
    return KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingPrevious);
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

@end

@implementation KWQTextFieldCell

- (void)editWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate event:(NSEvent *)event
{
    [super editWithFrame:frame inView:view editor:editor delegate:delegate event:event];
    ASSERT([delegate isKindOfClass:[KWQTextField class]]);
    [[(KWQTextField *)delegate controller] setHasFocus:YES];
}

- (void)selectWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate start:(int)start length:(int)length
{
    [super selectWithFrame:frame inView:view editor:editor delegate:delegate start:start length:length];
    ASSERT([delegate isKindOfClass:[KWQTextField class]]);
    [[(KWQTextField *)delegate controller] setHasFocus:YES];
}

- (NSMutableDictionary *)_textAttributes
{
    ASSERT([[self controlView] isKindOfClass:[KWQTextField class]]);
    NSMutableDictionary* attributes = [super _textAttributes];
    [[(KWQTextField*)[self controlView] controller] updateTextAttributes:attributes];
    return attributes;
}

// Ignore the per-application typesetter setting and instead always use the latest behavior for
// text fields in web pages. This fixes the "text fields too tall" problem.
- (NSTypesetterBehavior)_typesetterBehavior
{
    return NSTypesetterLatestBehavior;
}

@end


@implementation KWQSecureTextField

+ (Class)cellClass
{
    return [KWQSecureTextFieldCell class];
}

- (id)initWithQLineEdit:(QLineEdit *)w 
{
    [self init];
    controller = [[KWQTextFieldController alloc] initWithTextField:self QLineEdit:w];
    return self;
}

- (void)dealloc
{
    [controller release];
    [super dealloc];
}

- (KWQTextFieldController *)controller
{
    return controller;
}

- (QWidget *)widget
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
    QWidget* widget = [controller widget];
    if (!widget)
	return [super nextKeyView];
    return KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingNext);
}

- (NSView *)previousKeyView
{
    if (!inNextValidKeyView)
	return [super previousKeyView];
    QWidget* widget = [controller widget];
    if (!widget)
	return [super previousKeyView];
    return KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingPrevious);
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

// These next two methods are the workaround for bug 3024443.
// Basically, setFrameSize ends up calling an inappropriate selectText, so we just ignore
// calls to selectText while setFrameSize is running.

- (void)selectText:(id)sender
{
    if (sender == self && inSetFrameSize) {
        return;
    }

    // Don't call the NSSecureTextField's selectText if the field is already first responder.
    // If we do, we'll end up deactivating and then reactivating, which will send
    // unwanted onBlur events and wreak havoc in other ways as well by setting the focus
    // back to the window.
    NSText *editor = [self _KWQ_currentEditor];
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

@implementation KWQSecureTextFieldCell

- (void)editWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate event:(NSEvent *)event
{
    [super editWithFrame:frame inView:view editor:editor delegate:delegate event:event];
    ASSERT([delegate isKindOfClass:[KWQSecureTextField class]]);
    [[(KWQSecureTextField *)delegate controller] setHasFocus:YES];
}

- (void)selectWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate start:(int)start length:(int)length
{
    [super selectWithFrame:frame inView:view editor:editor delegate:delegate start:start length:length];
    ASSERT([delegate isKindOfClass:[KWQSecureTextField class]]);
    [[(KWQSecureTextField *)delegate controller] setHasFocus:YES];
}

- (NSMutableDictionary *)_textAttributes
{
    ASSERT([[self controlView] isKindOfClass:[KWQSecureTextField class]]);
    NSMutableDictionary* attributes = [super _textAttributes];
    [[(KWQSecureTextField*)[self controlView] controller] updateTextAttributes:attributes];
    return attributes;
}

// Ignore the per-application typesetter setting and instead always use the latest behavior for
// text fields in web pages. This fixes the "text fields too tall" problem.
- (NSTypesetterBehavior)_typesetterBehavior
{
    return NSTypesetterLatestBehavior;
}

@end

@implementation KWQSearchField

+ (Class)cellClass
{
    return [KWQSearchFieldCell class];
}

- (id)initWithQLineEdit:(QLineEdit *)w 
{
    [self init];
    controller = [[KWQTextFieldController alloc] initWithTextField:self QLineEdit:w];
    return self;
}

- (void)dealloc
{
    [controller release];
    [super dealloc];
}

- (KWQTextFieldController *)controller
{
    return controller;
}

- (QWidget *)widget
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
    QWidget* widget = [controller widget];
    if (!widget)
	return [super nextKeyView];
    return KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingNext);
}

- (NSView *)previousKeyView
{
    if (!inNextValidKeyView)
	return [super previousKeyView];
    QWidget* widget = [controller widget];
    if (!widget)
	return [super previousKeyView];
    return KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingPrevious);
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

@end

@implementation KWQSearchFieldCell

- (void)editWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate event:(NSEvent *)event
{
    [super editWithFrame:frame inView:view editor:editor delegate:delegate event:event];
    ASSERT([delegate isKindOfClass:[KWQSearchField class]]);
    [[(KWQSearchField *)delegate controller] setHasFocus:YES];
}

- (void)selectWithFrame:(NSRect)frame inView:(NSView *)view editor:(NSText *)editor delegate:(id)delegate start:(int)start length:(int)length
{
    [super selectWithFrame:frame inView:view editor:editor delegate:delegate start:start length:length];
    ASSERT([delegate isKindOfClass:[KWQSearchField class]]);
    [[(KWQSearchField *)delegate controller] setHasFocus:YES];
}

- (NSMutableDictionary *)_textAttributes
{
    ASSERT([[self controlView] isKindOfClass:[KWQSearchField class]]);
    NSMutableDictionary* attributes = [super _textAttributes];
    [[(KWQSearchField*)[self controlView] controller] updateTextAttributes:attributes];
    return attributes;
}

// Ignore the per-application typesetter setting and instead always use the latest behavior for
// text fields in web pages. This fixes the "text fields too tall" problem.
- (NSTypesetterBehavior)_typesetterBehavior
{
    return NSTypesetterLatestBehavior;
}

@end

@implementation KWQTextFieldFormatter

- init
{
    [super init];
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

    int length = [p _KWQ_numComposedCharacterSequences];
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

@implementation NSString (KWQTextField)

- (int)_KWQ_numComposedCharacterSequences
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

- (NSString *)_KWQ_truncateToNumComposedCharacterSequences:(int)num
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

@implementation NSTextField (KWQTextField)

// The currentEditor method does not work for secure text fields.
// This works around that limitation.
- (NSText *)_KWQ_currentEditor
{
    NSResponder *firstResponder = [[self window] firstResponder];
    if ([firstResponder isKindOfClass:[NSText class]]) {
        NSText *editor = (NSText *)firstResponder;
        id delegate = [editor delegate];
        if (delegate == self)
            return editor;
    }
    return nil;
}

@end
