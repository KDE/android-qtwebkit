/*	
    WebFrameLoadDelegate.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.
    
    Public header file.
*/

#import <Cocoa/Cocoa.h>

@class NSError;
@class WebFrame;
@class WebView;

/*!
    @category WebFrameLoadDelegate
    @discussion A WebView's WebFrameLoadDelegate tracks the loading progress of its frames.
    When a data source of a frame starts to load, the data source is considered "provisional".
    Once at least one byte is received, the data source is considered "committed". This is done
    so the contents of the frame will not be lost if the new data source fails to successfully load.
*/
@interface NSObject (WebFrameLoadDelegate)

/*!
    @method webView:didStartProvisionalLoadForFrame:
    @abstract Notifies the delegate that the provisional load of a frame has started
    @param webView The WebView sending the message
    @param frame The frame for which the provisional load has started
    @discussion This method is called after the provisional data source of a frame
    has started to load.
*/
- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame;

/*!
    @method webView:didReceiveServerRedirectForProvisionalLoadForFrame:
    @abstract Notifies the delegate that a server redirect occurred during the provisional load
    @param webView The WebView sending the message
    @param frame The frame for which the redirect occurred
*/
- (void)webView:(WebView *)sender didReceiveServerRedirectForProvisionalLoadForFrame:(WebFrame *)frame;

/*!
    @method webView:didFailProvisionalLoadWithError:forFrame:
    @abstract Notifies the delegate that the provisional load has failed
    @param webView The WebView sending the message
    @param error The error that occurred
    @param frame The frame for which the error occurred
    @discussion This method is called after the provisional data source has failed to load.
    The frame will continue to display the contents of the committed data source if there is one.
*/
- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;

/*!
    @method webView:didCommitLoadForFrame:
    @abstract Notifies the delegate that the load has changed from provisional to committed
    @param webView The WebView sending the message
    @param frame The frame for which the load has committed
    @discussion This method is called after the provisional data source has become the
    committed data source.

    In some cases, a single load may be committed more than once. This happens
    in the case of multipart/x-mixed-replace, also known as "server push". In this case,
    a single location change leads to multiple documents that are loaded in sequence. When
    this happens, a new commit will be sent for each document.
*/
- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame;

/*!
    @method webView:didReceiveTitle:forFrame:
    @abstract Notifies the delegate that the page title for a frame has been received
    @param webView The WebView sending the message
    @param title The new page title
    @param frame The frame for which the title has been received
    @discussion The title may update during loading; clients should be prepared for this.
*/
- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame;

/*!
    @method webView:didReceiveIcon:forFrame:
    @abstract Notifies the delegate that a page icon image for a frame has been received
    @param webView The WebView sending the message
    @param image The icon image. Also known as a "favicon".
    @param frame The frame for which a page icon has been received
*/
- (void)webView:(WebView *)sender didReceiveIcon:(NSImage *)image forFrame:(WebFrame *)frame;

/*!
    @method webView:didFinishLoadForFrame:
    @abstract Notifies the delegate that the committed load of a frame has completed
    @param webView The WebView sending the message
    @param frame The frame that finished loading
    @discussion This method is called after the committed data source of a frame has successfully loaded
    and will only be called when all subresources such as images and stylesheets are done loading.
    Plug-In content and JavaScript-requested loads may occur after this method is called.
*/
- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame;

/*!
    @method webView:didFailLoadWithError:forFrame:
    @abstract Notifies the delegate that the committed load of a frame has failed
    @param webView The WebView sending the message
    @param error The error that occurred
    @param frame The frame that failed to load
    @discussion This method is called after a data source has committed but failed to completely load.
*/
- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame;

/*!
    @method webView:didChangeLocationWithinPageForFrame:
    @abstract Notifies the delegate that the scroll position in a frame has changed
    @param webView The WebView sending the message
    @param frame The frame that scrolled
    @discussion This method is called when anchors within a page have been clicked.
*/
- (void)webView:(WebView *)sender didChangeLocationWithinPageForFrame:(WebFrame *)frame;

/*!
    @method webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:
    @abstract Notifies the delegate that a frame will perform a client-side redirect
    @param webView The WebView sending the message
    @param URL The URL to be redirected to
    @param seconds Seconds in which the redirect will happen
    @param date The fire date
    @param frame The frame on which the redirect will occur
    @discussion This method can be used to continue progress feedback while a client-side
    redirect is pending.
*/
- (void)webView:(WebView *)sender willPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame;

/*!
    @method webView:didCancelClientRedirectForFrame:
    @abstract Notifies the delegate that a pending client-side redirect has been cancelled
    @param webView The WebView sending the message
    @param frame The frame for which the pending redirect was cancelled
    @discussion A client-side redirect can be cancelled if a frame changes location before the timeout.
*/
- (void)webView:(WebView *)sender didCancelClientRedirectForFrame:(WebFrame *)frame;

/*!
    @method webView:willCloseFrame:
    @abstract Notifies the delegate that a frame will be closed
    @param webView The WebView sending the message
    @param frame The frame that will be closed
    @discussion This method is called right before WebKit is done with the frame
    and the objects that it contains.
*/
- (void)webView:(WebView *)sender willCloseFrame:(WebFrame *)frame;

@end

