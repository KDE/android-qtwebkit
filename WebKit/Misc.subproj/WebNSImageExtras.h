//
//  WebNSImageExtras.h
//  WebKit
//
//  Created by Chris Blumenberg on Wed Sep 25 2002.
//  Copyright (c) 2002 Apple Computer Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface NSImage (WebExtras)

- (void)_web_scaleToMaxSize:(NSSize)size;

- (void)_web_dissolveToFraction:(float)delta;

@end
