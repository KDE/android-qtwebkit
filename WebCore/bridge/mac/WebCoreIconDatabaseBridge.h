/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifdef __cplusplus
namespace WebCore { 
class IconDatabase; 
class Image;
class String;
} 
typedef WebCore::IconDatabase WebCoreIconDatabase;
#else
@class WebCoreIconDatabase;
#endif
@class WebCoreIconDatabaseBridge;

@interface WebCoreIconDatabaseBridge : NSObject
{
    WebCoreIconDatabaseBridge *_sharedInstance;
    WebCoreIconDatabase *_iconDB;
}
- (BOOL)openSharedDatabaseWithPath:(NSString *)path;
- (void)closeSharedDatabase;
- (BOOL)isOpen;

- (NSImage *)iconForPageURL:(NSString *)url withSize:(NSSize)size;
- (NSString *)iconURLForPageURL:(NSString *)url;
- (NSImage *)defaultIconWithSize:(NSSize)size;
- (void)retainIconForURL:(NSString *)url;
- (void)releaseIconForURL:(NSString *)url;

- (BOOL)isIconExpiredForIconURL:(NSString *)iconURL;
- (BOOL)isIconExpiredForPageURL:(NSString *)pageURL;

- (void)setPrivateBrowsingEnabled:(BOOL)flag;
- (BOOL)privateBrowsingEnabled;

- (NSString *)defaultDatabaseFilename;

- (void)_setIconData:(NSData *)data forIconURL:(NSString *)iconURL;
- (void)_setHaveNoIconForIconURL:(NSString *)iconURL;
- (void)_setIconURL:(NSString *)iconURL forPageURL:(NSString *)pageURL;
- (BOOL)_hasIconForIconURL:(NSString *)iconURL;

- (BOOL)_isEmpty;
@end


// The WebCoreIconDatabaseBridge protocol contains methods for use by the WebCore side of the bridge.

@protocol WebCoreIconDatabaseBridge
+ (WebCoreIconDatabaseBridge *)sharedBridgeInstance;
- (void)loadIconFromURL:(NSString *)iconURL;
@end


// This interface definition allows those who hold a WebCoreIconDatabaseBridge * to call all the methods
// in the WebCoreIconDatabaseBridge protocol without requiring the base implementation to supply the methods.
// This idiom is appropriate because WebCoreIconDatabaseBridge is an abstract class.

@interface WebCoreIconDatabaseBridge (SubclassResponsibility) <WebCoreIconDatabaseBridge>
@end
