/*	
    WebKitErrors.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
*/

extern NSString *WebKitErrorDomain;

extern NSString * const WebKitErrorMIMETypeKey;
extern NSString * const WebKitErrorPlugInNameKey;
extern NSString * const WebKitErrorPlugInPageURLStringKey;

/*!
    @enum
    @abstract Policy errors
    @constant WebKitErrorCannotShowMIMEType
    @constant WebKitErrorCannotShowURL
    @constant WebKitErrorFrameLoadInterruptedByPolicyChange
*/
enum {
    WebKitErrorCannotShowMIMEType = 				100,
    WebKitErrorCannotShowURL = 					101,
    WebKitErrorFrameLoadInterruptedByPolicyChange = 		102,
};

/*!
    @enum
    @abstract Plug-in and java errors
    @constant WebKitErrorCannotFindPlugin
    @constant WebKitErrorCannotLoadPlugin
    @constant WebKitErrorJavaUnavailable
*/
enum {
    WebKitErrorCannotFindPlugin = 				200,
    WebKitErrorCannotLoadPlugin = 				201,
    WebKitErrorJavaUnavailable = 				202,
};
