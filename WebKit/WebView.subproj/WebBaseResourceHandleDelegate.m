/*	
    WebBaseResourceHandleDelegate.m
    Copyright (c) 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebBaseResourceHandleDelegate.h>

#import <Foundation/NSURLAuthenticationChallenge.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLConnectionPrivate.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLRequestPrivate.h>
#import <Foundation/NSURLResponse.h>
#import <Foundation/NSURLResponsePrivate.h>
#import <WebKit/WebAssertions.h>
#import <Foundation/NSError_NSURLExtras.h>

#import <WebKit/WebDataProtocol.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultResourceLoadDelegate.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebViewPrivate.h>

@interface WebBaseResourceHandleDelegate (WebNSURLAuthenticationChallengeSender) <NSURLAuthenticationChallengeSender>
@end

@implementation WebBaseResourceHandleDelegate (WebNSURLAuthenticationChallengeSender) 

- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (challenge == nil || challenge != currentWebChallenge) {
	return;
    }

    [[currentConnectionChallenge sender] useCredential:credential forAuthenticationChallenge:currentConnectionChallenge];

    [currentConnectionChallenge release];
    currentConnectionChallenge = nil;
    
    [currentWebChallenge release];
    currentWebChallenge = nil;
}

- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (challenge == nil || challenge != currentWebChallenge) {
	return;
    }

    [[currentConnectionChallenge sender] continueWithoutCredentialForAuthenticationChallenge:currentConnectionChallenge];

    [currentConnectionChallenge release];
    currentConnectionChallenge = nil;
    
    [currentWebChallenge release];
    currentWebChallenge = nil;
}

- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (challenge == nil || challenge != currentWebChallenge) {
	return;
    }

    [self cancel];
}

@end

@implementation WebBaseResourceHandleDelegate

- (void)releaseResources
{
    ASSERT(!reachedTerminalState);
    
    // It's possible that when we release the handle, it will be
    // deallocated and release the last reference to this object.
    // We need to retain to avoid accessing the object after it
    // has been deallocated and also to avoid reentering this method.
    
    [self retain];
    
    [identifier release];
    identifier = nil;

    [connection release];
    connection = nil;

    [webView release];
    webView = nil;
    
    [dataSource release];
    dataSource = nil;
    
    [resourceLoadDelegate release];
    resourceLoadDelegate = nil;

    [downloadDelegate release];
    downloadDelegate = nil;
    
    reachedTerminalState = YES;
    
    [self release];
}

- (void)dealloc
{
    ASSERT(reachedTerminalState);
    [request release];
    [response release];
    [super dealloc];
}

- (BOOL)loadWithRequest:(NSURLRequest *)r
{
    ASSERT(connection == nil);
    
    r = [self connection:connection willSendRequest:r redirectResponse:nil];
    connection = [[NSURLConnection alloc] initWithRequest:r delegate:self];
    if (defersCallbacks) {
        [connection setDefersCallbacks:YES];
    }

    return YES;
}

- (void)setDefersCallbacks:(BOOL)defers
{
    defersCallbacks = defers;
    [connection setDefersCallbacks:defers];
}

- (BOOL)defersCallbacks
{
    return defersCallbacks;
}

- (void)setDataSource:(WebDataSource *)d
{
    ASSERT(d);
    ASSERT([d _webView]);
    
    [d retain];
    [dataSource release];
    dataSource = d;

    [webView release];
    webView = [[dataSource _webView] retain];
    
    [resourceLoadDelegate release];
    resourceLoadDelegate = [[webView resourceLoadDelegate] retain];
    implementations = [webView _resourceLoadDelegateImplementations];

    [downloadDelegate release];
    downloadDelegate = [[webView downloadDelegate] retain];
}

- (WebDataSource *)dataSource
{
    return dataSource;
}

- resourceLoadDelegate
{
    return resourceLoadDelegate;
}

- downloadDelegate
{
    return downloadDelegate;
}

- (NSURLRequest *)connection:(NSURLConnection *)con willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);
    NSMutableURLRequest *mutableRequest = [newRequest mutableCopy];
    NSURLRequest *clientRequest, *updatedRequest;
    BOOL haveDataSchemeRequest = NO;
    
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];

    [mutableRequest setHTTPUserAgent:[webView userAgentForURL:[newRequest URL]]];
    newRequest = [mutableRequest autorelease];

    clientRequest = [newRequest _webDataRequestExternalRequest];
    if(!clientRequest)
        clientRequest = newRequest;
    else
        haveDataSchemeRequest = YES;
    
    if (identifier == nil) {
        // The identifier is released after the last callback, rather than in dealloc
        // to avoid potential cycles.
        if (implementations.delegateImplementsIdentifierForRequest)
            identifier = [[resourceLoadDelegate webView: webView identifierForInitialRequest:clientRequest fromDataSource:dataSource] retain];
        else
            identifier = [[[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView identifierForInitialRequest:clientRequest fromDataSource:dataSource] retain];
    }

    // If we have a special "applewebdata" scheme URL we send a fake request to the delegate.
    if (implementations.delegateImplementsWillSendRequest)
        updatedRequest = [resourceLoadDelegate webView:webView resource:identifier willSendRequest:clientRequest redirectResponse:redirectResponse fromDataSource:dataSource];
    else
        updatedRequest = [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier willSendRequest:clientRequest redirectResponse:redirectResponse fromDataSource:dataSource];
        
    if (!haveDataSchemeRequest)
        newRequest = updatedRequest;
    else {
        // If the delegate modified the request use that instead of
        // our applewebdata request, otherwise use the original
        // applewebdata request.
        if (![updatedRequest isEqual:clientRequest])
            newRequest = updatedRequest;
    }

    // Store a copy of the request.
    [request autorelease];

    // Client may return a nil request, indicating that the request should be aborted.
    if (newRequest){
        request = [newRequest copy];
    }
    else {
        request = nil;
    }

    [self release];
    return request;
}

- (void)connection:(NSURLConnection *)con didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);

    ASSERT(!currentConnectionChallenge);
    ASSERT(!currentWebChallenge);

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    currentConnectionChallenge = [challenge retain];;
    currentWebChallenge = [[NSURLAuthenticationChallenge alloc] initWithAuthenticationChallenge:challenge sender:self];

    if (implementations.delegateImplementsDidReceiveAuthenticationChallenge) {
        [resourceLoadDelegate webView:webView resource:identifier didReceiveAuthenticationChallenge:currentWebChallenge fromDataSource:dataSource];
    } else {
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveAuthenticationChallenge:currentWebChallenge fromDataSource:dataSource];
    }
    [self release];
}

-(void)connection:(NSURLConnection *)con didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);

    ASSERT(currentConnectionChallenge);
    ASSERT(currentWebChallenge);
    ASSERT(currentConnectionChallenge = challenge);

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    if (implementations.delegateImplementsDidCancelAuthenticationChallenge) {
        [resourceLoadDelegate webView:webView resource:identifier didCancelAuthenticationChallenge:currentWebChallenge fromDataSource:dataSource];
    } else {
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didCancelAuthenticationChallenge:currentWebChallenge fromDataSource:dataSource];
    }
    [self release];
}

- (void)connection:(NSURLConnection *)con didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain]; 

    // If the URL is one of our whacky applewebdata URLs that
    // fake up a substitute URL to present to the delegate.
    if([WebDataProtocol _webIsDataProtocolURL:[r URL]]) {
	NSURL *baseURL = [request _webDataRequestBaseURL];
        if (baseURL == nil) {
            baseURL = [NSURL URLWithString:@"about:blank"];
	}
        r = [[[NSURLResponse alloc] initWithURL:baseURL MIMEType:[r MIMEType] expectedContentLength:[r expectedContentLength] textEncodingName:[r textEncodingName]] autorelease];
    }

    [r retain];
    [response release];
    response = r;

    [dataSource _addResponse: r];

    [webView _incrementProgressForConnection:con response:r];
        
    if (implementations.delegateImplementsDidReceiveResponse)
        [resourceLoadDelegate webView:webView resource:identifier didReceiveResponse:r fromDataSource:dataSource];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveResponse:r fromDataSource:dataSource];
    [self release];
}

- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data
{
    [self connection:con didReceiveData:data lengthReceived:[data length]];
}

- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    // The following assertions are not quite valid here, since a subclass
    // might override didReceiveData: in a way that invalidates them. This
    // happens with the steps listed in 3266216
    // ASSERT(con == connection);
    // ASSERT(!reachedTerminalState);

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [webView _incrementProgressForConnection:con data:data];

    if (implementations.delegateImplementsDidReceiveContentLength)
        [resourceLoadDelegate webView:webView resource:identifier didReceiveContentLength:lengthReceived fromDataSource:dataSource];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveContentLength:lengthReceived fromDataSource:dataSource];
    [self release];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)con
{
    // If load has been cancelled after finishing (which could happen with a 
    // javascript that changes the window location), do nothing.
    if (cancelledFlag) {
        return;
    }
    
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);

    [webView _completeProgressForConnection:con];

    if (implementations.delegateImplementsDidFinishLoadingFromDataSource)
        [resourceLoadDelegate webView:webView resource:identifier didFinishLoadingFromDataSource:dataSource];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didFinishLoadingFromDataSource:dataSource];

    [self releaseResources];
}

- (void)connection:(NSURLConnection *)con didFailWithError:(NSError *)result
{
    ASSERT(con == connection);
    ASSERT(!reachedTerminalState);

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [webView _completeProgressForConnection:con];

    [[webView _resourceLoadDelegateForwarder] webView:webView resource:identifier didFailLoadingWithError:result fromDataSource:dataSource];

    [self releaseResources];
    [self release];
}

- (void)cancelWithError:(NSError *)error
{
    ASSERT(!reachedTerminalState);

    // This flag prevents bad behvior when loads that finish cause the
    // load itself to be cancelled (which could happen with a javascript that 
    // changes the window location). This is used to prevent both the body
    // of this method and the body of connectionDidFinishLoading: running
    // for a single delegate. Cancelling wins.
    cancelledFlag = YES;
    
    [currentConnectionChallenge release];
    currentConnectionChallenge = nil;
    
    [currentWebChallenge release];
    currentWebChallenge = nil;

    [connection cancel];

    [webView _completeProgressForConnection:connection];

    if (error) {
        [[webView _resourceLoadDelegateForwarder] webView:webView resource:identifier didFailLoadingWithError:error fromDataSource:dataSource];
    }

    [self releaseResources];
}

- (void)cancel
{
    if (!reachedTerminalState) {
        [self cancelWithError:[self cancelledError]];
    }
}

- (NSError *)cancelledError
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain
                                    code:NSURLErrorCancelled
                                     URL:[request URL]];
}

- (void)setIdentifier: ident
{
    if (identifier != ident){
        [identifier release];
        identifier = [ident retain];
    }
}

- (NSURLResponse *)response
{
    return response;
}

@end
