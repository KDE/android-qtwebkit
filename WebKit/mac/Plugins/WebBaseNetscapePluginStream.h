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

#if ENABLE(NETSCAPE_PLUGIN_API)
#import <Foundation/Foundation.h>

#import <WebKit/npfunctions.h>
#import <WebKit/WebPlugInStreamLoaderDelegate.h>
#import <wtf/RefCounted.h>
#import <wtf/PassRefPtr.h>
#import <wtf/RetainPtr.h>

namespace WebCore {
    class FrameLoader;
    class NetscapePlugInStreamLoader;
}

class WebNetscapePlugInStreamLoaderClient;

@class WebBaseNetscapePluginView;
@class NSURLResponse;

class WebNetscapePluginStream : public RefCounted<WebNetscapePluginStream>
{
public:
    static PassRefPtr<WebNetscapePluginStream> create()
    {
        return adoptRef(new WebNetscapePluginStream);
    }
    
    // FIXME: These should all be private once WebBaseNetscapePluginStream is history...
public:
    RetainPtr<NSMutableData> m_deliveryData;
    RetainPtr<NSURL> m_requestURL;
    RetainPtr<NSURL> m_responseURL;
    RetainPtr<NSString> m_mimeType;

    NPP m_plugin;
    uint16 m_transferMode;
    int32 m_offset;
    NPStream m_stream;
    RetainPtr<NSString> m_path;
    int m_fileDescriptor;
    BOOL m_sendNotification;
    void *m_notifyData;
    
private:
    WebNetscapePluginStream()
        : m_plugin(0)
        , m_transferMode(0)
        , m_offset(0)
        , m_fileDescriptor(-1)
        , m_sendNotification(false)
        , m_notifyData(0)
    {
        memset(&m_stream, 0, sizeof(NPStream));
    }
};

@interface WebBaseNetscapePluginStream : NSObject<WebPlugInStreamLoaderDelegate>
{    
    char *headers;
    WebBaseNetscapePluginView *pluginView;
    NPReason reason;
    BOOL isTerminated;
    BOOL newStreamSuccessful;
 
    WebCore::FrameLoader* _frameLoader;
    WebCore::NetscapePlugInStreamLoader* _loader;
    WebNetscapePlugInStreamLoaderClient* _client;
    NSURLRequest *request;

    NPPluginFuncs *pluginFuncs;
    
    WebNetscapePluginStream *_impl;
}

+ (NPP)ownerForStream:(NPStream *)stream;
+ (NPReason)reasonForError:(NSError *)error;

- (NSError *)errorForReason:(NPReason)theReason;

- (id)initWithFrameLoader:(WebCore::FrameLoader *)frameLoader;

- (id)initWithRequest:(NSURLRequest *)theRequest
               plugin:(NPP)thePlugin
           notifyData:(void *)theNotifyData
     sendNotification:(BOOL)sendNotification;

- (id)initWithRequestURL:(NSURL *)theRequestURL
                  plugin:(NPP)thePlugin
              notifyData:(void *)theNotifyData
        sendNotification:(BOOL)flag;

- (void)setRequestURL:(NSURL *)theRequestURL;
- (void)setResponseURL:(NSURL *)theResponseURL;
- (void)setPlugin:(NPP)thePlugin;

- (uint16)transferMode;
- (NPP)plugin;

- (void)startStreamResponseURL:(NSURL *)theResponseURL
         expectedContentLength:(long long)expectedContentLength
              lastModifiedDate:(NSDate *)lastModifiedDate
                      MIMEType:(NSString *)MIMEType
                       headers:(NSData *)theHeaders;

- (void)cancelLoadWithError:(NSError *)error;

- (void)start;
- (void)stop;

@end
#endif
