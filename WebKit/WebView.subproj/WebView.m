/*	
    WebView.m
    Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebControllerSets.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultPolicyDelegate.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebException.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginDatabase.h>
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebPreferencesPrivate.h>
#import <WebKit/WebResourceLoadDelegate.h>
#import <WebKit/WebTextView.h>
#import <WebKit/WebTextRepresentation.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <WebKit/WebAssertions.h>
#import <Foundation/NSUserDefaults_NSURLExtras.h>
#import <Foundation/NSURLConnection.h>

static const struct UserAgentSpoofTableEntry *_web_findSpoofTableEntry(const char *, unsigned);

// Turn off inlining to avoid warning with newer gcc.
#undef __inline
#define __inline
#include "WebUserAgentSpoofTable.c"
#undef __inline

NSString *WebElementFrameKey = 			@"WebElementFrame";
NSString *WebElementImageKey = 			@"WebElementImage";
NSString *WebElementImageAltStringKey = 	@"WebElementImageAltString";
NSString *WebElementImageRectKey = 		@"WebElementImageRect";
NSString *WebElementImageURLKey = 		@"WebElementImageURL";
NSString *WebElementIsSelectedKey = 	        @"WebElementIsSelected";
NSString *WebElementLinkURLKey = 		@"WebElementLinkURL";
NSString *WebElementLinkTargetFrameKey =	@"WebElementTargetFrame";
NSString *WebElementLinkLabelKey = 		@"WebElementLinkLabel";
NSString *WebElementLinkTitleKey = 		@"WebElementLinkTitle";

NSString *WebViewProgressStartedNotification = @"WebProgressStartedNotification";
NSString *WebViewProgressEstimateChangedNotification = @"WebProgressEstimateChangedNotification";
NSString *WebViewProgressFinishedNotification = @"WebProgressFinishedNotification";

enum { WebViewVersion = 1 };


@implementation WebView

+ (BOOL)canShowMIMEType:(NSString *)MIMEType
{
    Class viewClass = [WebFrameView _viewClassForMIMEType:MIMEType];
    Class repClass = [WebDataSource _representationClassForMIMEType:MIMEType];

    if (!viewClass || !repClass) {
	[[WebPluginDatabase installedPlugins] loadPluginIfNeededForMIMEType: MIMEType];
        viewClass = [WebFrameView _viewClassForMIMEType:MIMEType];
        repClass = [WebDataSource _representationClassForMIMEType:MIMEType];
    }
    
    // Special-case WebTextView for text types that shouldn't be shown.
    if (viewClass && repClass) {
        if (viewClass == [WebTextView class] &&
            repClass == [WebTextRepresentation class] &&
            [[WebTextView unsupportedTextMIMETypes] containsObject:MIMEType]) {
            return NO;
        }
        return YES;
    }
    
    return NO;
}

+ (BOOL)canShowMIMETypeAsHTML:(NSString *)MIMEType
{
    return [WebFrameView _canShowMIMETypeAsHTML:MIMEType];
}

- (void)_commonInitializationFrameName:(NSString *)frameName groupName:(NSString *)groupName
{
    NSRect f = [self frame];
    WebFrameView *wv = [[WebFrameView alloc] initWithFrame: NSMakeRect(0,0,f.size.width,f.size.height)];
    [wv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: wv];
    [wv release];

    _private = [[WebViewPrivate alloc] init];
    _private->mainFrame = [[WebFrame alloc] initWithName: frameName webFrameView: wv  webView: self];
    [self setGroupName:groupName];

    [self setMaintainsBackForwardList: YES];

    ++WebViewCount;

    [self _updateWebCoreSettingsFromPreferences: [WebPreferences standardPreferences]];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                 name:WebPreferencesChangedNotification object:[self preferences]];

    [self _registerDraggedTypes];
}

- init
{
    return [self initWithFrame: NSZeroRect frameName: nil groupName: nil];
}

- initWithFrame: (NSRect)f
{
    [self initWithFrame: f frameName:nil groupName:nil];
    return self;
}

- initWithFrame: (NSRect)f frameName: (NSString *)frameName groupName: (NSString *)groupName;
{
    [super initWithFrame: f];
    [self _commonInitializationFrameName:frameName groupName:groupName];
    return self;
}

- (id)initWithCoder:(NSCoder *)decoder
{
    volatile id result = nil;

NS_DURING

    result = [super initWithCoder:decoder];
    // We don't want any of the archived subviews.
    [[result subviews] makeObjectsPerformSelector:@selector(removeFromSuperview)];
    
    if ([decoder allowsKeyedCoding]){
        NSString *frameName = [decoder decodeObjectForKey:@"FrameName"];
        NSString *groupName = [decoder decodeObjectForKey:@"GroupName"];
        [result _commonInitializationFrameName:frameName groupName:groupName];
        [result setPreferences: [decoder decodeObjectForKey:@"Preferences"]];
    }
    else {
        int version;
    
        [decoder decodeValueOfObjCType:@encode(int) at:&version];
        if (version == 1){
            NSString *frameName = [decoder decodeObject];
            NSString *groupName = [decoder decodeObject];
            [result _commonInitializationFrameName:frameName groupName:groupName];
            [result setPreferences: [decoder decodeObject]];
        }
    }
    
NS_HANDLER

    result = nil;
    [self release];

NS_ENDHANDLER

    return result;
}

- (void)encodeWithCoder:(NSCoder *)encoder
{
    [super encodeWithCoder:encoder];

    if ([encoder allowsKeyedCoding]){
        [encoder encodeObject:[[self mainFrame] name] forKey:@"FrameName"];
        [encoder encodeObject:[self groupName] forKey:@"GroupName"];
        [encoder encodeObject:[self preferences] forKey:@"Preferences"];
    }
    else {
        int version = WebViewVersion;
        [encoder encodeValueOfObjCType:@encode(int) at:&version];
        [encoder encodeObject:[[self mainFrame] name]];
        [encoder encodeObject:[self groupName]];
        [encoder encodeObject:[self preferences]];
    }
}

- (void)dealloc
{
    [self _close];
    
    --WebViewCount;
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [WebPreferences _removeReferenceForIdentifier: [self preferencesIdentifier]];
    
    [_private release];
    [super dealloc];
}

- (void)setPreferences: (WebPreferences *)prefs
{
    if (_private->preferences != prefs){
        [[NSNotificationCenter defaultCenter] removeObserver: self name: WebPreferencesChangedNotification object: [self preferences]];
        [WebPreferences _removeReferenceForIdentifier: [_private->preferences identifier]];
        [_private->preferences release];
        _private->preferences = [prefs retain];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_preferencesChangedNotification:)
                                                    name:WebPreferencesChangedNotification object:[self preferences]];
    }
}

- (WebPreferences *)preferences
{
    return _private->preferences ? _private->preferences : [WebPreferences standardPreferences];
}

- (void)setPreferencesIdentifier:(NSString *)anIdentifier
{
    if (![anIdentifier isEqual: [[self preferences] identifier]]){
        [self setPreferences: [[WebPreferences alloc] initWithIdentifier:anIdentifier]];
    }
}

- (NSString *)preferencesIdentifier
{
    return [[self preferences] identifier];
}


- (void)setUIDelegate:delegate
{
    _private->UIDelegate = delegate;
    [_private->UIDelegateForwarder release];
    _private->UIDelegateForwarder = nil;
}

- UIDelegate
{
    return _private->UIDelegate;
}

- (void)setResourceLoadDelegate: delegate
{
    _private->resourceProgressDelegate = delegate;
    [_private->resourceProgressDelegateForwarder release];
    _private->resourceProgressDelegateForwarder = nil;
    [self _cacheResourceLoadDelegateImplementations];
}


- resourceLoadDelegate
{
    return _private->resourceProgressDelegate;
}


- (void)setDownloadDelegate: delegate
{
    _private->downloadDelegate = delegate;
}


- downloadDelegate
{
    return _private->downloadDelegate;
}

- (void)setPolicyDelegate:delegate
{
    _private->policyDelegate = delegate;
    [_private->policyDelegateForwarder release];
    _private->policyDelegateForwarder = nil;
}

- policyDelegate
{
    return _private->policyDelegate;
}

- (void)setFrameLoadDelegate:delegate
{
    _private->frameLoadDelegate = delegate;
    [_private->frameLoadDelegateForwarder release];
    _private->frameLoadDelegateForwarder = nil;
}

- frameLoadDelegate
{
    return _private->frameLoadDelegate;
}

- (WebFrame *)mainFrame
{
    return _private->mainFrame;
}

- (WebBackForwardList *)backForwardList
{
    if (_private->useBackForwardList)
        return _private->backForwardList;
    return nil;
}

- (void)setMaintainsBackForwardList: (BOOL)flag
{
    _private->useBackForwardList = flag;
}

- (BOOL)goBack
{
    WebHistoryItem *item = [[self backForwardList] backItem];
    
    if (item){
        [self _goToItem: item withLoadType: WebFrameLoadTypeBack];
        return YES;
    }
    return NO;
}

- (BOOL)goForward
{
    WebHistoryItem *item = [[self backForwardList] forwardItem];
    
    if (item){
        [self _goToItem: item withLoadType: WebFrameLoadTypeForward];
        return YES;
    }
    return NO;
}

- (BOOL)goToBackForwardItem:(WebHistoryItem *)item
{
    [self _goToItem: item withLoadType: WebFrameLoadTypeIndexedBackForward];
    return YES;
}

- (void)setTextSizeMultiplier:(float)m
{
    if (_private->textSizeMultiplier == m) {
        return;
    }
    _private->textSizeMultiplier = m;
    [[self mainFrame] _textSizeMultiplierChanged];
}

- (float)textSizeMultiplier
{
    return _private->textSizeMultiplier;
}

- (void)setApplicationNameForUserAgent:(NSString *)applicationName
{
    NSString *name = [applicationName copy];
    [_private->applicationNameForUserAgent release];
    _private->applicationNameForUserAgent = name;
    int i;
    for (i = 0; i != NumUserAgentStringTypes; ++i) {
        [_private->userAgent[i] release];
        _private->userAgent[i] = nil;
    }
}

- (NSString *)applicationNameForUserAgent
{
    return [[_private->applicationNameForUserAgent retain] autorelease];
}

- (void)setCustomUserAgent:(NSString *)userAgentString
{
    NSString *override = [userAgentString copy];
    [_private->userAgentOverride release];
    _private->userAgentOverride = override;
}

- (NSString *)customUserAgent
{
    return [[_private->userAgentOverride retain] autorelease];
}

- (BOOL)supportsTextEncoding
{
    id documentView = [[[self mainFrame] frameView] documentView];
    return [documentView conformsToProtocol:@protocol(WebDocumentText)]
        && [documentView supportsTextEncoding];
}

- (void)setCustomTextEncodingName:(NSString *)encoding
{
    NSString *oldEncoding = [self customTextEncodingName];
    if (encoding == oldEncoding || [encoding isEqualToString:oldEncoding]) {
        return;
    }
    [[self mainFrame] _reloadAllowingStaleDataWithOverrideEncoding:encoding];
}

- (NSString *)_mainFrameOverrideEncoding
{
    WebDataSource *dataSource = [[self mainFrame] provisionalDataSource];
    if (dataSource == nil) {
        dataSource = [[self mainFrame] dataSource];
    }
    if (dataSource == nil) {
        return nil;
    }
    return [dataSource _overrideEncoding];
}

- (NSString *)customTextEncodingName
{
    return [self _mainFrameOverrideEncoding];
}

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script
{
    return [[[self mainFrame] _bridge] stringByEvaluatingJavaScriptFromString:script];
}

// Get the appropriate user-agent string for a particular URL.
- (NSString *)userAgentForURL:(NSURL *)URL
{
    if (_private->userAgentOverride) {
        return [[_private->userAgentOverride retain] autorelease];
    }
    
    // Look to see if we need to spoof.
    // First step is to get the host as a C-format string.
    UserAgentStringType type = Safari;
    NSString *host = [URL host];
    char hostBuffer[256];
    if (host && CFStringGetCString((CFStringRef)host, hostBuffer, sizeof(hostBuffer), kCFStringEncodingASCII)) {
        // Next step is to find the last part of the name.
        // We get the part with only two dots, and the part with only one dot.
        const char *thirdToLastPeriod = NULL;
        const char *nextToLastPeriod = NULL;
        const char *lastPeriod = NULL;
        char c;
        char *p = hostBuffer;
        while ((c = *p)) {
            if (c == '.') {
                thirdToLastPeriod = nextToLastPeriod;
                nextToLastPeriod = lastPeriod;
                lastPeriod = p;
            } else {
                *p = tolower(c);
            }
            ++p;
        }
        // Now look up that last part in the gperf spoof table.
        if (lastPeriod) {
            const char *lastPart;
            const struct UserAgentSpoofTableEntry *entry = NULL;
            if (nextToLastPeriod) {
                lastPart = thirdToLastPeriod ? thirdToLastPeriod + 1 : hostBuffer;
                entry = _web_findSpoofTableEntry(lastPart, p - lastPart);
            }
            if (!entry) {
                lastPart = nextToLastPeriod ? nextToLastPeriod + 1 : hostBuffer;
                entry = _web_findSpoofTableEntry(lastPart, p - lastPart);
            }
            if (entry) {
                type = entry->type;
            }
        }
    }

    NSString **userAgentStorage = &_private->userAgent[type];

    NSString *userAgent = *userAgentStorage;
    if (userAgent) {
        return [[userAgent retain] autorelease];
    }
    
    // FIXME: Some day we will start reporting the actual CPU here instead of hardcoding PPC.

    NSString *language = [NSUserDefaults _web_preferredLanguageCode];
    id sourceVersion = [[NSBundle bundleForClass:[WebView class]]
        objectForInfoDictionaryKey:(id)kCFBundleVersionKey];
    NSString *applicationName = _private->applicationNameForUserAgent;

    switch (type) {
        case Safari:
            if ([applicationName length]) {
                userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko) %@",
                    language, sourceVersion, applicationName];
            } else {
                userAgent = [NSString stringWithFormat:@"Mozilla/5.0 (Macintosh; U; PPC Mac OS X; %@) AppleWebKit/%@ (KHTML, like Gecko)",
                    language, sourceVersion];
            }
            break;
        case MacIE:
            if ([applicationName length]) {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 5.2; Mac_PowerPC) AppleWebKit/%@ %@",
                    language, sourceVersion, applicationName];
            } else {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 5.2; Mac_PowerPC) AppleWebKit/%@",
                    language, sourceVersion];
            }
            break;
        case WinIE:
            if ([applicationName length]) {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT) AppleWebKit/%@ %@",
                    language, sourceVersion, applicationName];
            } else {
                userAgent = [NSString stringWithFormat:@"Mozilla/4.0 (compatible; MSIE 6.0; Windows NT) AppleWebKit/%@",
                    language, sourceVersion];
            }
            break;
    }

    *userAgentStorage = [userAgent retain];
    return userAgent;
}

- (void)setHostWindow:(NSWindow *)hostWindow
{
    if (hostWindow != _private->hostWindow) {
        [[self mainFrame] _viewWillMoveToHostWindow:hostWindow];
        [_private->hostWindow release];
        _private->hostWindow = [hostWindow retain];
        [[self mainFrame] _viewDidMoveToHostWindow];
    }
}

- (NSWindow *)hostWindow
{
    return _private->hostWindow;
}


- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    return [self _web_dragOperationForDraggingInfo:sender];
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    NSURL *URL = [[sender draggingPasteboard] _web_bestURL];

    if (URL) {
        NSURLRequest *request = [[NSURLRequest alloc] initWithURL:URL];
        [[self mainFrame] loadRequest:request];
        [request release];
    }
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    if ([[self mainFrame] frameView]) {
        [[self window] makeFirstResponder:[[self mainFrame] frameView]];
    }
    return YES;
}

// Return the frame holding first responder
- (WebFrame *)_currentFrame
{
    // Find the frame holding the first responder, or holding the first form in the doc
    NSResponder *resp = [[self window] firstResponder];
    if (!resp || ![resp isKindOfClass:[NSView class]] || ![(NSView *)resp isDescendantOf:self]) {
        return nil;	// first responder outside our view tree
    } else {
        WebFrameView *frameView = (WebFrameView *)[(NSView *)resp _web_superviewOfClass:[WebFrameView class]];
        return [frameView webFrame];
    }
}

static WebFrame *incrementFrame(WebFrame *curr, BOOL forward, BOOL wrapFlag)
{
    return forward ? [curr _nextFrameWithWrap:wrapFlag]
                   : [curr _previousFrameWithWrap:wrapFlag];
}

// Search from the end of the currently selected location, or from the beginning of the
// document if nothing is selected.  Deals with subframes.
- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    // Get the frame holding the selection, or start with the main frame
    WebFrame *startFrame = [self _currentFrame];
    if (!startFrame) {
        startFrame = [self mainFrame];
    }

    // Search the first frame, then all the other frames, in order
    NSView <WebDocumentSearching> *startSearchView = nil;
    BOOL startHasSelection = NO;
    WebFrame *frame = startFrame;
    do {
        id <WebDocumentView> view = [[frame frameView] documentView];
        if ([view conformsToProtocol:@protocol(WebDocumentSearching)]) {
            NSView <WebDocumentSearching> *searchView = (NSView <WebDocumentSearching> *)view;

            // first time through
            if (frame == startFrame) {
                // Remember if start even has a selection, to know if we need to search more later
                if ([searchView isKindOfClass:[WebHTMLView class]]) {
                    // optimization for the common case, to avoid making giant string for selection
                    startHasSelection = [[startFrame _bridge] selectionStart] != nil;
                } else if ([searchView conformsToProtocol:@protocol(WebDocumentText)]) {
                    startHasSelection = [(id <WebDocumentText>)searchView selectedString] != nil;
                }
                startSearchView = searchView;
            }
            
            // Note at this point we are assuming the search will be done top-to-bottom,
            // not starting at any selection that exists.  See 3228554.
            BOOL success = [searchView searchFor:string direction:forward caseSensitive:caseFlag wrap:NO];
            if (success) {
                [[self window] makeFirstResponder:searchView];
                return YES;
            }
        }
        frame = incrementFrame(frame, forward, wrapFlag);
    } while (frame != startFrame);

    // Search contents of startFrame, on the other side of the selection that we did earlier.
    // We cheat a bit and just research with wrap on
    if (startHasSelection && startSearchView) {
        BOOL success = [startSearchView searchFor:string direction:forward caseSensitive:caseFlag wrap:YES];
        if (success) {
            [[self window] makeFirstResponder:startSearchView];
            return YES;
        }
    }
    return NO;
}

+ (void) registerViewClass:(Class)viewClass representationClass: (Class)representationClass forMIMEType:(NSString *)MIMEType
{
    [[WebFrameView _viewTypesAllowImageTypeOmission:YES] setObject:viewClass forKey:MIMEType];
    [[WebDataSource _repTypesAllowImageTypeOmission:YES] setObject:representationClass forKey:MIMEType];
}

- (void)setGroupName:(NSString *)groupName
{
    if (groupName != _private->setName){
        [_private->setName release];
        _private->setName = [groupName copy];
        [WebViewSets addWebView:self toSetNamed:_private->setName];
    }
}

- (NSString *)groupName
{
    return _private->setName;
}

- (double)estimatedProgress
{
    return _private->progressValue;
}

@end


@implementation WebView (WebIBActions)

- (IBAction)takeStringURLFrom: sender
{
    NSString *URLString = [sender stringValue];
    
    [[self mainFrame] loadRequest: [NSURLRequest requestWithURL: [NSURL _web_URLWithDataAsString: URLString]]];
}

- (BOOL)canGoBack
{
    return [[self backForwardList] backItem] != nil;
}

- (BOOL)canGoForward
{
    return [[self backForwardList] forwardItem] != nil;
}

- (IBAction)goBack:(id)sender
{
    [self goBack];
}

- (IBAction)goForward:(id)sender
{
    [self goForward];
}

- (IBAction)stopLoading:(id)sender
{
    [[self mainFrame] stopLoading];
}

- (IBAction)reload:(id)sender
{
    [[self mainFrame] reload];
}

#define MinimumTextSizeMultiplier	0.5
#define MaximumTextSizeMultiplier	3.0
#define TextSizeMultiplierRatio		1.2

- (BOOL)canMakeTextSmaller
{
    if ([[self mainFrame] dataSource] == nil) {
        return NO;
    }
    if ([self textSizeMultiplier]/TextSizeMultiplierRatio < MinimumTextSizeMultiplier) {
        return NO;
    }
    return YES;
}

- (BOOL)canMakeTextLarger
{
    if ([[self mainFrame] dataSource] == nil) {
        return NO;
    }
    if ([self textSizeMultiplier]*TextSizeMultiplierRatio > MaximumTextSizeMultiplier) {
        return NO;
    }
    return YES;
}

- (IBAction)makeTextSmaller:(id)sender
{
    if (![self canMakeTextSmaller]) {
        return;
    }
    [self setTextSizeMultiplier:[self textSizeMultiplier]/TextSizeMultiplierRatio];
}

- (IBAction)makeTextLarger:(id)sender
{
    if (![self canMakeTextLarger]) {
        return;
    }
    [self setTextSizeMultiplier:[self textSizeMultiplier]*TextSizeMultiplierRatio];
}

- (BOOL)_isLoading
{
    WebFrame *mainFrame = [self mainFrame];
    return [[mainFrame dataSource] isLoading]
        || [[mainFrame provisionalDataSource] isLoading];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];

    if (action == @selector(goBack:)) {
	return [self canGoBack];
    } else if (action == @selector(goForward:)) {
	return [self canGoForward];
    } else if (action == @selector(makeTextLarger:)) {
	return [self canMakeTextLarger];
    } else if (action == @selector(makeTextSmaller:)) {
	return [self canMakeTextSmaller];
    } else if (action == @selector(reload:)) {
	return [[self mainFrame] dataSource] != nil;
    } else if (action == @selector(stopLoading:)) {
	return [self _isLoading];
    }

    return YES;
}


@end


@implementation WebView (WebPendingPublic)

- (void)setMainFrameURL:(NSString *)URLString
{
    [[self mainFrame] loadRequest: [NSURLRequest requestWithURL: [NSURL _web_URLWithDataAsString: URLString]]];
}

- (NSString *)mainFrameURL
{
    WebDataSource *ds;
    ds = [[self mainFrame] provisionalDataSource];
    if (!ds)
        ds = [[self mainFrame] dataSource];
    return [[[ds request] URL] _web_originalDataAsString];
}

- (BOOL)isLoading
{
    return [[[self mainFrame] dataSource] isLoading];
}

- (NSString *)mainFrameTitle
{
    NSString *mainFrameTitle = [[[self mainFrame] dataSource] pageTitle];
    return (mainFrameTitle != nil) ? mainFrameTitle : @"";
}

- (NSImage *)mainFrameIcon
{
    return [[WebIconDatabase sharedIconDatabase] iconForURL:[[[[self mainFrame] dataSource] _URL] _web_originalDataAsString] withSize:WebIconSmallSize];
}

@end
