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

#import "WebCoreWidgetHolder.h"

namespace WebCore {
    class TextField;
}

@class WebCoreTextFieldFormatter;

@interface WebCoreTextFieldController : NSObject
{
@private
    NSTextField* field;
    WebCore::TextField *widget;
    WebCoreTextFieldFormatter *formatter;
    BOOL hasFocus;
    BOOL hasFocusAndSelectionSet;
    BOOL edited;
    NSRange lastSelectedRange;
    BOOL inDrawingMachinery;
    NSWritingDirection baseWritingDirection;
}

- (void)detachWidget;

- (BOOL)hasSelection;

- (void)setMaximumLength:(int)len;
- (int)maximumLength;

- (void)setEdited:(BOOL)edited;
- (BOOL)edited;

- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;

- (NSRange)selectedRange;
- (void)setSelectedRange:(NSRange)range;

- (NSString *)string;

@end

@interface WebCoreTextField : NSTextField <WebCoreWidgetHolder>
{
@private
    WebCoreTextFieldController* controller;
    BOOL inNextValidKeyView;
}

- (id)initWithWidget:(WebCore::TextField *)widget;
- (WebCoreTextFieldController *)controller;

@end

@interface WebCoreSecureTextField : NSSecureTextField <WebCoreWidgetHolder>
{
@private
    WebCoreTextFieldController* controller;
    BOOL inNextValidKeyView;
    BOOL inSetFrameSize;
}

- (id)initWithWidget:(WebCore::TextField *)widget;
- (WebCoreTextFieldController *)controller;

@end

@interface WebCoreSearchField : NSSearchField <WebCoreWidgetHolder>
{
@private
    WebCoreTextFieldController* controller;
    BOOL inNextValidKeyView;
}

- (id)initWithWidget:(WebCore::TextField *)widget;
- (WebCoreTextFieldController *)controller;

@end

