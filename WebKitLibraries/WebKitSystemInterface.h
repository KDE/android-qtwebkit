/*      
    WebKitSystemInterface.h
    Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    Public header file.
*/

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

@class QTMovie;
@class QTMovieView;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WKCertificateParseResultSucceeded  = 0,
    WKCertificateParseResultFailed     = 1,
    WKCertificateParseResultPKCS7      = 2,
} WKCertificateParseResult;

CFStringRef WKCopyCFLocalizationPreferredName(CFStringRef localization);
CFStringRef WKSignedPublicKeyAndChallengeString(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
WKCertificateParseResult WKAddCertificatesToKeychainFromData(const void *bytes, unsigned length);

NSString *WKGetPreferredExtensionForMIMEType(NSString *type);
NSArray *WKGetExtensionsForMIMEType(NSString *type);
NSString *WKGetMIMETypeForExtension(NSString *extension);

NSDate *WKGetNSURLResponseLastModifiedDate(NSURLResponse *response);
NSTimeInterval WKGetNSURLResponseFreshnessLifetime(NSURLResponse *response);

CFStringEncoding WKGetWebDefaultCFStringEncoding(void);

void WKSetMetadataURL(NSString *URLString, NSString *referrer, NSString *path);
void WKSetNSURLConnectionDefersCallbacks(NSURLConnection *connection, BOOL defers);

void WKShowKeyAndMain(void);
#ifndef __LP64__
OSStatus WKSyncWindowWithCGAfterMove(WindowRef);
unsigned WKCarbonWindowMask(void);
void *WKGetNativeWindowFromWindowRef(WindowRef);
OSType WKCarbonWindowPropertyCreator(void);
OSType WKCarbonWindowPropertyTag(void);
#endif

typedef id WKNSURLConnectionDelegateProxyPtr;

WKNSURLConnectionDelegateProxyPtr WKCreateNSURLConnectionDelegateProxy(void);

void WKDisableCGDeferredUpdates(void);

Class WKNSURLProtocolClassForRequest(NSURLRequest *request);
void WKSetNSURLRequestShouldContentSniff(NSMutableURLRequest *request, BOOL shouldContentSniff);

unsigned WKGetNSAutoreleasePoolCount(void);

void WKAdvanceDefaultButtonPulseAnimation(NSButtonCell *button);

NSString *WKMouseMovedNotification(void);
NSString *WKWindowWillOrderOnScreenNotification(void);
void WKSetNSWindowShouldPostEventNotifications(NSWindow *window, BOOL post);

CFTypeID WKGetAXTextMarkerTypeID(void);
CFTypeID WKGetAXTextMarkerRangeTypeID(void);
CFTypeRef WKCreateAXTextMarker(const void *bytes, size_t len);
BOOL WKGetBytesFromAXTextMarker(CFTypeRef textMarker, void *bytes, size_t length);
CFTypeRef WKCreateAXTextMarkerRange(CFTypeRef start, CFTypeRef end);
CFTypeRef WKCopyAXTextMarkerRangeStart(CFTypeRef range);
CFTypeRef WKCopyAXTextMarkerRangeEnd(CFTypeRef range);
void WKAccessibilityHandleFocusChanged(void);
AXUIElementRef WKCreateAXUIElementRef(id element);
void WKUnregisterUniqueIdForElement(id element);

void WKSetUpFontCache(void);

void WKSignalCFReadStreamEnd(CFReadStreamRef stream);
void WKSignalCFReadStreamHasBytes(CFReadStreamRef stream);
void WKSignalCFReadStreamError(CFReadStreamRef stream, CFStreamError *error);

CFReadStreamRef WKCreateCustomCFReadStream(void *(*formCreate)(CFReadStreamRef, void *), 
    void (*formFinalize)(CFReadStreamRef, void *), 
    Boolean (*formOpen)(CFReadStreamRef, CFStreamError *, Boolean *, void *), 
    CFIndex (*formRead)(CFReadStreamRef, UInt8 *, CFIndex, CFStreamError *, Boolean *, void *), 
    Boolean (*formCanRead)(CFReadStreamRef, void *), 
    void (*formClose)(CFReadStreamRef, void *), 
    void (*formSchedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *), 
    void (*formUnschedule)(CFReadStreamRef, CFRunLoopRef, CFStringRef, void *),
    void *context);

void WKDrawCapsLockIndicator(CGContextRef, CGRect);

void WKDrawFocusRing(CGContextRef context, CGColorRef color, int radius);
    // The CG context's current path is the focus ring's path.
    // A color of 0 means "use system focus ring color".
    // A radius of 0 means "use default focus ring radius".

void WKSetDragImage(NSImage *image, NSPoint offset);

void WKDrawBezeledTextFieldCell(NSRect, BOOL enabled);
void WKDrawTextFieldCellFocusRing(NSTextFieldCell*, NSRect);
void WKDrawBezeledTextArea(NSRect, BOOL enabled);
void WKPopupMenu(NSMenu*, NSPoint location, float width, NSView*, int selectedItem, NSFont*);

void WKSendUserChangeNotifications(void);
#ifndef __LP64__
BOOL WKConvertNSEventToCarbonEvent(EventRecord *carbonEvent, NSEvent *cocoaEvent);
void WKSendKeyEventToTSM(NSEvent *theEvent);
void WKCallDrawingNotification(CGrafPtr port, Rect *bounds);
#endif

BOOL WKGetGlyphTransformedAdvances(CGFontRef, NSFont*, CGAffineTransform *m, ATSGlyphRef *glyph, CGSize *advance);
NSFont *WKGetFontInLanguageForRange(NSFont *font, NSString *string, NSRange range);
NSFont *WKGetFontInLanguageForCharacter(NSFont *font, UniChar ch);
void WKSetCGFontRenderingMode(CGContextRef cgContext, NSFont *font);
BOOL WKCGContextGetShouldSmoothFonts(CGContextRef cgContext);

#ifdef BUILDING_ON_TIGER
// CGFontGetAscent, CGFontGetDescent, CGFontGetLeading and CGFontGetUnitsPerEm were not available until Leopard
void WKGetFontMetrics(CGFontRef font, int *ascent, int *descent, int *lineGap, unsigned *unitsPerEm);
// CTFontCopyGraphicsFont was not available until Leopard
CGFontRef WKGetCGFontFromNSFont(NSFont *font);
// CTFontGetPlatformFont was not available until Leopard
ATSUFontID WKGetNSFontATSUFontId(NSFont *font);
// CGFontCopyFullName was not available until Leopard
CFStringRef WKCopyFullFontName(CGFontRef font);
#endif

void WKSetPatternBaseCTM(CGContextRef, CGAffineTransform);
void WKSetPatternPhaseInUserSpace(CGContextRef, CGPoint);

#ifndef BUILDING_ON_TIGER
void WKGetGlyphsForCharacters(CGFontRef, const UniChar[], CGGlyph[], size_t);
#else
typedef void *WKGlyphVectorRef;
OSStatus WKConvertCharToGlyphs(void *styleGroup, const UniChar *characters, unsigned numCharacters, WKGlyphVectorRef glyphs);
OSStatus WKGetATSStyleGroup(ATSUStyle fontStyle, void **styleGroup);
void WKReleaseStyleGroup(void *group);
OSStatus WKInitializeGlyphVector(int count, WKGlyphVectorRef glyphs);
void WKClearGlyphVector(WKGlyphVectorRef glyphs);

int WKGetGlyphVectorNumGlyphs(WKGlyphVectorRef glyphVector);
ATSLayoutRecord *WKGetGlyphVectorFirstRecord(WKGlyphVectorRef glyphVector);
size_t WKGetGlyphVectorRecordSize(WKGlyphVectorRef glyphVector);
#endif

#ifndef __LP64__
NSEvent *WKCreateNSEventWithCarbonEvent(EventRef eventRef);
NSEvent *WKCreateNSEventWithCarbonMouseMoveEvent(EventRef inEvent, NSWindow *window);
NSEvent *WKCreateNSEventWithCarbonClickEvent(EventRef inEvent, WindowRef windowRef);
#endif

CGContextRef WKNSWindowOverrideCGContext(NSWindow *, CGContextRef);
void WKNSWindowRestoreCGContext(NSWindow *, CGContextRef);

void WKNSWindowMakeBottomCornersSquare(NSWindow *);

#ifdef BUILDING_ON_TIGER
// WKSupportsMultipartXMixedReplace is not required on Leopard as multipart/x-mixed-replace is always handled by NSURLRequest
BOOL WKSupportsMultipartXMixedReplace(NSMutableURLRequest *request);
#endif

BOOL WKCGContextIsBitmapContext(CGContextRef context);

void WKGetWheelEventDeltas(NSEvent *, float *deltaX, float *deltaY, BOOL *continuous);

BOOL WKAppVersionCheckLessThan(NSString *, int, double);

int WKQTMovieDataRate(QTMovie* movie);
float WKQTMovieMaxTimeLoaded(QTMovie* movie);
void WKQTMovieViewSetDrawSynchronously(QTMovieView* view, BOOL sync);

CFStringRef WKCopyFoundationCacheDirectory(void);

void WKDrawMediaFullscreenButton(CGContextRef context, CGRect rect, BOOL active);
void WKDrawMediaMuteButton(CGContextRef context, CGRect rect, BOOL active);
void WKDrawMediaPauseButton(CGContextRef context, CGRect rect, BOOL active);
void WKDrawMediaPlayButton(CGContextRef context, CGRect rect, BOOL active);
void WKDrawMediaSeekBackButton(CGContextRef context, CGRect rect, BOOL active);
void WKDrawMediaSeekForwardButton(CGContextRef context, CGRect rect, BOOL active);
void WKDrawMediaSliderTrack(CGContextRef context, CGRect rect, float percentLoaded);
void WKDrawMediaSliderThumb(CGContextRef context, CGRect rect, BOOL active);
void WKDrawMediaUnMuteButton(CGContextRef context, CGRect rect, BOOL active);

#ifdef __cplusplus
}
#endif
