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

#import <WebKit/WebNetscapePluginStream.h>

#import <Foundation/NSURLConnection.h>
#import <WebCore/FrameMac.h>
#import <WebCore/WebFrameLoader.h>
#import <WebCore/WebNetscapePlugInStreamLoader.h>
#import <WebKit/WebDataSourceInternal.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebFrameInternal.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebViewInternal.h>
#import <wtf/PassRefPtr.h>

using namespace WebCore;

@implementation WebNetscapePluginStream

- initWithRequest:(NSURLRequest *)theRequest
    pluginPointer:(NPP)thePluginPointer
       notifyData:(void *)theNotifyData 
 sendNotification:(BOOL)flag
{   
    WebBaseNetscapePluginView *view = (WebBaseNetscapePluginView *)thePluginPointer->ndata;

    WebFrameBridge *bridge = [[view webFrame] _bridge];
    BOOL hideReferrer;
    if (![bridge canLoadURL:[theRequest URL] fromReferrer:[bridge referrer] hideReferrer:&hideReferrer])
        return nil;

    if ([self initWithRequestURL:[theRequest URL]
                    pluginPointer:thePluginPointer
                       notifyData:theNotifyData
                 sendNotification:flag] == nil) {
        return nil;
    }
    
    // Temporarily set isTerminated to YES to avoid assertion failure in dealloc in case we are released in this method.
    isTerminated = YES;
    
    request = [theRequest mutableCopy];
    if (hideReferrer)
        [(NSMutableURLRequest *)request _web_setHTTPReferrer:nil];

    _loader = NetscapePlugInStreamLoader::create([bridge _frame], self).release();
    
    isTerminated = NO;

    return self;
}

- (void)dealloc
{
    if (_loader)
        _loader->deref();
    [request release];
    [super dealloc];
}

- (void)finalize
{
    if (_loader)
        _loader->deref();
    [super finalize];
}

- (void)start
{
    ASSERT(request);

    _loader->frameLoader()->addPlugInStreamLoader(_loader);
    if (!_loader->load(request))
        _loader->frameLoader()->removePlugInStreamLoader(_loader);
}

- (void)cancelLoadWithError:(NSError *)error
{
    if (!_loader->isDone())
        _loader->cancel(error);
}

- (void)stop
{
    if (!_loader->isDone())
        [self cancelLoadAndDestroyStreamWithError:_loader->cancelledError()];
}

@end

