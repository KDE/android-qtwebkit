/*
        NSCarbonWindowFrame.m
        Application Kit
        Copyright (c) 2000-2001, Apple Computer, Inc.
        All rights reserved.
        Original Author: Mark Piccirelli
        Responsibility: Mark Piccirelli

The class of objects that are meant to be used as _borderViews of NSCarbonWindow objects.  It's really only a subclass of NSView for the benefit of embedded NSViewCarbonControls.
*/


#import "CarbonWindowFrame.h"
#import "CarbonWindowAdapter.h"
#import "CarbonWindowContentView.h"
#import <Foundation/NSGeometry.h>
#import <Foundation/NSString.h>
#import <HIToolbox/MacWindows.h>
#import <assert.h>

@interface NSView(Secret)
- (void)_setWindow:(NSWindow *)window;
@end

// Turn off the assertions in this file.
// If this is commented out, uncomment it before committing to CVS.  M.P. Warning - 10/18/01
#undef assert
#define assert(X)


@class NSButton;
/*

@interface NSThemeFrame(NSHijackedClassMethods)

+ (float)_titlebarHeight:(unsigned int)style;

@end
*/

@implementation CarbonWindowFrame


- (NSRect)titlebarRect
{
    NSRect	titlebarRect;
    NSRect	boundsRect;

    boundsRect = [self bounds];

    titlebarRect.origin.x    = boundsRect.origin.x;
    titlebarRect.size.width  = boundsRect.size.width;
    titlebarRect.size.height = 22;
    titlebarRect.origin.y    = NSMaxY(boundsRect) - titlebarRect.size.height;

    return titlebarRect;
}

// Given a content rectangle and style mask, return a corresponding frame rectangle.
+ (NSRect)frameRectForContentRect:(NSRect)contentRect styleMask:(unsigned int)style {

    // We don't bother figuring out a good value, because content rects weren't so meaningful for NSCarbonWindows in the past, but this might not be a good assumption anymore.  M.P. Warning - 12/5/00
    return contentRect;

}

+ (NSRect)contentRectForFrameRect:(NSRect)frameRect styleMask:(unsigned int)style {

    // We don't bother figuring out a good value, because content rects weren't so meaningful for NSCarbonWindows in the past, but this might not be a good assumption anymore.  KW - copied from +frameRectForContentRect:styleMask
    return frameRect;

}

+ (NSSize)minFrameSizeForMinContentSize:(NSSize)cSize styleMask:(unsigned int)style {
    // See comments above.  We don't make any assumptions about the relationship between content rects and frame rects
    return cSize;
}

- (NSRect)frameRectForContentRect:(NSRect)cRect styleMask:(unsigned int)style {
    return [[self class] frameRectForContentRect: cRect styleMask:style];
}
- (NSRect)contentRectForFrameRect:(NSRect)fRect styleMask:(unsigned int)style {
    return [[self class] contentRectForFrameRect: fRect styleMask:style];
}
- (NSSize)minFrameSizeForMinContentSize:(NSSize)cSize styleMask:(unsigned int)style {
    return [[self class] minFrameSizeForMinContentSize:cSize styleMask: style];
}

// Initialize.
- (id)initWithFrame:(NSRect)inFrameRect styleMask:(unsigned int)inStyleMask owner:(NSWindow *)inOwningWindow {

    // Parameter check.
    if (![inOwningWindow isKindOfClass:[CarbonWindowAdapter class]]) NSLog(@"CarbonWindowFrames can only be owned by CarbonWindowAdapters.");

    // Do the standard Cocoa thing.
    self = [super initWithFrame:inFrameRect];
    if (!self) return nil;

    // Record what we'll need later.
    _styleMask = inStyleMask;

    // Do what NSFrameView's method of the same name does.
    [self _setWindow:inOwningWindow];
    [self setNextResponder:inOwningWindow];

    // Done.
    return self;

}


// Deallocate.
- (void)dealloc {

    // Simple.
    [super dealloc];
    
}


// Sink a method invocation.
- (void)_setFrameNeedsDisplay:(BOOL)needsDisplay {
    
}


// Sink a method invocation.
- (void)_setSheet:(BOOL)sheetFlag {

}


// Sink a method invocation.
- (void)_updateButtonState {

}

#if 0
// Sink a method invocation.
- (void)_windowChangedKeyState {
}
#endif

// Toolbar methods that NSWindow expects to be there.
- (BOOL)_canHaveToolbar { return NO; }
- (BOOL)_toolbarIsInTransition { return NO; }
- (BOOL)_toolbarIsShown { return NO; }
- (BOOL)_toolbarIsHidden { return NO; }
- (void)_showToolbarWithAnimation:(BOOL)animate {}
- (void)_hideToolbarWithAnimation:(BOOL)animate {}
- (float)_distanceFromToolbarBaseToTitlebar { return 0; }


// Refuse to admit there's a close button on the window.
- (NSButton *)closeButton {

    // Simple.
    return nil;
}


// Return what's asked for.
- (unsigned int)styleMask {

    // Simple.
    return _styleMask;

}


// Return what's asked for.
- (NSRect)dragRectForFrameRect:(NSRect)frameRect {

    // Do what NSThemeFrame would do.
    // If we just return NSZeroRect here, _NXMakeWindowVisible() gets all befuddled in the sheet-showing case, a window-moving loop is entered, and the sheet gets moved right off of the screen.  M.P. Warning - 3/23/01
    NSRect dragRect;
    dragRect.size.height = 27;//[NSThemeFrame _titlebarHeight:[self styleMask]];
    dragRect.origin.y = NSMaxY(frameRect) - dragRect.size.height;
    dragRect.size.width = frameRect.size.width;
    dragRect.origin.x = frameRect.origin.x;
    return dragRect;
    
}


// Return what's asked for.
- (BOOL)isOpaque {

    // Return a value that will make -[NSWindow displayIfNeeded] on our Carbon window actually work.
    return YES;

}


// Refuse to admit there's a minimize button on the window.
- (NSButton *)minimizeButton {

    // Simple.
    return nil;

}


// Do the right thing for a Carbon window.
- (void)setTitle:(NSString *)title {

    CarbonWindowAdapter *carbonWindow;
    OSStatus osStatus;
    WindowRef windowRef;

    // Set the Carbon window's title.
    carbonWindow = (CarbonWindowAdapter *)[self window];
    windowRef = [carbonWindow windowRef];
    osStatus = SetWindowTitleWithCFString(windowRef, (CFStringRef)title);
    if (osStatus!=noErr) NSLog(@"A Carbon window's title couldn't be set.");

}


// Return what's asked for.
- (NSString *)title {

    CFStringRef windowTitle;
    CarbonWindowAdapter *carbonWindow;
    NSString *windowTitleAsNSString;
    OSStatus osStatus;
    WindowRef windowRef;

    // Return the Carbon window's title.
    carbonWindow = (CarbonWindowAdapter *)[self window];
    windowRef = [carbonWindow windowRef];
    osStatus = CopyWindowTitleAsCFString(windowRef, &windowTitle);
    if (osStatus==noErr) {
        windowTitleAsNSString = (NSString *)windowTitle;
    } else {
        NSLog(@"A Carbon window's title couldn't be gotten.");
        windowTitleAsNSString = @"";
    }
    return [windowTitleAsNSString autorelease];

}


// Return what's asked for.
- (float)_sheetHeightAdjustment {

    // Do what NSThemeFrame would do.
    return 22;//[NSThemeFrame _titlebarHeight:([self styleMask] & ~NSDocModalWindowMask)];
    
}

// Return what's asked for.
- (float)_maxTitlebarTitleRect {

    // Do what NSThemeFrame would do.
    return 22;//[NSThemeFrame _titlebarHeight:([self styleMask] & ~NSDocModalWindowMask)];
    
}

- (void)_clearDragMargins {
}

- (void)_resetDragMargins {
}


@end // implementation NSCarbonWindowFrame
