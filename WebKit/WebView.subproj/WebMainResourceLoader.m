/*	
    WebMainResourceClient.m
    Copyright (c) 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebMainResourceClient.h>

#import <Foundation/NSHTTPCookie.h>
#import <Foundation/NSError_NSURLExtras.h>
#import <Foundation/NSString_NSURLExtras.h>
#import <Foundation/NSURLFileTypeMappings.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLConnectionPrivate.h>
#import <Foundation/NSURLDownloadPrivate.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLRequestPrivate.h>
#import <Foundation/NSURLResponse.h>
#import <Foundation/NSURLResponsePrivate.h>

#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDownload.h>
#import <WebKit/WebFrameLoadDelegate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPolicyDelegatePrivate.h>
#import <WebKit/WebViewPrivate.h>

// FIXME: More that is in common with WebSubresourceClient should move up into WebBaseResourceHandleDelegate.

@implementation WebMainResourceClient

- initWithDataSource:(WebDataSource *)ds
{
    self = [super init];
    
    if (self) {
        [self setDataSource:ds];
        proxy = [[NSURLConnectionDelegateProxy alloc] init];
        [proxy setDelegate:self];
    }

    return self;
}

- (void)dealloc
{
    [_initialRequest release];

    [proxy setDelegate:nil];
    [proxy release];
    
    [super dealloc];
}

- (void)receivedError:(NSError *)error
{
    // Calling _receivedError will likely result in a call to release, so we must retain.
    [self retain];
    [dataSource _receivedError:error complete:YES];
    [super connection:connection didFailWithError:error];
    [self release];
}

- (void)cancelContentPolicy
{
    [listener _invalidate];
    [listener release];
    listener = nil;
    [policyResponse release];
    policyResponse = nil;
}

-(void)cancelWithError:(NSError *)error
{
    // Calling _receivedError will likely result in a call to release, so we must retain.
    [self retain];
    [self cancelContentPolicy];
    [dataSource _receivedError:error complete:YES];
    [super cancelWithError:error];
    [self release];
}

- (NSError *)interruptForPolicyChangeError
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorFrameLoadInterruptedByPolicyChange URL:[request URL]];
}

-(void)stopLoadingForPolicyChange
{
    [self retain];
    [self cancelWithError:[self interruptForPolicyChangeError]];
    [self release];
}

-(void)continueAfterNavigationPolicy:(NSURLRequest *)_request formState:(WebFormState *)state
{
    [[dataSource _webView] setDefersCallbacks:NO];
    if (!_request) {
	[self stopLoadingForPolicyChange];
    }
}

- (BOOL)_isPostOrRedirectAfterPost:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    BOOL result = NO;
    
    if ([[newRequest HTTPMethod] isEqualToString:@"POST"]) {
        result = YES;
    }
    else if (redirectResponse && [redirectResponse isKindOfClass:[NSHTTPURLResponse class]]) {
        int status = [(NSHTTPURLResponse *)redirectResponse statusCode];
        if (((status >= 301 && status <= 303) || status == 307)
            && [[[dataSource initialRequest] HTTPMethod] isEqualToString:@"POST"]) {
            result = YES;
        }
    }
    
    return result;
}

- (void)addData:(NSData *)data
{
    // Override. We don't want to save the main resource as a subresource of the data source.
    [dataSource _receivedData:data];
}

- (void)saveResource
{
    // Override. We don't want to save the main resource as a subresource of the data source.
}

- (void)saveResourceWithCachedResponse:(NSCachedURLResponse *)cachedResponse
{
    // Override. We don't want to save the main resource as a subresource of the data source.
    // Replace the data on the data source with the cache copy to save memory.
    [dataSource _setData:[cachedResponse data]];
}

- (NSURLRequest *)connection:(NSURLConnection *)con willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(newRequest != nil);

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];

    NSURL *URL = [newRequest URL];

    LOG(Redirect, "URL = %@", URL);

    NSMutableURLRequest *mutableRequest = nil;
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if ([dataSource webFrame] == [[dataSource _webView] mainFrame]) {
        mutableRequest = [newRequest mutableCopy];
        [mutableRequest setMainDocumentURL:URL];
    }

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if ([newRequest cachePolicy] == NSURLRequestUseProtocolCachePolicy && 
        [self _isPostOrRedirectAfterPost:newRequest redirectResponse:redirectResponse]) {
        if (!mutableRequest) {
            mutableRequest = [newRequest mutableCopy];
        }
        [mutableRequest setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    }
    if (mutableRequest) {
        newRequest = [mutableRequest autorelease];
    }

    // note super will make a copy for us, so reassigning newRequest is important
    newRequest = [super connection:con willSendRequest:newRequest redirectResponse:redirectResponse];

    // Don't set this on the first request.  It is set
    // when the main load was started.
    [dataSource _setRequest:newRequest];
    
    [[dataSource _webView] setDefersCallbacks:YES];
    [[dataSource webFrame] _checkNavigationPolicyForRequest:newRequest
                                                 dataSource:dataSource
                                                  formState:nil
                                                    andCall:self
                                               withSelector:@selector(continueAfterNavigationPolicy:formState:)];

    [self release];
    return newRequest;
}

-(void)continueAfterContentPolicy:(WebPolicyAction)contentPolicy response:(NSURLResponse *)r
{
    [[dataSource _webView] setDefersCallbacks:NO];

    switch (contentPolicy) {
    case WebPolicyUse:
	if (![WebView canShowMIMEType:[r MIMEType]]) {
	    [[dataSource webFrame] _handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotShowMIMEType forURL:[[dataSource request] URL]];
	    [self stopLoadingForPolicyChange];
	    return;
	}
        break;

    case WebPolicyDownload:
        [proxy setDelegate:nil];
        [WebDownload _downloadWithLoadingConnection:connection
                                            request:request
                                           response:r
                                           delegate:[self downloadDelegate]
                                              proxy:proxy];
        [proxy release];
        proxy = nil;
        [self receivedError:[self interruptForPolicyChangeError]];
        return;

    case WebPolicyIgnore:
	[self stopLoadingForPolicyChange];
	return;
    
    default:
	ASSERT_NOT_REACHED();
    }

    [self retain];

    [super connection:connection didReceiveResponse:r];

    if (![dataSource _isStopping]
            && ([[request URL] _webkit_shouldLoadAsEmptyDocument]
            	|| [WebView _representationExistsForURLScheme:[[request URL] scheme]])) {
        [self connectionDidFinishLoading:connection];
    }
    
    [self release];
}

-(void)continueAfterContentPolicy:(WebPolicyAction)policy
{
    NSURLResponse *r = [policyResponse retain];
    BOOL isStopping = [dataSource _isStopping];

    [self cancelContentPolicy];
    if (!isStopping){
        [self continueAfterContentPolicy:policy response:r];
    }
    [r release];
}

-(void)checkContentPolicyForResponse:(NSURLResponse *)r
{
    listener = [[WebPolicyDecisionListener alloc]
		   _initWithTarget:self action:@selector(continueAfterContentPolicy:)];
    policyResponse = [r retain];

    WebView *wv = [dataSource _webView];
    [wv setDefersCallbacks:YES];
    [[wv _policyDelegateForwarder] webView:wv decidePolicyForMIMEType:[r MIMEType]
                                                            request:[dataSource request]
                                                              frame:[dataSource webFrame]
                                                   decisionListener:listener];
}


- (void)connection:(NSURLConnection *)con didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(![con defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _webView] defersCallbacks]);

    LOG(Loading, "main content type: %@", [r MIMEType]);

    // FIXME: This is a workaround to make web archive files work with Foundations that
    // are too old to know about web archive files. We should remove this before we ship.
    NSURL *URL = [r URL];
    if ([[[URL path] pathExtension] _web_isCaseInsensitiveEqualToString:@"webarchive"]) {
        r = [[[NSURLResponse alloc] initWithURL:URL 
                                       MIMEType:@"application/x-webarchive"
                          expectedContentLength:[r expectedContentLength] 
                               textEncodingName:[r textEncodingName]] autorelease];
    }
    
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [dataSource _setResponse:r];
    _contentLength = [r expectedContentLength];

    [self checkContentPolicyForResponse:r];
    [self release];
}

- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    ASSERT(data);
    ASSERT([data length] != 0);
    ASSERT(![connection defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _webView] defersCallbacks]);
 
    LOG(Loading, "URL = %@, data = %p, length %d", [dataSource _URL], data, [data length]);

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [[dataSource _webView] _mainReceivedBytesSoFar:[[dataSource data] length]
                                       fromDataSource:dataSource
                                             complete:NO];

    [super connection:con didReceiveData:data lengthReceived:lengthReceived];
    _bytesReceived += [data length];

    LOG(Loading, "%d of %d", _bytesReceived, _contentLength);
    [self release];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)con
{
    ASSERT(![con defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _webView] defersCallbacks]);

    LOG(Loading, "URL = %@", [dataSource _URL]);
        
    // Calls in this method will most likely result in a call to release, so we must retain.
    [self retain];

    [dataSource _finishedLoading];
    [[dataSource _webView] _mainReceivedBytesSoFar:[[dataSource data] length]
                                    fromDataSource:dataSource
                                            complete:YES];
    [super connectionDidFinishLoading:con];

    [self release];
}

- (void)connection:(NSURLConnection *)con didFailWithError:(NSError *)error
{
    ASSERT(![con defersCallbacks]);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _webView] defersCallbacks]);

    LOG(Loading, "URL = %@, error = %@", [error _web_failingURL], [error _web_localizedDescription]);

    [self receivedError:error];
}

- (void)loadWithRequestNow:(NSURLRequest *)r
{
    ASSERT(connection == nil);
    ASSERT(![self defersCallbacks]);
    ASSERT(![[dataSource _webView] defersCallbacks]);
    
    // Send this synthetic delegate callback since clients expect it, and
    // we no longer send the callback from within NSURLConnection for
    // initial requests.
    r = [self connection:nil willSendRequest:r redirectResponse:nil];

    NSURL *URL = [r URL];
    BOOL shouldLoadEmpty = [URL _webkit_shouldLoadAsEmptyDocument];
    if (shouldLoadEmpty || [WebView _representationExistsForURLScheme:[URL scheme]]) {
        NSString *MIMEType;
        if (shouldLoadEmpty) {
            MIMEType = @"text/html";
        } else {
            MIMEType = [WebView _generatedMIMETypeForURLScheme:[URL scheme]];
        }

        NSURLResponse *resp = [[NSURLResponse alloc] initWithURL:URL MIMEType:MIMEType
            expectedContentLength:0 textEncodingName:nil];
	[self connection:nil didReceiveResponse:resp];
	[resp release];
    } else {
        connection = [[NSURLConnection alloc] initWithRequest:r delegate:proxy];
    }
}

- (BOOL)loadWithRequest:(NSURLRequest *)r
{
    ASSERT(connection == nil);
    
    if (![self defersCallbacks]) {
        [self loadWithRequestNow:r];
    } else {
        NSURLRequest *copy = [r copy];
        [_initialRequest release];
        _initialRequest = copy;
    }

    return YES;
}

- (void)setDefersCallbacks:(BOOL)defers
{
    [super setDefersCallbacks:defers];
    if (!defers) {
        NSURLRequest *r = _initialRequest;
        if (r != nil) {
            _initialRequest = nil;
            [self loadWithRequestNow:r];
            [r release];
        }
    }
}

@end
