/*	
    WebFrame.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
*/

#import <Foundation/Foundation.h>

@class WebView;
@class WebDataSource;
@class WebFramePrivate;
@class WebFrameView;
@class NSURLRequest;

/*!
    @class WebFrame
    @discussion Every web page is represented by at least one WebFrame.  A WebFrame
    has a WebFrameView and a WebDataSource.
*/
@interface WebFrame : NSObject
{
@private
    WebFramePrivate *_private;
}

/*!
    @method initWithName:webFrameView:webView:
    @abstract The designated initializer of WebFrame.
    @discussion WebFrames are normally created for you by the WebView.  You should 
    not need to invoke this method directly.
    @param name The name of the frame.
    @param view The WebFrameView for the frame.
    @param webView The WebView that manages the frame.
    @result Returns an initialized WebFrame.
*/
- (id)initWithName:(NSString *)name webFrameView:(WebFrameView *)view webView:(WebView *)webView;

/*!
    @method name
    @result The frame name.
*/
- (NSString *)name;

/*!
    @method webView
    @result Returns the WebView for the document that includes this frame.
*/
- (WebView *)webView;

/*!
    @method frameView
    @result The WebFrameView for this frame.
*/
- (WebFrameView *)frameView;

/*!
    @method loadRequest:
    @param request The web request to load.
*/
- (void)loadRequest:(NSURLRequest *)request;

/*!
    @method loadData:MIMEType:textEncodingName:baseURL:
    @param data The data to use for the main page of the document.
    @param MIMEType The MIME type of the data.
    @param encodingName The encoding of the data.
    @param URL The base URL to apply to relative URLs within the document.
*/
- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL;

/*!
    @method loadHTMLString:baseURL:
    @param string The string to use for the main page of the document.
    @param URL The base URL to apply to relative URLs within the document.
*/
- (void)loadHTMLString:(NSString *)string baseURL:(NSURL *)URL;

/*!
    @method dataSource
    @discussion Returns the committed data source.  Will return nil if the
    provisional data source hasn't yet been loaded.
    @result The datasource for this frame.
*/
- (WebDataSource *)dataSource;

/*!
    @method provisionalDataSource
    @discussion Will return the provisional data source.  The provisional data source will
    be nil if no data source has been set on the frame, or the data source
    has successfully transitioned to the committed data source.
    @result The provisional datasource of this frame.
*/
- (WebDataSource *)provisionalDataSource;

/*!
    @method stopLoading
    @discussion Stop any pending loads on the frame's data source,
    and its children.
*/
- (void)stopLoading;

/*!
    @method reload
*/
- (void)reload;

/*!
    @method findFrameNamed:
    @discussion This method returns a frame with the given name. findFrameNamed returns self 
    for _self and _current, the parent frame for _parent and the main frame for _top. 
    findFrameNamed returns self for _parent and _top if the receiver is the mainFrame.
    findFrameNamed first searches from the current frame to all descending frames then the
    rest of the frames in the WebView. If still not found, findFrameNamed searches the
    frames of the other WebViews.
    @param name The name of the frame to find.
    @result The frame matching the provided name. nil if the frame is not found.
*/
- (WebFrame *)findFrameNamed:(NSString *)name;

/*!
    @method parentFrame
    @result The frame containing this frame, or nil if this is a top level frame.
*/
- (WebFrame *)parentFrame;

/*!
    @method childFrames
    @discussion The frames in the array are associated with a frame set or iframe.
    @result Returns an array of WebFrame.
*/
- (NSArray *)childFrames;

@end
