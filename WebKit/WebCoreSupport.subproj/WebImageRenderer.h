/*	WebImageRenderer.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@protocol WebCoreImageRenderer;

@interface WebImageRenderer : NSImage <WebCoreImageRenderer>
{
    NSTimer *frameTimer;
    NSView *frameView;
    NSRect imageRect;
    NSRect targetRect;

    int loadStatus;

    NSColor *patternColor;
    int patternColorLoadStatus;

    int repetitionsComplete;
    BOOL animationFinished;
    
    NSPoint tilePoint;
    BOOL animatedTile;

    int compositeOperator;
    id redirectContext;
    CGContextRef context;
    BOOL needFlushRasterCache;
    BOOL rasterFlushing;
    NSImageCacheMode rasterFlushingOldMode;
    
    NSString *MIMEType;
    BOOL isNull;
    int useCount;

    id _PDFDoc;
@public    
    NSData *originalData;
}

- (id)initWithMIMEType:(NSString *)MIME;
- (id)initWithData:(NSData *)data MIMEType:(NSString *)MIME;
+ (void)stopAnimationsInView:(NSView *)aView;
- (int)frameCount;

- (NSString *)MIMEType;

@end
