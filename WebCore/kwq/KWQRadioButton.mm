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

#import "KWQRadioButton.h"

#import "KWQExceptions.h"

enum {
    topMargin,
    bottomMargin,
    leftMargin,
    rightMargin,
    baselineFudgeFactor,
    dimWidth,
    dimHeight
};

QRadioButton::QRadioButton(QWidget *w)
{
    NSButton *button = (NSButton *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [button setButtonType:NSRadioButton];
    KWQ_UNBLOCK_EXCEPTIONS;
}

QSize QRadioButton::sizeHint() const 
{
    return QSize(dimensions()[dimWidth], dimensions()[dimHeight]);
}

QRect QRadioButton::frameGeometry() const
{
    QRect r = QWidget::frameGeometry();
    return QRect(r.x() + dimensions()[leftMargin], r.y() + dimensions()[topMargin],
        r.width() - (dimensions()[leftMargin] + dimensions()[rightMargin]),
        r.height() - (dimensions()[topMargin] + dimensions()[bottomMargin]));
}

void QRadioButton::setFrameGeometry(const QRect &r)
{
    QWidget::setFrameGeometry(QRect(r.x() - dimensions()[leftMargin], r.y() - dimensions()[topMargin],
        r.width() + dimensions()[leftMargin] + dimensions()[rightMargin],
        r.height() + dimensions()[topMargin] + dimensions()[bottomMargin]));
}

void QRadioButton::setChecked(bool isChecked)
{
    NSButton *button = (NSButton *)getView();
    KWQ_BLOCK_EXCEPTIONS;
    [button setState:isChecked ? NSOnState : NSOffState];
    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QRadioButton::isChecked() const
{
    NSButton *button = (NSButton *)getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    return [button state] == NSOnState;
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

int QRadioButton::baselinePosition(int height) const
{
    return height - dimensions()[baselineFudgeFactor];
}

const int *QRadioButton::dimensions() const
{
    // We empirically determined these dimensions.
    // It would be better to get this info from AppKit somehow.
    static const int w[3][7] = {
        { 2, 4, 2, 2, 2, 14, 15 },
        { 3, 3, 2, 2, 2, 12, 13 },
        { 1, 2, 0, 0, 2, 10, 10 },
    };
    NSControl * const button = static_cast<NSControl *>(getView());

    KWQ_BLOCK_EXCEPTIONS;
    return w[[[button cell] controlSize]];
    KWQ_UNBLOCK_EXCEPTIONS;

    return w[NSSmallControlSize];
}
