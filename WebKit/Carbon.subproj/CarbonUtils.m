/*
 *  CarbonUtils.m
 *  WebKit
 *
 *  Created by Ed Voas on Mon Feb 17 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */
#import <AppKit/NSBitmapImageRep_Private.h>

#include "CarbonUtils.h"


extern CGImageRef _NSCreateImageRef( unsigned char *const bitmapData[5], int pixelsWide, int pixelsHigh, int bitsPerSample, int samplesPerPixel, int bitsPerPixel, int bytesPerRow, BOOL isPlanar, BOOL hasAlpha, NSString *colorSpaceName, CGColorSpaceRef customColorSpace, id sourceObj);

static void				PoolCleaner( EventLoopTimerRef inTimer, EventLoopIdleTimerMessage inState, void *inUserData );

static NSAutoreleasePool*	sPool;
static unsigned numPools;

static unsigned getNumPools()
{
    void *v = NSPushAutoreleasePool(0);
    unsigned numPools = (unsigned)(v);
    NSPopAutoreleasePool (v);
    return numPools;
}


void
WebInitForCarbon()
{
    static bool			sAppKitLoaded;

    if ( !sAppKitLoaded )
    {
        ProcessSerialNumber    process;

        // Force us to register with process server, this ensure that the process
        // "flavour" is correctly established.
        GetCurrentProcess( &process ); 
        NSApplicationLoad();
                
        sPool = [[NSAutoreleasePool allocWithZone:NULL] init];
        numPools = getNumPools();
        
        InstallEventLoopIdleTimer( GetMainEventLoop(), 1.0, 0, PoolCleaner, 0, NULL );
        
        sAppKitLoaded = true;     

        [NSBitmapImageRep _setEnableFlippedImageFix:YES];
    }
}


static void
PoolCleaner( EventLoopTimerRef inTimer, EventLoopIdleTimerMessage inState, void *inUserData )
{
	if ( inState == kEventLoopIdleTimerStarted )
	{
        CFStringRef mode = CFRunLoopCopyCurrentMode( (CFRunLoopRef)GetCFRunLoopFromEventLoop( GetCurrentEventLoop() ));
        if ( CFEqual( mode, kCFRunLoopDefaultMode ) )
        {
            unsigned currentNumPools = getNumPools()-1;            
            if (currentNumPools == numPools){
                [sPool release];
                sPool = [[NSAutoreleasePool allocWithZone:NULL] init];
                numPools = getNumPools();
            }

        }
        CFRelease( mode );
	}
}

CGImageRef
WebConvertNSImageToCGImageRef(
	NSImage*         inImage )
{
	NSArray*				reps = [inImage representations];
	NSBitmapImageRep*		rep = NULL;
	CGImageRef				image = NULL;
	unsigned				i;

	for ( i=0; i < [reps count]; i++ )
	{
        if ( [[reps objectAtIndex:i] isKindOfClass:[NSBitmapImageRep class]] )
        {
            rep = [reps objectAtIndex:i];
            break;
        }
	}
    
	if ( rep )
	{
        //CGColorSpaceRef csync = (CGColorSpaceRef)[rep valueForProperty:NSImageColorSyncProfileData];
        
        unsigned char* planes[5];

        [rep getBitmapDataPlanes:planes];

        image = _NSCreateImageRef( planes, [rep pixelsWide], [rep pixelsHigh],
                    [rep bitsPerSample], [rep samplesPerPixel], [rep bitsPerPixel],
                    [rep bytesPerRow], [rep isPlanar], [rep hasAlpha], [rep colorSpaceName],
                    NULL, rep);
	}
    
	return image;
}

