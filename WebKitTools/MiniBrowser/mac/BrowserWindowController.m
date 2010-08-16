/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "BrowserWindowController.h"

#import <WebKit2/WKPagePrivate.h>
#import <WebKit2/WKStringCF.h>
#import <WebKit2/WKURLCF.h>

@interface BrowserWindowController ()
- (void)didStartProgress;
- (void)didChangeProgress:(double)value;
- (void)didFinishProgress;
- (void)didStartProvisionalLoadForFrame:(WKFrameRef)frame;
- (void)didCommitLoadForFrame:(WKFrameRef)frame;
- (void)didReceiveServerRedirectForProvisionalLoadForFrame:(WKFrameRef)frame;
- (void)didFailProvisionalLoadWithErrorForFrame:(WKFrameRef)frame;
- (void)didFailLoadWithErrorForFrame:(WKFrameRef)frame;
@end

@implementation BrowserWindowController

- (id)initWithPageNamespace:(WKPageNamespaceRef)pageNamespace
{
    if ((self = [super initWithWindowNibName:@"BrowserWindow"]))
        _pageNamespace = WKPageNamespaceRetain(pageNamespace);
    
    return self;
}

- (void)dealloc
{
    assert(!_pageNamespace);
    [super dealloc];
}

- (IBAction)fetch:(id)sender
{
    CFURLRef cfURL = CFURLCreateWithString(0, (CFStringRef)[urlText stringValue], 0);
    WKURLRef url = WKURLCreateWithCFURL(cfURL);
    CFRelease(cfURL);

    WKPageLoadURL(_webView.pageRef, url);
    WKURLRelease(url);
}

- (IBAction)showHideWebView:(id)sender
{
    BOOL hidden = ![_webView isHidden];
    
    [_webView setHidden:hidden];
}

- (IBAction)removeReinsertWebView:(id)sender
{
    if ([_webView window]) {
        [_webView retain];
        [_webView removeFromSuperview]; 
    } else {
        [containerView addSubview:_webView];
        [_webView release];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    if ([menuItem action] == @selector(showHideWebView:))
        [menuItem setTitle:[_webView isHidden] ? @"Show Web View" : @"Hide Web View"];
    else if ([menuItem action] == @selector(removeReinsertWebView:))
        [menuItem setTitle:[_webView window] ? @"Remove Web View" : @"Insert Web View"];
    return YES;
}

- (IBAction)reload:(id)sender
{
    WKPageReload(_webView.pageRef);
}

- (IBAction)forceRepaint:(id)sender
{
    [_webView setNeedsDisplay:YES];
}

- (IBAction)goBack:(id)sender
{
    WKPageGoBack(_webView.pageRef);
}

- (IBAction)goForward:(id)sender
{
    WKPageGoForward(_webView.pageRef);
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    SEL action = [item action];
    
    if (action == @selector(goBack:))
        return _webView && WKPageCanGoBack(_webView.pageRef);
    
    if (action == @selector(goForward:))
        return _webView && WKPageCanGoForward(_webView.pageRef);
    
    return YES;
}

- (void)validateToolbar
{
    [toolbar validateVisibleItems];
}

- (BOOL)windowShouldClose:(id)sender
{
    LOG(@"windowShouldClose");
    BOOL canCloseImmediately = WKPageTryClose(_webView.pageRef);
    return canCloseImmediately;
}

- (void)windowWillClose:(NSNotification *)notification
{
    WKPageNamespaceRelease(_pageNamespace);
    _pageNamespace = 0;
}

- (void)applicationTerminating
{
    WKPageClose(_webView.pageRef);
    WKPageRelease(_webView.pageRef);
}

#pragma mark Loader Client Callbacks

static void didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didStartProvisionalLoadForFrame:frame];
}

static void didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didReceiveServerRedirectForProvisionalLoadForFrame:frame];
}

static void didFailProvisionalLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didFailProvisionalLoadWithErrorForFrame:frame];
}

static void didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didCommitLoadForFrame:frame];
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    LOG(@"didFinishLoadForFrame");
}

static void didFailLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didFailLoadWithErrorForFrame:frame];
}

static void didReceiveTitleForFrame(WKPageRef page, WKStringRef title, WKFrameRef frame, const void *clientInfo)
{
    CFStringRef cfTitle = WKStringCopyCFString(0, title);
    LOG(@"didReceiveTitleForFrame \"%@\"", (NSString *)cfTitle);
    CFRelease(cfTitle);
}

static void didFirstLayoutForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    LOG(@"didFirstLayoutForFrame");
}

static void didFirstVisuallyNonEmptyLayoutForFrame(WKPageRef page, WKFrameRef frame, const void *clientInfo)
{
    LOG(@"didFirstVisuallyNonEmptyLayoutForFrame");
}

static void didStartProgress(WKPageRef page, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didStartProgress];
}

static void didChangeProgress(WKPageRef page, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didChangeProgress:WKPageGetEstimatedProgress(page)];
}

static void didFinishProgress(WKPageRef page, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo didFinishProgress];
}

static void didBecomeUnresponsive(WKPageRef page, const void *clientInfo)
{
    LOG(@"didBecomeUnresponsive");
}

static void didBecomeResponsive(WKPageRef page, const void *clientInfo)
{
    LOG(@"didBecomeResponsive");
}

static void processDidExit(WKPageRef page, const void *clientInfo)
{
    LOG(@"processDidExit");
}

static void didChangeBackForwardList(WKPageRef page, const void *clientInfo)
{
    [(BrowserWindowController *)clientInfo validateToolbar];
}

#pragma mark Policy Client Callbacks

static void decidePolicyForNavigationAction(WKPageRef page, WKFrameNavigationType navigationType, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void *clientInfo)
{
    LOG(@"decidePolicyForNavigationAction");
    WKFramePolicyListenerUse(listener);
}

static void decidePolicyForNewWindowAction(WKPageRef page, WKFrameNavigationType navigationType, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void *clientInfo)
{
    LOG(@"decidePolicyForNewWindowAction");
    WKFramePolicyListenerUse(listener);
}

static void decidePolicyForMIMEType(WKPageRef page, WKStringRef MIMEType, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void *clientInfo)
{
    WKFramePolicyListenerUse(listener);
}

#pragma mark UI Client Callbacks

static WKPageRef createNewPage(WKPageRef page, const void* clientInfo)
{
    LOG(@"createNewPage");
    BrowserWindowController *controller = [[BrowserWindowController alloc] initWithPageNamespace:WKPageGetPageNamespace(page)];
    [controller loadWindow];

    return controller->_webView.pageRef;
}

static void showPage(WKPageRef page, const void *clientInfo)
{
    LOG(@"showPage");
    [[(BrowserWindowController *)clientInfo window] orderFront:nil];
}

static void closePage(WKPageRef page, const void *clientInfo)
{
    LOG(@"closePage");
    WKPageClose(page);
    [[(BrowserWindowController *)clientInfo window] close];
    WKPageRelease(page);
}

static void runJavaScriptAlert(WKPageRef page, WKStringRef message, WKFrameRef frame, const void* clientInfo)
{
    NSAlert* alert = [[NSAlert alloc] init];

    WKURLRef wkURL = WKFrameCopyURL(frame);
    CFURLRef cfURL = WKURLCopyCFURL(0, wkURL);
    WKURLRelease(wkURL);

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript alert dialog from %@.", [(NSURL *)cfURL absoluteString]]];
    CFRelease(cfURL);

    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    [alert setInformativeText:(NSString *)cfMessage];
    CFRelease(cfMessage);

    [alert addButtonWithTitle:@"OK"];

    [alert runModal];
    [alert release];
}

static bool runJavaScriptConfirm(WKPageRef page, WKStringRef message, WKFrameRef frame, const void* clientInfo)
{
    NSAlert* alert = [[NSAlert alloc] init];

    WKURLRef wkURL = WKFrameCopyURL(frame);
    CFURLRef cfURL = WKURLCopyCFURL(0, wkURL);
    WKURLRelease(wkURL);

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript confirm dialog from %@.", [(NSURL *)cfURL absoluteString]]];
    CFRelease(cfURL);

    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    [alert setInformativeText:(NSString *)cfMessage];
    CFRelease(cfMessage);

    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSInteger button = [alert runModal];
    [alert release];

    return button == NSAlertFirstButtonReturn;
}

static WKStringRef runJavaScriptPrompt(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef frame, const void* clientInfo)
{
    NSAlert* alert = [[NSAlert alloc] init];

    WKURLRef wkURL = WKFrameCopyURL(frame);
    CFURLRef cfURL = WKURLCopyCFURL(0, wkURL);
    WKURLRelease(wkURL);

    [alert setMessageText:[NSString stringWithFormat:@"JavaScript prompt dialog from %@.", [(NSURL *)cfURL absoluteString]]];
    CFRelease(cfURL);

    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    [alert setInformativeText:(NSString *)cfMessage];
    CFRelease(cfMessage);

    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
    CFStringRef cfDefaultValue = WKStringCopyCFString(0, defaultValue);
    [input setStringValue:(NSString *)cfDefaultValue];
    CFRelease(cfDefaultValue);

    [alert setAccessoryView:input];

    NSInteger button = [alert runModal];

    NSString* result = nil;
    if (button == NSAlertFirstButtonReturn) {
        [input validateEditing];
        result = [input stringValue];
    }

    [alert release];

    if (!result)
        return 0;
    return WKStringCreateWithCFString((CFStringRef)result);
}

- (void)awakeFromNib
{
    _webView = [[WKView alloc] initWithFrame:[containerView frame] pageNamespaceRef:_pageNamespace];

    [containerView addSubview:_webView];
    [_webView setFrame:[containerView frame]];
    
    [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    
    WKPageLoaderClient loadClient = {
        0,      /* version */
        self,   /* clientInfo */
        didStartProvisionalLoadForFrame,
        didReceiveServerRedirectForProvisionalLoadForFrame,
        didFailProvisionalLoadWithErrorForFrame,
        didCommitLoadForFrame,
        didFinishLoadForFrame,
        didFailLoadWithErrorForFrame,
        didReceiveTitleForFrame,
        didFirstLayoutForFrame,
        didFirstVisuallyNonEmptyLayoutForFrame,
        didStartProgress,
        didChangeProgress,
        didFinishProgress,
        didBecomeUnresponsive,
        didBecomeResponsive,
        processDidExit,
        didChangeBackForwardList
    };
    WKPageSetPageLoaderClient(_webView.pageRef, &loadClient);
    
    WKPagePolicyClient policyClient = {
        0,          /* version */
        self,       /* clientInfo */
        decidePolicyForNavigationAction,
        decidePolicyForNewWindowAction,
        decidePolicyForMIMEType
    };
    WKPageSetPagePolicyClient(_webView.pageRef, &policyClient);

    WKPageUIClient uiClient = {
        0,          /* version */
        self,       /* clientInfo */
        createNewPage,
        showPage,
        closePage,
        runJavaScriptAlert,
        runJavaScriptConfirm,
        runJavaScriptPrompt
    };
    WKPageSetPageUIClient(_webView.pageRef, &uiClient);
}

- (void)didStartProgress
{
    [progressIndicator setDoubleValue:0.0];
    [progressIndicator setHidden:NO];
}

- (void)didChangeProgress:(double)value
{
    [progressIndicator setDoubleValue:value];
}

- (void)didFinishProgress
{
    [progressIndicator setHidden:YES];
    [progressIndicator setDoubleValue:1.0];
}

- (void)updateProvisionalURLForFrame:(WKFrameRef)frame
{
    WKURLRef url = WKFrameCopyProvisionalURL(frame);
    if (!url)
        return;

    CFURLRef cfSourceURL = WKURLCopyCFURL(0, url);
    WKURLRelease(url);

    [urlText setStringValue:(NSString*)CFURLGetString(cfSourceURL)];
    CFRelease(cfSourceURL);
}

- (void)didStartProvisionalLoadForFrame:(WKFrameRef)frame
{
    if (!WKFrameIsMainFrame(frame))
        return;

    [self updateProvisionalURLForFrame:frame];
}

- (void)didReceiveServerRedirectForProvisionalLoadForFrame:(WKFrameRef)frame
{
    if (!WKFrameIsMainFrame(frame))
        return;

    [self updateProvisionalURLForFrame:frame];
}

- (void)didFailProvisionalLoadWithErrorForFrame:(WKFrameRef)frame
{
    if (!WKFrameIsMainFrame(frame))
        return;

    [self updateProvisionalURLForFrame:frame];
}

- (void)didFailLoadWithErrorForFrame:(WKFrameRef)frame
{
    if (!WKFrameIsMainFrame(frame))
        return;

    [self updateProvisionalURLForFrame:frame];
}

- (void)didCommitLoadForFrame:(WKFrameRef)frame
{
}

- (void)loadURLString:(NSString *)urlString
{
    // FIXME: We shouldn't have to set the url text here.
    [urlText setStringValue:urlString];
    [self fetch:nil];
}

@end
