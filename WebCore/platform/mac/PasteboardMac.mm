/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#import "Pasteboard.h"

#import "Cache.h"
#import "CachedResource.h"
#import "CharacterNames.h"
#import "DOMRangeInternal.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "Editor.h"
#import "EditorClient.h"
#import "HitTestResult.h"
#import "Image.h"
#import "KURL.h"
#import "LoaderNSURLExtras.h"
#import "MimeTypeRegistry.h"
#import "RetainPtr.h"
#import "WebCoreNSStringExtras.h"
#import "WebCoreSystemInterface.h"
#import "markup.h"

@interface NSAttributedString (AppKitSecretsIKnowAbout)
- (id)_initWithDOMRange:(DOMRange *)domRange;
@end

namespace WebCore {

NSString *WebArchivePboardType;
NSString *WebSmartPastePboardType;
NSString *WebURLNamePboardType;
NSString *WebURLPboardType;
NSString *WebURLsWithTitlesPboardType;

#ifndef BUILDING_ON_TIGER
static NSArray* selectionPasteboardTypes(bool canSmartCopyOrDelete, bool selectionContainsAttachments)
{
    if (selectionContainsAttachments) {
        if (canSmartCopyOrDelete)
            return [NSArray arrayWithObjects:WebSmartPastePboardType, WebArchivePboardType, NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, nil];
        else
            return [NSArray arrayWithObjects:WebArchivePboardType, NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, nil];
    } else { // Don't write RTFD to the pasteboard when the copied attributed string has no attachments.
        if (canSmartCopyOrDelete)
            return [NSArray arrayWithObjects:WebSmartPastePboardType, WebArchivePboardType, NSRTFPboardType, NSStringPboardType, nil];
        else
            return [NSArray arrayWithObjects:WebArchivePboardType, NSRTFPboardType, NSStringPboardType, nil];
    }
}
#endif

static NSArray* writableTypesForURL()
{
    static NSArray *types = nil;
    if (!types) {
        types = [[NSArray alloc] initWithObjects:
            WebURLsWithTitlesPboardType,
            NSURLPboardType,
            WebURLPboardType,
            WebURLNamePboardType,
            NSStringPboardType,
            nil];
    }
    return types;
}

static NSArray* writableTypesForImage()
{
    static NSMutableArray *types = nil;
    if (!types) {
        types = [[NSMutableArray alloc] initWithObjects:NSTIFFPboardType, nil];
        [types addObjectsFromArray:writableTypesForURL()];
        [types addObject:NSRTFDPboardType];
    }
    return types;
}

Pasteboard* Pasteboard::generalPasteboard() 
{
    static Pasteboard* pasteboard = new Pasteboard([NSPasteboard generalPasteboard]);
    return pasteboard;
}

Pasteboard::Pasteboard(NSPasteboard* pboard)
    : m_pasteboard(pboard)
{
    WebArchivePboardType          = @"Apple Web Archive pasteboard type";
    WebSmartPastePboardType       = @"NeXT smart paste pasteboard type";
    WebURLNamePboardType          = wkCreateURLNPasteboardFlavorTypeName();
    WebURLPboardType              = wkCreateURLPasteboardFlavorTypeName();
    WebURLsWithTitlesPboardType   = @"WebURLsWithTitlesPboardType";
}

void Pasteboard::clear()
{
    [m_pasteboard declareTypes:[NSArray array] owner:nil];
}

static NSAttributedString *stripAttachmentCharacters(NSAttributedString *string)
{
    const unichar attachmentCharacter = NSAttachmentCharacter;
    static RetainPtr<NSString> attachmentCharacterString = [NSString stringWithCharacters:&attachmentCharacter length:1];
    NSMutableAttributedString *result = [[string mutableCopy] autorelease];
    NSRange attachmentRange = [[result string] rangeOfString:attachmentCharacterString.get()];
    while (attachmentRange.location != NSNotFound) {
        [result replaceCharactersInRange:attachmentRange withString:@""];
        attachmentRange = [[result string] rangeOfString:attachmentCharacterString.get()];
    }
    return result;
}

void Pasteboard::writeSelection(NSPasteboard* pasteboard, Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    if (WebArchivePboardType == nil)
        Pasteboard::generalPasteboard(); //Initialises pasteboard types
    
    NSAttributedString *attributedString = [[[NSAttributedString alloc] _initWithDOMRange:[DOMRange _rangeWith:selectedRange]] autorelease];
#ifdef BUILDING_ON_TIGER
    // 4930197: Mail overrides [WebHTMLView pasteboardTypesForSelection] in order to add another type to the pasteboard
    // after WebKit does.  On Tiger we must call this function so that Mail code will be executed, meaning that 
    // we can't call WebCore::Pasteboard's method for setting types. 
    
    NSArray *types = frame->editor()->client()->pasteboardTypesForSelection(frame);
    // Don't write RTFD to the pasteboard when the copied attributed string has no attachments.
    NSMutableArray *mutableTypes = nil;
    if (![attributedString containsAttachments]) {
        mutableTypes = [[types mutableCopy] autorelease];
        [mutableTypes removeObject:NSRTFDPboardType];
        types = mutableTypes;
    }
    [pasteboard declareTypes:types owner:nil];    
#else
    NSArray *types = selectionPasteboardTypes(canSmartCopyOrDelete, [attributedString containsAttachments]);
    [pasteboard declareTypes:types owner:nil];
    frame->editor()->client()->didSetSelectionTypesForPasteboard();
#endif
    
    // Put HTML on the pasteboard.
    if ([types containsObject:WebArchivePboardType]) {
        [pasteboard setData:frame->editor()->client()->dataForArchivedSelection(frame) forType:WebArchivePboardType];
    }
    
    // Put the attributed string on the pasteboard (RTF/RTFD format).
    if ([types containsObject:NSRTFDPboardType]) {
        NSData *RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFDData forType:NSRTFDPboardType];
    }
    if ([types containsObject:NSRTFPboardType]) {
        if ([attributedString containsAttachments])
            attributedString = stripAttachmentCharacters(attributedString);
        NSData *RTFData = [attributedString RTFFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFData forType:NSRTFPboardType];
    }
    
    // Put plain string on the pasteboard.
    if ([types containsObject:NSStringPboardType]) {
        // Map &nbsp; to a plain old space because this is better for source code, other browsers do it,
        // and because HTML forces you to do this any time you want two spaces in a row.
        String text = frame->selectedText();
        text.replace('\\', frame->backslashAsCurrencySymbol());
        NSMutableString *s = [[[(NSString*)text copy] autorelease] mutableCopy];
        
        NSString *NonBreakingSpaceString = [NSString stringWithCharacters:&noBreakSpace length:1];
        [s replaceOccurrencesOfString:NonBreakingSpaceString withString:@" " options:0 range:NSMakeRange(0, [s length])];
        [pasteboard setString:s forType:NSStringPboardType];
        [s release];
    }
    
    if ([types containsObject:WebSmartPastePboardType]) {
        [pasteboard setData:nil forType:WebSmartPastePboardType];
    }
}
    
void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    Pasteboard::writeSelection(m_pasteboard, selectedRange, canSmartCopyOrDelete, frame);
}

void Pasteboard::writeURL(NSPasteboard* pasteboard, NSArray* types, const KURL& url, const String& titleStr, Frame* frame, bool isImage)
{
    if (WebArchivePboardType == nil)
        Pasteboard::generalPasteboard(); //Initialises pasteboard types
   
    if (types == nil) {
        if (isImage)
            types = writableTypesForImage();
        else
            types = writableTypesForURL();
        [pasteboard declareTypes:types owner:nil];
    }
    
    ASSERT(!url.isEmpty());
    
    NSURL *URL = url.getNSURL();
    NSString *userVisibleString = frame->editor()->client()->userVisibleString(URL);
    
    NSString *title = (NSString*)titleStr;
    if ([title length] == 0) {
        title = [[URL path] lastPathComponent];
        if ([title length] == 0)
            title = userVisibleString;
    }
        
    if ([types containsObject:WebURLsWithTitlesPboardType])
        [pasteboard setPropertyList:[NSArray arrayWithObjects:[NSArray arrayWithObject:userVisibleString], 
                                     [NSArray arrayWithObject:(NSString*)titleStr.stripWhiteSpace()], 
                                     nil]
                            forType:WebURLsWithTitlesPboardType];
    if ([types containsObject:NSURLPboardType])
        [URL writeToPasteboard:pasteboard];
    if ([types containsObject:WebURLPboardType])
        [pasteboard setString:userVisibleString forType:WebURLPboardType];
    if ([types containsObject:WebURLNamePboardType])
        [pasteboard setString:title forType:WebURLNamePboardType];
    if ([types containsObject:NSStringPboardType])
        [pasteboard setString:userVisibleString forType:NSStringPboardType];
}
    
void Pasteboard::writeURL(const KURL& url, const String& titleStr, Frame* frame, bool isImage)
{
    Pasteboard::writeURL(m_pasteboard, nil, url, titleStr, frame, isImage);
}

static NSFileWrapper* fileWrapperForImage(CachedResource* resource, NSURL *URL)
{
    SharedBuffer* coreData = resource->allData();
    NSData *data = [[[NSData alloc] initWithBytes:coreData->platformData() 
        length:coreData->platformDataSize()] autorelease];
    NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:data] autorelease];
    String coreMIMEType = resource->response().mimeType();
    NSString *MIMEType = nil;
    if (!coreMIMEType.isNull())
        MIMEType = coreMIMEType;
    [wrapper setPreferredFilename:suggestedFilenameWithMIMEType(URL, MIMEType)];
    return wrapper;
}

void Pasteboard::writeFileWrapperAsRTFDAttachment(NSFileWrapper* wrapper)
{
    NSTextAttachment *attachment = [[NSTextAttachment alloc] initWithFileWrapper:wrapper];
    
    NSAttributedString *string = [NSAttributedString attributedStringWithAttachment:attachment];
    [attachment release];
    
    NSData *RTFDData = [string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:nil];
    [m_pasteboard setData:RTFDData forType:NSRTFDPboardType];
}

void Pasteboard::writeImage(const HitTestResult& result)
{    
    KURL coreURL = result.absoluteLinkURL();
    if (coreURL.isEmpty())
        coreURL = result.absoluteImageURL();
    NSURL *URL = coreURL.getNSURL();
    ASSERT(URL);

    NSString *title = result.altDisplayString().isNull() ? nil : (NSString*)(result.altDisplayString());
    Frame* frame = result.innerNonSharedNode()->document()->frame();

    writeURL(URL, title, frame, true);
    NSArray *types = [m_pasteboard types];
    [m_pasteboard declareTypes:types owner:nil];

    NSImage *image = (NSImage *)(result.image() ? result.image()->getNSImage() : nil);
    ASSERT(image);
    [m_pasteboard setData:[image TIFFRepresentation] forType:NSTIFFPboardType];

    CachedResource* imageResource = WebCore::cache()->resourceForURL(result.absoluteImageURL().url());
    ASSERT(imageResource);
    String MIMEType = imageResource->response().mimeType();
    ASSERT(MimeTypeRegistry::isSupportedImageResourceMIMEType(MIMEType));

    if (imageResource)
        writeFileWrapperAsRTFDAttachment(fileWrapperForImage(imageResource, URL));
}

bool Pasteboard::canSmartReplace()
{
    return [[m_pasteboard types] containsObject:WebSmartPastePboardType];
}

String Pasteboard::plainText(Frame* frame)
{
    NSArray *types = [m_pasteboard types];
    
    if ([types containsObject:NSStringPboardType])
        return [m_pasteboard stringForType:NSStringPboardType];
    
    NSAttributedString *attributedString = nil;
    NSString *string;

    if ([types containsObject:NSRTFDPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTFD:[m_pasteboard dataForType:NSRTFDPboardType] documentAttributes:NULL];
    if (attributedString == nil && [types containsObject:NSRTFPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTF:[m_pasteboard dataForType:NSRTFPboardType] documentAttributes:NULL];
    if (attributedString != nil) {
        string = [[attributedString string] copy];
        [attributedString release];
        return [string autorelease];
    }
    
    if ([types containsObject:NSFilenamesPboardType]) {
        string = [[m_pasteboard propertyListForType:NSFilenamesPboardType] componentsJoinedByString:@"\n"];
        if (string != nil)
            return string;
    }
    
    
    NSURL *URL;
    
    if ((URL = [NSURL URLFromPasteboard:m_pasteboard])) {
        // FIXME: using the editorClient to call into webkit, for now, since 
        // calling [URL _web_userVisibleString] from WebCore involves migrating a sizable web of 
        // helper code that should either be done in a separate patch or figured out in another way.
        string = frame->editor()->client()->userVisibleString(URL);
        if ([string length] > 0)
            return string;
    }

    
    return String(); 
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    NSArray *types = [m_pasteboard types];
    chosePlainText = false;

    if ([types containsObject:NSHTMLPboardType]) {
        NSString *HTMLString = [m_pasteboard stringForType:NSHTMLPboardType];
        // This is a hack to make Microsoft's HTML pasteboard data work. See 3778785.
        if ([HTMLString hasPrefix:@"Version:"]) {
            NSRange range = [HTMLString rangeOfString:@"<html" options:NSCaseInsensitiveSearch];
            if (range.location != NSNotFound) {
                HTMLString = [HTMLString substringFromIndex:range.location];
            }
        }
        if ([HTMLString length] != 0) {
            RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), HTMLString, "");
            if (fragment)
                return fragment.release();
        }
    }
    
    if (allowPlainText && [types containsObject:NSStringPboardType]) {
        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), [m_pasteboard stringForType:NSStringPboardType]);
        if (fragment)
            return fragment.release();
    }
    
    return 0;
}

}
