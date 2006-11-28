/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebNSAttributedStringExtras.h"

#import "BlockExceptions.h"
#import "Document.h"
#import "Element.h"
#import "FontData.h"
#import "HTMLNames.h"
#import "Image.h"
#import "InlineTextBox.h"
#import "KURL.h"
#import "Range.h"
#import "RenderImage.h"
#import "RenderListItem.h"
#import "RenderObject.h"
#import "RenderStyle.h"
#import "RenderText.h"

using namespace WebCore;
using namespace HTMLNames;

#define BULLET_CHAR 0x2022
#define SQUARE_CHAR 0x25AA
#define CIRCLE_CHAR 0x25E6

struct ListItemInfo {
    unsigned start;
    unsigned end;
};

@implementation NSAttributedString (WebKitExtras)

- (NSAttributedString *)_web_attributedStringByStrippingAttachmentCharacters
{
    // This code was originally copied from NSTextView
    NSRange attachmentRange;
    NSString *originalString = [self string];
    static NSString *attachmentCharString = nil;
    
    if (!attachmentCharString) {
        unichar chars[2];
        if (!attachmentCharString) {
            chars[0] = NSAttachmentCharacter;
            chars[1] = 0;
            attachmentCharString = [[NSString alloc] initWithCharacters:chars length:1];
        }
    }
    
    attachmentRange = [originalString rangeOfString:attachmentCharString];
    if (attachmentRange.location != NSNotFound && attachmentRange.length > 0) {
        NSMutableAttributedString *newAttributedString = [[self mutableCopyWithZone:NULL] autorelease];
        
        while (attachmentRange.location != NSNotFound && attachmentRange.length > 0) {
            [newAttributedString replaceCharactersInRange:attachmentRange withString:@""];
            attachmentRange = [[newAttributedString string] rangeOfString:attachmentCharString];
        }
        return newAttributedString;
    }
    
    return self;
}

@end
