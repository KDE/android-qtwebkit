/*
 * Copyright 2006 Apple Computer, Inc. All rights reserved.
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

#ifndef WebCoreSystemInterface_h
#define WebCoreSystemInterface_h

#include <ApplicationServices/ApplicationServices.h>
#include <objc/objc.h>

typedef struct _NSRange NSRange;

#ifdef NSGEOMETRY_TYPES_SAME_AS_CGGEOMETRY_TYPES
typedef struct CGPoint NSPoint;
typedef struct CGRect NSRect;
#else
typedef struct _NSPoint NSPoint;
typedef struct _NSRect NSRect;
#endif

#ifdef __OBJC__
@class NSFont;
@class NSMutableURLRequest;
@class NSURLRequest;
#else
typedef struct NSArray NSArray;
typedef struct NSDate NSDate;
typedef struct NSFont NSFont;
typedef struct NSImage NSImage;
typedef struct NSMenu NSMenu;
typedef struct NSMutableURLRequest NSMutableURLRequest;
typedef struct NSURLRequest NSURLRequest;
typedef struct NSString NSString;
typedef struct NSTextFieldCell NSTextFieldCell;
typedef struct NSURLConnection NSURLConnection;
typedef struct NSURLResponse NSURLResponse;
typedef struct NSView NSView;
typedef struct objc_object *id;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GLYPH_VECTOR_SIZE (50 * 32)

// In alphabetical order.

extern BOOL (*wkCGContextGetShouldSmoothFonts)(CGContextRef);
extern void (*wkClearGlyphVector)(void* glyphs);
extern CFReadStreamRef (*wkCreateCustomCFReadStream)(void *(*formCreate)(CFReadStreamRef, void *), 
    void (*formFinalize)(CFReadStreamRef, void *), 
    Boolean (*formOpen)(CFReadStreamRef, CFStreamError *, Boolean *, void *), 
    CFIndex (*formRead)(CFReadStreamRef, UInt8 *, CFIndex, CFStreamError *, Boolean *, void *), 
    Boolean (*formCanRead)(CFReadStreamRef, void *), 
    void (*formClose)(CFReadStreamRef, void *), 
    void (*formSchedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *), 
    void (*formUnschedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *),
    void *context);
extern OSStatus (*wkConvertCharToGlyphs)(void* styleGroup, const UniChar*, unsigned numCharacters, void* glyphs);
extern id (*wkCreateNSURLConnectionDelegateProxy)(void);
extern NSString* (*wkCreateURLPasteboardFlavorTypeName)(void);
extern NSString* (*wkCreateURLNPasteboardFlavorTypeName)(void);
extern void (*wkDrawBezeledTextFieldCell)(NSRect, BOOL enabled);
extern void (*wkDrawTextFieldCellFocusRing)(NSTextFieldCell*, NSRect);
extern void (*wkDrawBezeledTextArea)(NSRect, BOOL enabled);
extern void (*wkDrawFocusRing)(CGContextRef, CGRect clipRect, CGColorRef, int radius);
extern BOOL (*wkFontSmoothingModeIsLCD)(int mode);
extern OSStatus (*wkGetATSStyleGroup)(ATSUStyle, void** styleGroup);
extern CGFontRef (*wkGetCGFontFromNSFont)(NSFont*);
extern ATSGlyphRef (*wkGetDefaultGlyphForChar)(NSFont*, UniChar);
extern NSFont* (*wkGetFontInLanguageForRange)(NSFont*, NSString*, NSRange);
extern NSFont* (*wkGetFontInLanguageForCharacter)(NSFont*, UniChar);
extern void (*wkGetFontMetrics)(NSFont*, int* ascent, int* descent, int* lineGap, unsigned* unitsPerEm);
extern BOOL (*wkGetGlyphTransformedAdvances)(NSFont*, CGAffineTransform*, ATSGlyphRef*, CGSize* advance);
extern ATSLayoutRecord* (*wkGetGlyphVectorFirstRecord)(void* glyphVector);
extern int (*wkGetGlyphVectorNumGlyphs)(void* glyphVector);
extern size_t (*wkGetGlyphVectorRecordSize)(void* glyphVector);
extern NSString* (*wkGetPreferredExtensionForMIMEType)(NSString*);
extern NSArray* (*wkGetExtensionsForMIMEType)(NSString*);
extern NSString* (*wkGetMIMETypeForExtension)(NSString*);
extern ATSUFontID (*wkGetNSFontATSUFontId)(NSFont*);
extern double (*wkGetNSURLResponseCalculatedExpiration)(NSURLResponse *response);
extern NSDate *(*wkGetNSURLResponseLastModifiedDate)(NSURLResponse *response);
extern BOOL (*wkGetNSURLResponseMustRevalidate)(NSURLResponse *response);
extern OSStatus (*wkInitializeGlyphVector)(int count, void* glyphs);
extern NSString* (*wkPathFromFont)(NSFont*);
extern void (*wkPopupMenu)(NSMenu*, NSPoint location, float width, NSView*, int selectedItem, NSFont*);
extern void (*wkReleaseStyleGroup)(void* group);
extern void (*wkSetCGFontRenderingMode)(CGContextRef, NSFont*);
extern void (*wkSetDragImage)(NSImage*, NSPoint offset);
extern void (*wkSetNSURLConnectionDefersCallbacks)(NSURLConnection *, BOOL);
extern void (*wkSetPatternPhaseInUserSpace)(CGContextRef, CGPoint);
extern void (*wkSetUpFontCache)(size_t);
extern void (*wkSignalCFReadStreamEnd)(CFReadStreamRef stream);
extern void (*wkSignalCFReadStreamError)(CFReadStreamRef stream, CFStreamError *error);
extern void (*wkSignalCFReadStreamHasBytes)(CFReadStreamRef stream);
extern BOOL (*wkSupportsMultipartXMixedReplace)(NSMutableURLRequest *);
extern Class (*wkNSURLProtocolClassForReqest)(NSURLRequest *);
extern float (*wkSecondsSinceLastInputEvent)(void);
#ifdef __cplusplus
}
#endif

#endif
