//
//  WebPluginPackage.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebPluginPackage.h>

#import <WebKit/WebKitLogging.h>

#import <Foundation/NSPrivateDecls.h>

NSString *WebPlugInBaseURLKey = @"WebPlugInBaseURLKey";
NSString *WebPlugInAttributesKey = @"WebPlugInAttributesKey";
NSString *WebPlugInContainerKey = @"WebPlugInContainerKey";

/*!
	@constant WebPlugInContainingElementKey The DOMElement that was used to specify
	the plug-in.  May be nil.
*/
extern NSString *WebPlugInContainingElementKey;

@implementation WebPluginPackage

- initWithPath:(NSString *)pluginPath
{
    [super initWithPath:pluginPath];

    if (!bundle) {
        [self release];
        return nil;
    }
    
    UInt32 type = 0;
    CFBundleGetPackageInfo([bundle _cfBundle], &type, NULL);
    
    if (type != FOUR_CHAR_CODE('WBPL')) {
        [self release];
        return nil;
    }

    if (![self getPluginInfoFromBundleAndMIMEDictionary:nil]) {
        [self release];
        return nil;
    }

    return self;
}

- (Class)viewFactory
{
    return [bundle principalClass];
}

- (BOOL)load
{
#if !LOG_DISABLED
    BOOL wasLoaded = [self isLoaded];
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
#endif
    
    [bundle principalClass];

#if !LOG_DISABLED
    if (!wasLoaded) {
        CFAbsoluteTime duration = CFAbsoluteTimeGetCurrent() - start;
        LOG(Plugins, "principalClass took %f seconds for: %@", duration, [self name]);
    }
#endif
   
    return YES;
}

- (void)unload
{
}

- (BOOL)isLoaded
{
    return [bundle isLoaded];
}

@end

@implementation NSObject (WebScripting)

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    return YES;
}

+ (BOOL)isKeyExcludedFromWebScript:(const char *)name
{
    return YES;
}

@end

