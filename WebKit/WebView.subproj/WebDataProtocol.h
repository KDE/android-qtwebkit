/*
    WebDataProtocol.h
    Copyright 2003, Apple, Inc. All rights reserved.

    Private header file.
*/

#import <Foundation/Foundation.h>

#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLProtocol.h>


@interface WebDataProtocol : NSURLProtocol
{
}
+ (BOOL)_webIsDataProtocolURL:(NSURL *)URL;
@end

@interface NSURLRequest (WebDataRequest)
+ (NSURL *)_webDataRequestURLForData:(NSData *)data;
- (NSURL *)_webDataRequestBaseURL;
- (NSURL *)_webDataRequestUnreachableURL;
- (NSURL *)_webDataRequestExternalURL;
- (NSData *)_webDataRequestData;
- (NSString *)_webDataRequestEncoding;
- (NSString *)_webDataRequestMIMEType;
- (NSMutableURLRequest *)_webDataRequestExternalRequest;
@end

@interface NSMutableURLRequest (WebDataRequest)
- (void)_webDataRequestSetData:(NSData *)data;
- (void)_webDataRequestSetEncoding:(NSString *)encoding;
- (void)_webDataRequestSetMIMEType:(NSString *)MIMEType;
- (void)_webDataRequestSetBaseURL:(NSURL *)baseURL;
- (void)_webDataRequestSetUnreachableURL:(NSURL *)unreachableURL;
@end

