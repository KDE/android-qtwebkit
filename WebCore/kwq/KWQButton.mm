/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQButton.h"

#import "KWQAssertions.h"
#import "KWQCheckBox.h"
#import "KWQExceptions.h"
#import "KWQKHTMLPart.h"
#import "KWQNSViewExtras.h"
#import "WebCoreBridge.h"

#import "render_form.h"

@interface NSCell (KWQButtonKnowsAppKitSecrets)
- (NSMutableDictionary *)_textAttributes;
@end

@interface KWQButton : NSButton
{
    QButton *button;
    BOOL needToSendConsumedMouseUp;
    BOOL inNextValidKeyView;
}

- (id)initWithQButton:(QButton *)b;
- (void)sendConsumedMouseUpIfNeeded;

@end

@interface KWQButtonCell : NSButtonCell
{
    NSWritingDirection baseWritingDirection;
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;

@end

@implementation KWQButton

+ (Class)cellClass
{
    return [KWQButtonCell class];
}

- (id)initWithQButton:(QButton *)b
{
    button = b;
    return [self init];
}

- (void)action:(id)sender
{
    button->clicked();
}

- (void)sendConsumedMouseUpIfNeeded
{
    if (needToSendConsumedMouseUp) {
	needToSendConsumedMouseUp = NO;
	if ([self target]) {
            button->sendConsumedMouseUp();
        }
    } 
}

-(void)mouseDown:(NSEvent *)event
{
    needToSendConsumedMouseUp = YES;
    [super mouseDown:event];
    [self sendConsumedMouseUpIfNeeded];
}

- (BOOL)becomeFirstResponder
{
    BOOL become = [super becomeFirstResponder];
    if (become) {
        if (!KWQKHTMLPart::currentEventIsMouseDownInWidget(button)) {
            [self _KWQ_scrollFrameToVisible];
        }
        QFocusEvent event(QEvent::FocusIn);
        const_cast<QObject *>(button->eventFilterObject())->eventFilter(button, &event);
    }
    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        QFocusEvent event(QEvent::FocusOut);
        const_cast<QObject *>(button->eventFilterObject())->eventFilter(button, &event);
    }
    return resign;
}

-(NSView *)nextKeyView
{
    NSView *view = nil;
    if (button && inNextValidKeyView) {
        // resign so we send a blur before setting focus on
        // the next widget, otherwise the blur for this
        // widget will remove focus from the widget after
        // we tab to it
        [self resignFirstResponder];
        view = KWQKHTMLPart::nextKeyViewForWidget(button, KWQSelectingNext);
    }
    else { 
        view = [super nextKeyView];
    }
    return view;
}

-(NSView *)previousKeyView
{
    NSView *view = nil;
    if (button && inNextValidKeyView) {
        // resign so we send a blur before setting focus on
        // the next widget, otherwise the blur for this
        // widget will remove focus from the widget after
        // we tab to it
        [self resignFirstResponder];
        view = KWQKHTMLPart::nextKeyViewForWidget(button, KWQSelectingPrevious);
    }
    else { 
        view = [super previousKeyView];
    }
    return view;
}

- (BOOL)canBecomeKeyView {
    // Simplified method from NSView; overridden to replace NSView's way of checking
    // for full keyboard access with ours.
    if (!KWQKHTMLPart::partForWidget(button)->tabsToAllControls()) {
        return NO;
    }
    
    return ([self window] != nil) && ![self isHiddenOrHasHiddenAncestor] && [self acceptsFirstResponder];
}

-(NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

-(NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

@end

@implementation KWQButtonCell

- (NSWritingDirection)baseWritingDirection
{
    return baseWritingDirection;
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction
{
    baseWritingDirection = direction;
}

- (NSMutableDictionary *)_textAttributes
{
    NSMutableDictionary *attributes = [super _textAttributes];
    NSParagraphStyle *style = [attributes objectForKey:NSParagraphStyleAttributeName];
    ASSERT(style != nil);
    if ([style baseWritingDirection] != baseWritingDirection) {
        NSMutableParagraphStyle *mutableStyle = [style mutableCopy];
        [mutableStyle setBaseWritingDirection:baseWritingDirection];
        [attributes setObject:mutableStyle forKey:NSParagraphStyleAttributeName];
        [mutableStyle release];
    }
    return attributes;
}

@end

QButton::QButton()
    : m_clicked(this, SIGNAL(clicked()))
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQButton *button = [[KWQButton alloc] initWithQButton:this];
    setView(button);
    [button release];
    
    [button setTarget:button];
    [button setAction:@selector(action:)];
    
    [button setTitle:@""];
    [[button cell] setControlSize:NSSmallControlSize];
    [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]]];

    KWQ_UNBLOCK_EXCEPTIONS;
}

QButton::~QButton()
{
    KWQ_BLOCK_EXCEPTIONS;

    NSButton *button = (NSButton *)getView();
    [button setTarget:nil];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QButton::setText(const QString &s)
{
    KWQ_BLOCK_EXCEPTIONS;

    NSButton *button = (NSButton *)getView();
    [button setTitle:s.getNSString()];

    KWQ_UNBLOCK_EXCEPTIONS;
}

QString QButton::text() const
{
    QString result;

    KWQ_BLOCK_EXCEPTIONS;

    NSButton *button = (NSButton *)getView();
    result = QString::fromNSString([button title]);
    
    KWQ_UNBLOCK_EXCEPTIONS;

    return result;
}

void QButton::clicked()
{
    // Order of signals is:
    //   1) signals in subclasses (stateChanged, not sure if there are any others)
    //   2) mouse up
    //   3) clicked
    // Proper behavior of check boxes, at least, depends on this order.
    
    KWQ_BLOCK_EXCEPTIONS;

    KWQButton *button = (KWQButton *)getView();
    [button sendConsumedMouseUpIfNeeded];

    // Don't call clicked if the button was destroyed inside of sendConsumedMouseUpIfNeeded.
    if ([button target]) {
        m_clicked.call();
    }

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QButton::click()
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQButton *button = (KWQButton *)getView();
    [button performClick:nil];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QButton::setFont(const QFont &f)
{
    KWQ_BLOCK_EXCEPTIONS;

    QWidget::setFont(f);

    const NSControlSize size = KWQNSControlSizeForFont(f);    
    NSControl * const button = static_cast<NSControl *>(getView());
    [[button cell] setControlSize:size];
    [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];

    KWQ_UNBLOCK_EXCEPTIONS;
}

NSControlSize KWQNSControlSizeForFont(const QFont &f)
{
    const int fontSize = f.pixelSize();
    if (fontSize >= 16) {
        return NSRegularControlSize;
    }
    if (fontSize >= 11) {
        return NSSmallControlSize;
    }
    return NSMiniControlSize;
}

QWidget::FocusPolicy QButton::focusPolicy() const
{
    KWQ_BLOCK_EXCEPTIONS;

    // Add an additional check here.
    // For now, buttons are only focused when full
    // keyboard access is turned on.
    unsigned keyboardUIMode = [KWQKHTMLPart::bridgeForWidget(this) keyboardUIMode];
    if ((keyboardUIMode & WebCoreKeyboardAccessFull) == 0)
        return NoFocus;
    
    KWQ_UNBLOCK_EXCEPTIONS;

    return QWidget::focusPolicy();
}

void QButton::setWritingDirection(QPainter::TextDirection direction)
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQButton *button = getView();
    KWQButtonCell *cell = [button cell];
    NSWritingDirection d = direction == QPainter::RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    if ([cell baseWritingDirection] != d) {
        [cell setBaseWritingDirection:d];
        [button setNeedsDisplay:YES];
    }

    KWQ_UNBLOCK_EXCEPTIONS;
}
