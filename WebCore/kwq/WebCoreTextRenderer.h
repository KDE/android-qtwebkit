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

#import <Cocoa/Cocoa.h>

#ifndef ROUND_TO_INT
#define ROUND_TO_INT(x) (unsigned int)((x)+.5)
#endif

struct WebCoreTextStyle
{
    NSColor *textColor;
    NSColor *backgroundColor;
    int letterSpacing;
    int wordSpacing;
    int padding;
    NSString **families;
    unsigned smallCaps:1;
    unsigned rtl:1;
    unsigned applyRounding:1;
    unsigned attemptFontSubstitution:1;
};

struct WebCoreTextRun
{
    const UniChar *characters;
    unsigned int length;
    int from;
    int to;
};


#ifdef __cplusplus
extern "C" {
#endif

typedef struct WebCoreTextRun WebCoreTextRun;
typedef struct WebCoreTextStyle WebCoreTextStyle;

extern void WebCoreInitializeTextRun(WebCoreTextRun *run, const UniChar *characters, unsigned int length, int from, int to);
extern void WebCoreInitializeEmptyTextStyle(WebCoreTextStyle *style);

#ifdef __cplusplus
}
#endif


@protocol WebCoreTextRenderer <NSObject>

// vertical metrics
- (int)ascent;
- (int)descent;
- (int)lineSpacing;
- (float)xHeight;

// horizontal metrics
- (float)floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style widths:(float *)buffer;

// drawing
- (void)drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style atPoint:(NSPoint)point;
- (void)drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style atPoint:(NSPoint)point;
- (void)drawLineForCharacters:(NSPoint)point yOffset:(float)yOffset withWidth:(int)width withColor:(NSColor *)color;

// selection point check
- (int)pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed;
@end
