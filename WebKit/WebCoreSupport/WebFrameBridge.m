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

#import "WebFrameBridge.h"

#import "WebBackForwardList.h"
#import "WebBaseNetscapePluginView.h"
#import "WebBasePluginPackage.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultUIDelegate.h"
#import "WebEditingDelegate.h"
#import "WebFormDelegate.h"
#import "WebFrameInternal.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentationPrivate.h"
#import "WebHTMLViewInternal.h"
#import "WebHistoryItemPrivate.h"
#import "WebIconDatabase.h"
#import "WebIconDatabasePrivate.h"
#import "WebJavaPlugIn.h"
#import "WebJavaScriptTextInputPanel.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitStatisticsPrivate.h"
#import "WebKitSystemBits.h"
#import "WebLocalizableStrings.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSViewExtras.h"
#import "WebNetscapePluginEmbeddedView.h"
#import "WebNetscapePluginPackage.h"
#import "WebNullPluginView.h"
#import "WebPageBridge.h"
#import "WebPlugin.h"
#import "WebPluginController.h"
#import "WebPluginDatabase.h"
#import "WebPluginPackage.h"
#import "WebPluginViewFactoryPrivate.h"
#import "WebPreferencesPrivate.h"
#import "WebResourcePrivate.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLResponse.h>
#import <JavaScriptCore/Assertions.h>
#import <JavaVM/jni.h>
#import <WebCore/WebCoreFrameNamespaces.h>
#import <WebCore/WebDocumentLoader.h>
#import <WebCore/WebFormDataStream.h>
#import <WebCore/WebFrameLoader.h>
#import <WebCore/WebFrameLoaderClient.h>
#import <WebCore/WebLoader.h>
#import <WebCore/WebSubresourceLoader.h>
#import <WebKitSystemInterface.h>

// For compatibility only with old SPI. 
@interface NSObject (OldWebPlugin)
- (void)setIsSelected:(BOOL)f;
@end

@interface NSApplication (DeclarationStolenFromAppKit)
- (void)_cycleWindowsReversed:(BOOL)reversed;
@end

@interface NSView (AppKitSecretsWebBridgeKnowsAbout)
- (NSView *)_findLastViewInKeyViewLoop;
@end

@interface NSURLResponse (FoundationSecretsWebBridgeKnowsAbout)
- (NSTimeInterval)_calculatedExpiration;
@end

@interface NSView (JavaPluginSecrets)
- (jobject)pollForAppletInWindow:(NSWindow *)window;
@end

NSString *WebPluginBaseURLKey =     @"WebPluginBaseURL";
NSString *WebPluginAttributesKey =  @"WebPluginAttributes";
NSString *WebPluginContainerKey =   @"WebPluginContainer";

@implementation WebFrameBridge

- (WebView *)webView
{
    ASSERT([[self page] isKindOfClass:[WebPageBridge class]]);
    return [(WebPageBridge *)[self page] webView];
}

- (id)initMainFrameWithPage:(WebPageBridge *)page frameName:(NSString *)name view:(WebFrameView *)view
{
    self = [super initMainFrameWithPage:page];
    _frame = [[WebFrame alloc] _initWithWebFrameView:view webView:[self webView] bridge:self];

    ++WebBridgeCount;

    [self setName:name];
    [self initializeSettings:[[self webView] _settings]];
    [self setTextSizeMultiplier:[[self webView] textSizeMultiplier]];

    return self;
}

- (id)initSubframeWithOwnerElement:(WebCoreElement *)ownerElement frameName:(NSString *)name view:(WebFrameView *)view
{
    self = [super initSubframeWithOwnerElement:ownerElement];
    _frame = [[WebFrame alloc] _initWithWebFrameView:view webView:[self webView] bridge:self];

    ++WebBridgeCount;

    [self setName:name];
    [self initializeSettings:[[self webView] _settings]];
    [self setTextSizeMultiplier:[[self webView] textSizeMultiplier]];

    return self;
}

#define KeyboardUIModeDidChangeNotification @"com.apple.KeyboardUIModeDidChange"
#define AppleKeyboardUIMode CFSTR("AppleKeyboardUIMode")
#define UniversalAccessDomain CFSTR("com.apple.universalaccess")

- (void)fini
{
    if (_keyboardUIModeAccessed) {
        [[NSDistributedNotificationCenter defaultCenter] 
            removeObserver:self name:KeyboardUIModeDidChangeNotification object:nil];
        [[NSNotificationCenter defaultCenter] 
            removeObserver:self name:WebPreferencesChangedNotification object:nil];
    }
    ASSERT(_frame == nil);
    --WebBridgeCount;
}

- (void)dealloc
{
    [lastDashboardRegions release];
    [_frame release];
    
    [self fini];
    [super dealloc];
}

- (void)finalize
{
    [self fini];
    [super finalize];
}

- (WebPreferences *)_preferences
{
    return [[self webView] preferences];
}

- (void)_retrieveKeyboardUIModeFromPreferences:(NSNotification *)notification
{
    CFPreferencesAppSynchronize(UniversalAccessDomain);

    Boolean keyExistsAndHasValidFormat;
    int mode = CFPreferencesGetAppIntegerValue(AppleKeyboardUIMode, UniversalAccessDomain, &keyExistsAndHasValidFormat);
    
    // The keyboard access mode is reported by two bits:
    // Bit 0 is set if feature is on
    // Bit 1 is set if full keyboard access works for any control, not just text boxes and lists
    // We require both bits to be on.
    // I do not know that we would ever get one bit on and the other off since
    // checking the checkbox in system preferences which is marked as "Turn on full keyboard access"
    // turns on both bits.
    _keyboardUIMode = (mode & 0x2) ? WebCoreKeyboardAccessFull : WebCoreKeyboardAccessDefault;
    
    // check for tabbing to links
    if ([[self _preferences] tabsToLinks])
        _keyboardUIMode = (WebCoreKeyboardUIMode)(_keyboardUIMode | WebCoreKeyboardAccessTabsToLinks);
}

- (WebCoreKeyboardUIMode)keyboardUIMode
{
    if (!_keyboardUIModeAccessed) {
        _keyboardUIModeAccessed = YES;
        [self _retrieveKeyboardUIModeFromPreferences:nil];
        
        [[NSDistributedNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_retrieveKeyboardUIModeFromPreferences:) 
            name:KeyboardUIModeDidChangeNotification object:nil];

        [[NSNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_retrieveKeyboardUIModeFromPreferences:) 
                   name:WebPreferencesChangedNotification object:nil];
    }
    return _keyboardUIMode;
}

- (WebFrame *)webFrame
{
    return _frame;
}

- (WebCoreFrameBridge *)mainFrame
{
    ASSERT(_frame != nil);
    return [[[self webView] mainFrame] _bridge];
}

- (NSView *)documentView
{
    ASSERT(_frame != nil);
    return [[_frame frameView] documentView];
}

- (WebCorePageBridge *)createWindowWithURL:(NSURL *)URL
{
    ASSERT(_frame != nil);

    NSMutableURLRequest *request = nil;
    if (URL != nil && ![URL _web_isEmpty]) {
        request = [NSMutableURLRequest requestWithURL:URL];
        [request _web_setHTTPReferrer:[self referrer]];
    }

    WebView *currentWebView = [self webView];
    id wd = [currentWebView UIDelegate];
    WebView *newWebView;
    if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        newWebView = [wd webView:currentWebView createWebViewWithRequest:request];
    else
        newWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:currentWebView createWebViewWithRequest:request];
    return [newWebView _pageBridge];
}

- (void)showWindow
{
    WebView *wv = [self webView];
    [[wv _UIDelegateForwarder] webViewShow:wv];
}

- (BOOL)areToolbarsVisible
{
    ASSERT(_frame != nil);
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    if ([wd respondsToSelector:@selector(webViewAreToolbarsVisible:)])
        return [wd webViewAreToolbarsVisible:wv];
    return [[WebDefaultUIDelegate sharedUIDelegate] webViewAreToolbarsVisible:wv];
}

- (void)setToolbarsVisible:(BOOL)visible
{
    ASSERT(_frame != nil);
    WebView *wv = [self webView];
    [[wv _UIDelegateForwarder] webView:wv setToolbarsVisible:visible];
}

- (BOOL)areScrollbarsVisible
{
    ASSERT(_frame != nil);
    return [[_frame frameView] allowsScrolling];
}

- (void)setScrollbarsVisible:(BOOL)visible
{
    ASSERT(_frame != nil);
    [[_frame frameView] setAllowsScrolling:visible];
}

- (BOOL)isStatusbarVisible
{
    ASSERT(_frame != nil);
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    if ([wd respondsToSelector:@selector(webViewIsStatusBarVisible:)])
        return [wd webViewIsStatusBarVisible:wv];
    return [[WebDefaultUIDelegate sharedUIDelegate] webViewIsStatusBarVisible:wv];
}

- (void)setStatusbarVisible:(BOOL)visible
{
    ASSERT(_frame != nil);
    WebView *wv = [self webView];
    [[wv _UIDelegateForwarder] webView:wv setStatusBarVisible:visible];
}

- (void)setWindowIsResizable:(BOOL)resizable
{
    ASSERT(_frame != nil);
    WebView *webView = [self webView];
    [[webView _UIDelegateForwarder] webView:webView setResizable:resizable];
}

- (BOOL)windowIsResizable
{
    ASSERT(_frame != nil);
    WebView *webView = [self webView];
    return [[webView _UIDelegateForwarder] webViewIsResizable:webView];
}

- (NSResponder *)firstResponder
{
    ASSERT(_frame != nil);
    WebView *webView = [self webView];
    return [[webView _UIDelegateForwarder] webViewFirstResponder:webView];
}

- (void)makeFirstResponder:(NSResponder *)view
{
    ASSERT(_frame != nil);
    WebView *webView = [self webView];
    [webView _pushPerformingProgrammaticFocus];
    [[webView _UIDelegateForwarder] webView:webView makeFirstResponder:view];
    [webView _popPerformingProgrammaticFocus];
}

- (void)willMakeFirstResponderForNodeFocus
{
    ASSERT([[[_frame frameView] documentView] isKindOfClass:[WebHTMLView class]]);
    [(WebHTMLView *)[[_frame frameView] documentView] _willMakeFirstResponderForNodeFocus];
}


- (BOOL)textViewWasFirstResponderAtMouseDownTime:(NSTextView *)textView;
{
    ASSERT(_frame != nil);
    NSView *documentView = [[_frame frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]]) {
        return NO;
    }
    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    return [webHTMLView _textViewWasFirstResponderAtMouseDownTime:textView];
}

- (void)closeWindowSoon
{
    WebView *parentWebView = [self webView];

    // We need to remove the parent WebView from WebViewSets here, before it actually
    // closes, to make sure that JavaScript code that executes before it closes
    // can't find it. Otherwise, window.open will select a closed WebView instead of 
    // opening a new one <rdar://problem/3572585>.

    // We also need to stop the load to prevent further parsing or JavaScript execution
    // after the window has torn down <rdar://problem/4161660>.
  
    // FIXME: This code assumes that the UI delegate will respond to a webViewClose
    // message by actually closing the WebView. Safari guarantees this behavior, but other apps might not.
    // This approach is an inherent limitation of not making a close execute immediately
    // after a call to window.close.
    
    [parentWebView setGroupName:nil];
    [parentWebView stopLoading:self];
    [parentWebView performSelector:@selector(_closeWindow) withObject:nil afterDelay:0.0];
}

- (NSWindow *)window
{
    ASSERT(_frame != nil);
    return [[_frame frameView] window];
}

- (void)runJavaScriptAlertPanelWithMessage:(NSString *)message
{
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    // Check whether delegate implements new version, then whether delegate implements old version. If neither,
    // fall back to shared delegate's implementation of new version.
    if ([wd respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:)])
        [wd webView:wv runJavaScriptAlertPanelWithMessage:message initiatedByFrame:_frame];
    else if ([wd respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:)])
        [wd webView:wv runJavaScriptAlertPanelWithMessage:message];
    else
        [[WebDefaultUIDelegate sharedUIDelegate] webView:wv runJavaScriptAlertPanelWithMessage:message initiatedByFrame:_frame];
}

- (BOOL)runJavaScriptConfirmPanelWithMessage:(NSString *)message
{
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    // Check whether delegate implements new version, then whether delegate implements old version. If neither,
    // fall back to shared delegate's implementation of new version.
    if ([wd respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:)])
        return [wd webView:wv runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:_frame];
    if ([wd respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:)])
        return [wd webView:wv runJavaScriptConfirmPanelWithMessage:message];    
    return [[WebDefaultUIDelegate sharedUIDelegate] webView:wv runJavaScriptConfirmPanelWithMessage:message initiatedByFrame:_frame];
}

- (BOOL)shouldInterruptJavaScript
{
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    
    if ([wd respondsToSelector:@selector(webViewShouldInterruptJavaScript:)])
        return [wd webViewShouldInterruptJavaScript:wv];
    return NO;
}

- (BOOL)canRunBeforeUnloadConfirmPanel
{
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    return [wd respondsToSelector:@selector(webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:)];
}

- (BOOL)runBeforeUnloadConfirmPanelWithMessage:(NSString *)message
{
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    if ([wd respondsToSelector:@selector(webView:runBeforeUnloadConfirmPanelWithMessage:initiatedByFrame:)])
        return [wd webView:wv runBeforeUnloadConfirmPanelWithMessage:message initiatedByFrame:_frame];
    return YES;
}

- (BOOL)runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText returningText:(NSString **)result
{
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    // Check whether delegate implements new version, then whether delegate implements old version. If neither,
    // fall back to shared delegate's implementation of new version.
    if ([wd respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:)])
        *result = [wd webView:wv runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText initiatedByFrame:_frame];
    else if ([wd respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:)])
        *result = [wd webView:wv runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText];
    else
        *result = [[WebDefaultUIDelegate sharedUIDelegate] webView:wv runJavaScriptTextInputPanelWithPrompt:prompt defaultText:defaultText initiatedByFrame:_frame];
    return *result != nil;
}

- (void)addMessageToConsole:(NSDictionary *)message
{
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    if ([wd respondsToSelector:@selector(webView:addMessageToConsole:)])
        [wd webView:wv addMessageToConsole:message];
}

- (void)runOpenPanelForFileButtonWithResultListener:(id<WebCoreOpenPanelResultListener>)resultListener
{
    WebView *wv = [self webView];
    [[wv _UIDelegateForwarder] webView:wv runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener];
}

- (WebDataSource *)dataSource
{
    ASSERT(_frame != nil);
    WebDataSource *dataSource = [_frame dataSource];

    ASSERT(dataSource != nil);

    return dataSource;
}

- (void)setStatusText:(NSString *)status
{
    ASSERT(_frame != nil);
    WebView *wv = [self webView];
    [[wv _UIDelegateForwarder] webView:wv setStatusText:status];
}

// We would like a better value for a maximum time_t,
// but there is no way to do that in C with any certainty.
// INT_MAX should work well enough for our purposes.
#define MAX_TIME_T ((time_t)INT_MAX)    

- (time_t)expiresTimeForResponse:(NSURLResponse *)response
{
    // This check can be removed when the new Foundation method
    // has been around long enough for everyone to have it.
    ASSERT([response respondsToSelector:@selector(_calculatedExpiration)]);

    NSTimeInterval expiration = [response _calculatedExpiration];
    expiration += kCFAbsoluteTimeIntervalSince1970;
    
    return expiration > MAX_TIME_T ? MAX_TIME_T : (time_t)expiration;
}

- (void)close
{
    [super close];
    [_frame release];
    _frame = nil;
}

- (void)activateWindow
{
    [[[self webView] _UIDelegateForwarder] webViewFocus:[self webView]];
}

- (void)deactivateWindow
{
   [[[self webView] _UIDelegateForwarder] webViewUnfocus:[self webView]];
}

- (void)formControlIsBecomingFirstResponder:(NSView *)formControl
{
    // When a form element becomes first responder, its enclosing WebHTMLView might need to
    // change its focus-displaying state, but isn't otherwise notified.
    [(WebHTMLView *)[[_frame frameView] documentView] _formControlIsBecomingFirstResponder:formControl];
}

- (void)formControlIsResigningFirstResponder:(NSView *)formControl
{
    // When a form element resigns first responder, its enclosing WebHTMLView might need to
    // change its focus-displaying state, but isn't otherwise notified.
    [(WebHTMLView *)[[_frame frameView] documentView] _formControlIsResigningFirstResponder:formControl];
}

- (WebCoreFrameBridge *)createChildFrameNamed:(NSString *)frameName 
                                      withURL:(NSURL *)URL
                                     referrer:(NSString *)referrer
                                   ownerElement:(WebCoreElement *)ownerElement
                              allowsScrolling:(BOOL)allowsScrolling 
                                  marginWidth:(int)width
                                 marginHeight:(int)height
{
    BOOL hideReferrer;
    if (![self canLoadURL:URL fromReferrer:referrer hideReferrer:&hideReferrer])
        return nil;

    ASSERT(_frame);
    
    WebFrameView *childView = [[WebFrameView alloc] initWithFrame:NSMakeRect(0,0,0,0)];
    [childView setAllowsScrolling:allowsScrolling];
    WebFrameBridge *newBridge = [[WebFrameBridge alloc] initSubframeWithOwnerElement:ownerElement frameName:frameName view:childView];
    [_frame _addChild:[newBridge webFrame]];
    [childView release];

    [childView _setMarginWidth:width];
    [childView _setMarginHeight:height];

    [newBridge release];

    if (!newBridge)
        return nil;

    [_frame _loadURL:URL referrer:(hideReferrer ? nil : referrer) intoChild:[newBridge webFrame]];

    return newBridge;
}

- (void)saveDocumentState:(NSArray *)documentState
{
    WebHistoryItem *item = [_frame _itemForSavingDocState];
    LOG(Loading, "%@: saving form state from to 0x%x", [_frame name], item);
    if (item) {
        [item setDocumentState:documentState];
        // You might think we could save the scroll state here too, but unfortunately this
        // often gets called after WebFrame::_transitionToCommitted has restored the scroll
        // position of the next document.
    }
}

- (NSArray *)documentState
{
    LOG(Loading, "%@: restoring form state from item 0x%x", [_frame name], [_frame _itemForRestoringDocState]);
    return [[_frame _itemForRestoringDocState] documentState];
}

- (BOOL)saveDocumentToPageCache:(id)documentInfo
{
    WebHistoryItem *item = [_frame _itemForSavingDocState];
    if (![item hasPageCache]) {
        return NO;
    }
    [[item pageCache] setObject:documentInfo forKey:WebCorePageCacheStateKey];
    return YES;
}

- (NSString *)userAgentForURL:(NSURL *)URL
{
    return [[self webView] userAgentForURL:URL];
}

- (BOOL)inNextKeyViewOutsideWebFrameViews
{
    return _inNextKeyViewOutsideWebFrameViews;
}

- (NSView *)_nextKeyViewOutsideWebFrameViewsWithValidityCheck:(BOOL)mustBeValid
{
    // We can get here in unusual situations such as the one listed in 4451831, so we
    // return nil to avoid an infinite recursion.
    if (_inNextKeyViewOutsideWebFrameViews)
        return nil;
    
    _inNextKeyViewOutsideWebFrameViews = YES;
    WebView *webView = [self webView];
    // Do not ask webView for its next key view, but rather, ask it for 
    // the next key view of the last view in its key view loop.
    // Doing so gives us the correct answer as calculated by AppKit, 
    // and makes HTML views behave like other views.
    NSView *lastViewInLoop = [webView _findLastViewInKeyViewLoop];
    NSView *nextKeyView = mustBeValid ? [lastViewInLoop nextValidKeyView] : [lastViewInLoop nextKeyView];
    _inNextKeyViewOutsideWebFrameViews = NO;
    return nextKeyView;
}

- (NSView *)nextKeyViewOutsideWebFrameViews
{
    return [self _nextKeyViewOutsideWebFrameViewsWithValidityCheck:NO];
}

- (NSView *)nextValidKeyViewOutsideWebFrameViews
{
    return [self _nextKeyViewOutsideWebFrameViewsWithValidityCheck:YES];
}

- (NSView *)previousKeyViewOutsideWebFrameViews
{
    WebView *webView = [self webView];
    NSView *previousKeyView = [webView previousKeyView];
    return previousKeyView;
}

- (BOOL)defersLoading
{
    return [[self webView] defersCallbacks];
}

- (void)setDefersLoading:(BOOL)defers
{
    [[self webView] setDefersCallbacks:defers];
}

- (void)setNeedsReapplyStyles
{
    NSView <WebDocumentView> *view = [[_frame frameView] documentView];
    if ([view isKindOfClass:[WebHTMLView class]]) {
        [(WebHTMLView *)view setNeedsToApplyStyles:YES];
        [view setNeedsLayout:YES];
        [view setNeedsDisplay:YES];
    }
}

- (NSView *)pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                   attributeNames:(NSArray *)attributeNames
                  attributeValues:(NSArray *)attributeValues
                          baseURL:(NSURL *)baseURL
                       DOMElement:(DOMElement *)element
                     loadManually:(BOOL)loadManually
{
    WebHTMLView *docView = (WebHTMLView *)[[_frame frameView] documentView];
    ASSERT([docView isKindOfClass:[WebHTMLView class]]);
        
    WebPluginController *pluginController = [docView _pluginController];
    
    // Store attributes in a dictionary so they can be passed to WebPlugins.
    NSMutableDictionary *attributes = [[NSMutableDictionary alloc] initWithObjects:attributeValues forKeys:attributeNames];
    
    [pluginPackage load];
    Class viewFactory = [pluginPackage viewFactory];
    
    NSView *view = nil;
    NSDictionary *arguments = nil;
    
    if ([viewFactory respondsToSelector:@selector(plugInViewWithArguments:)]) {
        arguments = [NSDictionary dictionaryWithObjectsAndKeys:
            baseURL, WebPlugInBaseURLKey,
            attributes, WebPlugInAttributesKey,
            pluginController, WebPlugInContainerKey,
            [NSNumber numberWithInt:loadManually ? WebPlugInModeFull : WebPlugInModeEmbed], WebPlugInModeKey,
            [NSNumber numberWithBool:!loadManually], WebPlugInShouldLoadMainResourceKey,
            element, WebPlugInContainingElementKey,
            nil];
        LOG(Plugins, "arguments:\n%@", arguments);
    } else if ([viewFactory respondsToSelector:@selector(pluginViewWithArguments:)]) {
        arguments = [NSDictionary dictionaryWithObjectsAndKeys:
            baseURL, WebPluginBaseURLKey,
            attributes, WebPluginAttributesKey,
            pluginController, WebPluginContainerKey,
            element, WebPlugInContainingElementKey,
            nil];
        LOG(Plugins, "arguments:\n%@", arguments);
    }

    view = [WebPluginController plugInViewWithArguments:arguments fromPluginPackage:pluginPackage];
    
    [attributes release];
    return view;
}

- (NSString *)valueForKey:(NSString *)key keys:(NSArray *)keys values:(NSArray *)values
{
    unsigned count = [keys count];
    unsigned i;
    for (i = 0; i < count; i++) {
        if ([[keys objectAtIndex:i] _webkit_isCaseInsensitiveEqualToString:key])
            return [values objectAtIndex:i];
    }
    return nil;
}

- (NSView *)viewForPluginWithURL:(NSURL *)URL
                  attributeNames:(NSArray *)attributeNames
                 attributeValues:(NSArray *)attributeValues
                        MIMEType:(NSString *)MIMEType
                      DOMElement:(DOMElement *)element
                    loadManually:(BOOL)loadManually
{
    BOOL hideReferrer;
    if (![self canLoadURL:URL fromReferrer:[self referrer] hideReferrer:&hideReferrer])
        return nil;

    ASSERT([attributeNames count] == [attributeValues count]);

    WebBasePluginPackage *pluginPackage = nil;
    NSView *view = nil;
    int errorCode = 0;

    WebView *wv = [self webView];
    id wd = [wv UIDelegate];

    if ([wd respondsToSelector:@selector(webView:plugInViewWithArguments:)]) {
        NSMutableDictionary *attributes = [[NSMutableDictionary alloc] initWithObjects:attributeValues forKeys:attributeNames];
        NSDictionary *arguments = [NSDictionary dictionaryWithObjectsAndKeys:
            attributes, WebPlugInAttributesKey,
            [NSNumber numberWithInt:loadManually ? WebPlugInModeFull : WebPlugInModeEmbed], WebPlugInModeKey,
            URL, WebPlugInBaseURLKey, // URL might be nil, so add it last
            [NSNumber numberWithBool:!loadManually], WebPlugInShouldLoadMainResourceKey,
            element, WebPlugInContainingElementKey,
            nil];
        [attributes release];
        view = [wd webView:wv plugInViewWithArguments:arguments];
        if (view)
            return view;
    }

    if ([MIMEType length] != 0)
        pluginPackage = [[self webView] _pluginForMIMEType:MIMEType];
    else
        MIMEType = nil;
    
    NSString *extension = [[URL path] pathExtension];
    if (!pluginPackage && [extension length] != 0) {
        pluginPackage = [[self webView] _pluginForExtension:extension];
        if (pluginPackage) {
            NSString *newMIMEType = [pluginPackage MIMETypeForExtension:extension];
            if ([newMIMEType length] != 0)
                MIMEType = newMIMEType;
        }
    }

    NSURL *baseURL = [self baseURL];
    if (pluginPackage) {
        if ([pluginPackage isKindOfClass:[WebPluginPackage class]]) {
            view = [self pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                attributeNames:attributeNames
                               attributeValues:attributeValues
                                       baseURL:baseURL
                                    DOMElement:element
                                  loadManually:loadManually];
        } else if ([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]) {
            WebNetscapePluginEmbeddedView *embeddedView = [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:NSZeroRect
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:URL
                                                                 baseURL:baseURL
                                                                MIMEType:MIMEType
                                                           attributeKeys:attributeNames
                                                         attributeValues:attributeValues
                                                            loadManually:loadManually
                                                              DOMElement:element] autorelease];
            view = embeddedView;
            [_frame _addPlugInView:embeddedView];
        } else
            ASSERT_NOT_REACHED();
    } else
        errorCode = WebKitErrorCannotFindPlugIn;

    if (!errorCode && !view)
        errorCode = WebKitErrorCannotLoadPlugIn;

    if (errorCode) {
        NSString *pluginPage = [self valueForKey:@"pluginspage" keys:attributeNames values:attributeValues];
        NSURL *pluginPageURL = pluginPage != nil ? [self URLWithAttributeString:pluginPage] : nil;
        NSError *error = [[NSError alloc] _initWithPluginErrorCode:errorCode
                                                        contentURL:URL
                                                     pluginPageURL:pluginPageURL
                                                        pluginName:[pluginPackage name]
                                                          MIMEType:MIMEType];
        WebNullPluginView *nullView = [[[WebNullPluginView alloc] initWithFrame:NSZeroRect error:error] autorelease];
        [_frame _addPlugInView:nullView];
        view = nullView;
        [error release];
    }
    
    ASSERT(view);
    return view;
}

- (void)redirectDataToPlugin:(NSView *)pluginView
{
    WebHTMLRepresentation *representation = (WebHTMLRepresentation *)[[_frame dataSource] representation];

    if ([pluginView isKindOfClass:[WebNetscapePluginEmbeddedView class]])
        [representation _redirectDataToManualLoader:(WebNetscapePluginEmbeddedView *)pluginView forPluginView:pluginView];
    else {
        WebHTMLView *docView = (WebHTMLView *)[[_frame frameView] documentView];
        ASSERT([docView isKindOfClass:[WebHTMLView class]]);
        
        WebPluginController *pluginController = [docView _pluginController];
        [representation _redirectDataToManualLoader:pluginController forPluginView:pluginView];
    }
}

- (NSView *)viewForJavaAppletWithFrame:(NSRect)theFrame
                        attributeNames:(NSArray *)attributeNames
                       attributeValues:(NSArray *)attributeValues
                               baseURL:(NSURL *)baseURL
                            DOMElement:(DOMElement *)element
{
    NSString *MIMEType = @"application/x-java-applet";
    WebBasePluginPackage *pluginPackage;
    NSView *view = nil;
    
    pluginPackage = [[self webView] _pluginForMIMEType:MIMEType];

    if (pluginPackage) {
        if ([pluginPackage isKindOfClass:[WebPluginPackage class]]) {
            // For some reason, the Java plug-in requires that we pass the dimension of the plug-in as attributes.
            NSMutableArray *names = [attributeNames mutableCopy];
            NSMutableArray *values = [attributeValues mutableCopy];
            if ([self valueForKey:@"width" keys:attributeNames values:attributeValues] == nil) {
                [names addObject:@"width"];
                [values addObject:[NSString stringWithFormat:@"%d", (int)theFrame.size.width]];
            }
            if ([self valueForKey:@"height" keys:attributeNames values:attributeValues] == nil) {
                [names addObject:@"height"];
                [values addObject:[NSString stringWithFormat:@"%d", (int)theFrame.size.height]];
            }
            view = [self pluginViewWithPackage:(WebPluginPackage *)pluginPackage
                                attributeNames:names
                               attributeValues:values
                                       baseURL:baseURL
                                    DOMElement:element
                                  loadManually:NO];
            [names release];
            [values release];
        } else if ([pluginPackage isKindOfClass:[WebNetscapePluginPackage class]]) {
            view = [[[WebNetscapePluginEmbeddedView alloc] initWithFrame:theFrame
                                                                  plugin:(WebNetscapePluginPackage *)pluginPackage
                                                                     URL:nil
                                                                 baseURL:baseURL
                                                                MIMEType:MIMEType
                                                           attributeKeys:attributeNames
                                                         attributeValues:attributeValues
                                                            loadManually:NO
                                                              DOMElement:element] autorelease];
        } else {
            ASSERT_NOT_REACHED();
        }
    }

    if (!view) {
        NSError *error = [[NSError alloc] _initWithPluginErrorCode:WebKitErrorJavaUnavailable
                                                        contentURL:nil
                                                     pluginPageURL:nil
                                                        pluginName:[pluginPackage name]
                                                          MIMEType:MIMEType];
        view = [[[WebNullPluginView alloc] initWithFrame:theFrame error:error] autorelease];
        [error release];
    }

    ASSERT(view);

    return view;
}

#ifndef NDEBUG
static BOOL loggedObjectCacheSize = NO;
#endif

-(int)getObjectCacheSize
{
    vm_size_t memSize = WebSystemMainMemory();
    int cacheSize = [[self _preferences] _objectCacheSize];
    int multiplier = 1;
    if (memSize >= 1024 * 1024 * 1024)
        multiplier = 4;
    else if (memSize >= 512 * 1024 * 1024)
        multiplier = 2;

#ifndef NDEBUG
    if (!loggedObjectCacheSize){
        LOG (CacheSizes, "Object cache size set to %d bytes.", cacheSize * multiplier);
        loggedObjectCacheSize = YES;
    }
#endif

    return cacheSize * multiplier;
}

- (ObjectElementType)determineObjectFromMIMEType:(NSString*)MIMEType URL:(NSURL*)URL
{
    if ([MIMEType length] == 0) {
        // Try to guess the MIME type based off the extension.
        NSString *extension = [[URL path] pathExtension];
        if ([extension length] > 0) {
            MIMEType = WKGetMIMETypeForExtension(extension);
            if ([MIMEType length] == 0 && [[self webView] _pluginForExtension:extension])
                // If no MIME type is specified, use a plug-in if we have one that can handle the extension.
                return ObjectElementPlugin;
        }
    }

    if ([MIMEType length] == 0)
        return ObjectElementFrame; // Go ahead and hope that we can display the content.

    if ([[self webView] _isMIMETypeRegisteredAsPlugin:MIMEType])
        return ObjectElementPlugin;

    if ([WebFrameView _viewClassForMIMEType:MIMEType])
        return ObjectElementFrame;
    
    return ObjectElementNone;
}

- (NSString *)MIMETypeForPath:(NSString *)path
{
    ASSERT(path);
    NSString *extension = [path pathExtension];
    NSString *type = WKGetMIMETypeForExtension(extension);
    return [type length] == 0 ? (NSString *)@"application/octet-stream" : type;
}

- (void)allowDHTMLDrag:(BOOL *)flagDHTML UADrag:(BOOL *)flagUA
{
    WebHTMLView *docView = (WebHTMLView *)[[_frame frameView] documentView];
    ASSERT([docView isKindOfClass:[WebHTMLView class]]);
    unsigned int mask = [docView _delegateDragSourceActionMask];
    *flagDHTML = (mask & WebDragSourceActionDHTML) != 0;
    *flagUA = ((mask & WebDragSourceActionImage) || (mask & WebDragSourceActionLink) || (mask & WebDragSourceActionSelection));
}

- (BOOL)startDraggingImage:(NSImage *)dragImage at:(NSPoint)dragLoc operation:(NSDragOperation)op event:(NSEvent *)event sourceIsDHTML:(BOOL)flag DHTMLWroteData:(BOOL)dhtmlWroteData
{
    WebHTMLView *docView = (WebHTMLView *)[[_frame frameView] documentView];
    ASSERT([docView isKindOfClass:[WebHTMLView class]]);
    [docView _setInitiatedDrag:YES];
    return [docView _startDraggingImage:dragImage at:dragLoc operation:op event:event sourceIsDHTML:flag DHTMLWroteData:dhtmlWroteData];
}

- (BOOL)mayStartDragAtEventLocation:(NSPoint)location
{
    WebHTMLView *docView = (WebHTMLView *)[[_frame frameView] documentView];

    ASSERT([docView isKindOfClass:[WebHTMLView class]]);

    return [docView _mayStartDragAtEventLocation:location];
}

- (BOOL)selectWordBeforeMenuEvent
{
    return [[self webView] _selectWordBeforeMenuEvent];
}

- (int)historyLength
{
    return [[[self webView] backForwardList] backListCount] + 1;
}

- (BOOL)canGoBackOrForward:(int)distance
{
    if (distance == 0)
        return YES;

    if (distance > 0 && distance <= [[[self webView] backForwardList] forwardListCount])
        return YES;

    if (distance < 0 && -distance <= [[[self webView] backForwardList] backListCount])
        return YES;
    
    return NO;
}

- (void)goBackOrForward:(int)distance
{
    if (distance == 0) {
        return;
    }
    WebView *webView = [self webView];
    WebBackForwardList *list = [webView backForwardList];
    WebHistoryItem *item = [list itemAtIndex:distance];
    if (!item) {
        if (distance > 0) {
            int forwardListCount = [list forwardListCount];
            if (forwardListCount > 0) {
                item = [list itemAtIndex:forwardListCount];
            }
        } else {
            int backListCount = [list forwardListCount];
            if (backListCount > 0) {
                item = [list itemAtIndex:-backListCount];
            }
        }
    }
    if (item) {
        [webView goToBackForwardItem:item];
    }
}

- (NSURL*)historyURL:(int)distance
{
    WebView *webView = [self webView];
    WebBackForwardList *list = [webView backForwardList];
    WebHistoryItem *item = [list itemAtIndex:distance];
    if (!item) {
        if (distance > 0) {
            int forwardListCount = [list forwardListCount];
            if (forwardListCount > 0)
                item = [list itemAtIndex:forwardListCount];
        } else {
            int backListCount = [list forwardListCount];
            if (backListCount > 0)
                item = [list itemAtIndex:-backListCount];
        }
    }
    if (item)
        return [item URL];
    
    return nil;
}

static id <WebFormDelegate> formDelegate(WebFrameBridge *self)
{
    ASSERT(self->_frame != nil);
    return [[self->_frame webView] _formDelegate];
}

#define FormDelegateLog(ctrl)  LOG(FormDelegate, "control=%@", ctrl)

- (void)textFieldDidBeginEditing:(DOMHTMLInputElement *)element
{
    FormDelegateLog(element);
    [formDelegate(self) textFieldDidBeginEditing:element inFrame:_frame];
}

- (void)textFieldDidEndEditing:(DOMHTMLInputElement *)element
{
    FormDelegateLog(element);
    [formDelegate(self) textFieldDidEndEditing:element inFrame:_frame];
}

- (void)textDidChangeInTextField:(DOMHTMLInputElement *)element
{
    FormDelegateLog(element);
    [formDelegate(self) textDidChangeInTextField:(DOMHTMLInputElement *)element inFrame:_frame];
}

- (void)textDidChangeInTextArea:(DOMHTMLTextAreaElement *)element
{
    FormDelegateLog(element);
    [formDelegate(self) textDidChangeInTextArea:element inFrame:_frame];
}

- (BOOL)textField:(DOMHTMLInputElement *)element doCommandBySelector:(SEL)commandSelector
{
    FormDelegateLog(element);
    return [formDelegate(self) textField:element doCommandBySelector:commandSelector inFrame:_frame];
}

- (BOOL)textField:(DOMHTMLInputElement *)element shouldHandleEvent:(NSEvent *)event
{
    FormDelegateLog(element);
    return [formDelegate(self) textField:element shouldHandleEvent:event inFrame:_frame];
}

- (void)setHasBorder:(BOOL)hasBorder
{
    [[_frame frameView] _setHasBorder:hasBorder];
}

- (NSFileWrapper *)fileWrapperForURL:(NSURL *)URL
{
    return [[_frame dataSource] _fileWrapperForURL:URL];
}

- (void)print
{
    id wd = [[self webView] UIDelegate];
    
    if ([wd respondsToSelector:@selector(webView:printFrameView:)]) {
        [wd webView:[self webView] printFrameView:[_frame frameView]];
    } else {
        [[WebDefaultUIDelegate sharedUIDelegate] webView:[self webView] printFrameView:[_frame frameView]];
    }
}

- (jobject)getAppletInView:(NSView *)view
{
    jobject applet;

    if ([view respondsToSelector:@selector(webPlugInGetApplet)])
        applet = [view webPlugInGetApplet];
    else
        applet = [self pollForAppletInView:view];
        
    return applet;
}

// NOTE: pollForAppletInView: will block until the block is ready to use, or
// until a timeout is exceeded.  It will return nil if the timeour is
// exceeded.
// Deprecated, use getAppletInView:.
- (jobject)pollForAppletInView:(NSView *)view
{
    jobject applet = 0;
    
    if ([view respondsToSelector:@selector(pollForAppletInWindow:)]) {
        // The Java VM needs the containing window of the view to
        // initialize.  The view may not yet be in the window's view 
        // hierarchy, so we have to pass the window when requesting
        // the applet.
        applet = [view pollForAppletInWindow:[[self webView] window]];
    }
    
    return applet;
}

- (void)respondToChangedContents
{
    NSView <WebDocumentView> *view = [[_frame frameView] documentView];
    if ([view isKindOfClass:[WebHTMLView class]]) {
        [(WebHTMLView *)view _updateFontPanel];
    }
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewDidChangeNotification object:[self webView]];
}

- (void)respondToChangedSelection
{
    NSView <WebDocumentView> *view = [[_frame frameView] documentView];
    if ([view isKindOfClass:[WebHTMLView class]]) {
        [(WebHTMLView *)view _selectionChanged];
    }
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewDidChangeSelectionNotification object:[self webView]];
}

- (NSUndoManager *)undoManager
{
    return [[self webView] undoManager];
}

- (void)issueCutCommand
{
    NSView* documentView = [[_frame frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView*)documentView cut:nil];
}

- (void)issueCopyCommand
{
    NSView* documentView = [[_frame frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView*)documentView copy:nil];
}

- (void)issuePasteCommand
{
    NSView* documentView = [[_frame frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView*)documentView paste:nil];
}

- (void)issuePasteAndMatchStyleCommand
{
    NSView <WebDocumentView> *documentView = [[_frame frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView*)documentView pasteAsPlainText:nil];
}

- (void)issueTransposeCommand
{
    NSView <WebDocumentView> *view = [[_frame frameView] documentView];
    if ([view isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)view transpose:nil];
}

- (BOOL)canPaste
{
    NSView* documentView = [[_frame frameView] documentView];
    return [documentView isKindOfClass:[WebHTMLView class]] && [(WebHTMLView*)documentView _canPaste];
}

- (void)setIsSelected:(BOOL)isSelected forView:(NSView *)view
{
    if ([view respondsToSelector:@selector(webPlugInSetIsSelected:)]) {
        [view webPlugInSetIsSelected:isSelected];
    }
    else if ([view respondsToSelector:@selector(setIsSelected:)]) {
        [view setIsSelected:isSelected];
    }
}

- (NSString *)overrideMediaType
{
    return [[self webView] mediaStyle];
}

- (BOOL)isEditable
{
    return [[self webView] isEditable];
}

- (BOOL)shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag
{
    return [[self webView] _shouldChangeSelectedDOMRange:currentRange toDOMRange:proposedRange affinity:selectionAffinity stillSelecting:flag];
}

- (BOOL)shouldDeleteSelectedDOMRange:(DOMRange *)range
{
    WebView *webView = [self webView];
    return [[webView _editingDelegateForwarder] webView:webView shouldDeleteDOMRange:range];
}

- (BOOL)shouldBeginEditing:(DOMRange *)range
{
    return [[self webView] _shouldBeginEditingInDOMRange:range];
}

- (BOOL)shouldEndEditing:(DOMRange *)range
{
    return [[self webView] _shouldEndEditingInDOMRange:range];
}

- (void)didBeginEditing
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewDidBeginEditingNotification object:[_frame webView]];
}

- (void)didEndEditing
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebViewDidEndEditingNotification object:[_frame webView]];
}

- (void)windowObjectCleared
{
    WebView *wv = [self webView];
    [[wv _frameLoadDelegateForwarder] webView:wv windowScriptObjectAvailable:[self windowScriptObject]];
    if ([wv scriptDebugDelegate] || [WebScriptDebugServer listenerCount]) {
        [_frame _detachScriptDebugger]; // FIXME: remove this once <rdar://problem/4608404> is fixed
        [_frame _attachScriptDebugger];
    }
}

- (int)spellCheckerDocumentTag
{
    return [[self webView] spellCheckerDocumentTag];
}

- (BOOL)isContinuousSpellCheckingEnabled
{
    return [[self webView] isContinuousSpellCheckingEnabled];
}

- (BOOL)_compareDashboardRegions:(NSDictionary *)regions
{
    return [lastDashboardRegions isEqualToDictionary:regions];
}

- (void)dashboardRegionsChanged:(NSMutableDictionary *)regions
{
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
    
    [wv _addScrollerDashboardRegions:regions];
    
    if (![self _compareDashboardRegions:regions]) {
        if ([wd respondsToSelector:@selector(webView:dashboardRegionsChanged:)]) {
            [wd webView:wv dashboardRegionsChanged:regions];
            [lastDashboardRegions release];
            lastDashboardRegions = [regions retain];
        }
    }
}

- (void)willPopupMenu:(NSMenu *)menu
{
    WebView *wv = [self webView];
    id wd = [wv UIDelegate];
        
    if ([wd respondsToSelector:@selector(webView:willPopupMenu:)])
        [wd webView:wv willPopupMenu:menu];
}

- (NSRect)customHighlightRect:(NSString*)type forLine:(NSRect)lineRect
{
    ASSERT(_frame != nil);
    NSView *documentView = [[_frame frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]])
        return NSZeroRect;

    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    id<WebHTMLHighlighter> highlighter = [webHTMLView _highlighterForType:type];
    return [highlighter highlightRectForLine:lineRect];
}

- (void)paintCustomHighlight:(NSString*)type forBox:(NSRect)boxRect onLine:(NSRect)lineRect behindText:(BOOL)text
                  entireLine:(BOOL)line
{
    ASSERT(_frame != nil);
    NSView *documentView = [[_frame frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]])
        return;

    WebHTMLView *webHTMLView = (WebHTMLView *)documentView;
    id<WebHTMLHighlighter> highlighter = [webHTMLView _highlighterForType:type];
    [highlighter paintHighlightForBox:boxRect onLine:lineRect behindText:text entireLine:line];
}

- (NSString *)nameForUndoAction:(WebUndoAction)undoAction
{
    switch (undoAction) {
        case WebUndoActionUnspecified: return nil;
        case WebUndoActionSetColor: return UI_STRING_KEY("Set Color", "Set Color (Undo action name)", "Undo action name");
        case WebUndoActionSetBackgroundColor: return UI_STRING_KEY("Set Background Color", "Set Background Color (Undo action name)", "Undo action name");
        case WebUndoActionTurnOffKerning: return UI_STRING_KEY("Turn Off Kerning", "Turn Off Kerning (Undo action name)", "Undo action name");
        case WebUndoActionTightenKerning: return UI_STRING_KEY("Tighten Kerning", "Tighten Kerning (Undo action name)", "Undo action name");
        case WebUndoActionLoosenKerning: return UI_STRING_KEY("Loosen Kerning", "Loosen Kerning (Undo action name)", "Undo action name");
        case WebUndoActionUseStandardKerning: return UI_STRING_KEY("Use Standard Kerning", "Use Standard Kerning (Undo action name)", "Undo action name");
        case WebUndoActionTurnOffLigatures: return UI_STRING_KEY("Turn Off Ligatures", "Turn Off Ligatures (Undo action name)", "Undo action name");
        case WebUndoActionUseStandardLigatures: return UI_STRING_KEY("Use Standard Ligatures", "Use Standard Ligatures (Undo action name)", "Undo action name");
        case WebUndoActionUseAllLigatures: return UI_STRING_KEY("Use All Ligatures", "Use All Ligatures (Undo action name)", "Undo action name");
        case WebUndoActionRaiseBaseline: return UI_STRING_KEY("Raise Baseline", "Raise Baseline (Undo action name)", "Undo action name");
        case WebUndoActionLowerBaseline: return UI_STRING_KEY("Lower Baseline", "Lower Baseline (Undo action name)", "Undo action name");
        case WebUndoActionSetTraditionalCharacterShape: return UI_STRING_KEY("Set Traditional Character Shape", "Set Traditional Character Shape (Undo action name)", "Undo action name");
        case WebUndoActionSetFont: return UI_STRING_KEY("Set Font", "Set Font (Undo action name)", "Undo action name");
        case WebUndoActionChangeAttributes: return UI_STRING_KEY("Change Attributes", "Change Attributes (Undo action name)", "Undo action name");
        case WebUndoActionAlignLeft: return UI_STRING_KEY("Align Left", "Align Left (Undo action name)", "Undo action name");
        case WebUndoActionAlignRight: return UI_STRING_KEY("Align Right", "Align Right (Undo action name)", "Undo action name");
        case WebUndoActionCenter: return UI_STRING_KEY("Center", "Center (Undo action name)", "Undo action name");
        case WebUndoActionJustify: return UI_STRING_KEY("Justify", "Justify (Undo action name)", "Undo action name");
        case WebUndoActionSetWritingDirection: return UI_STRING_KEY("Set Writing Direction", "Set Writing Direction (Undo action name)", "Undo action name");
        case WebUndoActionSubscript: return UI_STRING_KEY("Subscript", "Subscript (Undo action name)", "Undo action name");
        case WebUndoActionSuperscript: return UI_STRING_KEY("Superscript", "Superscript (Undo action name)", "Undo action name");
        case WebUndoActionUnderline: return UI_STRING_KEY("Underline", "Underline (Undo action name)", "Undo action name");
        case WebUndoActionOutline: return UI_STRING_KEY("Outline", "Outline (Undo action name)", "Undo action name");
        case WebUndoActionUnscript: return UI_STRING_KEY("Unscript", "Unscript (Undo action name)", "Undo action name");
        case WebUndoActionDrag: return UI_STRING_KEY("Drag", "Drag (Undo action name)", "Undo action name");
        case WebUndoActionCut: return UI_STRING_KEY("Cut", "Cut (Undo action name)", "Undo action name");
        case WebUndoActionPaste: return UI_STRING_KEY("Paste", "Paste (Undo action name)", "Undo action name");
        case WebUndoActionPasteFont: return UI_STRING_KEY("Paste Font", "Paste Font (Undo action name)", "Undo action name");
        case WebUndoActionPasteRuler: return UI_STRING_KEY("Paste Ruler", "Paste Ruler (Undo action name)", "Undo action name");
        case WebUndoActionTyping: return UI_STRING_KEY("Typing", "Typing (Undo action name)", "Undo action name");
        case WebUndoActionCreateLink: return UI_STRING_KEY("Create Link", "Create Link (Undo action name)", "Undo action name");
        case WebUndoActionUnlink: return UI_STRING_KEY("Unlink", "Unlink (Undo action name)", "Undo action name");
        case WebUndoActionInsertList: return UI_STRING_KEY("Insert List", "Insert List (Undo action name)", "Undo action name");
        case WebUndoActionFormatBlock: return UI_STRING_KEY("Formatting", "Format Block (Undo action name)", "Undo action name");
        case WebUndoActionIndent: return UI_STRING_KEY("Indent", "Indent (Undo action name)", "Undo action name");
        case WebUndoActionOutdent: return UI_STRING_KEY("Outdent", "Outdent (Undo action name)", "Undo action name");
    }
    return nil;
}

- (WebCorePageBridge *)createModalDialogWithURL:(NSURL *)URL
{
    ASSERT(_frame != nil);

    NSMutableURLRequest *request = nil;

    if (URL != nil && ![URL _web_isEmpty]) {
        request = [NSMutableURLRequest requestWithURL:URL];
        [request _web_setHTTPReferrer:[self referrer]];
    }

    WebView *currentWebView = [self webView];
    id UIDelegate = [currentWebView UIDelegate];

    WebView *newWebView = nil;
    if ([UIDelegate respondsToSelector:@selector(webView:createWebViewModalDialogWithRequest:)])
        newWebView = [UIDelegate webView:currentWebView createWebViewModalDialogWithRequest:request];
    else if ([UIDelegate respondsToSelector:@selector(webView:createWebViewWithRequest:)])
        newWebView = [UIDelegate webView:currentWebView createWebViewWithRequest:request];
    else
        newWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:currentWebView createWebViewWithRequest:request];

    return [newWebView _pageBridge];
}

- (BOOL)canRunModal
{
    WebView *webView = [self webView];
    id UIDelegate = [webView UIDelegate];
    return [UIDelegate respondsToSelector:@selector(webViewRunModal:)];
}

- (BOOL)canRunModalNow
{
    return [self canRunModal] && ![WebLoader inConnectionCallback];
}

- (void)runModal
{
    if (![self canRunModal])
        return;

    WebView *webView = [self webView];
    if ([webView defersCallbacks]) {
        LOG_ERROR("tried to run modal in a view when it was deferring callbacks -- should never happen");
        return;
    }

    // Defer callbacks in all the other views in this group, so we don't try to run JavaScript
    // in a way that could interact with this view.
    NSMutableArray *deferredWebViews = [NSMutableArray array];
    NSString *groupName = [webView groupName];
    if (groupName) {
        NSEnumerator *enumerator = [WebCoreFrameNamespaces framesInNamespace:groupName];
        WebView *otherWebView;
        while ((otherWebView = [[enumerator nextObject] webView]) != nil) {
            if (otherWebView != webView && ![otherWebView defersCallbacks]) {
                [otherWebView setDefersCallbacks:YES];
                [deferredWebViews addObject:otherWebView];
            }
        }
    }

    // Go run the modal event loop.
    [[webView UIDelegate] webViewRunModal:webView];

    // Restore the callbacks for any views that we deferred them for.
    unsigned count = [deferredWebViews count];
    unsigned i;
    for (i = 0; i < count; ++i) {
        WebView *otherWebView = [deferredWebViews objectAtIndex:i];
        [otherWebView setDefersCallbacks:NO];
    }
}

- (void)closeURL
{
    [_frame _willCloseURL];
    [super closeURL];
}

- (NSString*)imageTitleForFilename:(NSString*)filename size:(NSSize)size
{
    return [NSString stringWithFormat:UI_STRING("%@ %.0f×%.0f pixels", "window title for a standalone image (uses multiplication symbol, not x)"), filename, size.width, size.height];
}

@end
