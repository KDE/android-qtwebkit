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

#import "WebCoreStatistics.h"

#import "WebCache.h"
#import <WebCore/IconDatabase.h>
#import <WebCore/JavaScriptStatistics.h>
#import <WebCore/Node.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebFrameInternal.h>

using namespace WebCore;

@implementation WebCoreStatistics

+ (NSArray *)statistics
{
    return [WebCache statistics];
}

+ (size_t)javaScriptObjectsCount
{
    return JavaScriptStatistics::objectCount();
}

+ (size_t)javaScriptInterpretersCount
{
    return JavaScriptStatistics::interpreterCount();
}

+ (size_t)javaScriptProtectedObjectsCount
{
    return JavaScriptStatistics::protectedObjectCount();
}

+ (NSCountedSet *)javaScriptRootObjectTypeCounts
{
    NSCountedSet *result = [NSCountedSet set];

    HashCountedSet<const char*>* counts = JavaScriptStatistics::rootObjectTypeCounts();
    HashCountedSet<const char*>::iterator end = counts->end();
    for (HashCountedSet<const char*>::iterator it = counts->begin(); it != end; ++it)
        for (unsigned i = 0; i < it->second; ++i)
            [result addObject:[NSString stringWithUTF8String:it->first]];
    
    delete counts;
    return result;
}

+ (void)garbageCollectJavaScriptObjects
{
    JavaScriptStatistics::garbageCollect();
}

+ (void)garbageCollectJavaScriptObjectsOnAlternateThread:(BOOL)waitUntilDone;
{
    JavaScriptStatistics::garbageCollectOnAlternateThread(waitUntilDone);
}

+ (size_t)iconPageURLMappingCount
{
    return iconDatabase()->pageURLMappingCount();
}

+ (size_t)iconRetainedPageURLCount
{
    return iconDatabase()->retainedPageURLCount();
}

+ (size_t)iconRecordCount
{
    return iconDatabase()->iconRecordCount();
}

+ (size_t)iconsWithDataCount
{
    return iconDatabase()->iconRecordCountWithData();
}

+ (BOOL)shouldPrintExceptions
{
    return JavaScriptStatistics::shouldPrintExceptions();
}

+ (void)setShouldPrintExceptions:(BOOL)print
{
    JavaScriptStatistics::setShouldPrintExceptions(print);
}

+ (void)emptyCache
{
    [WebCache empty];
}

+ (void)setCacheDisabled:(BOOL)disabled
{
    [WebCache setDisabled:disabled];
}

+ (void)startIgnoringWebCoreNodeLeaks
{
    WebCore::Node::startIgnoringLeaks();
}

+ (void)stopIgnoringWebCoreNodeLeaks;
{
    WebCore::Node::stopIgnoringLeaks();
}

// Deprecated
+ (size_t)javaScriptNoGCAllowedObjectsCount
{
    return 0;
}

+ (size_t)javaScriptReferencedObjectsCount
{
    return JavaScriptStatistics::protectedObjectCount();
}

+ (NSSet *)javaScriptRootObjectClasses
{
    return [self javaScriptRootObjectTypeCounts];
}

@end

@implementation WebFrame (WebKitDebug)

- (NSString *)renderTreeAsExternalRepresentation
{
    return [[self _bridge] renderTreeAsExternalRepresentation];
}

@end
