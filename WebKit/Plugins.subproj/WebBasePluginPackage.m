//
//  WebBasePluginPackage.m
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebBasePluginPackage.h>

#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebPluginPackage.h>

#import <Foundation/NSPrivateDecls.h>
#import <Foundation/NSString_NSURLExtras.h>

#import <CoreFoundation/CFBundlePriv.h>

#define JavaCocoaPluginIdentifier 	@"com.apple.JavaPluginCocoa"
#define JavaCarbonPluginIdentifier 	@"com.apple.JavaAppletPlugin"
#define JavaCFMPluginFilename		@"Java Applet Plugin Enabler"

#define QuickTimeCarbonPluginIdentifier       @"com.apple.QuickTime Plugin.plugin"
#define QuickTimeCocoaPluginIdentifier        @"com.apple.qtcocoaplugin"

@interface NSArray (WebPluginExtensions)
- (NSArray *)_web_lowercaseStrings;
@end;

@implementation WebBasePluginPackage

+ (WebBasePluginPackage *)pluginWithPath:(NSString *)pluginPath
{
    WebBasePluginPackage *pluginPackage = [[WebPluginPackage alloc] initWithPath:pluginPath];

    if (!pluginPackage) {
        pluginPackage = [[WebNetscapePluginPackage alloc] initWithPath:pluginPath];
    }

    return [pluginPackage autorelease];
}

+ (NSString *)preferredLocalizationName
{
    SInt32 languageCode;
    SInt32 regionCode;
    SInt32 scriptCode;
    CFStringEncoding stringEncoding;
    
    CFBundleGetLocalizationInfoForLocalization(NULL, &languageCode, &regionCode, &scriptCode, &stringEncoding);
    return WebCFAutorelease(CFBundleCopyLocalizationForLocalizationInfo(languageCode, regionCode, scriptCode, stringEncoding));
}

- (NSString *)pathByResolvingSymlinksAndAliasesInPath:(NSString *)thePath
{
    NSString *newPath = [thePath stringByResolvingSymlinksInPath];

    FSRef fref;
    OSStatus err;

    err = FSPathMakeRef((const UInt8 *)[thePath fileSystemRepresentation], &fref, NULL);
    if (err != noErr) {
        return newPath;
    }

    Boolean targetIsFolder;
    Boolean wasAliased;
    err = FSResolveAliasFileWithMountFlags(&fref, TRUE, &targetIsFolder, &wasAliased, kResolveAliasFileNoUI);
    if (err != noErr) {
        return newPath;
    }

    if (wasAliased) {
        CFURLRef URL = CFURLCreateFromFSRef(kCFAllocatorDefault, &fref);
        newPath = [(NSURL *)URL path];
        CFRelease(URL);
    }

    return newPath;
}

- initWithPath:(NSString *)pluginPath
{
    [super init];
    extensionToMIME = [[NSMutableDictionary alloc] init];
    path = [[self pathByResolvingSymlinksAndAliasesInPath:pluginPath] retain];
    bundle = [[NSBundle alloc] initWithPath:path];
    lastModifiedDate = [[[[NSFileManager defaultManager] fileAttributesAtPath:path traverseLink:YES] objectForKey:NSFileModificationDate] retain];
    return self;
}

- (BOOL)getPluginInfoFromBundleAndMIMEDictionary:(NSDictionary *)MIMETypes
{
    if (!bundle) {
        return NO;
    }
    
    if (!MIMETypes) {
        MIMETypes = [bundle objectForInfoDictionaryKey:WebPluginMIMETypesKey];
        if (!MIMETypes) {
            return NO;
        }
    }

    NSMutableDictionary *MIMEToExtensionsDictionary = [NSMutableDictionary dictionary];
    NSMutableDictionary *MIMEToDescriptionDictionary = [NSMutableDictionary dictionary];
    NSEnumerator *keyEnumerator = [MIMETypes keyEnumerator];
    NSDictionary *MIMEDictionary;
    NSString *MIME, *description;
    NSArray *extensions;

    while ((MIME = [keyEnumerator nextObject]) != nil) {
        MIMEDictionary = [MIMETypes objectForKey:MIME];
        
        // FIXME: Consider storing disabled MIME types.
        NSNumber *isEnabled = [MIMEDictionary objectForKey:WebPluginTypeEnabledKey];
        if (isEnabled && [isEnabled boolValue] == NO) {
            continue;
        }

        extensions = [[MIMEDictionary objectForKey:WebPluginExtensionsKey] _web_lowercaseStrings];
        if ([extensions count] == 0) {
            extensions = [NSArray arrayWithObject:@""];
        }

        MIME = [MIME lowercaseString];

        [MIMEToExtensionsDictionary setObject:extensions forKey:MIME];

        description = [MIMEDictionary objectForKey:WebPluginTypeDescriptionKey];
        if (!description) {
            description = @"";
        }

        [MIMEToDescriptionDictionary setObject:description forKey:MIME];
    }

    [self setMIMEToExtensionsDictionary:MIMEToExtensionsDictionary];
    [self setMIMEToDescriptionDictionary:MIMEToDescriptionDictionary];

    NSString *filename = [self filename];

    NSString *theName = [bundle objectForInfoDictionaryKey:WebPluginNameKey];
    if (!theName) {
        theName = filename;
    }
    [self setName:theName];

    description = [bundle objectForInfoDictionaryKey:WebPluginDescriptionKey];
    if (!description) {
        description = filename;
    }
    [self setPluginDescription:description];

    return YES;
}

- (NSDictionary *)pListForPath:(NSString *)pListPath createFile:(BOOL)createFile
{
    if (createFile && [self load] && BP_CreatePluginMIMETypesPreferences) {
        BP_CreatePluginMIMETypesPreferences();
    }
    
    NSDictionary *pList = nil;
    NSData *data = [NSData dataWithContentsOfFile:pListPath];
    if (data) {
        pList = [NSPropertyListSerialization propertyListFromData:data
                                                 mutabilityOption:NSPropertyListImmutable
                                                           format:nil
                                                 errorDescription:nil];
    }
    
    return pList;
}

- (BOOL)getPluginInfoFromPLists
{
    if (!bundle) {
        return NO;
    }
    
    NSDictionary *MIMETypes = nil;
    NSString *pListFilename = [bundle objectForInfoDictionaryKey:WebPluginMIMETypesFilenameKey];
    
    // Check if the MIME types are claimed in a plist in the user's preferences directory.
    if (pListFilename) {
        NSString *pListPath = [NSString stringWithFormat:@"%@/Library/Preferences/%@", NSHomeDirectory(), pListFilename];
        NSDictionary *pList = [self pListForPath:pListPath createFile:NO];
        if (pList) {
            // If the plist isn't localized, have the plug-in recreate it in the preferred language.
            NSString *localizationName = [pList objectForKey:WebPluginLocalizationNameKey];
            if (![localizationName isEqualToString:[[self class] preferredLocalizationName]]) {
                pList = [self pListForPath:pListPath createFile:YES];
            }
            MIMETypes = [pList objectForKey:WebPluginMIMETypesKey];
        } else {
            // Plist doesn't exist, ask the plug-in to create it.
            MIMETypes = [[self pListForPath:pListPath createFile:YES] objectForKey:WebPluginMIMETypesKey];
        }
    }
    
    // Pass the MIME dictionary to the superclass to parse it.
    return [self getPluginInfoFromBundleAndMIMEDictionary:MIMETypes];
}

- (BOOL)isLoaded
{
    return isLoaded;
}

- (BOOL)load
{
    if (isLoaded && bundle != nil && BP_CreatePluginMIMETypesPreferences == NULL) {
        BP_CreatePluginMIMETypesPreferences = (BP_CreatePluginMIMETypesPreferencesFuncPtr)CFBundleGetFunctionPointerForName([bundle _cfBundle], CFSTR("BP_CreatePluginMIMETypesPreferences"));
    }
    return isLoaded;
}

- (void)unload
{
}

- (void)dealloc
{
    [self unload];
    
    [name release];
    [path release];
    [pluginDescription release];

    [MIMEToDescription release];
    [MIMEToExtensions release];
    [extensionToMIME release];

    [bundle release];

    [lastModifiedDate release];
    
    [super dealloc];
}

- (void)finalize
{
    // FIXME: Bad design to unload at dealloc/finalize time.
    // Must be fixed for GC.
    [self unload];
    [super finalize];
}

- (NSString *)name
{
    return name;
}

- (NSString *)path
{
    return path;
}

- (NSString *)filename
{
    return [path lastPathComponent];
}

- (NSString *)pluginDescription
{
    return pluginDescription;
}

- (NSEnumerator *)extensionEnumerator
{
    return [extensionToMIME keyEnumerator];
}

- (NSEnumerator *)MIMETypeEnumerator
{
    return [MIMEToExtensions keyEnumerator];
}

- (NSString *)descriptionForMIMEType:(NSString *)MIMEType
{
    return [MIMEToDescription objectForKey:MIMEType];
}

- (NSString *)MIMETypeForExtension:(NSString *)extension
{
    return [extensionToMIME objectForKey:extension];
}

- (NSArray *)extensionsForMIMEType:(NSString *)MIMEType
{
    return [MIMEToExtensions objectForKey:MIMEType];
}

- (NSBundle *)bundle
{
    return bundle;
}

- (NSDate *)lastModifiedDate
{
    return lastModifiedDate;
}

- (void)setName:(NSString *)theName
{
    [name release];
    name = [theName retain];
}

- (void)setPath:(NSString *)thePath
{
    [path release];
    path = [thePath retain];
}

- (void)setPluginDescription:(NSString *)description
{
    [pluginDescription release];
    pluginDescription = [description retain];
}

- (void)setMIMEToDescriptionDictionary:(NSDictionary *)MIMEToDescriptionDictionary
{
    [MIMEToDescription release];
    MIMEToDescription = [MIMEToDescriptionDictionary retain];
}

- (void)setMIMEToExtensionsDictionary:(NSDictionary *)MIMEToExtensionsDictionary
{
    [MIMEToExtensions release];
    MIMEToExtensions = [MIMEToExtensionsDictionary retain];

    // Reverse the mapping
    [extensionToMIME removeAllObjects];

    NSEnumerator *MIMEEnumerator = [MIMEToExtensions keyEnumerator], *extensionEnumerator;
    NSString *MIME, *extension;
    NSArray *extensions;
    
    while ((MIME = [MIMEEnumerator nextObject]) != nil) {
        extensions = [MIMEToExtensions objectForKey:MIME];
        extensionEnumerator = [extensions objectEnumerator];

        while ((extension = [extensionEnumerator nextObject]) != nil) {
            if (![extension isEqualToString:@""]) {
                [extensionToMIME setObject:MIME forKey:extension];
            }
        }
    }
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"name: %@\npath: %@\nmimeTypes:\n%@\npluginDescription:%@",
        name, path, [MIMEToExtensions description], [MIMEToDescription description], pluginDescription];
}

- (BOOL)isEqual:(id)object
{
    return ([object isKindOfClass:[WebBasePluginPackage class]] &&
            [[object name] isEqualToString:name] &&
            [[object lastModifiedDate] isEqual:lastModifiedDate]);
}

- (unsigned)hash
{
    return [[name stringByAppendingString:[lastModifiedDate description]] hash];
}

- (BOOL)isQuickTimePlugIn
{
    NSString *bundleIdentifier = [[self bundle] bundleIdentifier];
    return [bundleIdentifier _web_isCaseInsensitiveEqualToString:QuickTimeCarbonPluginIdentifier] || 
        [bundleIdentifier _web_isCaseInsensitiveEqualToString:QuickTimeCocoaPluginIdentifier];
}

- (BOOL)isJavaPlugIn
{
    NSString *bundleIdentifier = [[self bundle] bundleIdentifier];
    return [bundleIdentifier _web_isCaseInsensitiveEqualToString:JavaCocoaPluginIdentifier] || 
        [bundleIdentifier _web_isCaseInsensitiveEqualToString:JavaCarbonPluginIdentifier] ||
        [[path lastPathComponent] _web_isCaseInsensitiveEqualToString:JavaCFMPluginFilename];
}

@end

@implementation NSArray (WebPluginExtensions)

- (NSArray *)_web_lowercaseStrings
{
    NSMutableArray *lowercaseStrings = [NSMutableArray arrayWithCapacity:[self count]];
    NSEnumerator *strings = [self objectEnumerator];
    NSString *string;

    while ((string = [strings nextObject]) != nil) {
        if ([string isKindOfClass:[NSString class]]) {
            [lowercaseStrings addObject:[string lowercaseString]];
        }
    }

    return lowercaseStrings;
}

@end;
