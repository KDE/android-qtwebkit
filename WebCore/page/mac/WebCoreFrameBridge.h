/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <Cocoa/Cocoa.h>
#import <JavaScriptCore/npruntime.h>
#import <JavaVM/jni.h>
#import <WebCore/WebCoreKeyboardUIMode.h>
#import <WebCore/EditAction.h>
#import <WebCore/SelectionController.h>
#import <WebCore/TextAffinity.h>
#import <WebCore/TextGranularity.h>

namespace WebCore {
    class Frame;
    class HTMLFrameOwnerElement;
    class Page;
    class String;
}

@class DOMCSSStyleDeclaration;
@class DOMDocument;
@class DOMDocumentFragment;
@class DOMElement;
@class DOMHTMLInputElement;
@class DOMHTMLTextAreaElement;
@class DOMNode;
@class DOMRange;
@class NSMenu;

@protocol WebCoreRenderTreeCopier;

extern NSString *WebCorePageCacheStateKey;

enum WebCoreDeviceType {
    WebCoreDeviceScreen,
    WebCoreDevicePrinter
};

enum WebScrollDirection {
    WebScrollUp,
    WebScrollDown,
    WebScrollLeft,
    WebScrollRight
};

enum WebScrollGranularity {
    WebScrollLine,
    WebScrollPage,
    WebScrollDocument,
    WebScrollWheel
};

typedef enum ObjectElementType {
    ObjectElementNone,
    ObjectElementImage,
    ObjectElementFrame,
    ObjectElementPlugin
};

@protocol WebCoreOpenPanelResultListener <NSObject>
- (void)chooseFilename:(NSString *)fileName;
- (void)cancel;
@end

// WebCoreFrameBridge objects are used by WebCore to abstract away operations that need
// to be implemented by library clients, for example WebKit. The objects are also
// used in the opposite direction, for simple access to WebCore functions without dealing
// directly with the KHTML C++ classes.

// A WebCoreFrameBridge creates and holds a reference to a Frame.

// The WebCoreFrameBridge interface contains methods for use by the non-WebCore side of the bridge.

@interface WebCoreFrameBridge : NSObject
{
@public
    WebCore::Frame* m_frame;
    BOOL _shouldCreateRenderers;
    BOOL _closed;
}

- (WebCore::Frame*)_frame; // underscore to prevent conflict with -[NSView frame]

+ (WebCoreFrameBridge *)bridgeForDOMDocument:(DOMDocument *)document;

- (id)init;
- (void)close;

- (void)addData:(NSData *)data;

- (void)clearFrame;

- (NSURL *)baseURL;

- (void)installInFrame:(NSView *)view;

- (BOOL)scrollOverflowInDirection:(WebScrollDirection)direction granularity:(WebScrollGranularity)granularity;

- (void)createFrameViewWithNSView:(NSView *)view marginWidth:(int)mw marginHeight:(int)mh;

- (void)reapplyStylesForDeviceType:(WebCoreDeviceType)deviceType;
- (void)forceLayoutAdjustingViewSize:(BOOL)adjustSizeFlag;
- (void)forceLayoutWithMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)adjustSizeFlag;
- (void)sendResizeEvent;
- (void)sendScrollEvent;
- (BOOL)needsLayout;
- (void)setNeedsLayout;
- (void)drawRect:(NSRect)rect;
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit;
- (NSArray*)computePageRectsWithPrintWidthScaleFactor:(float)printWidthScaleFactor printHeight:(float)printHeight;

- (NSObject *)copyRenderTree:(id <WebCoreRenderTreeCopier>)copier;
- (NSString *)renderTreeAsExternalRepresentation;

- (NSURL *)URLWithAttributeString:(NSString *)string;

- (DOMElement *)elementWithName:(NSString *)name inForm:(DOMElement *)form;
- (BOOL)elementDoesAutoComplete:(DOMElement *)element;
- (BOOL)elementIsPassword:(DOMElement *)element;
- (DOMElement *)formForElement:(DOMElement *)element;
- (DOMElement *)currentForm;
- (NSArray *)controlsInForm:(DOMElement *)form;
- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(DOMElement *)element;
- (NSString *)matchLabels:(NSArray *)labels againstElement:(DOMElement *)element;

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag startInSelection:(BOOL)startInSelection;
- (unsigned)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag limit:(unsigned)limit;
- (BOOL)markedTextMatchesAreHighlighted;
- (void)setMarkedTextMatchesAreHighlighted:(BOOL)doHighlight;
- (void)unmarkAllTextMatches;
- (NSArray *)rectsForTextMatches;

- (void)setTextSizeMultiplier:(float)multiplier;

- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string;
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)string forceUserGesture:(BOOL)forceUserGesture;
- (NSAppleEventDescriptor *)aeDescByEvaluatingJavaScriptFromString:(NSString *)string;

- (NSString *)selectedString;

- (NSString *)stringForRange:(DOMRange *)range;

- (NSString *)markupStringFromNode:(DOMNode *)node nodes:(NSArray **)nodes;
- (NSString *)markupStringFromRange:(DOMRange *)range nodes:(NSArray **)nodes;

- (NSRect)caretRectAtNode:(DOMNode *)node offset:(int)offset affinity:(NSSelectionAffinity)affinity;
- (NSRect)firstRectForDOMRange:(DOMRange *)range;
- (void)scrollDOMRangeToVisible:(DOMRange *)range;

// Emacs-style-editing "mark"
- (void)setMarkDOMRange:(DOMRange *)range;
- (DOMRange *)markDOMRange;

// international text input "marked text"
- (void)setMarkedTextDOMRange:(DOMRange *)range customAttributes:(NSArray *)attributes ranges:(NSArray *)ranges;
- (DOMRange *)markedTextDOMRange;
- (void)replaceMarkedTextWithText:(NSString *)text;

- (NSFont *)fontForSelection:(BOOL *)hasMultipleFonts;
- (NSWritingDirection)baseWritingDirectionForSelectionStart;

- (NSString *)stringWithData:(NSData *)data; // using the encoding of the frame's main resource
+ (NSString *)stringWithData:(NSData *)data textEncodingName:(NSString *)textEncodingName; // nil for textEncodingName means Latin-1

- (void)setShouldCreateRenderers:(BOOL)f;
- (BOOL)shouldCreateRenderers;

- (void)setBaseBackgroundColor:(NSColor *)backgroundColor;
- (void)setDrawsBackground:(BOOL)drawsBackround;

- (NSColor *)selectionColor;

- (id)accessibilityTree;

- (DOMRange *)rangeOfCharactersAroundCaret;
- (DOMRange *)rangeByAlteringCurrentSelection:(WebCore::SelectionController::EAlteration)alteration direction:(WebCore::SelectionController::EDirection)direction granularity:(WebCore::TextGranularity)granularity;
- (void)alterCurrentSelection:(WebCore::SelectionController::EAlteration)alteration verticalDistance:(float)distance;
- (WebCore::TextGranularity)selectionGranularity;
- (DOMRange *)smartDeleteRangeForProposedRange:(DOMRange *)proposedCharRange;
- (void)smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)charRangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString;
- (void)selectNSRange:(NSRange)range;
- (NSRange)selectedNSRange;
- (NSRange)markedTextNSRange;
- (DOMRange *)convertNSRangeToDOMRange:(NSRange)range;
- (NSRange)convertDOMRangeToNSRange:(DOMRange *)range;

- (DOMDocumentFragment *)documentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString;
- (DOMDocumentFragment *)documentFragmentWithText:(NSString *)text inContext:(DOMRange *)context;
- (DOMDocumentFragment *)documentFragmentWithNodesAsParagraphs:(NSArray *)nodes;

- (void)replaceSelectionWithFragment:(DOMDocumentFragment *)fragment selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
- (void)replaceSelectionWithNode:(DOMNode *)node selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace matchStyle:(BOOL)matchStyle;
- (void)replaceSelectionWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;
- (void)replaceSelectionWithText:(NSString *)text selectReplacement:(BOOL)selectReplacement smartReplace:(BOOL)smartReplace;

- (bool)canIncreaseSelectionListLevel;
- (bool)canDecreaseSelectionListLevel;
- (DOMNode *)increaseSelectionListLevel;
- (DOMNode *)increaseSelectionListLevelOrdered;
- (DOMNode *)increaseSelectionListLevelUnordered;
- (void)decreaseSelectionListLevel;

- (void)insertParagraphSeparatorInQuotedContent;

- (void)setSelectionToDragCaret;
- (void)moveSelectionToDragCaret:(DOMDocumentFragment *)selectionFragment smartMove:(BOOL)smartMove;
- (void)moveDragCaretToPoint:(NSPoint)point;
- (DOMRange *)dragCaretDOMRange;
- (BOOL)isDragCaretRichlyEditable;
- (DOMRange *)editableDOMRangeForPoint:(NSPoint)point;
- (DOMRange *)characterRangeAtPoint:(NSPoint)point;

- (void)deleteKeyPressedWithSmartDelete:(BOOL)smartDelete granularity:(WebCore::TextGranularity)granularity;
- (void)forwardDeleteKeyPressedWithSmartDelete:(BOOL)smartDelete granularity:(WebCore::TextGranularity)granularity;

- (DOMCSSStyleDeclaration *)typingStyle;
- (void)setTypingStyle:(DOMCSSStyleDeclaration *)style withUndoAction:(WebCore::EditAction)undoAction;
- (NSCellStateValue)selectionHasStyle:(DOMCSSStyleDeclaration *)style;

- (void)dragSourceMovedTo:(NSPoint)windowLoc;
- (void)dragSourceEndedAt:(NSPoint)windowLoc operation:(NSDragOperation)operation;

- (BOOL)isCharacterSmartReplaceExempt:(unichar)c isPreviousCharacter:(BOOL)isPreviousCharacter;

- (BOOL)getData:(NSData **)data andResponse:(NSURLResponse **)response forURL:(NSURL *)URL;
- (void)getAllResourceDatas:(NSArray **)datas andResponses:(NSArray **)responses;

- (BOOL)canProvideDocumentSource;
- (BOOL)canSaveAsWebArchive;

- (void)receivedData:(NSData *)data textEncodingName:(NSString *)textEncodingName;

@end

// The WebCoreFrameBridge protocol contains methods for use by the WebCore side of the bridge.

@protocol WebCoreFrameBridge

- (NSView *)documentView;

- (WebCore::Frame*)createChildFrameNamed:(NSString *)frameName withURL:(NSURL *)URL referrer:(const WebCore::String&)referrer
    ownerElement:(WebCore::HTMLFrameOwnerElement *)ownerElement allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height;

- (NSWindow *)window;

- (NSResponder *)firstResponder;
- (void)makeFirstResponder:(NSResponder *)responder;
- (void)willMakeFirstResponderForNodeFocus;

- (BOOL)textViewWasFirstResponderAtMouseDownTime:(NSTextView *)textView;

- (void)runOpenPanelForFileButtonWithResultListener:(id <WebCoreOpenPanelResultListener>)resultListener;

- (void)setNeedsReapplyStyles;

- (NSView *)viewForPluginWithURL:(NSURL *)URL
                  attributeNames:(NSArray *)attributeNames
                 attributeValues:(NSArray *)attributeValues
                        MIMEType:(NSString *)MIMEType
                      DOMElement:(DOMElement *)element
                    loadManually:(BOOL)loadManually;
- (NSView *)viewForJavaAppletWithFrame:(NSRect)frame
                        attributeNames:(NSArray *)attributeNames
                       attributeValues:(NSArray *)attributeValues
                               baseURL:(NSURL *)baseURL
                            DOMElement:(DOMElement *)element;
- (void)redirectDataToPlugin:(NSView *)pluginView;

- (int)getObjectCacheSize;

- (ObjectElementType)determineObjectFromMIMEType:(NSString*)MIMEType URL:(NSURL*)URL;

- (BOOL)startDraggingImage:(NSImage *)dragImage at:(NSPoint)dragLoc operation:(NSDragOperation)op event:(NSEvent *)event sourceIsDHTML:(BOOL)flag DHTMLWroteData:(BOOL)dhtmlWroteData;

- (void)setHasBorder:(BOOL)hasBorder;

- (void)print;

- (jobject)getAppletInView:(NSView *)view;

// Deprecated, use getAppletInView: instead.
- (jobject)pollForAppletInView:(NSView *)view;

- (void)issuePasteCommand;
- (void)issueTransposeCommand;
- (void)respondToChangedSelection;
- (void)setIsSelected:(BOOL)isSelected forView:(NSView *)view;

- (NSString *)overrideMediaType;

- (void)windowObjectCleared;

- (void)dashboardRegionsChanged:(NSMutableDictionary *)regions;
- (void)willPopupMenu:(NSMenu *)menu;

- (NSRect)customHighlightRect:(NSString*)type forLine:(NSRect)lineRect;
- (void)paintCustomHighlight:(NSString*)type forBox:(NSRect)boxRect onLine:(NSRect)lineRect behindText:(BOOL)text entireLine:(BOOL)line;

- (WebCore::KeyboardUIMode)keyboardUIMode;

- (NSString*)imageTitleForFilename:(NSString*)filename size:(NSSize)size;

@end

// This interface definition allows those who hold a WebCoreFrameBridge * to call all the methods
// in the WebCoreFrameBridge protocol without requiring the base implementation to supply the methods.
// This idiom is appropriate because WebCoreFrameBridge is an abstract class.

@interface WebCoreFrameBridge (SubclassResponsibility) <WebCoreFrameBridge>
@end

// Protocols that make up part of the interfaces above.

@protocol WebCoreRenderTreeCopier <NSObject>
- (NSObject *)nodeWithName:(NSString *)name position:(NSPoint)p rect:(NSRect)rect view:(NSView *)view children:(NSArray *)children;
@end
