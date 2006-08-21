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
#import <WebKit/WebIconDatabase.h>

#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebFileDatabase.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPreferences.h>

#import <WebKit/WebIconDatabaseBridge.h>

#import "WebTypesInternal.h"

NSString * const WebIconDatabaseVersionKey =    @"WebIconDatabaseVersion";
NSString * const WebURLToIconURLKey =           @"WebSiteURLToIconURLKey";

NSString * const ObsoleteIconsOnDiskKey =       @"WebIconsOnDisk";
NSString * const ObsoleteIconURLToURLsKey =     @"WebIconURLToSiteURLs";

static const int WebIconDatabaseCurrentVersion = 2;

NSString *WebIconDatabaseDidAddIconNotification =          @"WebIconDatabaseDidAddIconNotification";
NSString *WebIconNotificationUserInfoURLKey =              @"WebIconNotificationUserInfoURLKey";
NSString *WebIconDatabaseDidRemoveAllIconsNotification =   @"WebIconDatabaseDidRemoveAllIconsNotification";

NSString *WebIconDatabaseDirectoryDefaultsKey = @"WebIconDatabaseDirectoryDefaultsKey";
NSString *WebIconDatabaseEnabledDefaultsKey =   @"WebIconDatabaseEnabled";

NSString *WebIconDatabasePath = @"~/Library/Icons";

NSSize WebIconSmallSize = {16, 16};
NSSize WebIconMediumSize = {32, 32};
NSSize WebIconLargeSize = {128, 128};

@interface NSMutableDictionary (WebIconDatabase)
- (void)_web_setObjectUsingSetIfNecessary:(id)object forKey:(id)key;
@end

@interface WebIconDatabase (WebInternal)
- (void)_clearDictionaries;
- (void)_createFileDatabase;
- (void)_loadIconDictionaries;
- (void)_updateFileDatabase;
- (void)_forgetIconForIconURLString:(NSString *)iconURLString;
- (NSMutableDictionary *)_iconsForIconURLString:(NSString *)iconURL;
- (NSImage *)_iconForFileURL:(NSString *)fileURL withSize:(NSSize)size;
- (void)_retainIconForIconURLString:(NSString *)iconURL;
- (void)_releaseIconForIconURLString:(NSString *)iconURL;
- (void)_retainOriginalIconsOnDisk;
- (void)_releaseOriginalIconsOnDisk;
- (void)_resetCachedWebPreferences:(NSNotification *)notification;
- (int)_totalRetainCountForIconURLString:(NSString *)iconURLString;
- (NSImage *)_largestIconFromDictionary:(NSMutableDictionary *)icons;
- (NSMutableDictionary *)_iconsBySplittingRepresentationsOfIcon:(NSImage *)icon;
- (NSImage *)_iconFromDictionary:(NSMutableDictionary *)icons forSize:(NSSize)size cache:(BOOL)cache;
- (void)_scaleIcon:(NSImage *)icon toSize:(NSSize)size;
- (NSData *)_iconDataForIconURL:(NSString *)iconURLString;
- (void)_convertToWebCoreFormat; 
@end

@implementation WebIconDatabase

+ (WebIconDatabase *)sharedIconDatabase
{
    static WebIconDatabase *database = nil;
    
    if (!database) {
#if !LOG_DISABLED
        double start = CFAbsoluteTimeGetCurrent();
#endif
        database = [[WebIconDatabase alloc] init];
#if !LOG_DISABLED
        LOG(Timing, "initializing icon database with %d sites and %d icons took %f", 
            [database->_private->pageURLToIconURL count], [database->_private->iconURLToPageURLs count], (CFAbsoluteTimeGetCurrent() - start));
#endif
    }
    return database;
}


- init
{
    [super init];
    
    _private = [[WebIconDatabasePrivate alloc] init];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *initialDefaults = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], WebIconDatabaseEnabledDefaultsKey, nil];
    [defaults registerDefaults:initialDefaults];
    [initialDefaults release];
    if (![defaults boolForKey:WebIconDatabaseEnabledDefaultsKey]) {
        return self;
    }
    
    [self _createFileDatabase];
    [self _loadIconDictionaries];

    _isClosing = NO;

#ifdef ICONDEBUG
    _private->databaseBridge = [WebIconDatabaseBridge sharedBridgeInstance];
    if (_private->databaseBridge) {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSString *databaseDirectory = [defaults objectForKey:WebIconDatabaseDirectoryDefaultsKey];

        if (!databaseDirectory) {
            databaseDirectory = WebIconDatabasePath;
            [defaults setObject:databaseDirectory forKey:WebIconDatabaseDirectoryDefaultsKey];
        }
        databaseDirectory = [databaseDirectory stringByExpandingTildeInPath];
        [_private->databaseBridge openSharedDatabaseWithPath:databaseDirectory];
    }
    
    [self _convertToWebCoreFormat];
#else
    _private->databaseBridge = nil;
#endif
    
    _private->iconURLToIcons = [[NSMutableDictionary alloc] init];
    _private->iconURLToExtraRetainCount = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    _private->pageURLToRetainCount = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    _private->iconsToEraseWithURLs = [[NSMutableSet alloc] init];
    _private->iconsToSaveWithURLs = [[NSMutableSet alloc] init];
    _private->iconURLsWithNoIcons = [[NSMutableSet alloc] init];
    _private->iconURLsBoundDuringPrivateBrowsing = [[NSMutableSet alloc] init];
    _private->pageURLsBoundDuringPrivateBrowsing = [[NSMutableSet alloc] init];
    _private->privateBrowsingEnabled = [[WebPreferences standardPreferences] privateBrowsingEnabled];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(_applicationWillTerminate:)
                                                 name:NSApplicationWillTerminateNotification
                                               object:NSApp];
    [[NSNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_resetCachedWebPreferences:) 
                   name:WebPreferencesChangedNotification object:nil];
                   
    // FIXME - Once the new iconDB is the only game in town, we need to remove any of the WebFileDatabase code
    // that is threaded and expects certain files to exist - certain files we rip right out from underneath it
    // in the _convertToWebCoreFormat method
#ifndef ICONDEBUG
    // Retain icons on disk then release them once clean-up has begun.
    // This gives the client the opportunity to retain them before they are erased.
    [self _retainOriginalIconsOnDisk];
    [self performSelector:@selector(_releaseOriginalIconsOnDisk) withObject:nil afterDelay:0];
#endif

    return self;
}

- (NSImage *)iconForURL:(NSString *)URL withSize:(NSSize)size cache:(BOOL)cache
{
    ASSERT(size.width);
    ASSERT(size.height);

    if (!URL || ![self _isEnabled])
        return [self defaultIconWithSize:size];

    if ([URL _webkit_isFileURL])
        return [self _iconForFileURL:URL withSize:size];

#ifdef ICONDEBUG        
    NSImage* image = [_private->databaseBridge iconForPageURL:URL withSize:size];
        
    // FIXME - We currently don't embed the default icon in the new WebCore IconDB, so we'll return the old version of it;
    return image ? image : [self defaultIconWithSize:size];
#endif

    NSString *iconURLString = [_private->pageURLToIconURL objectForKey:URL];
    if (!iconURLString)
        // Don't have it
        return [self defaultIconWithSize:size];

    NSMutableDictionary *icons = [self _iconsForIconURLString:iconURLString];

    if (!icons) {
        if (![_private->iconURLsWithNoIcons containsObject:iconURLString]) {
           // We used to have this icon, but don't have it anymore for some reason. (Bug? Deleted from
           // disk behind our back?). Forget that we ever had it so it will be re-fetched next time.
            LOG_ERROR("WebIconDatabase used to contain %@, but the icon file is missing. Now forgetting that we ever knew about this icon.", iconURLString);
            [self _forgetIconForIconURLString:iconURLString];
        }
        return [self defaultIconWithSize:size];
    }        

    return [self _iconFromDictionary:icons forSize:size cache:cache];
}

- (NSImage *)iconForURL:(NSString *)URL withSize:(NSSize)size
{
    return [self iconForURL:URL withSize:size cache:YES];
}

- (NSString *)iconURLForURL:(NSString *)URL
{
    if (![self _isEnabled])
        return nil;
        
#ifdef ICONDEBUG
    NSString* iconurl = [_private->databaseBridge iconURLForPageURL:URL];
    return iconurl;
#endif

    return URL ? [_private->pageURLToIconURL objectForKey:URL] : nil;
}

- (NSImage *)defaultIconWithSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
    if (!_private->defaultIcons) {
        NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"url_icon" ofType:@"tiff"];
        if (path) {
            NSImage *icon = [[NSImage alloc] initByReferencingFile:path];
            _private->defaultIcons = [[NSMutableDictionary dictionaryWithObject:icon
                                            forKey:[NSValue valueWithSize:[icon size]]] retain];
            [icon release];
        }
    }

    return [self _iconFromDictionary:_private->defaultIcons forSize:size cache:YES];
}

- (void)retainIconForURL:(NSString *)URL
{
    ASSERT(URL);
    if (![self _isEnabled])
        return;

#ifdef ICONDEBUG
    [_private->databaseBridge retainIconForURL:URL];
    return;
#endif

    WebNSUInteger retainCount = (WebNSUInteger)(void *)CFDictionaryGetValue(_private->pageURLToRetainCount, URL);
    CFDictionarySetValue(_private->pageURLToRetainCount, URL, (void *)(retainCount + 1));

}

- (void)releaseIconForURL:(NSString *)pageURL
{
    ASSERT(pageURL);
    if (![self _isEnabled])
        return;
        
#ifdef ICONDEBUG
    [_private->databaseBridge releaseIconForURL:pageURL];
    return;
#endif

    WebNSUInteger retainCount = (WebNSUInteger)(void *)CFDictionaryGetValue(_private->pageURLToRetainCount, pageURL);
    
    if (retainCount <= 0) {
        LOG_ERROR("The icon for %@ was released more times than it was retained.", pageURL);
        return;
    }
    
    WebNSUInteger newRetainCount = retainCount - 1;

    if (newRetainCount == 0) {
        // Forget association between this page URL and a retain count
        CFDictionaryRemoveValue(_private->pageURLToRetainCount, pageURL);

        // If there's a known iconURL for this page URL, we need to do more cleanup
        NSString *iconURL = [_private->pageURLToIconURL objectForKey:pageURL];
        if (iconURL != nil) {
            // If there are no other retainers of this icon, forget it entirely
            if ([self _totalRetainCountForIconURLString:iconURL] == 0) {
                [self _forgetIconForIconURLString:iconURL];
            } else {
                // There's at least one other retainer of this icon, so we need to forget the
                // two-way links between this page URL and the icon URL, without blowing away
                // the icon entirely.                
                id pageURLs = [_private->iconURLToPageURLs objectForKey:iconURL];
                if ([pageURLs isKindOfClass:[NSMutableSet class]]) {
                    ASSERT([pageURLs containsObject:pageURL]);
                    [pageURLs removeObject:pageURL];
                    
                    // Maybe this was the last page URL mapped to this icon URL
                    if ([(NSMutableSet *)pageURLs count] == 0) {
                        [_private->iconURLToPageURLs removeObjectForKey:iconURL];
                    }
                } else {
                    // Only one page URL was associated with this icon URL; it must have been us
                    ASSERT([pageURLs isKindOfClass:[NSString class]]);
                    ASSERT([pageURLs isEqualToString:pageURL]);
                    [_private->iconURLToPageURLs removeObjectForKey:pageURL];
                }
                
                // Remove iconURL from this dictionary last, since this might be the last
                // reference and we need to use it as a key for _private->iconURLToPageURLs above.
                [_private->pageURLToIconURL removeObjectForKey:pageURL];
            }
        }
    } else {
        CFDictionarySetValue(_private->pageURLToRetainCount, pageURL, (void *)newRetainCount);
    }
}

- (void)delayDatabaseCleanup
{
    if (![self _isEnabled]) {
        return;
    }
    
    if(_private->didCleanup){
        LOG_ERROR("delayDatabaseCleanup cannot be called after cleanup has begun");
        return;
    }
    
    _private->cleanupCount++;
}

- (void)allowDatabaseCleanup
{
    if (![self _isEnabled]) {
        return;
    }
    
    if(_private->didCleanup){
        LOG_ERROR("allowDatabaseCleanup cannot be called after cleanup has begun");
        return;
    }
    
    _private->cleanupCount--;

    if(_private->cleanupCount == 0 && _private->waitingToCleanup){
        [self _releaseOriginalIconsOnDisk];
    }
}

@end


@implementation WebIconDatabase (WebPendingPublic)

- (void)removeAllIcons
{
    NSArray *keys = [(NSDictionary *)_private->iconURLToPageURLs allKeys];
    unsigned count = [keys count];
    for (unsigned i = 0; i < count; i++)
        [self _forgetIconForIconURLString:[keys objectAtIndex:i]];
    
    // Delete entire file database immediately. This has at least three advantages over waiting for
    // _updateFileDatabase to execute:
    // (1) _updateFileDatabase won't execute until an icon has been added
    // (2) this is faster
    // (3) this deletes all the on-disk hierarchy (especially useful if due to past problems there are
    // some stale files in that hierarchy)
    [_private->fileDatabase removeAllObjects];
    [_private->iconsToEraseWithURLs removeAllObjects];
    [_private->iconsToSaveWithURLs removeAllObjects];
    [_private->iconURLsBoundDuringPrivateBrowsing removeAllObjects];
    [_private->pageURLsBoundDuringPrivateBrowsing removeAllObjects];
    [self _clearDictionaries];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebIconDatabaseDidRemoveAllIconsNotification
                                                        object:self
                                                      userInfo:nil];
}
- (BOOL)isIconExpiredForIconURL:(NSString *)iconURL
{
    return [_private->databaseBridge isIconExpiredForIconURL:iconURL];
}

@end

@implementation WebIconDatabase (WebPrivate)

- (BOOL)_isEnabled
{
    return (_private->fileDatabase != nil);
}

- (void)_setIcon:(NSImage *)icon forIconURL:(NSString *)iconURL
{
    ASSERT(icon);
    ASSERT(iconURL);
    ASSERT([self _isEnabled]);
    
    NSMutableDictionary *icons = [self _iconsBySplittingRepresentationsOfIcon:icon];
    
    if (!icons)
        return;
    
    [_private->iconURLToIcons setObject:icons forKey:iconURL];
    
    // Don't update any icon information on disk during private browsing. Remember which icons have been
    // affected during private browsing so we can forget this information when private browsing is turned off.
    if (_private->privateBrowsingEnabled)
        [_private->iconURLsBoundDuringPrivateBrowsing addObject:iconURL];

    [self _retainIconForIconURLString:iconURL];
    
    // Release the newly created icon much like an autorelease.
    // This gives the client enough time to retain it.
    // FIXME: Should use an actual autorelease here using a proxy object instead.
    [self performSelector:@selector(_releaseIconForIconURLString:) withObject:iconURL afterDelay:0];
}

- (void)_setHaveNoIconForIconURL:(NSString *)iconURL
{
    ASSERT(iconURL);
    ASSERT([self _isEnabled]);

#ifdef ICONDEBUG
    [_private->databaseBridge _setHaveNoIconForIconURL:iconURL];
    return;
#endif

    [_private->iconURLsWithNoIcons addObject:iconURL];
    
    // Don't update any icon information on disk during private browsing. Remember which icons have been
    // affected during private browsing so we can forget this information when private browsing is turned off.
    if (_private->privateBrowsingEnabled)
        [_private->iconURLsBoundDuringPrivateBrowsing addObject:iconURL];

    [self _retainIconForIconURLString:iconURL];

    // Release the newly created icon much like an autorelease.
    // This gives the client enough time to retain it.
    // FIXME: Should use an actual autorelease here using a proxy object instead.
    [self performSelector:@selector(_releaseIconForIconURLString:) withObject:iconURL afterDelay:0];
}


- (void)_setIconURL:(NSString *)iconURL forURL:(NSString *)URL
{
    ASSERT(iconURL);
    ASSERT(URL);
    ASSERT([self _isEnabled]);
    ASSERT(_private->pageURLToIconURL);
    
#ifdef ICONDEBUG
    [_private->databaseBridge _setIconURL:iconURL forPageURL:URL];
    [self _sendNotificationForURL:URL];
    return;
#endif

    if ([[_private->pageURLToIconURL objectForKey:URL] isEqualToString:iconURL]) {
        // Don't do any work if the icon URL is already bound to the site URL
        return;
    }

    // Keep track of which entries in pageURLToIconURL were created during private browsing so that they can be skipped
    // when saving to disk.
    if (_private->privateBrowsingEnabled)
        [_private->pageURLsBoundDuringPrivateBrowsing addObject:URL];

    [_private->pageURLToIconURL setObject:iconURL forKey:URL];
    [_private->iconURLToPageURLs _web_setObjectUsingSetIfNecessary:URL forKey:iconURL];

    if ([_private->iconURLsWithNoIcons containsObject:iconURL]) {
        // If the icon is in the negative cache (ie, there is no icon), avoid the
        // work of delivering a notification for it or saving it to disk. This is a significant
        // win on the iBench HTML test.
        
        // This return must occur after the dictionary set calls above, so the icon record
        // is properly retained. Otherwise, we'll forget that the site had no icon, and
        // inefficiently request its icon again.
        return;
    }
    
    [self _sendNotificationForURL:URL];
    [self _updateFileDatabase];
}

- (BOOL)_hasEntryForIconURL:(NSString *)iconURL;
{
    ASSERT([self _isEnabled]);

#ifdef ICONDEBUG
    BOOL result = [_private->databaseBridge _hasEntryForIconURL:iconURL];
    if (result)
        LOG(IconDatabase, "NewDB has icon for IconURL %@", iconURL);
    else
        LOG(IconDatabase, "NewDB has NO icon for IconURL %@", iconURL);
    return result;
#endif

    return (([_private->iconURLToIcons objectForKey:iconURL] ||
             [_private->iconURLsWithNoIcons containsObject:iconURL] ||
             [_private->iconsOnDiskWithURLs containsObject:iconURL]) &&
             [self _totalRetainCountForIconURLString:iconURL] > 0);
}

- (void)_sendNotificationForURL:(NSString *)URL
{
    ASSERT(URL);
    
    NSDictionary *userInfo = [NSDictionary dictionaryWithObject:URL
                                                         forKey:WebIconNotificationUserInfoURLKey];
    [[NSNotificationCenter defaultCenter] postNotificationName:WebIconDatabaseDidAddIconNotification
                                                        object:self
                                                      userInfo:userInfo];
}

- (void)loadIconFromURL:(NSString *)iconURL
{
    [_private->databaseBridge loadIconFromURL:iconURL];
}

@end

@implementation WebIconDatabase (WebInternal)

- (void)_createFileDatabase
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *databaseDirectory = [defaults objectForKey:WebIconDatabaseDirectoryDefaultsKey];

    if (!databaseDirectory) {
        databaseDirectory = WebIconDatabasePath;
        [defaults setObject:databaseDirectory forKey:WebIconDatabaseDirectoryDefaultsKey];
    }
    databaseDirectory = [databaseDirectory stringByExpandingTildeInPath];
    
    _private->fileDatabase = [[WebFileDatabase alloc] initWithPath:databaseDirectory];
    [_private->fileDatabase setSizeLimit:20000000];
    [_private->fileDatabase open];
}

- (void)_clearDictionaries
{
    [_private->pageURLToIconURL release];
    [_private->iconURLToPageURLs release];
    [_private->iconsOnDiskWithURLs release];
    [_private->originalIconsOnDiskWithURLs release];
    [_private->iconURLsBoundDuringPrivateBrowsing release];
    [_private->pageURLsBoundDuringPrivateBrowsing release];
    _private->pageURLToIconURL = [[NSMutableDictionary alloc] init];
    _private->iconURLToPageURLs = [[NSMutableDictionary alloc] init];
    _private->iconsOnDiskWithURLs = [[NSMutableSet alloc] init];
    _private->originalIconsOnDiskWithURLs = [[NSMutableSet alloc] init];
    _private->iconURLsBoundDuringPrivateBrowsing = [[NSMutableSet alloc] init];
    _private->pageURLsBoundDuringPrivateBrowsing = [[NSMutableSet alloc] init];
}

- (void)_loadIconDictionaries
{
    WebFileDatabase *fileDB = _private->fileDatabase;
    
    // fileDB should be non-nil here because it should have been created by _createFileDatabase 
    if (!fileDB) {
        LOG_ERROR("Couldn't load icon dictionaries because file database didn't exist");
        return;
    }
    
    NSNumber *version = [fileDB objectForKey:WebIconDatabaseVersionKey];
    int v = 0;
    // no version means first version
    if (version == nil) {
        v = 1;
    } else if ([version isKindOfClass:[NSNumber class]]) {
        v = [version intValue];
    }
    
    // Get the site URL to icon URL dictionary from the file DB.
    NSMutableDictionary *pageURLToIconURL = nil;
    if (v <= WebIconDatabaseCurrentVersion) {
        pageURLToIconURL = [fileDB objectForKey:WebURLToIconURLKey];
        // Remove the old unnecessary mapping files.
        if (v < WebIconDatabaseCurrentVersion) {
            [fileDB removeObjectForKey:ObsoleteIconsOnDiskKey];
            [fileDB removeObjectForKey:ObsoleteIconURLToURLsKey];
        }        
    }
    
    // Must double-check all values read from disk. If any are bogus, we just throw out the whole icon cache.
    // We expect this to be nil if the icon cache has been cleared, so we shouldn't whine in that case.
    if (![pageURLToIconURL isKindOfClass:[NSMutableDictionary class]]) {
        if (pageURLToIconURL)
            LOG_ERROR("Clearing icon cache because bad value %@ was found on disk, expected an NSMutableDictionary", pageURLToIconURL);
        [self _clearDictionaries];
        return;
    }

    // Keep a set of icon URLs on disk so we know what we need to write out or remove.
    NSMutableSet *iconsOnDiskWithURLs = [NSMutableSet setWithArray:[pageURLToIconURL allValues]];

    // Reverse pageURLToIconURL so we have an icon URL to page URLs dictionary. 
    NSMutableDictionary *iconURLToPageURLs = [NSMutableDictionary dictionaryWithCapacity:[_private->iconsOnDiskWithURLs count]];
    NSEnumerator *enumerator = [pageURLToIconURL keyEnumerator];
    NSString *URL;
    while ((URL = [enumerator nextObject])) {
        NSString *iconURL = (NSString *)[pageURLToIconURL objectForKey:URL];
        // Must double-check all values read from disk. If any are bogus, we just throw out the whole icon cache.
        if (![URL isKindOfClass:[NSString class]] || ![iconURL isKindOfClass:[NSString class]]) {
            LOG_ERROR("Clearing icon cache because either %@ or %@ was a bad value on disk, expected both to be NSStrings", URL, iconURL);
            [self _clearDictionaries];
            return;
        }
        [iconURLToPageURLs _web_setObjectUsingSetIfNecessary:URL forKey:iconURL];
    }

    ASSERT(!_private->pageURLToIconURL);
    ASSERT(!_private->iconURLToPageURLs);
    ASSERT(!_private->iconsOnDiskWithURLs);
    ASSERT(!_private->originalIconsOnDiskWithURLs);
    
    _private->pageURLToIconURL = [pageURLToIconURL retain];
    _private->iconURLToPageURLs = [iconURLToPageURLs retain];
    _private->iconsOnDiskWithURLs = [iconsOnDiskWithURLs retain];
    _private->originalIconsOnDiskWithURLs = [iconsOnDiskWithURLs copy];
}

// Only called by _setIconURL:forKey:
- (void)_updateFileDatabase
{
    if (_private->cleanupCount != 0)
        return;

    WebFileDatabase *fileDB = _private->fileDatabase;
    if (!fileDB) {
        LOG_ERROR("Couldn't update file database because it didn't exist");
        return;
    }

    [fileDB setObject:[NSNumber numberWithInt:WebIconDatabaseCurrentVersion] forKey:WebIconDatabaseVersionKey];

    // Erase icons that have been released that are on disk.
    // Must remove icons before writing them to disk or else we could potentially remove the newly written ones.
    NSEnumerator *enumerator = [_private->iconsToEraseWithURLs objectEnumerator];
    NSString *iconURLString;
    
    while ((iconURLString = [enumerator nextObject]) != nil) {
        [fileDB removeObjectForKey:iconURLString];
        [_private->iconsOnDiskWithURLs removeObject:iconURLString];
    }

    // Save icons that have been retained that are not already on disk
    enumerator = [_private->iconsToSaveWithURLs objectEnumerator];

    while ((iconURLString = [enumerator nextObject]) != nil) {
        NSMutableDictionary *icons = [_private->iconURLToIcons objectForKey:iconURLString];
        if (icons) {
            // Save the 16 x 16 size icons as this is the only size that Safari uses.
            // If we ever use larger sizes, we should save the largest size so icons look better when scaling up.
            // This also works around the problem with cnet's blank 32x32 icon (3105486).
            NSImage *icon = [icons objectForKey:[NSValue valueWithSize:NSMakeSize(16,16)]];
            if (!icon) {
                // In case there is no 16 x 16 size.
                icon = [self _largestIconFromDictionary:icons];
            }
            NSData *iconData = [icon TIFFRepresentation];
            if (iconData) {
                //NSLog(@"Writing icon: %@", iconURLString);
                [fileDB setObject:iconData forKey:iconURLString];
                [_private->iconsOnDiskWithURLs addObject:iconURLString];
            }
        } else if ([_private->iconURLsWithNoIcons containsObject:iconURLString]) {
            [fileDB setObject:[NSNull null] forKey:iconURLString];
            [_private->iconsOnDiskWithURLs addObject:iconURLString];
        }
    }
    
    [_private->iconsToEraseWithURLs removeAllObjects];
    [_private->iconsToSaveWithURLs removeAllObjects];

    // Save the icon dictionaries to disk, after removing any values created during private browsing.
    // Even if we weren't modifying the dictionary we'd still want to use a copy so that WebFileDatabase
    // doesn't look at the original from a different thread. (We used to think this would fix 3566336
    // but that bug's progeny are still alive and kicking.)
    NSMutableDictionary *pageURLToIconURLCopy = [_private->pageURLToIconURL mutableCopy];
    [pageURLToIconURLCopy removeObjectsForKeys:[_private->pageURLsBoundDuringPrivateBrowsing allObjects]];
    [fileDB setObject:pageURLToIconURLCopy forKey:WebURLToIconURLKey];
    [pageURLToIconURLCopy release];
}

- (void)_applicationWillTerminate:(NSNotification *)notification
{
    // Should only cause a write if user quit before 3 seconds after the last _updateFileDatabase
    [_private->fileDatabase sync];
    
    [_private->databaseBridge closeSharedDatabase];
    [_private->databaseBridge release];
    _private->databaseBridge = nil;
    _isClosing = YES;
}

- (int)_totalRetainCountForIconURLString:(NSString *)iconURLString
{
    // Add up the retain counts for each associated page, plus the retain counts not associated
    // with any page, which are stored in _private->iconURLToExtraRetainCount
    WebNSUInteger result = (WebNSUInteger)(void *)CFDictionaryGetValue(_private->iconURLToExtraRetainCount, iconURLString);
    
    id URLStrings = [_private->iconURLToPageURLs objectForKey:iconURLString];
    if (URLStrings != nil) {
        if ([URLStrings isKindOfClass:[NSMutableSet class]]) {
            NSEnumerator *e = [(NSMutableSet *)URLStrings objectEnumerator];
            NSString *URLString;
            while ((URLString = [e nextObject]) != nil) {
                ASSERT([URLString isKindOfClass:[NSString class]]);
                result += (WebNSUInteger)(void *)CFDictionaryGetValue(_private->pageURLToRetainCount, URLString);
            }
        } else {
            ASSERT([URLStrings isKindOfClass:[NSString class]]);
            result += (WebNSUInteger)(void *)CFDictionaryGetValue(_private->pageURLToRetainCount, URLStrings);
        }
    }

    return result;
}

- (NSMutableDictionary *)_iconsForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);

    if ([_private->iconURLsWithNoIcons containsObject:iconURLString])
        return nil;
    
    NSMutableDictionary *icons = [_private->iconURLToIcons objectForKey:iconURLString];

    if (icons)
        return icons;
        
    // Not in memory, check disk
    if(![_private->iconsOnDiskWithURLs containsObject:iconURLString])
        return nil;
    
#if !LOG_DISABLED         
    double start = CFAbsoluteTimeGetCurrent();
#endif
    NSData *iconData = [_private->fileDatabase objectForKey:iconURLString];

    if ([iconData isKindOfClass:[NSNull class]]) {
        [_private->iconURLsWithNoIcons addObject:iconURLString];
        return nil;
    }
    
    if (iconData) {
        NS_DURING
            NSImage *icon = [[NSImage alloc] initWithData:iconData];
            icons = [self _iconsBySplittingRepresentationsOfIcon:icon];
            if (icons) {
#if !LOG_DISABLED 
                double duration = CFAbsoluteTimeGetCurrent() - start;
                LOG(Timing, "loading and creating icon %@ took %f seconds", iconURLString, duration);
#endif
                [_private->iconURLToIcons setObject:icons forKey:iconURLString];
            }
        NS_HANDLER
            icons = nil;
        NS_ENDHANDLER
    }
    
    return icons;
}

- (NSImage *)_iconForFileURL:(NSString *)file withSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);

    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSString *path = [[NSURL _web_URLWithDataAsString:file] path];
    NSString *suffix = [path pathExtension];
    NSImage *icon = nil;
    
    if ([suffix _webkit_isCaseInsensitiveEqualToString:@"htm"] || [suffix _webkit_isCaseInsensitiveEqualToString:@"html"]) {
        if (!_private->htmlIcons) {
            icon = [workspace iconForFileType:@"html"];
            _private->htmlIcons = [[self _iconsBySplittingRepresentationsOfIcon:icon] retain];
        }
        icon = [self _iconFromDictionary:_private->htmlIcons forSize:size cache:YES];
    } else {
        if (!path || ![path isAbsolutePath]) {
            // Return the generic icon when there is no path.
            icon = [workspace iconForFileType:NSFileTypeForHFSTypeCode(kGenericDocumentIcon)];
        } else {
            icon = [workspace iconForFile:path];
        }
        [self _scaleIcon:icon toSize:size];
    }

    return icon;
}

- (void)_retainIconForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    WebNSUInteger retainCount = (WebNSUInteger)(void *)CFDictionaryGetValue(_private->iconURLToExtraRetainCount, iconURLString);
    WebNSUInteger newRetainCount = retainCount + 1;

    CFDictionarySetValue(_private->iconURLToExtraRetainCount, iconURLString, (void *)newRetainCount);

    if (newRetainCount == 1 && !_private->privateBrowsingEnabled) {

        // Either we know nothing about this icon and need to save it to disk, or we were planning to remove it
        // from disk (as set up in _forgetIconForIconURLString:) and should stop that process.
        if ([_private->iconsOnDiskWithURLs containsObject:iconURLString]) {
            ASSERT(![_private->iconsToSaveWithURLs containsObject:iconURLString]);
            [_private->iconsToEraseWithURLs removeObject:iconURLString];
        } else {
            ASSERT(![_private->iconsToEraseWithURLs containsObject:iconURLString]);
            [_private->iconsToSaveWithURLs addObject:iconURLString];
        }
    }
}

- (void)_forgetIconForIconURLString:(NSString *)iconURLString
{
    ASSERT_ARG(iconURLString, iconURLString != nil);
    if([_private->iconsOnDiskWithURLs containsObject:iconURLString]){
        [_private->iconsToEraseWithURLs addObject:iconURLString];
        [_private->iconsToSaveWithURLs removeObject:iconURLString];
    }
        
    // Remove the icon's images
    [_private->iconURLToIcons removeObjectForKey:iconURLString];
    
    // Remove negative cache item for icon, if any
    [_private->iconURLsWithNoIcons removeObject:iconURLString];
    
    // Remove the icon's associated site URLs, if any
    [iconURLString retain];
    id URLs = [_private->iconURLToPageURLs objectForKey:iconURLString];
    if (URLs != nil) {
        if ([URLs isKindOfClass:[NSMutableSet class]])
            [_private->pageURLToIconURL removeObjectsForKeys:[URLs allObjects]];
        else {
            ASSERT([URLs isKindOfClass:[NSString class]]);
            [_private->pageURLToIconURL removeObjectForKey:URLs];
        }
    }
    [_private->iconURLToPageURLs removeObjectForKey:iconURLString];
    CFDictionaryRemoveValue(_private->iconURLToExtraRetainCount, iconURLString);
    [iconURLString release];
}

- (void)_releaseIconForIconURLString:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    if (![self _isEnabled])
        return;
    
    WebNSUInteger retainCount = (WebNSUInteger)(void *)CFDictionaryGetValue(_private->iconURLToExtraRetainCount, iconURLString);

    // This error used to be an ASSERT() that was causing the build bot to fail.  The build bot was getting itself into a reproducible
    // situation of having an icon for 127.0.0.1:8000/favicon.ico registered in the database but not finding the file for it.  This situation
    // triggers a call to _forgetIconForIconURL which dumps everything about the icon - including the retain count.  A later call to releaseIconForURL
    // would then ASSERT and crash the test as the retain count had be internally reset to zero
    // The reason the build bot was getting into this situation is not yet understood but the cause of the ASSERT is - and the condition was already
    // handled gracefully in release builds.  Therefore we're changing it to a LOG_ERROR with the understanding that the sqlite icon database will not 
    // have this issue due to its entirely different nature
    if (retainCount <= 0) {
        if (!_isClosing)
            LOG_ERROR("Trying to release an icon whose retain-count is already non-positive");
        return;
    }
    
    WebNSUInteger newRetainCount = retainCount - 1;
    if (newRetainCount == 0) {
        CFDictionaryRemoveValue(_private->iconURLToExtraRetainCount, iconURLString);
        if ([self _totalRetainCountForIconURLString:iconURLString] == 0) {
            [self _forgetIconForIconURLString:iconURLString];
        }
    } else {
        CFDictionarySetValue(_private->iconURLToExtraRetainCount, iconURLString, (void *)newRetainCount);
    }
}

- (void)_retainOriginalIconsOnDisk
{
    NSEnumerator *enumerator = [_private->originalIconsOnDiskWithURLs objectEnumerator];;
    NSString *iconURLString;
    while ((iconURLString = [enumerator nextObject]) != nil) {
        [self _retainIconForIconURLString:iconURLString];
    }
}

- (void)_releaseOriginalIconsOnDisk
{
    if (_private->cleanupCount > 0) {
        _private->waitingToCleanup = YES;
        return;
    }

    NSEnumerator *enumerator = [_private->originalIconsOnDiskWithURLs objectEnumerator];
    NSString *iconURLString;
    while ((iconURLString = [enumerator nextObject]) != nil) {
        [self _releaseIconForIconURLString:iconURLString];
    }
    
    [_private->originalIconsOnDiskWithURLs release];
    _private->originalIconsOnDiskWithURLs = nil;

    _private->didCleanup = YES;
}

- (void)_resetCachedWebPreferences:(NSNotification *)notification
{
    BOOL privateBrowsingEnabledNow = [[WebPreferences standardPreferences] privateBrowsingEnabled];

#ifdef ICONDEBUG
    [_private->databaseBridge setPrivateBrowsingEnabled:privateBrowsingEnabledNow];
    return;
#endif

    if (privateBrowsingEnabledNow == _private->privateBrowsingEnabled)
        return;
    
    _private->privateBrowsingEnabled = privateBrowsingEnabledNow;
    
    // When private browsing is turned off, forget everything we learned while it was on 
    if (!_private->privateBrowsingEnabled) {
        // Forget all of the icons whose existence we learned of during private browsing.
        NSEnumerator *iconEnumerator = [_private->iconURLsBoundDuringPrivateBrowsing objectEnumerator];
        NSString *iconURLString;
        while ((iconURLString = [iconEnumerator nextObject]) != nil)
            [self _forgetIconForIconURLString:iconURLString];

        // Forget the relationships between page and icon that we learned during private browsing.
        NSEnumerator *pageEnumerator = [_private->pageURLsBoundDuringPrivateBrowsing objectEnumerator];
        NSString *pageURLString;
        while ((pageURLString = [pageEnumerator nextObject]) != nil) {
            [_private->pageURLToIconURL removeObjectForKey:pageURLString];
            // Tell clients that these pages' icons have changed (to generic). The notification is named
            // WebIconDatabaseDidAddIconNotification but it really means just "did change icon".
            [self _sendNotificationForURL:pageURLString];
        }
        
        [_private->iconURLsBoundDuringPrivateBrowsing removeAllObjects];
        [_private->pageURLsBoundDuringPrivateBrowsing removeAllObjects];
    }
}

- (NSImage *)_largestIconFromDictionary:(NSMutableDictionary *)icons
{
    ASSERT(icons);
    
    NSEnumerator *enumerator = [icons keyEnumerator];
    NSValue *currentSize, *largestSize=nil;
    float largestSizeArea=0;

    while ((currentSize = [enumerator nextObject]) != nil) {
        NSSize currentSizeSize = [currentSize sizeValue];
        float currentSizeArea = currentSizeSize.width * currentSizeSize.height;
        if(!largestSizeArea || (currentSizeArea > largestSizeArea)){
            largestSize = currentSize;
            largestSizeArea = currentSizeArea;
        }
    }

    return [icons objectForKey:largestSize];
}

- (NSMutableDictionary *)_iconsBySplittingRepresentationsOfIcon:(NSImage *)icon
{
    ASSERT(icon);

    NSMutableDictionary *icons = [NSMutableDictionary dictionary];
    NSEnumerator *enumerator = [[icon representations] objectEnumerator];
    NSImageRep *rep;

    while ((rep = [enumerator nextObject]) != nil) {
        NSSize size = [rep size];
        NSImage *subIcon = [[NSImage alloc] initWithSize:size];
        [subIcon addRepresentation:rep];
        [icons setObject:subIcon forKey:[NSValue valueWithSize:size]];
        [subIcon release];
    }

    if([icons count] > 0)
        return icons;

    LOG_ERROR("icon has no representations");
    
    return nil;
}

- (NSImage *)_iconFromDictionary:(NSMutableDictionary *)icons forSize:(NSSize)size cache:(BOOL)cache
{
    ASSERT(size.width);
    ASSERT(size.height);

    NSImage *icon = [icons objectForKey:[NSValue valueWithSize:size]];

    if(!icon){
        icon = [[[self _largestIconFromDictionary:icons] copy] autorelease];
        [self _scaleIcon:icon toSize:size];

        if(cache){
            [icons setObject:icon forKey:[NSValue valueWithSize:size]];
        }
    }

    return icon;
}

- (void)_scaleIcon:(NSImage *)icon toSize:(NSSize)size
{
    ASSERT(size.width);
    ASSERT(size.height);
    
#if !LOG_DISABLED        
    double start = CFAbsoluteTimeGetCurrent();
#endif
    
    [icon setScalesWhenResized:YES];
    [icon setSize:size];
    
#if !LOG_DISABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "scaling icon took %f seconds.", duration);
#endif
}

- (NSData *)_iconDataForIconURL:(NSString *)iconURLString
{
    ASSERT(iconURLString);
    
    NSData *iconData = [_private->fileDatabase objectForKey:iconURLString];
    
    if ((id)iconData == (id)[NSNull null]) 
        return nil;
        
    return iconData;
}

- (void)_convertToWebCoreFormat
{
    ASSERT(_private);
    ASSERT(_private->databaseBridge);
    
    // If the WebCore Icon Database is not empty, we assume that this conversion has already
    // taken place and skip the rest of the steps 
    if (![_private->databaseBridge _isEmpty])
        return;
                
    NSEnumerator *enumerator = [_private->pageURLToIconURL keyEnumerator];
    NSString *url, *iconURL;
    
    // First, we'll iterate through the PageURL->IconURL map
    while ((url = [enumerator nextObject]) != nil) {
        iconURL = [_private->pageURLToIconURL objectForKey:url];
        if (!iconURL)
            continue;
        [_private->databaseBridge _setIconURL:iconURL forPageURL:url];
    }    
    
    // Second, we'll iterate through the icon data we do have
    enumerator = [_private->iconsOnDiskWithURLs objectEnumerator];
    NSData *iconData;
    
    while ((url = [enumerator nextObject]) != nil) {
        iconData = [self _iconDataForIconURL:url];
        if (iconData)
            [_private->databaseBridge _setIconData:iconData forIconURL:url];
        else {
            // This really *shouldn't* happen, so it'd be good to track down why it might happen in a debug build
            // however, we do know how to handle it gracefully in release
            LOG_ERROR("%@ is marked as having an icon on disk, but we couldn't get the data for it", url);
            [_private->databaseBridge _setHaveNoIconForIconURL:url];
        }
    }
    
    // Finally, we'll iterate through the negative cache we have
    enumerator = [_private->iconURLsWithNoIcons objectEnumerator];
    while ((url = [enumerator nextObject]) != nil) 
        [_private->databaseBridge _setHaveNoIconForIconURL:url];
   
    // After we're done converting old style icons over to webcore icons, we delete the entire directory hierarchy 
    // for the old icon DB
    NSString *iconPath = [[NSUserDefaults standardUserDefaults] objectForKey:WebIconDatabaseDirectoryDefaultsKey];
    if (!iconPath)
        iconPath = WebIconDatabasePath;
    
    NSString *fullIconPath = [iconPath stringByExpandingTildeInPath];    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    enumerator = [[fileManager directoryContentsAtPath:fullIconPath] objectEnumerator];
    
    NSString *databaseFilename = [_private->databaseBridge defaultDatabaseFilename];

    NSString *file;
    while ((file = [enumerator nextObject]) != nil) {
        if ([file isEqualTo:databaseFilename])
            continue;
        NSString *filePath = [fullIconPath stringByAppendingPathComponent:file];
        if (![fileManager  removeFileAtPath:filePath handler:nil])
            LOG_ERROR("Failed to delete %@ from old icon directory", filePath);
    }
}

@end

@implementation WebIconDatabasePrivate

@end

@implementation NSMutableDictionary (WebIconDatabase)

- (void)_web_setObjectUsingSetIfNecessary:(id)object forKey:(id)key
{
    id previousObject = [self objectForKey:key];
    if (previousObject == nil) {
        [self setObject:object forKey:key];
    } else if ([previousObject isKindOfClass:[NSMutableSet class]]) {
        [previousObject addObject:object];
    } else {
        NSMutableSet *objects = [[NSMutableSet alloc] initWithObjects:previousObject, object, nil];
        [self setObject:objects forKey:key];
        [objects release];
    }
}

@end
