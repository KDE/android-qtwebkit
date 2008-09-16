/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#if ENABLE(NETSCAPE_PLUGIN_API)
#import "WebBaseNetscapePluginStream.h"

#import "WebBaseNetscapePluginView.h"
#import "WebFrameInternal.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNetscapePluginPackage.h"
#import "WebNetscapePlugInStreamLoaderClient.h"
#import <Foundation/NSURLResponse.h>
#import <kjs/JSLock.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebKitSystemInterface.h>
#import <wtf/HashMap.h>

using namespace WebCore;

#define WEB_REASON_NONE -1

static NSString *CarbonPathFromPOSIXPath(NSString *posixPath);

typedef HashMap<NPStream*, NPP> StreamMap;
static StreamMap& streams()
{
    static StreamMap staticStreams;
    return staticStreams;
}

@implementation WebBaseNetscapePluginStream

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

+ (NPP)ownerForStream:(NPStream *)stream
{
    return streams().get(stream);
}

+ (NPReason)reasonForError:(NSError *)error
{
    if (error == nil) {
        return NPRES_DONE;
    }
    if ([[error domain] isEqualToString:NSURLErrorDomain] && [error code] == NSURLErrorCancelled) {
        return NPRES_USER_BREAK;
    }
    return NPRES_NETWORK_ERR;
}

- (NSError *)_pluginCancelledConnectionError
{
    return [[[NSError alloc] _initWithPluginErrorCode:WebKitErrorPlugInCancelledConnection
                                           contentURL:_impl->m_responseURL ? _impl->m_responseURL.get() : _impl->m_requestURL.get()
                                        pluginPageURL:nil
                                           pluginName:[[pluginView pluginPackage] name]
                                             MIMEType:_impl->m_mimeType.get()] autorelease];
}

- (NSError *)errorForReason:(NPReason)theReason
{
    if (theReason == NPRES_DONE) {
        return nil;
    }
    if (theReason == NPRES_USER_BREAK) {
        return [NSError _webKitErrorWithDomain:NSURLErrorDomain
                                          code:NSURLErrorCancelled 
                                           URL:_impl->m_responseURL ? _impl->m_responseURL.get() : _impl->m_requestURL.get()];
    }
    return [self _pluginCancelledConnectionError];
}

- (id)initWithFrameLoader:(FrameLoader *)frameLoader
{
    [super init];
    
    _impl = WebNetscapePluginStream::create().releaseRef();
    _frameLoader = frameLoader;
    
    return self;
}

- (id)initWithRequest:(NSURLRequest *)theRequest
               plugin:(NPP)thePlugin
           notifyData:(void *)theNotifyData 
     sendNotification:(BOOL)flag
{   
    WebBaseNetscapePluginView *view = (WebBaseNetscapePluginView *)thePlugin->ndata;
    
    if (!core([view webFrame])->loader()->canLoad([theRequest URL], String(), core([view webFrame])->document()))
        return nil;
    
    if ([self initWithRequestURL:[theRequest URL]
                          plugin:thePlugin
                      notifyData:theNotifyData
                sendNotification:flag] == nil) {
        return nil;
    }
    
    // Temporarily set isTerminated to YES to avoid assertion failure in dealloc in case we are released in this method.
    isTerminated = YES;
    
    request = [theRequest mutableCopy];
    if (core([view webFrame])->loader()->shouldHideReferrer([theRequest URL], core([view webFrame])->loader()->outgoingReferrer()))
        [(NSMutableURLRequest *)request _web_setHTTPReferrer:nil];
    
    _client = new WebNetscapePlugInStreamLoaderClient(self);
    _loader = NetscapePlugInStreamLoader::create(core([view webFrame]), _client).releaseRef();
    _loader->setShouldBufferData(false);
    
    isTerminated = NO;
    
    return self;
}

- (id)initWithRequestURL:(NSURL *)theRequestURL
                  plugin:(NPP)thePlugin
              notifyData:(void *)theNotifyData
        sendNotification:(BOOL)flag
{
    [super init];
 
    _impl = WebNetscapePluginStream::create().releaseRef();

    // Temporarily set isTerminated to YES to avoid assertion failure in dealloc in case we are released in this method.
    isTerminated = YES;

    if (theRequestURL == nil || thePlugin == NULL) {
        [self release];
        return nil;
    }
    
    [self setRequestURL:theRequestURL];
    [self setPlugin:thePlugin];
    _impl->m_notifyData = theNotifyData;
    _impl->m_sendNotification = flag;
    _impl->m_fileDescriptor = -1;
    newStreamSuccessful = NO;

    streams().add(&_impl->m_stream, thePlugin);
    
    isTerminated = NO;
    
    return self;
}

- (void)dealloc
{
    ASSERT(!_impl->m_plugin);
    ASSERT(isTerminated);
    ASSERT(_impl->m_stream.ndata == nil);

    // The stream file should have been deleted, and the path freed, in -_destroyStream
    ASSERT(!_impl->m_path);
    ASSERT(_impl->m_fileDescriptor == -1);

    ASSERT(_impl);
    _impl->deref();
    
    if (_loader)
        _loader->deref();
    delete _client;
    [request release];
    
    [pluginView release];
    
    free((void *)_impl->m_stream.url);
    free(headers);

    streams().remove(&_impl->m_stream);

    [super dealloc];
}

- (void)finalize
{
    ASSERT_MAIN_THREAD();
    ASSERT(isTerminated);
    ASSERT(_impl->m_stream.ndata == nil);

    // The stream file should have been deleted, and the path freed, in -_destroyStream
    ASSERT(!_impl->m_path);
    ASSERT(_impl->m_fileDescriptor == -1);

    ASSERT(_impl);
    _impl->deref();

    if (_loader)
        _loader->deref();
    delete _client;
    
    free((void *)_impl->m_stream.url);
    free(headers);

    streams().remove(&_impl->m_stream);

    [super finalize];
}

- (uint16)transferMode
{
    return _impl->m_transferMode;
}

- (NPP)plugin
{
    return _impl->m_plugin;
}

- (void)setRequestURL:(NSURL *)theRequestURL
{
    _impl->m_requestURL = theRequestURL;
}

- (void)setResponseURL:(NSURL *)theResponseURL
{
    _impl->m_responseURL = theResponseURL;
}

- (void)setPlugin:(NPP)thePlugin
{
    if (thePlugin) {
        _impl->m_plugin = thePlugin;
        pluginView = [(WebBaseNetscapePluginView *)_impl->m_plugin->ndata retain];
        WebNetscapePluginPackage *pluginPackage = [pluginView pluginPackage];
        
        pluginFuncs = [pluginPackage pluginFuncs];
    } else {
        WebBaseNetscapePluginView *view = pluginView;

        _impl->m_plugin = 0;
        pluginView = nil;

        pluginFuncs = NULL;

        [view disconnectStream:self];
        [view release];
    }
}

- (void)setMIMEType:(NSString *)theMIMEType
{
    _impl->m_mimeType = theMIMEType;
}

- (void)startStreamResponseURL:(NSURL *)URL
         expectedContentLength:(long long)expectedContentLength
              lastModifiedDate:(NSDate *)lastModifiedDate
                      MIMEType:(NSString *)theMIMEType
                       headers:(NSData *)theHeaders
{
    ASSERT(!isTerminated);
    
    [self setResponseURL:URL];
    [self setMIMEType:theMIMEType];
    
    free((void *)_impl->m_stream.url);
    _impl->m_stream.url = strdup([_impl->m_responseURL.get() _web_URLCString]);

    _impl->m_stream.ndata = self;
    _impl->m_stream.end = expectedContentLength > 0 ? (uint32)expectedContentLength : 0;
    _impl->m_stream.lastmodified = (uint32)[lastModifiedDate timeIntervalSince1970];
    _impl->m_stream.notifyData = _impl->m_notifyData;

    if (theHeaders) {
        unsigned len = [theHeaders length];
        headers = (char*) malloc(len + 1);
        [theHeaders getBytes:headers];
        headers[len] = 0;
        _impl->m_stream.headers = headers;
    }
    
    _impl->m_transferMode = NP_NORMAL;
    _impl->m_offset = 0;
    reason = WEB_REASON_NONE;
    // FIXME: If WebNetscapePluginStream called our initializer we wouldn't have to do this here.
    _impl->m_fileDescriptor = -1;

    // FIXME: Need a way to check if stream is seekable

    WebBaseNetscapePluginView *pv = pluginView;
    [pv willCallPlugInFunction];
    NPError npErr = pluginFuncs->newstream(_impl->m_plugin, (char *)[_impl->m_mimeType.get() UTF8String], &_impl->m_stream, NO, &_impl->m_transferMode);
    [pv didCallPlugInFunction];
    LOG(Plugins, "NPP_NewStream URL=%@ MIME=%@ error=%d", _impl->m_responseURL.get(), _impl->m_mimeType.get(), npErr);

    if (npErr != NPERR_NO_ERROR) {
        LOG_ERROR("NPP_NewStream failed with error: %d responseURL: %@", npErr, _impl->m_responseURL.get());
        // Calling cancelLoadWithError: cancels the load, but doesn't call NPP_DestroyStream.
        [self cancelLoadWithError:[self _pluginCancelledConnectionError]];
        return;
    }

    newStreamSuccessful = YES;

    switch (_impl->m_transferMode) {
        case NP_NORMAL:
            LOG(Plugins, "Stream type: NP_NORMAL");
            break;
        case NP_ASFILEONLY:
            LOG(Plugins, "Stream type: NP_ASFILEONLY");
            break;
        case NP_ASFILE:
            LOG(Plugins, "Stream type: NP_ASFILE");
            break;
        case NP_SEEK:
            LOG_ERROR("Stream type: NP_SEEK not yet supported");
            [self cancelLoadAndDestroyStreamWithError:[self _pluginCancelledConnectionError]];
            break;
        default:
            LOG_ERROR("unknown stream type");
    }
}

- (void)start
{
    ASSERT(request);
    ASSERT(!_frameLoader);
    
    _loader->documentLoader()->addPlugInStreamLoader(_loader);
    _loader->load(request);    
}

- (void)stop
{
    ASSERT(!_frameLoader);
    
    if (!_loader->isDone())
        [self cancelLoadAndDestroyStreamWithError:_loader->cancelledError()];    
}

- (void)startStreamWithResponse:(NSURLResponse *)r
{
    NSMutableData *theHeaders = nil;
    long long expectedContentLength = [r expectedContentLength];

    if ([r isKindOfClass:[NSHTTPURLResponse class]]) {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)r;
        theHeaders = [NSMutableData dataWithCapacity:1024];
        
        // FIXME: it would be nice to be able to get the raw HTTP header block.
        // This includes the HTTP version, the real status text,
        // all headers in their original order and including duplicates,
        // and all original bytes verbatim, rather than sent through Unicode translation.
        // Unfortunately NSHTTPURLResponse doesn't provide access at that low a level.
        
        [theHeaders appendBytes:"HTTP " length:5];
        char statusStr[10];
        long statusCode = [httpResponse statusCode];
        snprintf(statusStr, sizeof(statusStr), "%ld", statusCode);
        [theHeaders appendBytes:statusStr length:strlen(statusStr)];
        [theHeaders appendBytes:" OK\n" length:4];

        // HACK: pass the headers through as UTF-8.
        // This is not the intended behavior; we're supposed to pass original bytes verbatim.
        // But we don't have the original bytes, we have NSStrings built by the URL loading system.
        // It hopefully shouldn't matter, since RFC2616/RFC822 require ASCII-only headers,
        // but surely someone out there is using non-ASCII characters, and hopefully UTF-8 is adequate here.
        // It seems better than NSASCIIStringEncoding, which will lose information if non-ASCII is used.

        NSDictionary *headerDict = [httpResponse allHeaderFields];
        NSArray *keys = [[headerDict allKeys] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        NSEnumerator *i = [keys objectEnumerator];
        NSString *k;
        while ((k = [i nextObject]) != nil) {
            NSString *v = [headerDict objectForKey:k];
            [theHeaders appendData:[k dataUsingEncoding:NSUTF8StringEncoding]];
            [theHeaders appendBytes:": " length:2];
            [theHeaders appendData:[v dataUsingEncoding:NSUTF8StringEncoding]];
            [theHeaders appendBytes:"\n" length:1];
        }

        // If the content is encoded (most likely compressed), then don't send its length to the plugin,
        // which is only interested in the decoded length, not yet known at the moment.
        // <rdar://problem/4470599> tracks a request for -[NSURLResponse expectedContentLength] to incorporate this logic.
        NSString *contentEncoding = (NSString *)[[(NSHTTPURLResponse *)r allHeaderFields] objectForKey:@"Content-Encoding"];
        if (contentEncoding && ![contentEncoding isEqualToString:@"identity"])
            expectedContentLength = -1;

        // startStreamResponseURL:... will null-terminate.
    }

    [self startStreamResponseURL:[r URL]
           expectedContentLength:expectedContentLength
                lastModifiedDate:WKGetNSURLResponseLastModifiedDate(r)
                        MIMEType:[r MIMEType]
                         headers:theHeaders];
}

- (BOOL)wantsAllStreams
{
    if (!pluginFuncs->getvalue)
        return NO;
    
    void *value = 0;
    NPError error;
    WebBaseNetscapePluginView *pv = pluginView;
    [pv willCallPlugInFunction];
    {
        JSC::JSLock::DropAllLocks dropAllLocks(false);
        error = pluginFuncs->getvalue(_impl->m_plugin, NPPVpluginWantsAllNetworkStreams, &value);
    }
    [pv didCallPlugInFunction];
    if (error != NPERR_NO_ERROR)
        return NO;
    
    return value != 0;
}

- (void)_destroyStream
{
    if (isTerminated)
        return;

    [self retain];

    ASSERT(reason != WEB_REASON_NONE);
    ASSERT([_impl->m_deliveryData.get() length] == 0);
    
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_deliverData) object:nil];

    if (_impl->m_stream.ndata != nil) {
        if (reason == NPRES_DONE && (_impl->m_transferMode == NP_ASFILE || _impl->m_transferMode == NP_ASFILEONLY)) {
            ASSERT(_impl->m_fileDescriptor == -1);
            ASSERT(_impl->m_path);
            NSString *carbonPath = CarbonPathFromPOSIXPath(_impl->m_path.get());
            ASSERT(carbonPath != NULL);
            WebBaseNetscapePluginView *pv = pluginView;
            [pv willCallPlugInFunction];
            pluginFuncs->asfile(_impl->m_plugin, &_impl->m_stream, [carbonPath fileSystemRepresentation]);
            [pv didCallPlugInFunction];
            LOG(Plugins, "NPP_StreamAsFile responseURL=%@ path=%s", _impl->m_responseURL.get(), carbonPath);
        }

        if (_impl->m_path) {
            // Delete the file after calling NPP_StreamAsFile(), instead of in -dealloc/-finalize.  It should be OK
            // to delete the file here -- NPP_StreamAsFile() is always called immediately before NPP_DestroyStream()
            // (the stream destruction function), so there can be no expectation that a plugin will read the stream
            // file asynchronously after NPP_StreamAsFile() is called.
            unlink([_impl->m_path.get() fileSystemRepresentation]);
            _impl->m_path = 0;

            if (isTerminated)
                goto exit;
        }

        if (_impl->m_fileDescriptor != -1) {
            // The file may still be open if we are destroying the stream before it completed loading.
            close(_impl->m_fileDescriptor);
            _impl->m_fileDescriptor = -1;
        }

        if (newStreamSuccessful) {
            NPError npErr;
            WebBaseNetscapePluginView *pv = pluginView;
            [pv willCallPlugInFunction];
            npErr = pluginFuncs->destroystream(_impl->m_plugin, &_impl->m_stream, reason);
            [pv didCallPlugInFunction];
            LOG(Plugins, "NPP_DestroyStream responseURL=%@ error=%d", _impl->m_responseURL.get(), npErr);
        }

        free(headers);
        headers = NULL;
        _impl->m_stream.headers = NULL;

        _impl->m_stream.ndata = nil;

        if (isTerminated)
            goto exit;
    }

    if (_impl->m_sendNotification) {
        // NPP_URLNotify expects the request URL, not the response URL.
        WebBaseNetscapePluginView *pv = pluginView;
        [pv willCallPlugInFunction];
        pluginFuncs->urlnotify(_impl->m_plugin, [_impl->m_requestURL.get() _web_URLCString], reason, _impl->m_notifyData);
        [pv didCallPlugInFunction];
        LOG(Plugins, "NPP_URLNotify requestURL=%@ reason=%d", _impl->m_requestURL.get(), reason);
    }

    isTerminated = YES;

    [self setPlugin:NULL];

exit:
    [self release];
}

- (void)_destroyStreamWithReason:(NPReason)theReason
{
    reason = theReason;
    if (reason != NPRES_DONE) {
        // Stop any pending data from being streamed.
        [_impl->m_deliveryData.get() setLength:0];
    } else if ([_impl->m_deliveryData.get() length] > 0) {
        // There is more data to be streamed, don't destroy the stream now.
        return;
    }
    [self _destroyStream];
    ASSERT(_impl->m_stream.ndata == nil);
}

- (void)cancelLoadWithError:(NSError *)error
{
    if (_frameLoader) {
        ASSERT(!_loader);
        
        DocumentLoader* documentLoader = _frameLoader->activeDocumentLoader();
        ASSERT(documentLoader);
        
        if (documentLoader->isLoadingMainResource())
            documentLoader->cancelMainResourceLoad(error);
        return;
    }
    
    if (!_loader->isDone())
        _loader->cancel(error);
}

- (void)destroyStreamWithError:(NSError *)error
{
    [self _destroyStreamWithReason:[[self class] reasonForError:error]];
}

- (void)cancelLoadAndDestroyStreamWithError:(NSError *)error
{
    [self retain];
    [self cancelLoadWithError:error];
    [self destroyStreamWithError:error];
    [self setPlugin:NULL];
    [self release];
}

- (void)_deliverData
{
    if (!_impl->m_stream.ndata || [_impl->m_deliveryData.get() length] == 0)
        return;

    [self retain];

    int32 totalBytes = [_impl->m_deliveryData.get() length];
    int32 totalBytesDelivered = 0;

    while (totalBytesDelivered < totalBytes) {
        WebBaseNetscapePluginView *pv = pluginView;
        [pv willCallPlugInFunction];
        int32 deliveryBytes = pluginFuncs->writeready(_impl->m_plugin, &_impl->m_stream);
        [pv didCallPlugInFunction];
        LOG(Plugins, "NPP_WriteReady responseURL=%@ bytes=%d", _impl->m_responseURL.get(), deliveryBytes);

        if (isTerminated)
            goto exit;

        if (deliveryBytes <= 0) {
            // Plug-in can't receive anymore data right now. Send it later.
            [self performSelector:@selector(_deliverData) withObject:nil afterDelay:0];
            break;
        } else {
            deliveryBytes = MIN(deliveryBytes, totalBytes - totalBytesDelivered);
            NSData *subdata = [_impl->m_deliveryData.get() subdataWithRange:NSMakeRange(totalBytesDelivered, deliveryBytes)];
            pv = pluginView;
            [pv willCallPlugInFunction];
            deliveryBytes = pluginFuncs->write(_impl->m_plugin, &_impl->m_stream, _impl->m_offset, [subdata length], (void *)[subdata bytes]);
            [pv didCallPlugInFunction];
            if (deliveryBytes < 0) {
                // Netscape documentation says that a negative result from NPP_Write means cancel the load.
                [self cancelLoadAndDestroyStreamWithError:[self _pluginCancelledConnectionError]];
                return;
            }
            deliveryBytes = MIN((unsigned)deliveryBytes, [subdata length]);
            _impl->m_offset += deliveryBytes;
            totalBytesDelivered += deliveryBytes;
            LOG(Plugins, "NPP_Write responseURL=%@ bytes=%d total-delivered=%d/%d", _impl->m_responseURL.get(), deliveryBytes, _impl->m_offset, _impl->m_stream.end);
        }
    }

    if (totalBytesDelivered > 0) {
        if (totalBytesDelivered < totalBytes) {
            NSMutableData *newDeliveryData = [[NSMutableData alloc] initWithCapacity:totalBytes - totalBytesDelivered];
            [newDeliveryData appendBytes:(char *)[_impl->m_deliveryData.get() bytes] + totalBytesDelivered length:totalBytes - totalBytesDelivered];
            [_impl->m_deliveryData.get() release];
            _impl->m_deliveryData = newDeliveryData;
            [newDeliveryData release];
        } else {
            [_impl->m_deliveryData.get() setLength:0];
            if (reason != WEB_REASON_NONE) {
                [self _destroyStream];
            }
        }
    }

exit:
    [self release];
}

- (void)_deliverDataToFile:(NSData *)data
{
    if (_impl->m_fileDescriptor == -1 && !_impl->m_path) {
        NSString *temporaryFileMask = [NSTemporaryDirectory() stringByAppendingPathComponent:@"WebKitPlugInStreamXXXXXX"];
        char *temporaryFileName = strdup([temporaryFileMask fileSystemRepresentation]);
        _impl->m_fileDescriptor = mkstemp(temporaryFileName);
        if (_impl->m_fileDescriptor == -1) {
            LOG_ERROR("Can't create a temporary file.");
            // This is not a network error, but the only error codes are "network error" and "user break".
            [self _destroyStreamWithReason:NPRES_NETWORK_ERR];
            free(temporaryFileName);
            return;
        }

        _impl->m_path.adoptNS([[NSString stringWithUTF8String:temporaryFileName] retain]);
        free(temporaryFileName);
    }

    int dataLength = [data length];
    if (!dataLength)
        return;

    int byteCount = write(_impl->m_fileDescriptor, [data bytes], dataLength);
    if (byteCount != dataLength) {
        // This happens only rarely, when we are out of disk space or have a disk I/O error.
        LOG_ERROR("error writing to temporary file, errno %d", errno);
        close(_impl->m_fileDescriptor);
        _impl->m_fileDescriptor = -1;

        // This is not a network error, but the only error codes are "network error" and "user break".
        [self _destroyStreamWithReason:NPRES_NETWORK_ERR];
        _impl->m_path = 0;
    }
}

- (void)finishedLoading
{
    if (!_impl->m_stream.ndata)
        return;

    if (_impl->m_transferMode == NP_ASFILE || _impl->m_transferMode == NP_ASFILEONLY) {
        // Fake the delivery of an empty data to ensure that the file has been created
        [self _deliverDataToFile:[NSData data]];
        if (_impl->m_fileDescriptor != -1)
            close(_impl->m_fileDescriptor);
        _impl->m_fileDescriptor = -1;
    }

    [self _destroyStreamWithReason:NPRES_DONE];
}

- (void)receivedData:(NSData *)data
{
    ASSERT([data length] > 0);
    
    if (_impl->m_transferMode != NP_ASFILEONLY) {
        if (!_impl->m_deliveryData)
            _impl->m_deliveryData.adoptNS([[NSMutableData alloc] initWithCapacity:[data length]]);
        [_impl->m_deliveryData.get() appendData:data];
        [self _deliverData];
    }
    if (_impl->m_transferMode == NP_ASFILE || _impl->m_transferMode == NP_ASFILEONLY)
        [self _deliverDataToFile:data];

}

@end

static NSString *CarbonPathFromPOSIXPath(NSString *posixPath)
{
    // Doesn't add a trailing colon for directories; this is a problem for paths to a volume,
    // so this function would need to be revised if we ever wanted to call it with that.

    CFURLRef url = (CFURLRef)[NSURL fileURLWithPath:posixPath];
    if (!url)
        return nil;

    return WebCFAutorelease(CFURLCopyFileSystemPath(url, kCFURLHFSPathStyle));
}

#endif
