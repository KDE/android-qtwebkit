/*	
    WebImageRepresentation.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebImageRepresentation.h"

#import <WebKit/WebDataSource.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebLocalizableStrings.h>

#import <WebCore/WebCoreImageRenderer.h>

#import <Foundation/NSURLRequest.h>

@implementation WebImageRepresentation

- (void)dealloc
{
    [image release];
    [URL release];
    [super dealloc];
}

- (WebImageRenderer *)image
{
    return image;
}

- (NSURL *)URL
{
    return URL;
}

- (BOOL)doneLoading
{
    return doneLoading;
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    URL = [[[dataSource request] URL] retain];
    image = [[[WebImageRendererFactory sharedFactory] imageRendererWithMIMEType:[[dataSource response] MIMEType]] retain];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    NSData *allData = [dataSource data];
    [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:NO];
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
    NSData *allData = [dataSource data];
    if ([allData length] > 0) {
        [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:YES];
    }
    doneLoading = YES;
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    NSData *allData = [dataSource data];
    [image incrementalLoadWithBytes:[allData bytes] length:[allData length] complete:YES];
    doneLoading = YES;
}

- (BOOL)canProvideDocumentSource
{
    return NO;
}

- (NSString *)documentSource
{
    return nil;
}

- (NSString *)title
{
    NSString *lastPathComponent = [[URL path] lastPathComponent];
    NSSize size = [image size];
    if (!NSEqualSizes(size, NSZeroSize)) {
        return [NSString stringWithFormat:UI_STRING("%@ %dx%d pixels", "window title for a standalone image"), lastPathComponent, (int)size.width, (int)size.height];
    }
    return lastPathComponent;
}

@end
