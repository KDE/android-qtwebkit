/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebDataProtocol.h"

#import <Foundation/NSURLResponse.h>
#import <wtf/Assertions.h>

NSString *WebDataProtocolScheme = @"applewebdata";
static NSString *WebDataRequestPropertyKey = @"WebDataRequest";

@interface WebDataRequestParameters : NSObject <NSCopying>
{
@public
    NSData *data;
    NSString *MIMEType;
    NSString *encoding;
    NSURL *baseURL;
    NSURL *unreachableURL;
}
@end

@implementation WebDataRequestParameters

-(id)copyWithZone:(NSZone *)zone
{
    WebDataRequestParameters *newInstance = [[WebDataRequestParameters allocWithZone:zone] init];
    newInstance->data = [data copyWithZone:zone];
    newInstance->encoding = [encoding copyWithZone:zone];
    newInstance->baseURL = [baseURL copyWithZone:zone];
    newInstance->unreachableURL = [unreachableURL copyWithZone:zone];
    newInstance->MIMEType = [MIMEType copyWithZone:zone];
    return newInstance;
}

- (void)dealloc
{
    [data release];
    [MIMEType release];
    [encoding release];
    [baseURL release];
    [unreachableURL release];
    [super dealloc];
}

@end

@implementation NSURL (WebDataURL)

+ (NSURL *)_web_uniqueWebDataURL
{
    static BOOL registered;
    
    if (!registered) {
        [NSURLProtocol registerClass:[WebDataProtocol class]];
        registered = YES;
    }
    
    CFUUIDRef UUIDRef = CFUUIDCreate(kCFAllocatorDefault);
    NSString *UUIDString = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, UUIDRef);
    CFRelease(UUIDRef);
    NSURL *URL = [NSURL URLWithString:[NSString stringWithFormat:@"%@://%@", WebDataProtocolScheme, UUIDString]];
    CFRelease(UUIDString);
    return URL;
}

+ (NSURL *)_web_uniqueWebDataURLWithRelativeString:(NSString *)string
{
    return [NSURL URLWithString:string relativeToURL:[self _web_uniqueWebDataURL]];
}

@end


@implementation NSURLRequest (WebDataRequest)

+ (NSString *)_webDataRequestPropertyKey
{
    return WebDataRequestPropertyKey;
}

- (WebDataRequestParameters *)_webDataRequestParametersForReading
{
    return [NSURLProtocol propertyForKey:WebDataRequestPropertyKey inRequest:self];
}

- (NSData *)_webDataRequestData
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    return parameters ? parameters->data : nil;
}

- (NSString *)_webDataRequestEncoding
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    return parameters ? parameters->encoding : nil;
}

- (NSString *)_webDataRequestMIMEType
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    return parameters ? parameters->MIMEType : nil;
}

- (NSURL *)_webDataRequestBaseURL
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    return parameters ? parameters->baseURL : nil;
}

- (NSURL *)_webDataRequestUnreachableURL
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    return parameters ? parameters->unreachableURL : nil;
}

- (NSURL *)_webDataRequestExternalURL
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    if (!parameters) {
        return nil;
    }
    
    if (parameters->baseURL) {
        return parameters->baseURL;
    }
    
    return [NSURL URLWithString:@"about:blank"];
}

- (NSMutableURLRequest *)_webDataRequestExternalRequest
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForReading];
    NSMutableURLRequest *newRequest = nil;
    
    if (parameters){
        newRequest = [[self mutableCopyWithZone:[self zone]] autorelease];
        [newRequest setURL:[self _webDataRequestExternalURL]];
    } 
    return newRequest;
}

@end

@implementation NSMutableURLRequest (WebDataRequest)

- (WebDataRequestParameters *)_webDataRequestParametersForWriting
{
    WebDataRequestParameters *result = [NSURLProtocol propertyForKey:WebDataRequestPropertyKey inRequest:self];
    if (!result) {
        result = [[WebDataRequestParameters alloc] init];
        [NSURLProtocol setProperty:result forKey:WebDataRequestPropertyKey inRequest:self];
        [result release];
    }
    return result;
}

- (void)_webDataRequestSetData:(NSData *)data
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForWriting];
    [parameters->data release];
    parameters->data = [data retain];
}

- (void)_webDataRequestSetEncoding:(NSString *)encoding
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForWriting];
    [parameters->encoding release];
    parameters->encoding = [encoding retain];
}

- (void)_webDataRequestSetMIMEType:(NSString *)type
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForWriting];
    [parameters->MIMEType release];
    parameters->MIMEType = [type retain];
}

- (void)_webDataRequestSetBaseURL:(NSURL *)baseURL
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForWriting];
    [parameters->baseURL release];
    parameters->baseURL = [baseURL retain];
}

- (void)_webDataRequestSetUnreachableURL:(NSURL *)unreachableURL
{
    WebDataRequestParameters *parameters = [self _webDataRequestParametersForWriting];
    [parameters->unreachableURL release];
    parameters->unreachableURL = [unreachableURL retain];
}

@end



// Implement the required methods for the concrete subclass of WebProtocolHandler
// that will handle our custom protocol.
@implementation WebDataProtocol

static BOOL isCaseInsensitiveEqual(NSString *a, NSString *b)
{
    return [a caseInsensitiveCompare:b] == NSOrderedSame;
}

+(BOOL)_webIsDataProtocolURL:(NSURL *)URL
{
    ASSERT(URL);
    NSString *scheme = [URL scheme];
    return scheme && isCaseInsensitiveEqual(scheme, WebDataProtocolScheme);
}

+(BOOL)canInitWithRequest:(NSURLRequest *)request
{
    NSURL *URL = [request URL];
    ASSERT(request);
    ASSERT(URL);
    return [self _webIsDataProtocolURL:URL];
}

+(NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    return request;
}

- (void)startLoading
{
    id<NSURLProtocolClient> client = [self client];
    NSURLRequest *request = [self request];
    NSData *data = [request _webDataRequestData];

    if (data) {
        NSURLResponse *response = [[NSURLResponse alloc] initWithURL:[request URL] MIMEType:[request _webDataRequestMIMEType] expectedContentLength:[data length] textEncodingName:[request _webDataRequestEncoding]];
        [client URLProtocol:self didReceiveResponse:response cacheStoragePolicy:NSURLCacheStorageNotAllowed];
        [client URLProtocol:self didLoadData:data];
        [client URLProtocolDidFinishLoading:self];
        [response release];
    } else {
        NSError *error = [[[NSError alloc] initWithDomain:NSURLErrorDomain code:NSURLErrorResourceUnavailable userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
            [request URL], @"NSErrorFailingURLKey",
            [[request URL] absoluteString], @"NSErrorFailingURLStringKey",
            nil, NSLocalizedDescriptionKey,
            nil]] autorelease];
        
        [client URLProtocol:self didFailWithError:error];
    }
}

- (void)stopLoading
{
}

@end

