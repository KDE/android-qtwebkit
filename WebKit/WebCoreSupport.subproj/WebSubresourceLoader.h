/*	
    WebSubresourceClient.h
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/
#import <Foundation/Foundation.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>

@class WebDataSource;
@class NSURLResponse;

@protocol WebCoreResourceHandle;
@protocol WebCoreResourceLoader;

@interface WebSubresourceClient : WebBaseResourceHandleDelegate <WebCoreResourceHandle>
{
    id <WebCoreResourceLoader> loader;
}

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
    withURL:(NSURL *)URL referrer:(NSString *)referrer forDataSource:(WebDataSource *)source;

+ (WebSubresourceClient *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
    withURL:(NSURL *)URL postData:(NSData *)data referrer:(NSString *)referrer forDataSource:(WebDataSource *)source;

@end
