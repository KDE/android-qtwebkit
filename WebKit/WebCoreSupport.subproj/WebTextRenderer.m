/*	
    WebTextRenderer.m	    
    Copyright 2004, Apple, Inc. All rights reserved.
*/

#import "WebTextRenderer.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#import <AppKit/NSFont_Private.h>
#import <CoreGraphics/CoreGraphicsPrivate.h>
#import <QD/ATSUnicodePriv.h>

#import <WebCore/WebCoreUnicode.h>

#import <WebKit/WebGlyphBuffer.h>
#import <WebKit/WebGraphicsBridge.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebUnicode.h>

#import <float.h>

#import <unicode/uchar.h>

// FIXME: FATAL_ALWAYS seems like a bad idea; lets stop using it.

// SPI from other frameworks.

@interface NSLanguage : NSObject 
+ (NSLanguage *)defaultLanguage;
@end

@interface NSFont (WebPrivate)
- (ATSUFontID)_atsFontID;
- (CGFontRef)_backingCGSFont;
// Private method to find a font for a character.
+ (NSFont *) findFontLike:(NSFont *)aFont forCharacter:(UInt32)c inLanguage:(NSLanguage *) language;
+ (NSFont *) findFontLike:(NSFont *)aFont forString:(NSString *)string withRange:(NSRange)range inLanguage:(NSLanguage *) language;
- (NSGlyph)_defaultGlyphForChar:(unichar)uu;
- (BOOL)_isFakeFixedPitch;
@end

// Macros
#define SPACE 0x0020
#define NO_BREAK_SPACE 0x00A0
#define ZERO_WIDTH_SPACE 0x200B

#define ROUND_TO_INT(x) (int)((x)+.5)

// Lose precision beyond 1000ths place. This is to work around an apparent
// bug in CoreGraphics where there seem to be small errors to some metrics.
#define CEIL_TO_INT(x) ((int)(x + 0.999)) /* ((int)(x + 1.0 - FLT_EPSILON)) */

// MAX_GLYPH_EXPANSION is the maximum numbers of glyphs that may be
// use to represent a single Unicode code point.
#define MAX_GLYPH_EXPANSION 4
#define LOCAL_BUFFER_SIZE 2048

// Covers Latin-1.
#define INITIAL_BLOCK_SIZE 0x200

// Get additional blocks of glyphs and widths in bigger chunks.
// This will typically be for other character sets.
#define INCREMENTAL_BLOCK_SIZE 0x400

#define UNINITIALIZED_GLYPH_WIDTH 65535

#define ATSFontRefFromNSFont(font) (FMGetATSFontRefFromFont((FMFont)[font _atsFontID]))

#define SMALLCAPS_FONTSIZE_MULTIPLIER 0.7
#define INVALID_WIDTH -(__FLT_MAX__)

#if !defined(ScaleEmToUnits)
#define CONTEXT_DPI	(72.0)

#define ScaleEmToUnits(X, U_PER_EM)	(X * ((1.0 * CONTEXT_DPI) / (CONTEXT_DPI * U_PER_EM)))
#endif

// Datatypes
typedef float WebGlyphWidth;
typedef UInt32 UnicodeChar;

struct WidthEntry {
    WebGlyphWidth width;
};

struct WidthMap {
    ATSGlyphRef startRange;
    ATSGlyphRef endRange;
    WidthMap *next;
    WidthEntry *widths;
};

struct GlyphEntry
{
    ATSGlyphRef glyph;
    NSFont *font;
};

struct GlyphMap {
    UniChar startRange;
    UniChar endRange;
    GlyphMap *next;
    GlyphEntry *glyphs;
};

struct UnicodeGlyphMap {
    UnicodeChar startRange;
    UnicodeChar endRange;
    UnicodeGlyphMap *next;
    GlyphEntry *glyphs;
};

struct SubstituteFontWidthMap {
    NSFont *font;
    WidthMap *map;
};

struct CharacterWidthIterator
{
    WebTextRenderer *renderer;
    const WebCoreTextRun *run;
    const WebCoreTextStyle *style;
    unsigned currentCharacter;
    float runWidthSoFar;
    float widthToStart;
    int padding;
    int padPerSpace;
};

// Internal API
@interface WebTextRenderer (WebInternal)

- (NSFont *)_substituteFontForCharacters: (const unichar *)characters length: (int)numCharacters families: (NSString **)families;

- (WidthMap *)_extendGlyphToWidthMapToInclude:(ATSGlyphRef)glyphID font:(NSFont *)font;
- (ATSGlyphRef)_extendCharacterToGlyphMapToInclude:(UniChar) c;
- (ATSGlyphRef)_extendUnicodeCharacterToGlyphMapToInclude: (UnicodeChar)c;
- (void)_updateGlyphEntryForCharacter: (UniChar)c glyphID: (ATSGlyphRef)glyphID font: (NSFont *)substituteFont;

- (float)_floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style widths:(float *)widthBuffer fonts:(NSFont **)fontBuffer glyphs:(CGGlyph *)glyphBuffer startPosition:(float *)startPosition numGlyphs:(int *)_numGlyphs;

// Measuring runs.
- (float)_CG_floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style widths: (float *)widthBuffer fonts: (NSFont **)fontBuffer glyphs: (CGGlyph *)glyphBuffer startPosition:(float *)startPosition numGlyphs: (int *)_numGlyphs;
- (float)_ATSU_floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style;

// Drawing runs.
- (void)_CG_drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;
- (void)_ATSU_drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;

// Selection point detection in runs.
- (int)_CG_pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs;
- (int)_ATSU_pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs;

// Drawing highlight for runs.
- (void)_CG_drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;
- (void)_ATSU_drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry;

- (BOOL)_setupFont;

// Small caps
- (void)_setIsSmallCapsRenderer:(BOOL)flag;
- (BOOL)_isSmallCapsRenderer;
- (WebTextRenderer *)_smallCapsRenderer;
- (NSFont *)_smallCapsFont;

@end


// Character property functions.

static inline BOOL isSpace(UniChar c)
{
    return c == SPACE || c == '\n' || c == NO_BREAK_SPACE;
}

static const uint8_t isRoundingHackCharacterTable[0x100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*\n*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*-*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*?*/,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*no-break space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static inline BOOL isRoundingHackCharacter(UniChar c)
{
    return (c & ~0xFF) == 0 && isRoundingHackCharacterTable[c];
}

// Map utility functions
static void freeWidthMap(WidthMap *map);
static void freeGlyphMap(GlyphMap *map);
static void freeUnicodeGlyphMap(UnicodeGlyphMap *map);
static inline ATSGlyphRef glyphForUnicodeCharacter (UnicodeGlyphMap *map, UnicodeChar c, NSFont **font);
static inline SubstituteFontWidthMap *mapForSubstituteFont(WebTextRenderer *renderer, NSFont *font);
static inline ATSGlyphRef glyphForCharacter (GlyphMap *map, UniChar c, NSFont **font);
static inline SubstituteFontWidthMap *mapForSubstituteFont(WebTextRenderer *renderer, NSFont *font);
static inline WebGlyphWidth widthFromMap (WebTextRenderer *renderer, WidthMap *map, ATSGlyphRef glyph, NSFont *font);
static inline WebGlyphWidth widthForGlyph (WebTextRenderer *renderer, ATSGlyphRef glyph, NSFont *font);

#if BUILDING_ON_PANTHER

static WebGlyphWidth getUncachedWidth(WebTextRenderer *renderer, WidthMap *map, ATSGlyphRef glyph, NSFont *font)
{
    WebGlyphWidth width;

    if (font == NULL)
        font = renderer->font;

    if (!CGFontGetGlyphScaledAdvances ([font _backingCGSFont], &glyph, 1, &width, [font pointSize]))
        FATAL_ALWAYS ("Unable to cache glyph widths for %@ %f",  [font displayName], [font pointSize]);

    return width;
}

#else

static inline CGFontRenderingMode _AppkitGetCGRenderingMode(NSFont *font) {
    switch ([font renderingMode]) {
        case NSFontIntegerAdvancementsRenderingMode: return kCGFontRenderingMode1BitPixelAligned;
        case NSFontAntialiasedIntegerAdvancementsRenderingMode: return kCGFontRenderingModeAntialiasedPixelAligned;
        default: return kCGFontRenderingModeAntialiased;
    }
}

static WebGlyphWidth getUncachedWidth(WebTextRenderer *renderer, WidthMap *map, ATSGlyphRef glyph, NSFont *font)
{
    float pointSize;
    CGAffineTransform m;
    CGSize advance;

    if (font == NULL)
        font = renderer->font;

    pointSize = [font pointSize];
    m = CGAffineTransformMakeScale(pointSize, pointSize);
    if (!CGFontGetGlyphTransformedAdvances([font _backingCGSFont], &m, _AppkitGetCGRenderingMode(font), &glyph, 1, &advance))
        FATAL_ALWAYS ("Unable to cache glyph widths for %@ %f", [font displayName], pointSize);

    return advance.width;
}

#endif

static inline WebGlyphWidth widthFromMap (WebTextRenderer *renderer, WidthMap *map, ATSGlyphRef glyph, NSFont *font)
{
    WebGlyphWidth width = UNINITIALIZED_GLYPH_WIDTH;
    
    while (1){
        if (map == 0)
            map = [renderer _extendGlyphToWidthMapToInclude: glyph font:font];

        if (glyph >= map->startRange && glyph <= map->endRange){
            width = map->widths[glyph - map->startRange].width;
            if (width == UNINITIALIZED_GLYPH_WIDTH){
                width = getUncachedWidth (renderer, map, glyph, font);
                map->widths[glyph - map->startRange].width = width;
            }
        }
        else {
            map = map->next;
            continue;
        }
        
        return width;
    }
}    

static inline WebGlyphWidth widthForGlyph (WebTextRenderer *renderer, ATSGlyphRef glyph, NSFont *font)
{
    WidthMap *map;

    if (font && font != renderer->font)
        map = mapForSubstituteFont(renderer, font)->map;
    else
        map = renderer->glyphToWidthMap;

    return widthFromMap (renderer, map, glyph, font);
}

// Iterator functions
static void initializeCharacterWidthIterator (CharacterWidthIterator *iterator, WebTextRenderer *renderer, const WebCoreTextRun *run , const WebCoreTextStyle *style);
static float widthForNextCharacter (CharacterWidthIterator *iterator, ATSGlyphRef *glyphUsed, NSFont **fontUsed);


// Misc.
static BOOL fillStyleWithAttributes(ATSUStyle style, NSFont *theFont);
static BOOL shouldUseATSU(const WebCoreTextRun *run);
static NSString *pathFromFont(NSFont *font);


// Globals
static CFCharacterSetRef nonBaseChars = NULL;
static BOOL bufferTextDrawing = NO;
static BOOL alwaysUseATSU = NO;


@implementation WebTextRenderer

+ (NSString *)webFallbackFontFamily
{
    static NSString *webFallbackFontFamily = nil;
    if (!webFallbackFontFamily)
	webFallbackFontFamily = [[[NSFont systemFontOfSize:16.0] familyName] retain];
    return webFallbackFontFamily;
}

+ (BOOL)shouldBufferTextDrawing
{
    return bufferTextDrawing;
}

+ (void)initialize
{
    nonBaseChars = CFCharacterSetGetPredefined(kCFCharacterSetNonBase);
    bufferTextDrawing = [[[NSUserDefaults standardUserDefaults] stringForKey:@"BufferTextDrawing"] isEqual: @"YES"];
}

- initWithFont:(NSFont *)f usingPrinterFont:(BOOL)p
{
    [super init];
    
    // Quartz can only handle fonts with these glyph packings.  Other packings have
    // been deprecated.
    if ([f glyphPacking] != NSNativeShortGlyphPacking &&
        [f glyphPacking] != NSTwoByteGlyphPacking) {
        // Apparantly there are many deprecated fonts out there with unsupported packing types.
        // Log and use fallback font.
        // This change fixes the many crashes reported in 3782533.  Most likely, the
        // problem is encountered when people upgrade from OS 9, or have OS 9
        // fonts installed on OS X.
        NSLog (@"%s:%d  Unable to use deprecated font %@ %f, using system font instead", __FILE__, __LINE__, [f displayName], [f pointSize]);
        f = [NSFont systemFontOfSize:[f pointSize]];
    }
        
    maxSubstituteFontWidthMaps = NUM_SUBSTITUTE_FONT_MAPS;
    substituteFontWidthMaps = calloc (1, maxSubstituteFontWidthMaps * sizeof(SubstituteFontWidthMap));
    font = [(p ? [f printerFont] : [f screenFont]) retain];
    usingPrinterFont = p;
    
    if (![self _setupFont]){
        // Ack!  Something very bad happened, like a corrupt font.  Try
        // looking for an alternate 'base' font for this renderer.

        // Special case hack to use "Times New Roman" in place of "Times".  "Times RO" is a common font
        // whose family name is "Times".  It overrides the normal "Times" family font.  It also
        // appears to have a corrupt regular variant.
        NSString *fallbackFontFamily;

        if ([[font familyName] isEqual:@"Times"])
            fallbackFontFamily = @"Times New Roman";
        else {
            fallbackFontFamily = [WebTextRenderer webFallbackFontFamily];
        }
        
        // Try setting up the alternate font.  This is a last ditch effort to use a
	// substitute font when something has gone wrong.
        NSFont *initialFont = font;
        [initialFont autorelease];
        NSFont *af = [[NSFontManager sharedFontManager] convertFont:font toFamily:fallbackFontFamily];
        font = [(p ? [af printerFont] : [af screenFont]) retain];
        NSString *filePath = pathFromFont(initialFont);
        filePath = filePath ? filePath : @"not known";
        if (![self _setupFont]){
	    if ([fallbackFontFamily isEqual:@"Times New Roman"]) {
		// OK, couldn't setup Times New Roman as an alternate to Times, fallback
		// on the system font.  If this fails we have no alternative left.
		af = [[NSFontManager sharedFontManager] convertFont:font toFamily:[WebTextRenderer webFallbackFontFamily]];
		font = [(p ? [af printerFont] : [af screenFont]) retain];
		if (![self _setupFont]){
		    // We tried, Times, Times New Roman, and the system font.  No joy.  We have to give up.
		    FATAL_ALWAYS ("%@ unable to initialize with font %@ at %@", self, initialFont, filePath);
		}
	    }
	    else {
		// We tried the requested font and the syste, font.  No joy.  We have to give up.
		FATAL_ALWAYS ("%@ unable to initialize with font %@ at %@", self, initialFont, filePath);
	    }
        }

        // Report the problem.
        ERROR ("Corrupt font detected, using %@ in place of %@ located at \"%@\".", 
                    [font familyName], 
                    [initialFont familyName],
                    filePath);
    }

    // We emulate the appkit metrics by applying rounding as is done
    // in the appkit.
    CGFontRef cgFont = [font _backingCGSFont];
    const CGFontHMetrics *metrics = CGFontGetHMetrics(cgFont);
    unsigned unitsPerEm = CGFontGetUnitsPerEm(cgFont);
    float pointSize = [font pointSize];
    float asc = (ScaleEmToUnits(metrics->ascent, unitsPerEm)*pointSize);
    float dsc = (-ScaleEmToUnits(metrics->descent, unitsPerEm)*pointSize);
    float _lineGap = ScaleEmToUnits(metrics->lineGap, unitsPerEm)*pointSize;
    float adjustment;

    // We need to adjust Times, Helvetica, and Courier to closely match the
    // vertical metrics of their Microsoft counterparts that are the de facto
    // web standard.  The AppKit adjustment of 20% is too big and is
    // incorrectly added to line spacing, so we use a 15% adjustment instead
    // and add it to the ascent.
    if ([[font familyName] isEqualToString:@"Times"] ||
        [[font familyName] isEqualToString:@"Helvetica"] ||
        [[font familyName] isEqualToString:@"Courier"]) {
        adjustment = floor(((asc + dsc) * 0.15) + 0.5);
    } else {
        adjustment = 0.0;
    }

    ascent = ROUND_TO_INT(asc + adjustment);
    descent = ROUND_TO_INT(dsc);

    _lineGap = (_lineGap > 0.0 ? floor(_lineGap + 0.5) : 0.0);
    lineGap = (int)_lineGap;
    lineSpacing =  ascent + descent + lineGap;

#ifdef COMPARE_APPKIT_CG_METRICS
    printf ("\nCG/Appkit metrics for font %s, %f, lineGap %f, adjustment %f\n", [[font displayName] cString], [font pointSize], lineGap, adjustment);
    if (ROUND_TO_INT([font ascender]) != ascent ||
        ROUND_TO_INT(-[font descender]) != descent ||
        ROUND_TO_INT([font defaultLineHeightForFont]) != lineSpacing){
        printf ("\nCG/Appkit mismatched metrics for font %s, %f (%s)\n", [[font displayName] cString], [font pointSize],
                ([font screenFont] ? [[[font screenFont] displayName] cString] : "none"));
        printf ("ascent(%s), descent(%s), lineSpacing(%s)\n",
                (ROUND_TO_INT([font ascender]) != ascent) ? "different" : "same",
                (ROUND_TO_INT(-[font descender]) != descent) ? "different" : "same",
                (ROUND_TO_INT([font defaultLineHeightForFont]) != lineSpacing) ? "different" : "same");
        printf ("CG:  ascent %f, ", asc);
        printf ("descent %f, ", dsc);
        printf ("lineGap %f, ", lineGap);
        printf ("lineSpacing %d\n", lineSpacing);
        
        printf ("NSFont:  ascent %f, ", [font ascender]);
        printf ("descent %f, ", [font descender]);
        printf ("lineSpacing %f\n", [font defaultLineHeightForFont]);
    }
#endif
     
    isSmallCapsRenderer = NO;
    
    return self;
}

- (void)dealloc
{
    [font release];
    [smallCapsFont release];
    [smallCapsRenderer release];

    if (styleGroup)
        ATSUDisposeStyleGroup(styleGroup);

    freeWidthMap(glyphToWidthMap);
    freeGlyphMap(characterToGlyphMap);
    freeUnicodeGlyphMap(unicodeCharacterToGlyphMap);

    if (ATSUStyleInitialized)
        ATSUDisposeStyle(_ATSUSstyle);
    
    [super dealloc];
}

- (void)finalize
{
    if (styleGroup)
        ATSUDisposeStyleGroup(styleGroup);

    freeWidthMap(glyphToWidthMap);
    freeGlyphMap(characterToGlyphMap);
    freeUnicodeGlyphMap(unicodeCharacterToGlyphMap);

    if (ATSUStyleInitialized)
        ATSUDisposeStyle(_ATSUSstyle);
    
    [super finalize];
}

- (int)ascent
{
    // This simple return obviously can't throw an exception.
    return ascent;
}

- (int)descent
{
    // This simple return obviously can't throw an exception.
    return descent;
}

- (int)lineSpacing
{
    // This simple return obviously can't throw an exception.
    return lineSpacing;
}

- (float)xHeight
{
    // Measure the actual character "x", because AppKit synthesizes X height rather
    // than getting it from the font. Unfortunately, NSFont will round this for us
    // so we don't quite get the right value.
    NSGlyph xGlyph = [font glyphWithName:@"x"];
    if (xGlyph) {
        NSRect xBox = [font boundingRectForGlyph:xGlyph];
        return NSMaxY(xBox);
    }

    return [font xHeight];
}

- (void)drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    if (style->smallCaps && !isSmallCapsRenderer) {
        [[self _smallCapsRenderer] drawRun:run style:style geometry:geometry];
    }
    else {
        if (shouldUseATSU(run))
            [self _ATSU_drawRun:run style:style geometry:geometry];
        else
            [self _CG_drawRun:run style:style geometry:geometry];
    }
}

- (float)floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style widths:(float *)widthBuffer
{
    if (style->smallCaps && !isSmallCapsRenderer) {
        return [[self _smallCapsRenderer] _floatWidthForRun:run style:style widths:widthBuffer fonts:nil glyphs:nil startPosition:nil numGlyphs:nil];
    }
    return [self _floatWidthForRun:run style:style widths:widthBuffer fonts:nil glyphs:nil startPosition:nil numGlyphs:nil];
}

- (void)drawLineForCharacters:(NSPoint)point yOffset:(float)yOffset width: (int)width color:(NSColor *)color thickness:(float)thickness
{
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];
    CGContextRef cgContext;

    // This will draw the text from the top of the bounding box down.
    // Qt expects to draw from the baseline.
    // Remember that descender is negative.
    point.y -= [self lineSpacing] - [self descent];
    
    BOOL flag = [graphicsContext shouldAntialias];

    [graphicsContext setShouldAntialias: NO];

    // We don't want antialiased lines on screen, but we do when printing (else they are too thick)
    if ([graphicsContext isDrawingToScreen]) {
        [graphicsContext setShouldAntialias:NO];
    }
    
    [color set];

    cgContext = (CGContextRef)[graphicsContext graphicsPort];

    // hack to make thickness 2 underlines for internation text input look right
    if (thickness > 1.5 && thickness < 2.5) {
        yOffset += .5;
    }

    if (thickness == 0.0) {
        CGSize size = CGSizeApplyAffineTransform(CGSizeMake(1.0, 1.0), CGAffineTransformInvert(CGContextGetCTM(cgContext)));
        CGContextSetLineWidth(cgContext, size.width);
    } else {
        CGContextSetLineWidth(cgContext, thickness);
    }


#if BUILDING_ON_PANTHER            
    CGContextMoveToPoint(cgContext, point.x, point.y + [self lineSpacing] + 1.5 - [self descent] + yOffset);
    // Subtract 1 to ensure that the line is always within bounds of element.
    CGContextAddLineToPoint(cgContext, point.x + width - 1.0, point.y + [self lineSpacing] + 1.5 - [self descent] + yOffset);
    CGContextStrokePath(cgContext);
#else
    // Use CGContextStrokeLineSegments on Tiger.  J. Burkey says this will be a big performance win.

    CGPoint linePoints[2];
    linePoints[0].x = point.x;
    linePoints[0].y = point.y + [self lineSpacing] + 1.5 - [self descent] + yOffset;
    linePoints[1].x = point.x + width - 1.0;
    linePoints[1].y = linePoints[0].y;
    CGContextStrokeLineSegments (cgContext, linePoints, 2);
#endif

    [graphicsContext setShouldAntialias: flag];
}


- (void)drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    if (style->smallCaps && !isSmallCapsRenderer) {
        [[self _smallCapsRenderer] drawHighlightForRun:run style:style geometry:geometry];
    }
    else {
        if (shouldUseATSU(run))
            [self _ATSU_drawHighlightForRun:run style:style geometry:geometry];
        else
            [self _CG_drawHighlightForRun:run style:style geometry:geometry];
    }
}

// Constants for pattern underline
#define patternWidth 4
#define patternHeight 3

- (void)drawLineForMisspelling:(NSPoint)point withWidth:(int)width
{
    // Constants for pattern color
    static NSColor *spellingPatternColor = nil;
    static bool usingDot = false;
 
    // Initialize pattern color if needed
    if (!spellingPatternColor) {
        NSImage *image = [NSImage imageNamed:@"SpellingDot"];
        ASSERT(image); // if image is not available, we want to know
        NSColor *color = (image ? [NSColor colorWithPatternImage:image] : nil);
        if (color)
            usingDot = true;
        else
            color = [NSColor redColor];
        spellingPatternColor = [color retain];
    }

    // Width must be divisible by 4 to make sure we always draw full misspelling dots under words.
    // Do a small adjustment to shift the underline back to the left if the pattern was
    // expanded to the right "too much" to accomodate the drawing of a full dot.
    if (usingDot) {
        int w = (width + patternWidth) - (width % patternWidth);
        if (w - width > 2) 
            point.x -= 1;
        width = w;
    }

    // Compute the appropriate phase relative to the top level view in the window.
    NSPoint originInWindow = [[NSView focusView] convertPoint:point toView:nil];
    // WebCore may translate the focus, and thus need an extra phase correction
    NSPoint extraPhase = [[WebGraphicsBridge sharedBridge] additionalPatternPhase];
    originInWindow.x += extraPhase.x;
    originInWindow.y += extraPhase.y;
    CGSize phase = CGSizeMake(fmodf(originInWindow.x, patternWidth), fmodf(originInWindow.y, patternHeight));

    // Draw underline
    NSGraphicsContext *currentContext = [NSGraphicsContext currentContext];
    [currentContext saveGraphicsState];
    [spellingPatternColor set];
    CGContextSetPatternPhase((CGContextRef)[currentContext graphicsPort], phase);
    NSRectFillUsingOperation(NSMakeRect(point.x, point.y, width, patternHeight), NSCompositeSourceOver);
    [currentContext restoreGraphicsState];
}

#undef patternWidth
#undef patternHeight

- (int)pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs
{
    if (style->smallCaps && !isSmallCapsRenderer) {
        return [[self _smallCapsRenderer] pointToOffset:run style:style position:x reversed:reversed includePartialGlyphs:includePartialGlyphs];
    }

    if (shouldUseATSU(run))
        return [self _ATSU_pointToOffset:run style:style position:x reversed:reversed includePartialGlyphs:includePartialGlyphs];
    return [self _CG_pointToOffset:run style:style position:x reversed:reversed includePartialGlyphs:includePartialGlyphs];
}

@end


// ------------------- Private API -------------------


@implementation WebTextRenderer (WebInternal)

+ (void)_setAlwaysUseATSU:(BOOL)f
{
    alwaysUseATSU = f;
}

- (void)_setIsSmallCapsRenderer:(BOOL)flag
{
    isSmallCapsRenderer = flag;
}

- (BOOL)_isSmallCapsRenderer
{
    return isSmallCapsRenderer;
}

- (WebTextRenderer *)_smallCapsRenderer
{
    if (!smallCapsRenderer) {
	NS_DURING
	    smallCapsRenderer = [[WebTextRenderer alloc] initWithFont:font usingPrinterFont:usingPrinterFont];
	NS_HANDLER
	    if (ASSERT_DISABLED) {
		NSLog(@"Uncaught exception - %@\n", localException);
	    } else {
		ASSERT_WITH_MESSAGE(0, "Uncaught exception - %@", localException);
	    } 
	NS_ENDHANDLER

        [smallCapsRenderer _setIsSmallCapsRenderer:YES];
    }
    return smallCapsRenderer;
}

- (NSFont *)_smallCapsFont
{
    if (!smallCapsFont)
        smallCapsFont = [[NSFontManager sharedFontManager] convertFont:font toSize:([font pointSize] * SMALLCAPS_FONTSIZE_MULTIPLIER)];
    return smallCapsFont;
}

static inline BOOL fontContainsString(NSFont *font, NSString *string)
{
    NSCharacterSet *set = [[font coveredCharacterSet] invertedSet];
    return set && [string rangeOfCharacterFromSet:set].location == NSNotFound;
}

- (NSFont *)_substituteFontForString: (NSString *)string families: (NSString **)families
{
    NSFont *substituteFont = nil;

    // First search the CSS family fallback list.  Start at 1 (2nd font)
    // because we've already failed on the first lookup.
    NSString *family = nil;
    int i = 1;
    while (families && families[i] != 0) {
        family = families[i++];
        substituteFont = [[WebTextRendererFactory sharedFactory] cachedFontFromFamily: family traits:[[NSFontManager sharedFontManager] traitsOfFont:font] size:[font pointSize]];
        if (substituteFont) {
            if (fontContainsString(substituteFont, string))
                break;
            substituteFont = nil; 
        }
    }
    
    // Now do string based lookup.
    if (substituteFont == nil)
        substituteFont = [NSFont findFontLike:font forString:string withRange:NSMakeRange (0,[string length]) inLanguage:[NSLanguage defaultLanguage]];

    // Now do character based lookup.
    if (substituteFont == nil && [string length] == 1)
        substituteFont = [NSFont findFontLike:font forCharacter: [string characterAtIndex: 0] inLanguage:[NSLanguage defaultLanguage]];

    // Get the screen or printer variation of the font.
    substituteFont = usingPrinterFont ? [substituteFont printerFont] : [substituteFont screenFont];

    if ([substituteFont isEqual: font])
        substituteFont = nil;

    return substituteFont;
}

- (NSFont *)_substituteFontForCharacters: (const unichar *)characters length: (int)numCharacters families: (NSString **)families
{
    NSString *string = [[NSString alloc] initWithCharactersNoCopy:(unichar *)characters length: numCharacters freeWhenDone: NO];
    NSFont *substituteFont = [self _substituteFontForString: string families: families];
    [string release];
    return substituteFont;
}

- (void)_convertCharacters: (const UniChar *)characters length: (unsigned)numCharacters toGlyphs: (ATSGlyphVector *)glyphs
{
    OSStatus status = ATSUConvertCharToGlyphs(styleGroup, characters, 0, numCharacters, 0, glyphs);
    if (status != noErr){
        FATAL_ALWAYS ("unable to get glyphsfor %@ %f error = (%d)", self, [font displayName], [font pointSize], status);
    }

#ifdef DEBUG_GLYPHS
    int foundGlyphs = 0;
    ATSLayoutRecord *glyphRecord;
    for (i = 0; i < numCharacters; i++) {
        glyphRecord = (ATSLayoutRecord *)glyphs->firstRecord;
        for (i = 0; i < numCharacters; i++) {
            if (glyphRecord->glyphID != 0)
                foundGlyphs++;
            glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + glyphs->recordSize);
        }
    }
    printf ("For %s found %d glyphs in range 0x%04x to 0x%04x\n", [[font displayName] cString], foundGlyphs, characters[0], characters[numCharacters-1]);
#endif
}

- (void)_convertUnicodeCharacters: (const UnicodeChar *)characters length: (unsigned)numCharacters toGlyphs: (ATSGlyphVector *)glyphs
{
    UniChar localBuffer[LOCAL_BUFFER_SIZE];
    UniChar *buffer = localBuffer;
    unsigned i, bufPos = 0;
    
    if (numCharacters*2 > LOCAL_BUFFER_SIZE) {
        buffer = (UniChar *)malloc(sizeof(UniChar) * numCharacters * 2);
    }
    
    for (i = 0; i < numCharacters; i++) {
        UnicodeChar c = characters[i];
        UniChar h = HighSurrogatePair(c);
        UniChar l = LowSurrogatePair(c);
        buffer[bufPos++] = h;
        buffer[bufPos++] = l;
    }
        
    OSStatus status = ATSUConvertCharToGlyphs(styleGroup, buffer, 0, numCharacters*2, 0, glyphs);
    if (status != noErr){
        FATAL_ALWAYS ("unable to get glyphsfor %@ %f error = (%d)", self, [font displayName], [font pointSize], status);
    }
    
    if (buffer != localBuffer) {
        free(buffer);
    }
}

// Nasty hack to determine if we should round or ceil space widths.
// If the font is monospace or fake monospace we ceil to ensure that 
// every character and the space are the same width.  Otherwise we round.
- (BOOL)_computeWidthForSpace
{
    spaceGlyph = [self _extendCharacterToGlyphMapToInclude:SPACE];
    if (spaceGlyph == 0) {
        return NO;
    }

    float width = widthForGlyph(self, spaceGlyph, 0);
    spaceWidth = width;

    treatAsFixedPitch = [font isFixedPitch] || [font _isFakeFixedPitch];
    adjustedSpaceWidth = treatAsFixedPitch ? CEIL_TO_INT(width) : ROUND_TO_INT(width);
    
    return YES;
}

- (BOOL)_setupFont
{
    ATSUStyle fontStyle;
    if (ATSUCreateStyle(&fontStyle) != noErr)
        return NO;

    if (!fillStyleWithAttributes(fontStyle, font)) {
        ATSUDisposeStyle(fontStyle);
        return NO;
    }

    if (ATSUGetStyleGroup(fontStyle, &styleGroup) != noErr) {
        ATSUDisposeStyle(fontStyle);
        return NO;
    }
    
    ATSUDisposeStyle(fontStyle);

    if (![self _computeWidthForSpace]) {
        freeGlyphMap(characterToGlyphMap);
        characterToGlyphMap = NULL;
        ATSUDisposeStyleGroup(styleGroup);
        styleGroup = NULL;
        return NO;
    }
    
    return YES;
}

static NSString *pathFromFont (NSFont *font)
{
    UInt8 _filePathBuffer[PATH_MAX];
    NSString *filePath = nil;
    FSSpec oFile;
    OSStatus status = ATSFontGetFileSpecification(
            ATSFontRefFromNSFont(font),
            &oFile);
    if (status == noErr){
        OSErr err;
        FSRef fileRef;
        err = FSpMakeFSRef(&oFile,&fileRef);
        if (err == noErr){
            status = FSRefMakePath(&fileRef,_filePathBuffer, PATH_MAX);
            if (status == noErr){
                filePath = [NSString stringWithUTF8String:&_filePathBuffer[0]];
            }
        }
    }
    return filePath;
}

// Useful page for testing http://home.att.net/~jameskass
static void _drawGlyphs(NSFont *font, NSColor *color, CGGlyph *glyphs, CGSize *advances, float x, float y, int numGlyphs)
{
    CGContextRef cgContext;

    if ([WebTextRenderer shouldBufferTextDrawing] && [[WebTextRendererFactory sharedFactory] coalesceTextDrawing]){
        // Add buffered glyphs and advances
        // FIXME:  If we ever use this again, need to add RTL.
        WebGlyphBuffer *gBuffer = [[WebTextRendererFactory sharedFactory] glyphBufferForFont: font andColor: color];
        [gBuffer addGlyphs: glyphs advances: advances count: numGlyphs at: x : y];
    }
    else {
        NSGraphicsContext *gContext = [NSGraphicsContext currentContext];
        cgContext = (CGContextRef)[gContext graphicsPort];
        // Setup the color and font.
        
#if BUILDING_ON_PANTHER        
        if ([gContext isDrawingToScreen]){
            NSFont *screenFont = [font screenFont];
            if (screenFont != font){
                // We are getting this in too many places (3406411); use ERROR so it only prints on
                // debug versions for now. (We should debug this also, eventually).
                ERROR ("Attempting to set non-screen font (%@) when drawing to screen.  Using screen font anyway, may result in incorrect metrics.", [[[font fontDescriptor] fontAttributes] objectForKey: NSFontNameAttribute]);
            }
            [screenFont set];
        }
        else {
            NSFont *printerFont = [font printerFont];
            if (printerFont != font){
                NSLog (@"Attempting to set non-printer font (%@) when printing.  Using printer font anyway, may result in incorrect metrics.", [[[font fontDescriptor] fontAttributes] objectForKey: NSFontNameAttribute]);
            }
            [printerFont set];
        }
#else
        NSFont *drawFont;
        
        if ([gContext isDrawingToScreen]){
            drawFont = [font screenFont];
            if (drawFont != font){
                // We are getting this in too many places (3406411); use ERROR so it only prints on
                // debug versions for now. (We should debug this also, eventually).
                ERROR ("Attempting to set non-screen font (%@) when drawing to screen.  Using screen font anyway, may result in incorrect metrics.", [[[font fontDescriptor] fontAttributes] objectForKey: NSFontNameAttribute]);
            }
        }
        else {
            drawFont = [font printerFont];
            if (drawFont != font){
                NSLog (@"Attempting to set non-printer font (%@) when printing.  Using printer font anyway, may result in incorrect metrics.", [[[font fontDescriptor] fontAttributes] objectForKey: NSFontNameAttribute]);
            }
        }
        
        CGContextSetFont (cgContext, [drawFont _backingCGSFont]);
        
        // Deal will flipping flippyness.
        const float *matrix = [drawFont matrix];
        float flip = [[NSView focusView] isFlipped] ? -1 : 1;
        CGContextSetTextMatrix(cgContext, CGAffineTransformMake(matrix[0], matrix[1] * flip, matrix[2], matrix[3] * flip, matrix[4], matrix[5]));
        CGContextSetFontRenderingMode (cgContext, _AppkitGetCGRenderingMode(drawFont));
        CGContextSetFontSize(cgContext, 1.0);
#endif

        [color set];

        CGContextSetTextPosition (cgContext, x, y);
        CGContextShowGlyphsWithAdvances (cgContext, glyphs, advances, numGlyphs);
    }
}


- (void)_CG_drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    if (run->length == 0)
        return;

    CharacterWidthIterator widthIterator;
    WebCoreTextRun completeRun = *run;
    completeRun.from = 0;
    completeRun.to = run->length;
    initializeCharacterWidthIterator(&widthIterator, self, &completeRun, style);
    
    float startPosition = 0;

    // The starting point needs to be adjusted to account for the width of
    // the glyphs at the start of the run.
    while (widthIterator.currentCharacter < (unsigned)run->from) {
        startPosition += widthForNextCharacter(&widthIterator, 0, 0);
    }
    float startX = startPosition + geometry->point.x;
    
    float backgroundWidth = 0.0;
    while (widthIterator.currentCharacter < (unsigned)run->to) {
        backgroundWidth += widthForNextCharacter(&widthIterator, 0, 0);
    }

    if (style->backgroundColor != nil){
        // Calculate the width of the selection background by adding
        // up the advances of all the glyphs in the selection.
        
        [style->backgroundColor set];

        float yPos = geometry->useFontMetricsForSelectionYAndHeight ? geometry->point.y - [self ascent] - (lineGap/2) : geometry->selectionY;
        float height = geometry->useFontMetricsForSelectionYAndHeight ? [self lineSpacing] : geometry->selectionHeight;
        if (style->rtl){
            float completeRunWidth = startPosition + backgroundWidth;
            while (widthIterator.currentCharacter < run->length) {
                completeRunWidth += widthForNextCharacter(&widthIterator, 0, 0);
            }

            [NSBezierPath fillRect:NSMakeRect(geometry->point.x + completeRunWidth - startPosition - backgroundWidth, yPos, backgroundWidth, height)];
        }
        else {
            [NSBezierPath fillRect:NSMakeRect(startX, yPos, backgroundWidth, height)];
        }
    }
}


- (void)_CG_drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    float *widthBuffer, localWidthBuffer[LOCAL_BUFFER_SIZE];
    CGGlyph *glyphBuffer, localGlyphBuffer[LOCAL_BUFFER_SIZE];
    NSFont **fontBuffer, *localFontBuffer[LOCAL_BUFFER_SIZE];
    CGSize *advances, localAdvanceBuffer[LOCAL_BUFFER_SIZE];
    int numGlyphs = 0, i;
    float startX;
    unsigned length = run->length;
    
    if (run->length == 0)
        return;

    if (length*MAX_GLYPH_EXPANSION > LOCAL_BUFFER_SIZE) {
        advances = (CGSize *)calloc(length*MAX_GLYPH_EXPANSION, sizeof(CGSize));
        widthBuffer = (float *)calloc(length*MAX_GLYPH_EXPANSION, sizeof(float));
        glyphBuffer = (CGGlyph *)calloc(length*MAX_GLYPH_EXPANSION, sizeof(ATSGlyphRef));
        fontBuffer = (NSFont **)calloc(length*MAX_GLYPH_EXPANSION, sizeof(NSFont *));
    } else {
        advances = localAdvanceBuffer;
        widthBuffer = localWidthBuffer;
        glyphBuffer = localGlyphBuffer;
        fontBuffer = localFontBuffer;
    }

    [self _floatWidthForRun:run
        style:style
        widths:widthBuffer 
        fonts:fontBuffer
        glyphs:glyphBuffer
        startPosition:&startX
        numGlyphs: &numGlyphs];
        
    // Eek.  We couldn't generate ANY glyphs for the run.
    if (numGlyphs <= 0)
        return;
        
    // Fill the advances array.
    for (i = 0; i < numGlyphs; i++){
        advances[i].width = widthBuffer[i];
        advances[i].height = 0;
    }

    // Calculate the starting point of the glyphs to be displayed by adding
    // all the advances up to the first glyph.
    startX += geometry->point.x;

    if (style->backgroundColor != nil)
        [self _CG_drawHighlightForRun:run style:style geometry:geometry];
    
    // Finally, draw the glyphs.
    int lastFrom = 0;
    int pos = 0;

    // Swap the order of the glyphs if right-to-left.
    if (style->rtl && numGlyphs > 1){
        int i;
        int end = numGlyphs;
        CGGlyph gswap1, gswap2;
        CGSize aswap1, aswap2;
        NSFont *fswap1, *fswap2;
        
        for (i = pos, end = numGlyphs-1; i < (numGlyphs - pos)/2; i++){
            gswap1 = glyphBuffer[i];
            gswap2 = glyphBuffer[--end];
            glyphBuffer[i] = gswap2;
            glyphBuffer[end] = gswap1;
        }
        for (i = pos, end = numGlyphs - 1; i < (numGlyphs - pos)/2; i++){
            aswap1 = advances[i];
            aswap2 = advances[--end];
            advances[i] = aswap2;
            advances[end] = aswap1;
        }
        for (i = pos, end = numGlyphs - 1; i < (numGlyphs - pos)/2; i++){
            fswap1 = fontBuffer[i];
            fswap2 = fontBuffer[--end];
            fontBuffer[i] = fswap2;
            fontBuffer[end] = fswap1;
        }
    }

    // Draw each contiguous run of glyphs that are included in the same font.
    NSFont *currentFont = fontBuffer[pos];
    float nextX = startX;
    int nextGlyph = pos;

    while (nextGlyph < numGlyphs){
        if ((fontBuffer[nextGlyph] != 0 && fontBuffer[nextGlyph] != currentFont)){
            _drawGlyphs(currentFont, style->textColor, &glyphBuffer[lastFrom], &advances[lastFrom], startX, geometry->point.y, nextGlyph - lastFrom);
            lastFrom = nextGlyph;
            currentFont = fontBuffer[nextGlyph];
            startX = nextX;
        }
        nextX += advances[nextGlyph].width;
        nextGlyph++;
    }
    _drawGlyphs(currentFont, style->textColor, &glyphBuffer[lastFrom], &advances[lastFrom], startX, geometry->point.y, nextGlyph - lastFrom);

    if (advances != localAdvanceBuffer) {
        free(advances);
        free(widthBuffer);
        free(glyphBuffer);
        free(fontBuffer);
    }
}

#ifdef DEBUG_COMBINING
static const char *directionNames[] = {
        "DirectionL", 	// Left Letter 
        "DirectionR",	// Right Letter
        "DirectionEN",	// European Number
        "DirectionES",	// European Separator
        "DirectionET",	// European Terminator (post/prefix e.g. $ and %)
        "DirectionAN",	// Arabic Number
        "DirectionCS",	// Common Separator 
        "DirectionB", 	// Paragraph Separator (aka as PS)
        "DirectionS", 	// Segment Separator (TAB)
        "DirectionWS", 	// White space
        "DirectionON",	// Other Neutral

	// types for explicit controls
        "DirectionLRE", 
        "DirectionLRO", 

        "DirectionAL", 	// Arabic Letter (Right-to-left)

        "DirectionRLE", 
        "DirectionRLO", 
        "DirectionPDF", 

        "DirectionNSM", 	// Non-spacing Mark
        "DirectionBN"	// Boundary neutral (type of RLE etc after explicit levels)
};

static const char *joiningNames[] = {
        "JoiningOther",
        "JoiningDual",
        "JoiningRight",
        "JoiningCausing"
};
#endif

- (float)_floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style widths:(float *)widthBuffer fonts:(NSFont **)fontBuffer glyphs:(CGGlyph *)glyphBuffer startPosition:(float *)startPosition numGlyphs:(int *)_numGlyphs
{
    if (shouldUseATSU(run))
        return [self _ATSU_floatWidthForRun:run style:style];
    
    return [self _CG_floatWidthForRun:run style:style widths:widthBuffer fonts:fontBuffer glyphs:glyphBuffer startPosition:startPosition numGlyphs:_numGlyphs];

}

- (float)_CG_floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style widths: (float *)widthBuffer fonts: (NSFont **)fontBuffer glyphs: (CGGlyph *)glyphBuffer startPosition:(float *)startPosition numGlyphs: (int *)_numGlyphs
{
    float _totalWidth = 0, _nextWidth;
    CharacterWidthIterator widthIterator;
    NSFont *fontUsed = 0;
    ATSGlyphRef glyphUsed;
    int numGlyphs = 0;
    
    initializeCharacterWidthIterator(&widthIterator, self, run, style);
    if (startPosition)
        *startPosition = widthIterator.widthToStart;
    while ((_nextWidth = widthForNextCharacter(&widthIterator, &glyphUsed, &fontUsed)) != INVALID_WIDTH){
        if (fontBuffer)
            fontBuffer[numGlyphs] = fontUsed;
        if (glyphBuffer)
            glyphBuffer[numGlyphs] = glyphUsed;
        if (widthBuffer)
            widthBuffer[numGlyphs] = _nextWidth;
        numGlyphs++;
        _totalWidth += _nextWidth;
    }
        
    if (_numGlyphs)
        *_numGlyphs = numGlyphs;

    return _totalWidth;
}

- (ATSGlyphRef)_extendUnicodeCharacterToGlyphMapToInclude:(UnicodeChar)c
{
    UnicodeGlyphMap *map = (UnicodeGlyphMap *)calloc (1, sizeof(UnicodeGlyphMap));
    ATSLayoutRecord *glyphRecord;
    ATSGlyphVector glyphVector;
    UnicodeChar end, start;
    unsigned blockSize;
    ATSGlyphRef glyphID;
    
    if (unicodeCharacterToGlyphMap == 0)
        blockSize = INITIAL_BLOCK_SIZE;
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (c / blockSize) * blockSize;
    end = start + (blockSize - 1);
        
    LOG(FontCache, "%@ (0x%04x) adding glyphs for 0x%04x to 0x%04x", font, c, start, end);

    map->startRange = start;
    map->endRange = end;
    
    unsigned i, count = end - start + 1;
    UnicodeChar buffer[INCREMENTAL_BLOCK_SIZE+2];
    
    for (i = 0; i < count; i++){
        buffer[i] = i+start;
    }

    OSStatus status;
    status = ATSInitializeGlyphVector(count*2, 0, &glyphVector);
    if (status != noErr){
        // This should never happen, indicates a bad font!  If it does the
        // font substitution code will find an alternate font.
        free(map);
        return 0;
    }
    
    [self _convertUnicodeCharacters: &buffer[0] length: count toGlyphs: &glyphVector];
    unsigned numGlyphs = glyphVector.numGlyphs;
    if (numGlyphs != count){
        // This should never happen, indicates a bad font!  If it does the
        // font substitution code will find an alternate font.
        free(map);
        return 0;
    }
            
    map->glyphs = (GlyphEntry *)malloc (count * sizeof(GlyphEntry));
    glyphRecord = (ATSLayoutRecord *)glyphVector.firstRecord;
    for (i = 0; i < count; i++) {
        map->glyphs[i].glyph = glyphRecord->glyphID;
        map->glyphs[i].font = 0;
        glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + glyphVector.recordSize);
    }
    ATSClearGlyphVector(&glyphVector);
    
    if (unicodeCharacterToGlyphMap == 0)
        unicodeCharacterToGlyphMap = map;
    else {
        UnicodeGlyphMap *lastMap = unicodeCharacterToGlyphMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    glyphID = map->glyphs[c - start].glyph;
    
    return glyphID;
}

- (void)_updateGlyphEntryForCharacter:(UniChar)c glyphID:(ATSGlyphRef)glyphID font:(NSFont *)substituteFont
{
    GlyphMap *lastMap = characterToGlyphMap;
    while (lastMap != 0){
        if (c >= lastMap->startRange && c <= lastMap->endRange){
            lastMap->glyphs[c - lastMap->startRange].glyph = glyphID;
            // This font will leak.  No problem though, it has to stick around
            // forever.  Max theoretical retain counts applied here will be
            // num_fonts_on_system * num_glyphs_in_font.
            lastMap->glyphs[c - lastMap->startRange].font = [substituteFont retain];
            break;
        }
        lastMap = lastMap->next;
    }
}

- (ATSGlyphRef)_extendCharacterToGlyphMapToInclude:(UniChar) c
{
    GlyphMap *map = (GlyphMap *)calloc (1, sizeof(GlyphMap));
    ATSLayoutRecord *glyphRecord;
    ATSGlyphVector glyphVector;
    UniChar end, start;
    unsigned blockSize;
    ATSGlyphRef glyphID;
    
    if (characterToGlyphMap == 0)
        blockSize = INITIAL_BLOCK_SIZE;
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (c / blockSize) * blockSize;
    end = start + (blockSize - 1);
        
    LOG(FontCache, "%@ (0x%04x) adding glyphs for 0x%04x to 0x%04x", font, c, start, end);

    map->startRange = start;
    map->endRange = end;
    
    unsigned i, count = end - start + 1;
    short unsigned buffer[INCREMENTAL_BLOCK_SIZE+2];
    
    for (i = 0; i < count; i++) {
        buffer[i] = i+start;
    }

    if (start == 0) {
        // Control characters must not render at all.
        for (i = 0; i < 0x20; ++i)
            buffer[i] = ZERO_WIDTH_SPACE;
        buffer[0x7F] = ZERO_WIDTH_SPACE;

        // But both \n and nonbreaking space must render as a space.
        buffer['\n'] = ' ';
        buffer[NO_BREAK_SPACE] = ' ';
    }

    OSStatus status = ATSInitializeGlyphVector(count, 0, &glyphVector);
    if (status != noErr) {
        // This should never happen, perhaps indicates a bad font!  If it does the
        // font substitution code will find an alternate font.
        free(map);
        return 0;
    }

    [self _convertCharacters: &buffer[0] length: count toGlyphs: &glyphVector];
    unsigned numGlyphs = glyphVector.numGlyphs;
    if (numGlyphs != count){
        // This should never happen, perhaps indicates a bad font!  If it does the
        // font substitution code will find an alternate font.
        free(map);
        return 0;
    }
            
    map->glyphs = (GlyphEntry *)malloc (count * sizeof(GlyphEntry));
    glyphRecord = (ATSLayoutRecord *)glyphVector.firstRecord;
    for (i = 0; i < count; i++) {
        map->glyphs[i].glyph = glyphRecord->glyphID;
        map->glyphs[i].font = 0;
        glyphRecord = (ATSLayoutRecord *)((char *)glyphRecord + glyphVector.recordSize);
    }
    ATSClearGlyphVector(&glyphVector);
    
    if (characterToGlyphMap == 0)
        characterToGlyphMap = map;
    else {
        GlyphMap *lastMap = characterToGlyphMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

    glyphID = map->glyphs[c - start].glyph;
    
    // Special case for characters 007F-00A0.
    if (glyphID == 0 && c >= 0x7F && c <= 0xA0){
        glyphID = [font _defaultGlyphForChar: c];
        map->glyphs[c - start].glyph = glyphID;
        map->glyphs[c - start].font = 0;
    }

    return glyphID;
}


- (WidthMap *)_extendGlyphToWidthMapToInclude:(ATSGlyphRef)glyphID font:(NSFont *)subFont
{
    WidthMap *map = (WidthMap *)calloc (1, sizeof(WidthMap)), **rootMap;
    unsigned end;
    ATSGlyphRef start;
    unsigned blockSize;
    unsigned i, count;
    
    if (subFont && subFont != font)
        rootMap = &mapForSubstituteFont(self,subFont)->map;
    else
        rootMap = &glyphToWidthMap;
        
    if (*rootMap == 0){
        if ([(subFont ? subFont : font) numberOfGlyphs] < INITIAL_BLOCK_SIZE)
            blockSize = [font numberOfGlyphs];
         else
            blockSize = INITIAL_BLOCK_SIZE;
    }
    else
        blockSize = INCREMENTAL_BLOCK_SIZE;
    start = (glyphID / blockSize) * blockSize;
    end = ((unsigned)start) + blockSize; 
    if (end > 0xffff)
        end = 0xffff;

    LOG(FontCache, "%@ (0x%04x) adding widths for range 0x%04x to 0x%04x", font, glyphID, start, end);

    map->startRange = start;
    map->endRange = end;
    count = end - start + 1;

    map->widths = (WidthEntry *)malloc (count * sizeof(WidthEntry));

    for (i = 0; i < count; i++){
        map->widths[i].width = UNINITIALIZED_GLYPH_WIDTH;
    }

    if (*rootMap == 0)
        *rootMap = map;
    else {
        WidthMap *lastMap = *rootMap;
        while (lastMap->next != 0)
            lastMap = lastMap->next;
        lastMap->next = map;
    }

#ifdef _TIMING
    LOG(FontCache, "%@ total time to advances lookup %f seconds", font, totalCGGetAdvancesTime);
#endif
    return map;
}


- (void)_initializeATSUStyle
{
    // The two NSFont calls in this method (pointSize and _atsFontID)
    // are both exception-safe.

    if (!ATSUStyleInitialized){
        OSStatus status;
        
        status = ATSUCreateStyle(&_ATSUSstyle);
        if(status != noErr)
            FATAL_ALWAYS ("ATSUCreateStyle failed (%d)", status);
    
        ATSUFontID fontID = [font _atsFontID];
        if (fontID == 0){
            ATSUDisposeStyle(_ATSUSstyle);
            ERROR ("unable to get ATSUFontID for %@", font);
            return;
        }
        
        CGAffineTransform transform = CGAffineTransformMakeScale (1,-1);
        Fixed fontSize = FloatToFixed([font pointSize]);
        ATSUAttributeTag styleTags[] = { kATSUSizeTag, kATSUFontTag, kATSUFontMatrixTag};
        ByteCount styleSizes[] = {  sizeof(Fixed), sizeof(ATSUFontID), sizeof(CGAffineTransform) };
        ATSUAttributeValuePtr styleValues[] = { &fontSize, &fontID, &transform  };
        status = ATSUSetAttributes (_ATSUSstyle, 3, styleTags, styleSizes, styleValues);
        if(status != noErr)
            FATAL_ALWAYS ("ATSUSetAttributes failed (%d)", status);

        ATSUStyleInitialized = YES;
    }
}

- (ATSUTextLayout)_createATSUTextLayoutForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style
{
    // The only Cocoa calls here are to NSGraphicsContext and the self
    // call to _initializeATSUStyle, which are all exception-safe.

    ATSUTextLayout layout;
    UniCharCount runLength;
    OSStatus status;
    
    [self _initializeATSUStyle];
    
    // FIXME: This is missing the following features that the CoreGraphics code path has:
    // - Both \n and nonbreaking space render as a space.
    // - All other control characters must not render at all (other code path uses zero-width spaces).

    runLength = run->to - run->from;
    status = ATSUCreateTextLayoutWithTextPtr(
            run->characters,
            run->from,           // offset
            runLength,        // length
            run->length,         // total length
            1,              // styleRunCount
            &runLength,    // length of style run
            &_ATSUSstyle, 
            &layout);
    if(status != noErr)
        FATAL_ALWAYS ("ATSUCreateTextLayoutWithTextPtr failed(%d)", status);

    CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    ATSLineLayoutOptions lineLayoutOptions = (kATSLineFractDisable | kATSLineDisableAutoAdjustDisplayPos | kATSLineUseDeviceMetrics);
    Boolean rtl = style->rtl;
    ATSUAttributeTag tags[] = { kATSUCGContextTag, kATSULineLayoutOptionsTag, kATSULineDirectionTag };
    ByteCount sizes[] = { sizeof(CGContextRef), sizeof(ATSLineLayoutOptions), sizeof(Boolean)  };
    ATSUAttributeValuePtr values[] = { &cgContext, &lineLayoutOptions, &rtl };
    
    status = ATSUSetLayoutControls(layout, 3, tags, sizes, values);
    if(status != noErr)
        FATAL_ALWAYS ("ATSUSetLayoutControls failed(%d)", status);

    status = ATSUSetTransientFontMatching (layout, YES);
    if(status != noErr)
        FATAL_ALWAYS ("ATSUSetTransientFontMatching failed(%d)", status);
        
    return layout;
}


- (ATSTrapezoid)_trapezoidForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style atPoint:(NSPoint )p
{
    // The only Cocoa call here is the self call to
    // _createATSUTextLayoutForRun:, which is exception-safe.

    OSStatus status;
    
    if (run->to - run->from <= 0){
        ATSTrapezoid nilTrapezoid = { {0,0} , {0,0}, {0,0}, {0,0} };
        return nilTrapezoid;
    }
        
    ATSUTextLayout layout = [self _createATSUTextLayoutForRun:run style:style];

    ATSTrapezoid firstGlyphBounds;
    ItemCount actualNumBounds;
    status = ATSUGetGlyphBounds (layout, FloatToFixed(p.x), FloatToFixed(p.y), run->from, run->to - run->from, kATSUseDeviceOrigins, 1, &firstGlyphBounds, &actualNumBounds);    
    if(status != noErr)
        FATAL_ALWAYS ("ATSUGetGlyphBounds() failed(%d)", status);
    
    if (actualNumBounds != 1)
        FATAL_ALWAYS ("unexpected result from ATSUGetGlyphBounds():  actualNumBounds(%d) != 1", actualNumBounds);

    ATSUDisposeTextLayout (layout); // Ignore the error.  Nothing we can do anyway.
            
    return firstGlyphBounds;
}


- (float)_ATSU_floatWidthForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style
{
    ATSTrapezoid oGlyphBounds;
    
    oGlyphBounds = [self _trapezoidForRun:run style:style atPoint:NSMakePoint (0,0)];
    
    float width = 
        MAX(FixedToFloat(oGlyphBounds.upperRight.x), FixedToFloat(oGlyphBounds.lowerRight.x)) - 
        MIN(FixedToFloat(oGlyphBounds.upperLeft.x), FixedToFloat(oGlyphBounds.lowerLeft.x));
    
    return width;
}

// Be sure to free the run.characters allocated by this function.
static WebCoreTextRun reverseCharactersInRun(const WebCoreTextRun *run)
{
    WebCoreTextRun swappedRun;
    unsigned int i;
    
    UniChar *swappedCharacters = (UniChar *)malloc(sizeof(UniChar)*run->length);
    for (i = 0; i < run->length; i++) {
        swappedCharacters[i] = run->characters[run->length-i-1];
    }
    swappedRun.characters = swappedCharacters;
    swappedRun.from = run->length - (run->to == -1 ? (int)run->length : run->to);
    swappedRun.to = run->length - (run->from == -1 ? 0 : run->from);
    swappedRun.length = run->length;

    return swappedRun;
}

- (void)_ATSU_drawHighlightForRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    // The only Cocoa calls made here are to NSColor and NSBezierPath,
    // plus the self calls to _createATSUTextLayoutForRun: and
    // _trapezoidForRun:. These are all exception-safe.

    ATSUTextLayout layout;
    int from, to;
    float selectedLeftX;
    const WebCoreTextRun *aRun = run;
    WebCoreTextRun swappedRun;

    if (style->backgroundColor == nil)
        return;
    
    if (style->visuallyOrdered) {
        swappedRun = reverseCharactersInRun(run);
        aRun = &swappedRun;
    }

    from = aRun->from;
    to = aRun->to;
    if (from == -1)
        from = 0;
    if (to == -1)
        to = run->length;
   
    int runLength = to - from;
    if (runLength <= 0){
        return;
    }

    layout = [self _createATSUTextLayoutForRun:aRun style:style];

    WebCoreTextRun leadingRun = *aRun;
    leadingRun.from = 0;
    leadingRun.to = run->from;
    
    // ATSU provides the bounds of the glyphs for the run with an origin of
    // (0,0), so we need to find the width of the glyphs immediately before
    // the actually selected glyphs.
    ATSTrapezoid leadingTrapezoid = [self _trapezoidForRun:&leadingRun style:style atPoint:geometry->point];
    ATSTrapezoid selectedTrapezoid = [self _trapezoidForRun:run style:style atPoint:geometry->point];

    float backgroundWidth = 
            MAX(FixedToFloat(selectedTrapezoid.upperRight.x), FixedToFloat(selectedTrapezoid.lowerRight.x)) - 
            MIN(FixedToFloat(selectedTrapezoid.upperLeft.x), FixedToFloat(selectedTrapezoid.lowerLeft.x));

    if (run->from == 0)
        selectedLeftX = geometry->point.x;
    else
        selectedLeftX = MIN(FixedToFloat(leadingTrapezoid.upperRight.x), FixedToFloat(leadingTrapezoid.lowerRight.x));
    
    [style->backgroundColor set];

    float yPos = geometry->useFontMetricsForSelectionYAndHeight ? geometry->point.y - [self ascent] : geometry->selectionY;
    float height = geometry->useFontMetricsForSelectionYAndHeight ? [self lineSpacing] : geometry->selectionHeight;
    if (style->rtl || style->visuallyOrdered){
        WebCoreTextRun completeRun = *aRun;
        completeRun.from = 0;
        completeRun.to = aRun->length;
        float completeRunWidth = [self floatWidthForRun:&completeRun style:style widths:0];
        [NSBezierPath fillRect:NSMakeRect(geometry->point.x + completeRunWidth - (selectedLeftX-geometry->point.x) - backgroundWidth, yPos, backgroundWidth, height)];
    }
    else {
        [NSBezierPath fillRect:NSMakeRect(selectedLeftX, yPos, backgroundWidth, height)];
    }

    ATSUDisposeTextLayout (layout); // Ignore the error.  Nothing we can do anyway.

    if (style->visuallyOrdered)
        free ((void *)swappedRun.characters);
}


- (void)_ATSU_drawRun:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style geometry:(const WebCoreTextGeometry *)geometry
{
    // The only Cocoa calls made here are to NSColor, plus the self
    // calls to _createATSUTextLayoutForRun: and
    // _ATSU_drawHighlightForRun:. These are all exception-safe.

    ATSUTextLayout layout;
    OSStatus status;
    int from, to;
    const WebCoreTextRun *aRun = run;
    WebCoreTextRun swappedRun;
    
    if (style->visuallyOrdered) {
        swappedRun = reverseCharactersInRun(run);
        aRun = &swappedRun;
    }

    from = aRun->from;
    to = aRun->to;
    if (from == -1)
        from = 0;
    if (to == -1)
        to = run->length;

    int runLength = to - from;
    if (runLength <= 0)
        return;

    layout = [self _createATSUTextLayoutForRun:aRun style:style];

    if (style->backgroundColor != nil)
        [self _ATSU_drawHighlightForRun:run style:style geometry:geometry];

    [style->textColor set];

    status = ATSUDrawText(layout, 
            aRun->from,
            runLength,
            FloatToFixed(geometry->point.x),   // these values are
            FloatToFixed(geometry->point.y));  // also of type Fixed
    if (status != noErr){
        // Nothing to do but report the error (dev build only).
        ERROR ("ATSUDrawText() failed(%d)", status);
    }

    ATSUDisposeTextLayout (layout); // Ignore the error.  Nothing we can do anyway.
    
    if (style->visuallyOrdered)
        free ((void *)swappedRun.characters);
}

- (int)_ATSU_pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs
{
    // The only Cocoa calls made here is to the self call to
    // _createATSUTextLayoutForRun:. This is exception-safe.

    unsigned offset = 0;
    ATSUTextLayout layout;
    UniCharArrayOffset primaryOffset = 0;
    UniCharArrayOffset secondaryOffset = 0;
    OSStatus status;
    Boolean isLeading;
    const WebCoreTextRun *aRun = run;
    WebCoreTextRun swappedRun;
    
    // Reverse the visually ordered characters.  ATSU will re-reverse.  Ick!
    if (style->visuallyOrdered) {
        swappedRun = reverseCharactersInRun(run);
        aRun = &swappedRun;
    }

    layout = [self _createATSUTextLayoutForRun:aRun style:style];

    primaryOffset = aRun->from;
    
    // FIXME: No idea how to avoid including partial glyphs.   Not even sure if that's the behavior
    // this yields now.
    status = ATSUPositionToOffset(layout, FloatToFixed(x), FloatToFixed(-1), &primaryOffset, &isLeading, &secondaryOffset);
    if (status == noErr){
        offset = (unsigned)primaryOffset;
    }
    else {
        // Failed to find offset!  Return 0 offset.
    }
       
    if (style->visuallyOrdered) {
        free ((void *)swappedRun.characters);
    }

    return offset - aRun->from;
}

- (int)_CG_pointToOffset:(const WebCoreTextRun *)run style:(const WebCoreTextStyle *)style position:(int)x reversed:(BOOL)reversed includePartialGlyphs:(BOOL)includePartialGlyphs
{
    float delta = (float)x;
    float width;
    unsigned offset = run->from;
    CharacterWidthIterator widthIterator;
    
    initializeCharacterWidthIterator(&widthIterator, self, run, style);

    if (reversed) {
        width = [self floatWidthForRun:run style:style widths:nil];
        delta -= width;
        while (offset < run->length) {
            float w = widthForNextCharacter(&widthIterator, 0, 0);
            if (w == INVALID_WIDTH) {
                // Something very bad happened, like we only have half of a surrogate pair.
                break;
            }
            else {
                if (w) {
                    if (includePartialGlyphs)
                       w -= w/2;
                    delta += w;
                    if(delta >= 0)
                        break;
                    if (includePartialGlyphs)
                        delta += w;
                }
                offset = widthIterator.currentCharacter;
            }
        }
    } else {
        while (offset < run->length) {
            float w = widthForNextCharacter(&widthIterator, 0, 0);
            if (w == INVALID_WIDTH) {
                // Something very bad happened, like we only have half of a surrogate pair.
                break;
            }
            else {
                if (w) {
                    if (includePartialGlyphs)
                        w -= w/2;
                    delta -= w;
                    if(delta <= 0) 
                        break;
                    if (includePartialGlyphs)
                        delta -= w;
                }
                offset = widthIterator.currentCharacter;
            }
        }
    }
    
    return offset - run->from;
}

@end

// ------------------- Private functions -------------------

static void freeWidthMap(WidthMap *map)
{
    while (map) {
        WidthMap *next = map->next;
        free(map->widths);
        free(map);
        map = next;
    }
}


static void freeGlyphMap(GlyphMap *map)
{
    while (map) {
        GlyphMap *next = map->next;
        free(map->glyphs);
        free(map);
        map = next;
    }
}


static void freeUnicodeGlyphMap(UnicodeGlyphMap *map)
{
    while (map) {
        UnicodeGlyphMap *next = map->next;
        free(map->glyphs);
        free(map);
        map = next;
    }
}


static inline ATSGlyphRef glyphForCharacter (GlyphMap *map, UniChar c, NSFont **font)
{
    if (map == 0)
        return nonGlyphID;
        
    while (map) {
        if (c >= map->startRange && c <= map->endRange){
            *font = map->glyphs[c-map->startRange].font;
            return map->glyphs[c-map->startRange].glyph;
        }
        map = map->next;
    }
    return nonGlyphID;
}
 
 
static inline ATSGlyphRef glyphForUnicodeCharacter (UnicodeGlyphMap *map, UnicodeChar c, NSFont **font)
{
    if (map == 0)
        return nonGlyphID;
        
    while (map) {
        if (c >= map->startRange && c <= map->endRange){
            *font = map->glyphs[c-map->startRange].font;
            return map->glyphs[c-map->startRange].glyph;
        }
        map = map->next;
    }
    return nonGlyphID;
}
 

#ifdef _TIMING        
static double totalCGGetAdvancesTime = 0;
#endif

static inline SubstituteFontWidthMap *mapForSubstituteFont(WebTextRenderer *renderer, NSFont *font)
{
    int i;
    
    for (i = 0; i < renderer->numSubstituteFontWidthMaps; i++){
        if (font == renderer->substituteFontWidthMaps[i].font)
            return &renderer->substituteFontWidthMaps[i];
    }
    
    if (renderer->numSubstituteFontWidthMaps == renderer->maxSubstituteFontWidthMaps){
        renderer->maxSubstituteFontWidthMaps = renderer->maxSubstituteFontWidthMaps * 2;
        renderer->substituteFontWidthMaps = realloc (renderer->substituteFontWidthMaps, renderer->maxSubstituteFontWidthMaps * sizeof(SubstituteFontWidthMap));
        for (i = renderer->numSubstituteFontWidthMaps; i < renderer->maxSubstituteFontWidthMaps; i++){
            renderer->substituteFontWidthMaps[i].font = 0;
            renderer->substituteFontWidthMaps[i].map = 0;
        }
    }
    
    renderer->substituteFontWidthMaps[renderer->numSubstituteFontWidthMaps].font = font;
    return &renderer->substituteFontWidthMaps[renderer->numSubstituteFontWidthMaps++];
}

static void initializeCharacterWidthIterator (CharacterWidthIterator *iterator, WebTextRenderer *renderer, const WebCoreTextRun *run , const WebCoreTextStyle *style) 
{
    iterator->renderer = renderer;
    iterator->run = run;
    iterator->style = style;
    iterator->currentCharacter = run->from;
    iterator->runWidthSoFar = 0;

    // If the padding is non-zero, count the number of spaces in the run
    // and divide that by the padding for per space addition.
    iterator->padding = style->padding;
    if (iterator->padding > 0){
        uint numSpaces = 0;
        int from = run->from;
        int len = run->to - from;
        int k;
        for (k = from; k < from + len; k++) {
            if (isSpace(run->characters[k])) {
                numSpaces++;
            }
        }
        iterator->padPerSpace = CEIL_TO_INT ((((float)style->padding) / ((float)numSpaces)));
    }
    else {
        iterator->padPerSpace = 0;
    }
    
    // Calculate width up to starting position of the run.  This is
    // necessary to ensure that our rounding hacks are always consistently
    // applied.
    if (run->from != 0){
        WebCoreTextRun startPositionRun = *run;
        startPositionRun.from = 0;
        startPositionRun.to = run->from;
        CharacterWidthIterator startPositionIterator;
        initializeCharacterWidthIterator (&startPositionIterator, renderer, &startPositionRun, style);
        
        while (startPositionIterator.currentCharacter < (unsigned)startPositionRun.to){
            widthForNextCharacter(&startPositionIterator, 0, 0);
        }
        iterator->widthToStart = startPositionIterator.runWidthSoFar;
    }
    else
        iterator->widthToStart = 0;
}

static inline float ceilCurrentWidth (CharacterWidthIterator *iterator)
{
    float delta = CEIL_TO_INT(iterator->widthToStart + iterator->runWidthSoFar) - (iterator->widthToStart + iterator->runWidthSoFar);
    iterator->runWidthSoFar += delta;
    return delta;
}

// Return INVALID_WIDTH if an error is encountered or we're at the end of the range in the run.
static float widthForNextCharacter(CharacterWidthIterator *iterator, ATSGlyphRef *glyphUsed, NSFont **fontUsed)
{
    WebTextRenderer *renderer = iterator->renderer;
    const WebCoreTextRun *run = iterator->run;
    unsigned currentCharacter = iterator->currentCharacter;

    NSFont *_fontUsed = nil;
    ATSGlyphRef _glyphUsed;

    if (!fontUsed)
        fontUsed = &_fontUsed;
    if (!glyphUsed)
        glyphUsed = &_glyphUsed;
        
    if (currentCharacter >= (unsigned)run->to)
        // Error! Offset specified beyond end of run.
        return INVALID_WIDTH;

    const UniChar *cp = &run->characters[currentCharacter];
    UnicodeChar c = *cp;

    if (IsLowSurrogatePair(c))
        return INVALID_WIDTH;

    // Do we have a surrogate pair?  If so, determine the full Unicode (32 bit)
    // code point before glyph lookup.
    unsigned clusterLength = 1;
    if (IsHighSurrogatePair(c)) {
        // Make sure we have another character and it's a low surrogate.
        UniChar low;
        if (currentCharacter + 1 >= run->length || !IsLowSurrogatePair((low = cp[1]))) {
            // Error!  The second component of the surrogate pair is missing.
            return INVALID_WIDTH;
        }

        c = UnicodeValueForSurrogatePair(c, low);
        clusterLength = 2;
    }

    // If small-caps convert lowercase to upper.
    BOOL useSmallCapsFont = NO;
    if (renderer->isSmallCapsRenderer) {
        if (!u_isUUppercase(c)) {
            // Only use small cap font if the the uppercase version of the character
            // is different than the lowercase.
            UnicodeChar newC = u_toupper(c);
            if (newC != c) {
                useSmallCapsFont = YES;
                c = newC;
            }
        }
    }

    if (c <= 0xFFFF) {
        *glyphUsed = glyphForCharacter(renderer->characterToGlyphMap, c, fontUsed);
        if (*glyphUsed == nonGlyphID) {
            *glyphUsed = [renderer _extendCharacterToGlyphMapToInclude:c];
        }
    } else {
        *glyphUsed = glyphForUnicodeCharacter(renderer->unicodeCharacterToGlyphMap, c, fontUsed);
        if (*glyphUsed == nonGlyphID) {
            *glyphUsed = [renderer _extendUnicodeCharacterToGlyphMapToInclude:c];
        }
    }

    // Check to see if we're rendering in 'small-caps' mode.
    // ASSUMPTION:  We assume the same font in a smaller size has
    // the same glyphs as the large font.
    if (useSmallCapsFont) {
        if (*fontUsed == nil)
            *fontUsed = [renderer _smallCapsFont];
        else {
            // Potential for optimization.  This path should only be taken if we're
            // using a cached substituted font.
            *fontUsed = [[NSFontManager sharedFontManager] convertFont:*fontUsed toSize:[*fontUsed pointSize] * SMALLCAPS_FONTSIZE_MULTIPLIER];
        }
    }

    // Now that we have glyph and font, get its width.
    WebGlyphWidth width = widthForGlyph(renderer, *glyphUsed, *fontUsed);

    // We special case spaces in two ways when applying word rounding.
    // First, we round spaces to an adjusted width in all fonts.
    // Second, in fixed-pitch fonts we ensure that all characters that
    // match the width of the space character have the same width as the space character.
    if ((renderer->treatAsFixedPitch ? width == renderer->spaceWidth : *glyphUsed == renderer->spaceGlyph) && iterator->style->applyWordRounding)
        width = renderer->adjustedSpaceWidth;

    // Try to find a substitute font if this font didn't have a glyph for a character in the
    // string.  If one isn't found we end up drawing and measuring the 0 glyph, usually a box.
    if (*glyphUsed == 0 && iterator->style->attemptFontSubstitution) {
        UniChar characterArray[2];
        unsigned characterArrayLength;
        
        if (c <= 0xFFFF) {
            characterArray[0] = c;
            characterArrayLength = 1;
        } else {
            characterArray[0] = HighSurrogatePair(c);
            characterArray[1] = LowSurrogatePair(c);
            characterArrayLength = 2;
        }
        
        NSFont *substituteFont = [renderer _substituteFontForCharacters:characterArray length:characterArrayLength
            families:iterator->style->families];
        if (substituteFont) {
            int cNumGlyphs = 0;
            ATSGlyphRef localGlyphBuffer[MAX_GLYPH_EXPANSION];
            
            WebCoreTextRun clusterRun;
            WebCoreInitializeTextRun(&clusterRun, characterArray, characterArrayLength, 0, characterArrayLength);
            WebCoreTextStyle clusterStyle = *iterator->style;
            clusterStyle.padding = 0;
            clusterStyle.applyRunRounding = false;
            clusterStyle.attemptFontSubstitution = false;
            
            WebTextRenderer *substituteRenderer;
            substituteRenderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:substituteFont usingPrinterFont:renderer->usingPrinterFont];
            width = [substituteRenderer
                            _floatWidthForRun:&clusterRun
                            style:&clusterStyle 
                            widths: nil
                            fonts: nil
                            glyphs: &localGlyphBuffer[0]
                            startPosition:nil
                            numGlyphs:&cNumGlyphs];
            
            *fontUsed = substituteFont;
            *glyphUsed = localGlyphBuffer[0];
            
            if (c <= 0xFFFF && cNumGlyphs == 1 && localGlyphBuffer[0] != 0){
                [renderer _updateGlyphEntryForCharacter:c glyphID:localGlyphBuffer[0] font:substituteFont];
            }
        }
    }

    if (!*fontUsed)
        *fontUsed = renderer->font;

    // Force characters that are used to determine word boundaries for the rounding hack
    // to be integer width, so following words will start on an integer boundary.
    if (isRoundingHackCharacter(c) && iterator->style->applyWordRounding) {
        width = CEIL_TO_INT(width);
    }
    
    // Account for letter-spacing
    if (iterator->style->letterSpacing && width > 0)
        width += iterator->style->letterSpacing;

    // Account for padding.  khtml uses space padding to justify text.  We
    // distribute the specified padding over the available spaces in the run.
    if (isSpace(c)) {
        if (iterator->padding > 0) {
            // Only use left over padding if note evenly divisible by 
            // number of spaces.
            if (iterator->padding < iterator->padPerSpace){
                width += iterator->padding;
                iterator->padding = 0;
            }
            else {
                width += iterator->padPerSpace;
                iterator->padding -= iterator->padPerSpace;
            }
        }
        
        // Account for word-spacing.  We apply additional space between "words" by
        // adding width to the space character.
        if (currentCharacter > 0 && !isSpace(cp[-1]))
            width += iterator->style->wordSpacing;
    }

    iterator->runWidthSoFar += width;

    // Advance past the character we just dealt with.
    currentCharacter += clusterLength;
    iterator->currentCharacter = currentCharacter;

    int len = run->to - run->from;

    // Account for float/integer impedance mismatch between CG and khtml.  "Words" (characters 
    // followed by a character defined by isSpace()) are always an integer width.  We adjust the 
    // width of the last character of a "word" to ensure an integer width.  When we move khtml to
    // floats we can remove this (and related) hacks.
    //
    // Check to see if the next character is a "RoundingHackCharacter", if so, adjust.
    if (currentCharacter < run->length && isRoundingHackCharacter(cp[clusterLength]) && iterator->style->applyWordRounding) {
        width += ceilCurrentWidth(iterator);
    }
    else if (currentCharacter >= (unsigned)run->to && (len > 1 || run->length == 1) && iterator->style->applyRunRounding) {
        width += ceilCurrentWidth(iterator);
    }
    
    return width;
}


static BOOL fillStyleWithAttributes(ATSUStyle style, NSFont *theFont)
{
    if (theFont) {
        ATSUFontID fontId = [theFont _atsFontID];
        LOG (FontCache, "fillStyleWithAttributes:  font = %p,%@, _atsFontID = %x\n", theFont, theFont, (unsigned)fontId);
        ATSUAttributeTag tag = kATSUFontTag;
        ByteCount size = sizeof(ATSUFontID);
        ATSUFontID *valueArray[1] = {&fontId};
        OSStatus status;

        if (fontId) {
            status = ATSUSetAttributes(style, 1, &tag, &size, (void *)valueArray);
            if (status != noErr){
                LOG (FontCache, "fillStyleWithAttributes failed(%d):  font = %p,%@, _atsFontID = %x\n", (int)status, theFont, theFont, (unsigned)fontId);
                return NO;
            }
        }
        else {
            return NO;
        }
        return YES;
    }
    return NO;
}

static BOOL shouldUseATSU(const WebCoreTextRun *run)
{
    UniChar c;
    const UniChar *characters = run->characters;
    int i, from = run->from, to = run->to;
    
    if (alwaysUseATSU)
        return YES;
        
    for (i = from; i < to; i++){
        c = characters[i];
        if (c < 0x300)                      // Early continue to avoid other checks for the common case.
            continue;
            
        if (c >= 0x300 && c <= 0x36F)       // U+0300 through U+036F Combining diacritical marks
            return YES;
        if (c >= 0x20D0 && c <= 0x20FF)     // U+20D0 through U+20FF Combining marks for symbols
            return YES;
        if (c >= 0xFE20 && c <= 0xFE2f)     // U+FE20 through U+FE2F Combining half marks
            return YES;
        if (c >= 0x591 && c <= 0x1059)      // U+0591 through U+1059 Arabic, Hebrew, Syriac, Thaana, Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
            return YES;
        if (c >= 0x1100 && c <= 0x11FF)     // U+1100 through U+11FF Hangul Jamo (only Ancient Korean should be left here if you precompose; Modern Korean will be precomposed as a result of step A)
            return YES;
        if (c >= 0x1780 && c <= 0x18AF)     // U+1780 through U+18AF Khmer, Mongolian
            return YES;
        if (c >= 0x1900 && c <= 0x194F)     // U+1900 through U+194F Limbu (Unicode 4.0)
            return YES;
    }
    
    return NO;
}
