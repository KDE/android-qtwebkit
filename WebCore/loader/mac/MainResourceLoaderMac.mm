/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "MainResourceLoader.h"

#import "FrameLoader.h"
#import "PlatformString.h"
#import "WebCoreSystemInterface.h"
#import "WebDataProtocol.h"
#import <Foundation/NSHTTPCookie.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <wtf/Assertions.h>
#import <wtf/PassRefPtr.h>
#import <wtf/Vector.h>

// FIXME: More that is in common with SubresourceLoader should move up into ResourceLoader.

using namespace WebCore;

namespace WebCore {

const size_t URLBufferLength = 2048;

MainResourceLoader::MainResourceLoader(Frame* frame)
    : ResourceLoader(frame)
    , m_proxy(wkCreateNSURLConnectionDelegateProxy())
    , m_loadingMultipartContent(false)
    , m_waitingForContentPolicy(false)
{
    [m_proxy.get() setDelegate:delegate()];
    [m_proxy.get() release];
}

MainResourceLoader::~MainResourceLoader()
{
}

void MainResourceLoader::releaseDelegate()
{
    [m_proxy.get() setDelegate:nil];
    ResourceLoader::releaseDelegate();
}

PassRefPtr<MainResourceLoader> MainResourceLoader::create(Frame* frame)
{
    return new MainResourceLoader(frame);
}

void MainResourceLoader::receivedError(NSError *error)
{
    // Calling receivedMainResourceError will likely result in the last reference to this object to go away.
    RefPtr<MainResourceLoader> protect(this);

    if (!cancelled()) {
        ASSERT(!reachedTerminalState());
        frameLoader()->didFailToLoad(this, error);
    }

    frameLoader()->receivedMainResourceError(error, true);

    if (!cancelled())
        releaseResources();

    ASSERT(reachedTerminalState());
}

void MainResourceLoader::didCancel(NSError *error)
{
    // Calling receivedMainResourceError will likely result in the last reference to this object to go away.
    RefPtr<MainResourceLoader> protect(this);

    if (m_waitingForContentPolicy) {
        frameLoader()->cancelContentPolicyCheck();
        ASSERT(m_waitingForContentPolicy);
        m_waitingForContentPolicy = false;
        deref(); // balances ref in didReceiveResponse
    }
    frameLoader()->receivedMainResourceError(error, true);
    ResourceLoader::didCancel(error);
}

NSError *MainResourceLoader::interruptionForPolicyChangeError() const
{
    return frameLoader()->interruptionForPolicyChangeError(request());
}

void MainResourceLoader::stopLoadingForPolicyChange()
{
    cancel(interruptionForPolicyChangeError());
}

void MainResourceLoader::callContinueAfterNavigationPolicy(void* argument, NSURLRequest *request, PassRefPtr<FormState>)
{
    static_cast<MainResourceLoader*>(argument)->continueAfterNavigationPolicy(request);
}

void MainResourceLoader::continueAfterNavigationPolicy(NSURLRequest *request)
{
    if (!request)
        stopLoadingForPolicyChange();
    deref(); // balances ref in willSendRequest
}

bool MainResourceLoader::isPostOrRedirectAfterPost(NSURLRequest *newRequest, NSURLResponse *redirectResponse)
{
    if ([[newRequest HTTPMethod] isEqualToString:@"POST"])
        return true;

    if (redirectResponse && [redirectResponse isKindOfClass:[NSHTTPURLResponse class]]) {
        int status = [(NSHTTPURLResponse *)redirectResponse statusCode];
        if (((status >= 301 && status <= 303) || status == 307)
                && [[frameLoader()->initialRequest() HTTPMethod] isEqualToString:@"POST"])
            return true;
    }
    
    return false;
}

void MainResourceLoader::addData(NSData *data, bool allAtOnce)
{
    ResourceLoader::addData(data, allAtOnce);
    frameLoader()->receivedData(data);
}

NSURLRequest *MainResourceLoader::willSendRequest(NSURLRequest *newRequest, NSURLResponse *redirectResponse)
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(newRequest != nil);

    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    NSURL *URL = [newRequest URL];

    NSMutableURLRequest *mutableRequest = nil;
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if (frameLoader()->isLoadingMainFrame()) {
        mutableRequest = [newRequest mutableCopy];
        [mutableRequest setMainDocumentURL:URL];
    }

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if ([newRequest cachePolicy] == NSURLRequestUseProtocolCachePolicy && isPostOrRedirectAfterPost(newRequest, redirectResponse)) {
        if (!mutableRequest)
            mutableRequest = [newRequest mutableCopy];
        [mutableRequest setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    }
    if (mutableRequest)
        newRequest = [mutableRequest autorelease];

    // Note super will make a copy for us, so reassigning newRequest is important. Since we are returning this value, but
    // it's only guaranteed to be retained by self, and self might be dealloc'ed in this method, we have to autorelease.
    // See 3777253 for an example.
    newRequest = [[ResourceLoader::willSendRequest(newRequest, redirectResponse) retain] autorelease];

    // Don't set this on the first request. It is set when the main load was started.
    frameLoader()->setRequest(newRequest);

    ref(); // balanced by deref in continueAfterNavigationPolicy
    frameLoader()->checkNavigationPolicy(newRequest, callContinueAfterNavigationPolicy, this);

    return newRequest;
}

static bool shouldLoadAsEmptyDocument(NSURL *url)
{
    Vector<UInt8, URLBufferLength> buffer(URLBufferLength);
    CFIndex bytesFilled = CFURLGetBytes((CFURLRef)url, buffer.data(), URLBufferLength);
    if (bytesFilled == -1) {
        CFIndex bytesToAllocate = CFURLGetBytes((CFURLRef)url, NULL, 0);
        buffer.resize(bytesToAllocate);
        bytesFilled = CFURLGetBytes((CFURLRef)url, buffer.data(), bytesToAllocate);
        ASSERT(bytesFilled == bytesToAllocate);
    }
    
    return (bytesFilled == 0) || (bytesFilled > 5 && strncmp((char *)buffer.data(), "about:", 6) == 0);
}

void MainResourceLoader::continueAfterContentPolicy(PolicyAction contentPolicy, NSURLResponse *r)
{
    NSURL *URL = [request() URL];
    String MIMEType = [r MIMEType]; 
    
    switch (contentPolicy) {
    case PolicyUse: {
        // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks (4120255).
        bool isRemote = ![URL isFileURL] && ![WebDataProtocol _webIsDataProtocolURL:URL];
        bool isRemoteWebArchive = isRemote && equalIgnoringCase("application/x-webarchive", MIMEType);
        if (!frameLoader()->canShowMIMEType(MIMEType) || isRemoteWebArchive) {
            frameLoader()->cannotShowMIMEType(r);
            // Check reachedTerminalState since the load may have already been cancelled inside of _handleUnimplementablePolicyWithErrorCode::.
            if (!reachedTerminalState())
                stopLoadingForPolicyChange();
            return;
        }
        break;
    }

    case PolicyDownload:
        [m_proxy.get() setDelegate:nil];
        frameLoader()->download(connection(), request(), r, m_proxy.get());
        m_proxy = nil;
        receivedError(interruptionForPolicyChangeError());
        return;

    case PolicyIgnore:
        stopLoadingForPolicyChange();
        return;
    
    default:
        ASSERT_NOT_REACHED();
    }

    RefPtr<MainResourceLoader> protect(this);

    if ([r isKindOfClass:[NSHTTPURLResponse class]]) {
        int status = [(NSHTTPURLResponse *)r statusCode];
        if (status < 200 || status >= 300) {
            bool hostedByObject = frameLoader()->isHostedByObjectElement();

            frameLoader()->handleFallbackContent();
            // object elements are no longer rendered after we fallback, so don't
            // keep trying to process data from their load

            if (hostedByObject)
                cancel();
        }
    }

    // we may have cancelled this load as part of switching to fallback content
    if (!reachedTerminalState())
        ResourceLoader::didReceiveResponse(r);

    if (frameLoader() && !frameLoader()->isStopping() && (shouldLoadAsEmptyDocument(URL) || frameLoader()->representationExistsForURLScheme([URL scheme])))
        didFinishLoading();
}

void MainResourceLoader::callContinueAfterContentPolicy(void* argument, PolicyAction policy)
{
    static_cast<MainResourceLoader*>(argument)->continueAfterContentPolicy(policy);
}

void MainResourceLoader::continueAfterContentPolicy(PolicyAction policy)
{
    ASSERT(m_waitingForContentPolicy);
    m_waitingForContentPolicy = false;
    if (!frameLoader()->isStopping())
        continueAfterContentPolicy(policy, m_response.get());
    deref(); // balances ref in didReceiveResponse
}

void MainResourceLoader::didReceiveResponse(NSURLResponse *r)
{
    ASSERT(shouldLoadAsEmptyDocument([r URL]) || !defersLoading());

    if (m_loadingMultipartContent) {
        frameLoader()->setupForReplaceByMIMEType([r MIMEType]);
        clearResourceData();
    }
    
    if ([[r MIMEType] isEqualToString:@"multipart/x-mixed-replace"])
        m_loadingMultipartContent = true;
        
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    frameLoader()->setResponse(r);

    m_response = r;

    ASSERT(!m_waitingForContentPolicy);
    m_waitingForContentPolicy = true;
    ref(); // balanced by deref in continueAfterContentPolicy and didCancel
    frameLoader()->checkContentPolicy([m_response.get() MIMEType], callContinueAfterContentPolicy, this);
}

void MainResourceLoader::didReceiveData(NSData *data, long long lengthReceived, bool allAtOnce)
{
    ASSERT(data);
    ASSERT([data length] != 0);
    ASSERT(!defersLoading());
 
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    ResourceLoader::didReceiveData(data, lengthReceived, allAtOnce);
}

void MainResourceLoader::didFinishLoading()
{
    ASSERT(shouldLoadAsEmptyDocument(frameLoader()->URL()) || !defersLoading());

    // The additional processing can do anything including possibly removing the last
    // reference to this object.
    RefPtr<MainResourceLoader> protect(this);

    frameLoader()->finishedLoading();
    ResourceLoader::didFinishLoading();
}

void MainResourceLoader::didFail(NSError *error)
{
    ASSERT(!defersLoading());

    receivedError(error);
}

NSURLRequest *MainResourceLoader::loadNow(NSURLRequest *r)
{
    bool shouldLoadEmptyBeforeRedirect = shouldLoadAsEmptyDocument([r URL]);

    ASSERT(!connection());
    ASSERT(shouldLoadEmptyBeforeRedirect || !defersLoading());

    // Send this synthetic delegate callback since clients expect it, and
    // we no longer send the callback from within NSURLConnection for
    // initial requests.
    r = willSendRequest(r, nil);
    NSURL *URL = [r URL];
    bool shouldLoadEmpty = shouldLoadAsEmptyDocument(URL);

    if (shouldLoadEmptyBeforeRedirect && !shouldLoadEmpty && defersLoading())
        return r;

    if (shouldLoadEmpty || frameLoader()->representationExistsForURLScheme([URL scheme])) {
        NSString *MIMEType;
        if (shouldLoadEmpty)
            MIMEType = @"text/html";
        else
            MIMEType = frameLoader()->generatedMIMETypeForURLScheme([URL scheme]);

        NSURLResponse *resp = [[NSURLResponse alloc] initWithURL:URL MIMEType:MIMEType
            expectedContentLength:0 textEncodingName:nil];
        didReceiveResponse(resp);
        [resp release];
    } else {
        NSURLConnection *conn = [[NSURLConnection alloc] initWithRequest:r delegate:m_proxy.get()];
        m_connection = conn;
        [conn release];
    }

    return nil;
}

bool MainResourceLoader::load(NSURLRequest *r)
{
    ASSERT(!connection());

    bool defer = defersLoading();
    if (defer) {
        bool shouldLoadEmpty = shouldLoadAsEmptyDocument([r URL]);
        if (shouldLoadEmpty)
            defer = false;
    }
    if (!defer) {
        r = loadNow(r);
        if (r) {
            // Started as an empty document, but was redirected to something non-empty.
            ASSERT(defersLoading());
            defer = true;
        }
    }
    if (defer) {
        NSURLRequest *copy = [r copy];
        m_initialRequest = copy;
        [copy release];
    }

    return true;
}

void MainResourceLoader::setDefersLoading(bool defers)
{
    ResourceLoader::setDefersLoading(defers);
    if (!defers) {
        RetainPtr<NSURLRequest> r = m_initialRequest;
        if (r) {
            m_initialRequest = nil;
            loadNow(r.get());
        }
    }
}

}
