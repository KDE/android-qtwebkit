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

#import <WebKit/WebView.h>
#import <WebKit/WebFramePrivate.h>

@class NSError;
@class WebFrame;
@class WebPreferences;
@class WebCoreSettings;

@protocol WebFormDelegate;

typedef struct _WebResourceDelegateImplementationCache {
    uint delegateImplementsDidCancelAuthenticationChallenge:1;
    uint delegateImplementsDidReceiveAuthenticationChallenge:1;
    uint delegateImplementsDidReceiveResponse:1;
    uint delegateImplementsDidReceiveContentLength:1;
    uint delegateImplementsDidFinishLoadingFromDataSource:1;
    uint delegateImplementsWillSendRequest:1;
    uint delegateImplementsIdentifierForRequest:1;
} WebResourceDelegateImplementationCache;

extern NSString *_WebCanGoBackKey;
extern NSString *_WebCanGoForwardKey;
extern NSString *_WebEstimatedProgressKey;
extern NSString *_WebIsLoadingKey;
extern NSString *_WebMainFrameIconKey;
extern NSString *_WebMainFrameTitleKey;
extern NSString *_WebMainFrameURLKey;
extern NSString *_WebMainFrameDocumentKey;

extern NSString *WebElementTitleKey;   // NSString of the title of the element (pending public, used by Safari)

typedef enum {
    WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows,
    WebDashboardBehaviorAlwaysSendActiveNullEventsToPlugIns,
    WebDashboardBehaviorAlwaysAcceptsFirstMouse,
    WebDashboardBehaviorAllowWheelScrolling
} WebDashboardBehavior;

@interface WebController : NSTreeController {
    IBOutlet WebView *webView;
}
- (WebView *)webView;
- (void)setWebView:(WebView *)newWebView;
@end

@interface WebView (WebPendingPublic)

- (void)setMainFrameDocumentReady:(BOOL)mainFrameDocumentReady;

- (void)setTabKeyCyclesThroughElements:(BOOL)cyclesElements;
- (BOOL)tabKeyCyclesThroughElements;

/*!
@method setScriptDebugDelegate:
@abstract Set the WebView's WebScriptDebugDelegate delegate.
@param delegate The WebScriptDebugDelegate to set as the delegate.
*/    
- (void)setScriptDebugDelegate:(id)delegate;

/*!
@method scriptDebugDelegate
@abstract Return the WebView's WebScriptDebugDelegate.
@result The WebView's WebScriptDebugDelegate.
*/    
- (id)scriptDebugDelegate;

- (BOOL)shouldClose;

/*!
    @method aeDescByEvaluatingJavaScriptFromString:
    @param script The text of the JavaScript.
    @result The result of the script, converted to an NSAppleEventDescriptor, or nil for failure.
*/
- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)script;

// Support for displaying multiple text matches.
// These methods might end up moving into a protocol, so different document types can specify
// whether or not they implement the protocol. For now we'll just deal with HTML.
// These methods are still in flux; don't rely on them yet.
- (unsigned)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag highlight:(BOOL)highlight;
- (void)unmarkAllTextMatches;
- (NSArray *)rectsForTextMatches;

@end

@interface WebView (WebPrivate)

+ (BOOL)_developerExtrasEnabled;
+ (BOOL)_scriptDebuggerEnabled;

/*!
Could be worth adding to the API.
 @method loadItemsFromOtherView:
 @abstract Loads the view with the contents of the other view, including its backforward list.
 @param otherView   The WebView from which to copy contents.
 */
- (void)_loadBackForwardListFromOtherView:(WebView *)otherView;

+ (NSArray *)_supportedFileExtensions;

/*!
    @method canShowFile:
    @abstract Checks if the WebKit can show the content of the file at the specified path.
    @param path The path of the file to check
    @result YES if the WebKit can show the content of the file at the specified path.
*/
+ (BOOL)canShowFile:(NSString *)path;

/*!
    @method suggestedFileExtensionForMIMEType:
    @param MIMEType The MIME type to check.
    @result The extension based on the MIME type
*/
+ (NSString *)suggestedFileExtensionForMIMEType: (NSString *)MIMEType;


// May well become public
- (void)_setFormDelegate:(id<WebFormDelegate>)delegate;
- (id<WebFormDelegate>)_formDelegate;

- (void)_close;

/*!
    @method _registerViewClass:representationClass:forURLScheme:
    @discussion Register classes that implement WebDocumentView and WebDocumentRepresentation respectively.
    @param viewClass The WebDocumentView class to use to render data for a given MIME type.
    @param representationClass The WebDocumentRepresentation class to use to represent data of the given MIME type.
    @param scheme The URL scheme to represent with an object of the given class.
*/
+ (void)_registerViewClass:(Class)viewClass representationClass:(Class)representationClass forURLScheme:(NSString *)URLScheme;

+ (void)_unregisterViewClassAndRepresentationClassForMIMEType:(NSString *)MIMEType;

/*!
     @method _canHandleRequest:
     @abstract Performs a "preflight" operation that performs some
     speculative checks to see if a request can be used to create
     a WebDocumentView and WebDocumentRepresentation.
     @discussion The result of this method is valid only as long as no
     protocols or schemes are registered or unregistered, and as long as
     the request is not mutated (if the request is mutable). Hence, clients
     should be prepared to handle failures even if they have performed request
     preflighting by caling this method.
     @param request The request to preflight.
     @result YES if it is likely that a WebDocumentView and WebDocumentRepresentation
     can be created for the request, NO otherwise.
*/
+ (BOOL)_canHandleRequest:(NSURLRequest *)request;

+ (NSString *)_decodeData:(NSData *)data;

+ (void)_setAlwaysUseATSU:(BOOL)f;

- (NSCachedURLResponse *)_cachedResponseForURL:(NSURL *)URL;

- (void)_addScrollerDashboardRegions:(NSMutableDictionary *)regions;
- (NSDictionary *)_dashboardRegions;

- (void)_setDashboardBehavior:(WebDashboardBehavior)behavior to:(BOOL)flag;
- (void)handleAuthenticationForResource:(id)identifier challenge:(NSURLAuthenticationChallenge *)challenge fromDataSource:(WebDataSource *)dataSource;
- (BOOL)_dashboardBehavior:(WebDashboardBehavior)behavior;

+ (void)_setShouldUseFontSmoothing:(BOOL)f;
+ (BOOL)_shouldUseFontSmoothing;

+ (NSString *)_minimumRequiredSafariBuildNumber;

/*!
    @method setAlwaysShowVerticalScroller:
    @result Forces the vertical scroller to be visible if flag is YES, otherwise
    if flag is NO the scroller with automatically show and hide as needed.
 */
- (void)setAlwaysShowVerticalScroller:(BOOL)flag;

/*!
    @method alwaysShowVerticalScroller
    @result YES if the vertical scroller is always shown
 */
- (BOOL)alwaysShowVerticalScroller;

/*!
    @method setAlwaysShowHorizontalScroller:
    @result Forces the horizontal scroller to be visible if flag is YES, otherwise
    if flag is NO the scroller with automatically show and hide as needed.
 */
- (void)setAlwaysShowHorizontalScroller:(BOOL)flag;

/*!
    @method alwaysShowHorizontalScroller
    @result YES if the horizontal scroller is always shown
 */
- (BOOL)alwaysShowHorizontalScroller;

/*!
    @method setProhibitsMainFrameScrolling:
    @abstract Prohibits scrolling in the WebView's main frame.  Used to "lock" a WebView
    to a specific scroll position.
  */
- (void)setProhibitsMainFrameScrolling:(BOOL)prohibits;

/*!
    @method _setAdditionalWebPlugInPaths:
    @abstract Sets additional plugin search paths for a specific WebView.
 */
- (void)_setAdditionalWebPlugInPaths:(NSArray *)newPaths;

/*!
    @method _setInViewSourceMode:
    @abstract Used to place a WebView into a special source-viewing mode.
  */
- (void)_setInViewSourceMode:(BOOL)flag;

/*!
    @method _inViewSourceMode;
    @abstract Whether or not the WebView is in source-view mode for HTML.
  */
- (BOOL)_inViewSourceMode;

/*!
    @method _attachScriptDebuggerToAllFrames
    @abstract Attaches a script debugger to all frames belonging to the receiver.
 */
- (void)_attachScriptDebuggerToAllFrames;

/*!
    @method _detachScriptDebuggerFromAllFrames
    @abstract Detaches any script debuggers from all frames belonging to the receiver.
 */
- (void)_detachScriptDebuggerFromAllFrames;

@end

@interface WebView (WebViewPrintingPrivate)
/*!
    @method _adjustPrintingMarginsForHeaderAndFooter:
    @abstract Increase the top and bottom margins for the current print operation to
    account for the header and footer height. 
    @discussion Called by <WebDocument> implementors once when a print job begins. If the
    <WebDocument> implementor implements knowsPageRange:, this should be called from there.
    Otherwise this should be called from beginDocument. The <WebDocument> implementors need
    to also call _drawHeaderAndFooter.
*/
- (void)_adjustPrintingMarginsForHeaderAndFooter;

/*!
    @method _drawHeaderAndFooter
    @abstract Gives the WebView's UIDelegate a chance to draw a header and footer on the
    printed page. 
    @discussion This should be called by <WebDocument> implementors from an override of
    drawPageBorderWithSize:.
*/
- (void)_drawHeaderAndFooter;
@end

@interface WebView (WebViewEditingInMail)
- (void)_insertNewlineInQuotedContent;
- (BOOL)_selectWordBeforeMenuEvent;
- (void)_setSelectWordBeforeMenuEvent:(BOOL)flag;
- (void)_replaceSelectionWithNode:(DOMNode *)node matchStyle:(BOOL)matchStyle;
@end

@interface _WebSafeForwarder : NSObject
{
    id target; // Non-retained. Don't retain delegates.
    id defaultTarget;
    Class templateClass;
}
- (id)initWithTarget:(id)t defaultTarget:(id)dt templateClass:(Class)aClass;
+ (id)safeForwarderWithTarget:(id)t defaultTarget:(id)dt templateClass:(Class)aClass;
@end

@interface NSObject (WebFrameLoadDelegatePrivate)
- (void)webView:(WebView *)sender didFirstLayoutInFrame:(WebFrame *)frame;
// Addresses 4192534.  Private API for now.
- (void)webView:(WebView *)sender didHandleOnloadEventsForFrame:(WebFrame *)frame;
@end
