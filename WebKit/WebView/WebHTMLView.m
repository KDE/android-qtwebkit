/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#import "WebHTMLView.h"

#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebArchive.h"
#import "WebArchiver.h"
#import "WebBaseNetscapePluginViewInternal.h"
#import "WebClipView.h"
#import "WebDOMOperationsPrivate.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultUIDelegate.h"
#import "WebDocumentInternal.h"
#import "WebEditingDelegate.h"
#import "WebElementDictionary.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebFramePrivate.h"
#import "WebFrameViewInternal.h"
#import "WebHTMLRepresentationPrivate.h"
#import "WebHTMLViewInternal.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebLocalizableStrings.h"
#import "WebNSAttributedStringExtras.h"
#import "WebNSEventExtras.h"
#import "WebNSFileManagerExtras.h"
#import "WebNSImageExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSPasteboardExtras.h"
#import "WebNSPrintOperationExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSViewExtras.h"
#import "WebNetscapePluginEmbeddedView.h"
#import "WebPluginController.h"
#import "WebPreferences.h"
#import "WebPreferencesPrivate.h"
#import "WebResourcePrivate.h"
#import "WebStringTruncator.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <AppKit/NSAccessibility.h>
#import <ApplicationServices/ApplicationServices.h>
#import <WebCore/ContextMenuController.h>
#import <WebCore/Document.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/ExceptionHandlers.h>
#import <WebCore/FloatRect.h>
#import <WebCore/FrameMac.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/Page.h>
#import <WebCore/Range.h>
#import <WebCore/SelectionController.h>
#import <WebCore/WebCoreTextRenderer.h>
#import <WebCore/WebDataProtocol.h>
#import <WebCore/WebMimeTypeRegistryBridge.h>
#import <WebKit/DOM.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMPrivate.h>
#import <WebKitSystemInterface.h>
#import <mach-o/dyld.h> 

using namespace WebCore;

extern "C" {

// need to declare this because AppKit does not make it available as API or SPI
extern NSString *NSMarkedClauseSegmentAttributeName; 
extern NSString *NSTextInputReplacementRangeAttributeName; 

// Kill ring calls. Would be better to use NSKillRing.h, but that's not available in SPI.

void _NSInitializeKillRing(void);
void _NSAppendToKillRing(NSString *);
void _NSPrependToKillRing(NSString *);
NSString *_NSYankFromKillRing(void);
NSString *_NSYankPreviousFromKillRing(void);
void _NSNewKillRingSequence(void);
void _NSSetKillRingToYankedState(void);
void _NSResetKillRingOperationFlag(void);

}

@interface NSView (AppKitSecretsIKnowAbout)
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView;
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect;
- (NSRect)_dirtyRect;
- (void)_setDrawsOwnDescendants:(BOOL)drawsOwnDescendants;
- (void)_propagateDirtyRectsToOpaqueAncestors;
@end

@interface NSApplication (AppKitSecretsIKnowAbout)
- (void)speakString:(NSString *)string;
@end

@interface NSWindow (AppKitSecretsIKnowAbout)
- (id)_newFirstResponderAfterResigning;
@end

@interface NSAttributedString (AppKitSecretsIKnowAbout)
- (id)_initWithDOMRange:(DOMRange *)domRange;
- (DOMDocumentFragment *)_documentFromRange:(NSRange)range document:(DOMDocument *)document documentAttributes:(NSDictionary *)dict subresources:(NSArray **)subresources;
@end

@interface NSSpellChecker (CurrentlyPrivateForTextView)
- (void)learnWord:(NSString *)word;
@end

// By imaging to a width a little wider than the available pixels,
// thin pages will be scaled down a little, matching the way they
// print in IE and Camino. This lets them use fewer sheets than they
// would otherwise, which is presumably why other browsers do this.
// Wide pages will be scaled down more than this.
#define PrintingMinimumShrinkFactor     1.25f

// This number determines how small we are willing to reduce the page content
// in order to accommodate the widest line. If the page would have to be
// reduced smaller to make the widest line fit, we just clip instead (this
// behavior matches MacIE and Mozilla, at least)
#define PrintingMaximumShrinkFactor     2.0f

// This number determines how short the last printed page of a multi-page print session
// can be before we try to shrink the scale in order to reduce the number of pages, and
// thus eliminate the orphan.
#define LastPrintedPageOrphanRatio      0.1f

// This number determines the amount the scale factor is adjusted to try to eliminate orphans.
// It has no direct mathematical relationship to LastPrintedPageOrphanRatio, due to variable
// numbers of pages, logic to avoid breaking elements, and CSS-supplied hard page breaks.
#define PrintingOrphanShrinkAdjustment  1.1f

#define AUTOSCROLL_INTERVAL             0.1f

#define DRAG_LABEL_BORDER_X             4.0f
#define DRAG_LABEL_BORDER_Y             2.0f
#define DRAG_LABEL_RADIUS               5.0f
#define DRAG_LABEL_BORDER_Y_OFFSET              2.0f

#define MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP        120.0f
#define MAX_DRAG_LABEL_WIDTH                    320.0f

#define DRAG_LINK_LABEL_FONT_SIZE   11.0f
#define DRAG_LINK_URL_FONT_SIZE   10.0f

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

// FIXME: This constant is copied from AppKit's _NXSmartPaste constant.
#define WebSmartPastePboardType @"NeXT smart paste pasteboard type"

#define STANDARD_WEIGHT 5
#define MIN_BOLD_WEIGHT 9
#define STANDARD_BOLD_WEIGHT 10

typedef enum {
    deleteSelectionAction,
    deleteKeyAction,
    forwardDeleteKeyAction
} WebDeletionAction;

// if YES, do the standard NSView hit test (which can't give the right result when HTML overlaps a view)
static BOOL forceNSViewHitTest = NO;

// if YES, do the "top WebHTMLView" it test (which we'd like to do all the time but can't because of Java requirements [see bug 4349721])
static BOOL forceWebHTMLViewHitTest = NO;

// Used to avoid linking with ApplicationServices framework for _DCMDictionaryServiceWindowShow
extern "C" void *_NSSoftLinkingGetFrameworkFuncPtr(NSString *inUmbrellaFrameworkName,
    NSString *inFrameworkName, const char *inFuncName, const struct mach_header **);

@interface WebHTMLView (WebTextSizing) <_WebDocumentTextSizing>
@end

@interface WebHTMLView (WebHTMLViewFileInternal)
- (BOOL)_imageExistsAtPaths:(NSArray *)paths;
- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard inContext:(DOMRange *)context allowPlainText:(BOOL)allowPlainText chosePlainText:(BOOL *)chosePlainText;
- (NSString *)_plainTextFromPasteboard:(NSPasteboard *)pasteboard;
- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText;
- (void)_pasteAsPlainTextWithPasteboard:(NSPasteboard *)pasteboard;
- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)_shouldInsertText:(NSString *)text replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action;
- (float)_calculatePrintHeight;
- (void)_updateTextSizeMultiplier;
- (DOMRange *)_selectedRange;
- (BOOL)_shouldDeleteRange:(DOMRange *)range;
- (void)_deleteRange:(DOMRange *)range 
            killRing:(BOOL)killRing 
             prepend:(BOOL)prepend 
       smartDeleteOK:(BOOL)smartDeleteOK
      deletionAction:(WebDeletionAction)deletionAction
         granularity:(TextGranularity)granularity;
- (void)_deleteSelection;
- (BOOL)_canSmartReplaceWithPasteboard:(NSPasteboard *)pasteboard;
- (NSView *)_hitViewForEvent:(NSEvent *)event;
- (void)_writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard cachedAttributedString:(NSAttributedString *)attributedString;
- (DOMRange *)_documentRange;
- (WebFrameBridge *)_bridge;
- (void)_setMouseDownEvent:(NSEvent *)event;
@end

@interface WebHTMLView (WebForwardDeclaration) // FIXME: Put this in a normal category and stop doing the forward declaration trick.
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize;
@end

@interface WebHTMLView (WebNSTextInputSupport) <NSTextInput>
- (void)_updateSelectionForInputManager;
- (void)_insertText:(NSString *)text selectInsertedText:(BOOL)selectText;
@end

@interface WebHTMLView (WebEditingStyleSupport)
- (DOMCSSStyleDeclaration *)_emptyStyle;
- (NSString *)_colorAsString:(NSColor *)color;
@end

@interface NSView (WebHTMLViewFileInternal)
- (void)_web_setPrintingModeRecursive;
- (void)_web_clearPrintingModeRecursive;
- (void)_web_layoutIfNeededRecursive;
- (void)_web_layoutIfNeededRecursive:(NSRect)rect testDirtyRect:(bool)testDirtyRect;
@end

@interface NSMutableDictionary (WebHTMLViewFileInternal)
- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key;
@end

// Handles the complete: text command
@interface WebTextCompleteController : NSObject
{
@private
    WebHTMLView *_view;
    NSWindow *_popupWindow;
    NSTableView *_tableView;
    NSArray *_completions;
    NSString *_originalString;
    int prefixLength;
}
- (id)initWithHTMLView:(WebHTMLView *)view;
- (void)doCompletion;
- (void)endRevertingChange:(BOOL)revertChange moveLeft:(BOOL)goLeft;
- (BOOL)filterKeyDown:(NSEvent *)event;
- (void)_reflectSelection;
@end

@implementation WebHTMLViewPrivate

- (void)dealloc
{
    ASSERT(autoscrollTimer == nil);
    ASSERT(autoscrollTriggerEvent == nil);
    
    [mouseDownEvent release];
    [keyDownEvent release];
    [draggingImageURL release];
    [pluginController release];
    [toolTip release];
    [compController release];
    [firstResponderTextViewAtMouseDownTime release];
    [dataSource release];
    [highlighters release];

    [super dealloc];
}

- (void)clear
{
    [mouseDownEvent release];
    [keyDownEvent release];
    [draggingImageURL release];
    [pluginController release];
    [toolTip release];
    [compController release];
    [firstResponderTextViewAtMouseDownTime release];
    [dataSource release];
    [highlighters release];

    mouseDownEvent = nil;
    keyDownEvent = nil;
    draggingImageURL = nil;
    pluginController = nil;
    toolTip = nil;
    compController = nil;
    firstResponderTextViewAtMouseDownTime = nil;
    dataSource = nil;
    highlighters = nil;
}

@end

@implementation WebHTMLView (WebHTMLViewFileInternal)

- (DOMRange *)_documentRange
{
    return [[[self _frame] DOMDocument] _documentRange];
}

- (BOOL)_imageExistsAtPaths:(NSArray *)paths
{
    NSEnumerator *enumerator = [paths objectEnumerator];
    NSString *path;
    
    while ((path = [enumerator nextObject]) != nil) {
        NSString *MIMEType = WKGetMIMETypeForExtension([path pathExtension]);
        if ([WebMimeTypeRegistryBridge supportsImageResourceWithMIMEType:MIMEType]) {
            return YES;
        }
    }
    
    return NO;
}

- (WebDataSource *)_dataSource
{
    return _private->dataSource;
}

- (WebFrameBridge *)_bridge
{
    return [_private->dataSource _bridge];
}

- (WebView *)_webView
{
    return [_private->dataSource _webView];
}

- (WebFrameView *)_frameView
{
    return [[_private->dataSource webFrame] frameView];
}

- (DOMDocumentFragment *)_documentFragmentWithPaths:(NSArray *)paths
{
    DOMDocumentFragment *fragment;
    NSEnumerator *enumerator = [paths objectEnumerator];
    WebDataSource *dataSource = [self _dataSource];
    NSMutableArray *domNodes = [[NSMutableArray alloc] init];
    NSString *path;
    
    while ((path = [enumerator nextObject]) != nil) {
        NSString *MIMEType = WKGetMIMETypeForExtension([path pathExtension]);
        if ([WebMimeTypeRegistryBridge supportsImageResourceWithMIMEType:MIMEType]) {
            WebResource *resource = [[WebResource alloc] initWithData:[NSData dataWithContentsOfFile:path]
                                                                  URL:[NSURL fileURLWithPath:path]
                                                             MIMEType:MIMEType 
                                                     textEncodingName:nil
                                                            frameName:nil];
            if (resource) {
                [domNodes addObject:[dataSource _imageElementWithImageResource:resource]];
                [resource release];
            }
        } else {
            // Non-image file types
            NSString *url = [[[NSURL fileURLWithPath:path] _webkit_canonicalize] _web_userVisibleString];
            [domNodes addObject:[[[self _frame] DOMDocument] createTextNode: url]];
        }
    }
    
    fragment = [[self _bridge] documentFragmentWithNodesAsParagraphs:domNodes]; 
    
    [domNodes release];
    
    return [fragment firstChild] != nil ? fragment : nil;
}

+ (NSArray *)_excludedElementsForAttributedStringConversion
{
    static NSArray *elements = nil;
    if (elements == nil) {
        elements = [[NSArray alloc] initWithObjects:
            // Omit style since we want style to be inline so the fragment can be easily inserted.
            @"style",
            // Omit xml so the result is not XHTML.
            @"xml", 
            // Omit tags that will get stripped when converted to a fragment anyway.
            @"doctype", @"html", @"head", @"body",
            // Omit deprecated tags.
            @"applet", @"basefont", @"center", @"dir", @"font", @"isindex", @"menu", @"s", @"strike", @"u",
            // Omit object so no file attachments are part of the fragment.
            @"object", nil];
    }
    return elements;
}

- (DOMDocumentFragment *)_documentFragmentFromPasteboard:(NSPasteboard *)pasteboard
                                               inContext:(DOMRange *)context
                                          allowPlainText:(BOOL)allowPlainText
                                          chosePlainText:(BOOL *)chosePlainText
{
    NSArray *types = [pasteboard types];
    *chosePlainText = NO;

    if ([types containsObject:WebArchivePboardType]) {
        WebArchive *archive = [[WebArchive alloc] initWithData:[pasteboard dataForType:WebArchivePboardType]];
        if (archive) {
            DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithArchive:archive];
            [archive release];
            if (fragment) {
                return fragment;
            }
        }
    }
    
    if ([types containsObject:NSFilenamesPboardType]) {
        DOMDocumentFragment *fragment = [self _documentFragmentWithPaths:[pasteboard propertyListForType:NSFilenamesPboardType]];
        if (fragment != nil) {
            return fragment;
        }
    }
    
    NSURL *URL;
    
    if ([types containsObject:NSHTMLPboardType]) {
        NSString *HTMLString = [pasteboard stringForType:NSHTMLPboardType];
        // This is a hack to make Microsoft's HTML pasteboard data work. See 3778785.
        if ([HTMLString hasPrefix:@"Version:"]) {
            NSRange range = [HTMLString rangeOfString:@"<html" options:NSCaseInsensitiveSearch];
            if (range.location != NSNotFound) {
                HTMLString = [HTMLString substringFromIndex:range.location];
            }
        }
        if ([HTMLString length] != 0) {
            return [[self _bridge] documentFragmentWithMarkupString:HTMLString baseURLString:nil];
        }
    }
        
    NSAttributedString *string = nil;
    if ([types containsObject:NSRTFDPboardType]) {
        string = [[NSAttributedString alloc] initWithRTFD:[pasteboard dataForType:NSRTFDPboardType] documentAttributes:NULL];
    }
    if (string == nil && [types containsObject:NSRTFPboardType]) {
        string = [[NSAttributedString alloc] initWithRTF:[pasteboard dataForType:NSRTFPboardType] documentAttributes:NULL];
    }
    if (string != nil) {
        NSDictionary *documentAttributes = [[NSDictionary alloc] initWithObjectsAndKeys:
            [[self class] _excludedElementsForAttributedStringConversion], NSExcludedElementsDocumentAttribute,
            self, @"WebResourceHandler", nil];
        NSArray *subresources;
        DOMDocumentFragment *fragment = [string _documentFromRange:NSMakeRange(0, [string length]) 
                                                          document:[[self _frame] DOMDocument] 
                                                documentAttributes:documentAttributes
                                                      subresources:&subresources];
        [documentAttributes release];
        [string release];
        return fragment;
    }
    
    if ([types containsObject:NSTIFFPboardType]) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:NSTIFFPboardType]
                                                              URL:[NSURL _web_uniqueWebDataURLWithRelativeString:@"/image.tiff"]
                                                         MIMEType:@"image/tiff" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }
    
    if ([types containsObject:NSPICTPboardType]) {
        WebResource *resource = [[WebResource alloc] initWithData:[pasteboard dataForType:NSPICTPboardType]
                                                              URL:[NSURL _web_uniqueWebDataURLWithRelativeString:@"/image.pict"]
                                                         MIMEType:@"image/pict" 
                                                 textEncodingName:nil
                                                        frameName:nil];
        DOMDocumentFragment *fragment = [[self _dataSource] _documentFragmentWithImageResource:resource];
        [resource release];
        return fragment;
    }    
    
    if ((URL = [NSURL URLFromPasteboard:pasteboard])) {
        DOMDocument* document = [[self _frame] DOMDocument];
        ASSERT(document);
        if (document) {
            DOMHTMLAnchorElement* anchor = (DOMHTMLAnchorElement*)[document createElement:@"a"];
            NSString *URLString = [URL _web_userVisibleString];
            NSString *URLTitleString = [pasteboard stringForType:WebURLNamePboardType];
            DOMText* text = [document createTextNode:URLTitleString];
            [anchor setHref:URLString];
            [anchor appendChild:text];
            DOMDocumentFragment* fragment = [document createDocumentFragment];
            [fragment appendChild:anchor];
            if ([URLString length] > 0)
                return fragment;
        }
    }
    
    if (allowPlainText && [types containsObject:NSStringPboardType]) {
        *chosePlainText = YES;
        return [[self _bridge] documentFragmentWithText:[pasteboard stringForType:NSStringPboardType]
                                              inContext:context];
    }
    
    return nil;
}

- (NSString *)_plainTextFromPasteboard:(NSPasteboard *)pasteboard
{
    NSArray *types = [pasteboard types];
    
    if ([types containsObject:NSStringPboardType])
        return [pasteboard stringForType:NSStringPboardType];
    
    NSAttributedString *attributedString = nil;
    NSString *string;

    if ([types containsObject:NSRTFDPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTFD:[pasteboard dataForType:NSRTFDPboardType] documentAttributes:NULL];
    if (attributedString == nil && [types containsObject:NSRTFPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTF:[pasteboard dataForType:NSRTFPboardType] documentAttributes:NULL];
    if (attributedString != nil) {
        string = [[attributedString string] copy];
        [attributedString release];
        return [string autorelease];
    }
    
    if ([types containsObject:NSFilenamesPboardType]) {
        string = [[pasteboard propertyListForType:NSFilenamesPboardType] componentsJoinedByString:@"\n"];
        if (string != nil)
            return string;
    }
    
    NSURL *URL;
    
    if ((URL = [NSURL URLFromPasteboard:pasteboard])) {
        string = [URL _web_userVisibleString];
        if ([string length] > 0)
            return string;
    }
    
    return nil;
}

- (void)_pasteWithPasteboard:(NSPasteboard *)pasteboard allowPlainText:(BOOL)allowPlainText
{
    DOMRange *range = [self _selectedRange];
    BOOL chosePlainText;
    DOMDocumentFragment *fragment = [self _documentFragmentFromPasteboard:pasteboard
        inContext:range allowPlainText:allowPlainText chosePlainText:&chosePlainText];
    WebFrameBridge *bridge = [self _bridge];
    if (fragment && [self _shouldInsertFragment:fragment replacingDOMRange:[self _selectedRange] givenAction:WebViewInsertActionPasted]) {
        [bridge replaceSelectionWithFragment:fragment selectReplacement:NO smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard] matchStyle:chosePlainText];
    }
}

- (void)_pasteAsPlainTextWithPasteboard:(NSPasteboard *)pasteboard
{
    NSString *text = [self _plainTextFromPasteboard:pasteboard];
    if ([self _shouldReplaceSelectionWithText:text givenAction:WebViewInsertActionPasted])
        [[self _bridge] replaceSelectionWithText:text selectReplacement:NO smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard]];
}

- (BOOL)_shouldInsertFragment:(DOMDocumentFragment *)fragment replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    DOMNode *child = [fragment firstChild];
    if ([fragment lastChild] == child && [child isKindOfClass:[DOMCharacterData class]])
        return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:[(DOMCharacterData *)child data] replacingDOMRange:range givenAction:action];
    return [[webView _editingDelegateForwarder] webView:webView shouldInsertNode:fragment replacingDOMRange:range givenAction:action];
}

- (BOOL)_shouldInsertText:(NSString *)text replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    WebView *webView = [self _webView];
    return [[webView _editingDelegateForwarder] webView:webView shouldInsertText:text replacingDOMRange:range givenAction:action];
}

- (BOOL)_shouldReplaceSelectionWithText:(NSString *)text givenAction:(WebViewInsertAction)action
{
    return [self _shouldInsertText:text replacingDOMRange:[self _selectedRange] givenAction:action];
}

// Calculate the vertical size of the view that fits on a single page
- (float)_calculatePrintHeight
{
    // Obtain the print info object for the current operation
    NSPrintInfo *pi = [[NSPrintOperation currentOperation] printInfo];
    
    // Calculate the page height in points
    NSSize paperSize = [pi paperSize];
    return paperSize.height - [pi topMargin] - [pi bottomMargin];
}

- (void)_updateTextSizeMultiplier
{
    [[self _bridge] setTextSizeMultiplier:[[self _webView] textSizeMultiplier]];    
}

- (DOMRange *)_selectedRange
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return nil;
    return kit(coreFrame->selectionController()->toRange().get());
}

- (BOOL)_shouldDeleteRange:(DOMRange *)range
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return nil;
    return coreFrame->editor()->shouldDeleteRange(core(range));
}

- (void)_deleteRange:(DOMRange *)range 
            killRing:(BOOL)killRing 
             prepend:(BOOL)prepend 
       smartDeleteOK:(BOOL)smartDeleteOK 
      deletionAction:(WebDeletionAction)deletionAction
         granularity:(TextGranularity)granularity
{
    WebFrameBridge *bridge = [self _bridge];
    Frame* coreFrame = core([self _frame]);
    BOOL smartDelete = smartDeleteOK ? [self _canSmartCopyOrDelete] : NO;

    BOOL startNewKillRingSequence = _private->startNewKillRingSequence;

    if (killRing) {
        if (startNewKillRingSequence) {
            _NSNewKillRingSequence();
        }
        NSString *string = [bridge stringForRange:range];
        if (prepend) {
            _NSPrependToKillRing(string);
        } else {
            _NSAppendToKillRing(string);
        }
        startNewKillRingSequence = NO;
    }

    if (coreFrame) {
        SelectionController* selectionController = coreFrame->selectionController();
        Editor* editor = coreFrame->editor();
        switch (deletionAction) {
            case deleteSelectionAction:
                selectRange(selectionController, core(range), DOWNSTREAM, true);
                editor->deleteSelectionWithSmartDelete(smartDelete);
                break;
            case deleteKeyAction:
                selectRange(coreFrame->selectionController(), core(range), DOWNSTREAM, (granularity != CharacterGranularity));
                [bridge deleteKeyPressedWithSmartDelete:smartDelete granularity:granularity];
                break;
            case forwardDeleteKeyAction:
                selectRange(coreFrame->selectionController(), core(range), DOWNSTREAM, (granularity != CharacterGranularity));
                [bridge forwardDeleteKeyPressedWithSmartDelete:smartDelete granularity:granularity];
                break;
        }
    }

    _private->startNewKillRingSequence = startNewKillRingSequence;
}

- (void)_deleteSelection
{
    [self _deleteRange:[self _selectedRange]
              killRing:YES 
               prepend:NO
         smartDeleteOK:YES
        deletionAction:deleteSelectionAction
           granularity:CharacterGranularity];
}

- (BOOL)_canSmartReplaceWithPasteboard:(NSPasteboard *)pasteboard
{
    return [[self _webView] smartInsertDeleteEnabled] && [[pasteboard types] containsObject:WebSmartPastePboardType];
}

- (NSView *)_hitViewForEvent:(NSEvent *)event
{
    // Usually, we hack AK's hitTest method to catch all events at the topmost WebHTMLView.  
    // Callers of this method, however, want to query the deepest view instead.
    forceNSViewHitTest = YES;
    NSView *hitView = [[[self window] contentView] hitTest:[event locationInWindow]];
    forceNSViewHitTest = NO;    
    return hitView;
}

- (void)_writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard cachedAttributedString:(NSAttributedString *)attributedString
{
    // Put HTML on the pasteboard.
    if ([types containsObject:WebArchivePboardType]) {
        WebArchive *archive = [WebArchiver archiveSelectionInFrame:[self _frame]];
        [pasteboard setData:[archive data] forType:WebArchivePboardType];
    }
    
    // Put the attributed string on the pasteboard (RTF/RTFD format).
    if ([types containsObject:NSRTFDPboardType]) {
        if (attributedString == nil) {
            attributedString = [self selectedAttributedString];
        }        
        NSData *RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFDData forType:NSRTFDPboardType];
    }        
    if ([types containsObject:NSRTFPboardType]) {
        if (attributedString == nil) {
            attributedString = [self selectedAttributedString];
        }
        if ([attributedString containsAttachments]) {
            attributedString = [attributedString _web_attributedStringByStrippingAttachmentCharacters];
        }
        NSData *RTFData = [attributedString RTFFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [pasteboard setData:RTFData forType:NSRTFPboardType];
    }
    
    // Put plain string on the pasteboard.
    if ([types containsObject:NSStringPboardType]) {
        // Map &nbsp; to a plain old space because this is better for source code, other browsers do it,
        // and because HTML forces you to do this any time you want two spaces in a row.
        NSMutableString *s = [[self selectedString] mutableCopy];
        const unichar NonBreakingSpaceCharacter = 0xA0;
        NSString *NonBreakingSpaceString = [NSString stringWithCharacters:&NonBreakingSpaceCharacter length:1];
        [s replaceOccurrencesOfString:NonBreakingSpaceString withString:@" " options:0 range:NSMakeRange(0, [s length])];
        [pasteboard setString:s forType:NSStringPboardType];
        [s release];
    }
    
    if ([self _canSmartCopyOrDelete] && [types containsObject:WebSmartPastePboardType]) {
        [pasteboard setData:nil forType:WebSmartPastePboardType];
    }
}

- (void)_setMouseDownEvent:(NSEvent *)event
{
    ASSERT(!event || [event type] == NSLeftMouseDown || [event type] == NSRightMouseDown || [event type] == NSOtherMouseDown);

    if (event == _private->mouseDownEvent)
        return;

    [event retain];
    [_private->mouseDownEvent release];
    _private->mouseDownEvent = event;

    [_private->firstResponderTextViewAtMouseDownTime release];
    
    // The only code that checks this ivar only cares about NSTextViews. The code used to be more general,
    // but it caused reference cycles leading to world leaks (see 4557386). We should be able to eliminate
    // firstResponderTextViewAtMouseDownTime entirely when all the form controls are native widgets, because 
    // the only caller (in WebCore) will be unnecessary.
    if (event) {
        NSResponder *firstResponder = [[self window] firstResponder];
        if ([firstResponder isKindOfClass:[NSTextView class]])
            _private->firstResponderTextViewAtMouseDownTime = [firstResponder retain];
        else
            _private->firstResponderTextViewAtMouseDownTime = nil;
    } else
        _private->firstResponderTextViewAtMouseDownTime = nil;
}

@end

@implementation WebHTMLView (WebPrivate)

+ (NSArray *)supportedMIMETypes
{
    return [WebHTMLRepresentation supportedMIMETypes];
}

+ (NSArray *)supportedImageMIMETypes
{
    return [WebHTMLRepresentation supportedImageMIMETypes];
}

+ (NSArray *)supportedNonImageMIMETypes
{
    return [WebHTMLRepresentation supportedNonImageMIMETypes];
}

+ (NSArray *)unsupportedTextMIMETypes
{
    return [NSArray arrayWithObjects:
        @"text/calendar",       // iCal
        @"text/x-calendar",
        @"text/x-vcalendar",
        @"text/vcalendar",
        @"text/vcard",          // vCard
        @"text/x-vcard",
        @"text/directory",
        @"text/ldif",           // Netscape Address Book
        @"text/qif",            // Quicken
        @"text/x-qif",
        @"text/x-csv",          // CSV (for Address Book and Microsoft Outlook)
        @"text/x-vcf",          // vCard type used in Sun affinity app
        @"text/rtf",            // Rich Text Format
        nil];
}

+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[flagsChangedEvent window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[flagsChangedEvent modifierFlags]
        timestamp:[flagsChangedEvent timestamp]
        windowNumber:[flagsChangedEvent windowNumber]
        context:[flagsChangedEvent context]
        eventNumber:0 clickCount:0 pressure:0];

    // Pretend it's a mouse move.
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WKMouseMovedNotification() object:self
        userInfo:[NSDictionary dictionaryWithObject:fakeEvent forKey:@"NSEvent"]];
}

- (void)_updateMouseoverWithFakeEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    
    [self _updateMouseoverWithEvent:fakeEvent];
}

- (void)_frameOrBoundsChanged
{
    if (!NSEqualSizes(_private->lastLayoutSize, [(NSClipView *)[self superview] documentVisibleRect].size)) {
        [self setNeedsLayout:YES];
        [self setNeedsDisplay:YES];
        [_private->compController endRevertingChange:NO moveLeft:NO];
    }

    NSPoint origin = [[self superview] bounds].origin;
    if (!NSEqualPoints(_private->lastScrollPosition, origin)) {
        [[self _bridge] sendScrollEvent];
        [_private->compController endRevertingChange:NO moveLeft:NO];
        
        WebView *webView = [self _webView];
        [[webView _UIDelegateForwarder] webView:webView didScrollDocumentInFrameView:[self _frameView]];
    }
    _private->lastScrollPosition = origin;

    SEL selector = @selector(_updateMouseoverWithFakeEvent);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:selector object:nil];
    [self performSelector:selector withObject:nil afterDelay:0];
}

- (void)_setAsideSubviews
{
    ASSERT(!_private->subviewsSetAside);
    ASSERT(_private->savedSubviews == nil);
    _private->savedSubviews = _subviews;
    _subviews = nil;
    _private->subviewsSetAside = YES;
 }
 
 - (void)_restoreSubviews
 {
    ASSERT(_private->subviewsSetAside);
    ASSERT(_subviews == nil);
    _subviews = _private->savedSubviews;
    _private->savedSubviews = nil;
    _private->subviewsSetAside = NO;
}

// This is called when we are about to draw, but before our dirty rect is propagated to our ancestors.
// That's the perfect time to do a layout, except that ideally we'd want to be sure that we're dirty
// before doing it. As a compromise, when we're opaque we do the layout only when actually asked to
// draw, but when we're transparent we do the layout at this stage so views behind us know that they
// need to be redrawn (in case the layout causes some things to get dirtied).
- (void)_propagateDirtyRectsToOpaqueAncestors
{
    if (![[self _webView] drawsBackground]) {
        [self _web_layoutIfNeededRecursive];
    }
    [super _propagateDirtyRectsToOpaqueAncestors];
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayRectIfNeededIgnoringOpacity:(NSRect)rect isVisibleRect:(BOOL)isVisibleRect rectIsVisibleRectForView:(NSView *)visibleView topView:(BOOL)topView
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];
    if (wasInPrintingMode != isPrinting) {
        if (isPrinting) {
            [self _web_setPrintingModeRecursive];
        } else {
            [self _web_clearPrintingModeRecursive];
        }
    }

    [self _web_layoutIfNeededRecursive: rect testDirtyRect:YES];
    [_subviews makeObjectsPerformSelector:@selector(_propagateDirtyRectsToOpaqueAncestors)];

    [self _setAsideSubviews];
    [super _recursiveDisplayRectIfNeededIgnoringOpacity:rect isVisibleRect:isVisibleRect
        rectIsVisibleRectForView:visibleView topView:topView];
    [self _restoreSubviews];

    if (wasInPrintingMode != isPrinting) {
        if (wasInPrintingMode) {
            [self _web_setPrintingModeRecursive];
        } else {
            [self _web_clearPrintingModeRecursive];
        }
    }
}

// Don't let AppKit even draw subviews. We take care of that.
- (void)_recursiveDisplayAllDirtyWithLockFocus:(BOOL)needsLockFocus visRect:(NSRect)visRect
{
    BOOL needToSetAsideSubviews = !_private->subviewsSetAside;

    BOOL wasInPrintingMode = _private->printing;
    BOOL isPrinting = ![NSGraphicsContext currentContextDrawingToScreen];

    if (needToSetAsideSubviews) {
        // This helps when we print as part of a larger print process.
        // If the WebHTMLView itself is what we're printing, then we will never have to do this.
        if (wasInPrintingMode != isPrinting) {
            if (isPrinting) {
                [self _web_setPrintingModeRecursive];
            } else {
                [self _web_clearPrintingModeRecursive];
            }
        }

        NSRect boundsBeforeLayout = [self bounds];
        [self _web_layoutIfNeededRecursive: visRect testDirtyRect:NO];

        // If layout changes the view's bounds, then we need to recompute the visRect.
        // That's because the visRect passed to us was based on the bounds at the time
        // we were called. This method is only displayed to draw "all", so it's safe
        // to just call visibleRect to compute the entire rectangle.
        if (!NSEqualRects(boundsBeforeLayout, [self bounds])) {
            visRect = [self visibleRect];
        }

        [self _setAsideSubviews];
    }
    
    [super _recursiveDisplayAllDirtyWithLockFocus:needsLockFocus visRect:visRect];
    
    if (needToSetAsideSubviews) {
        if (wasInPrintingMode != isPrinting) {
            if (wasInPrintingMode) {
                [self _web_setPrintingModeRecursive];
            } else {
                [self _web_clearPrintingModeRecursive];
            }
        }

        [self _restoreSubviews];
    }
}

- (WebHTMLView *)_topHTMLView
{
    WebHTMLView *view = (WebHTMLView *)[[[[_private->dataSource _webView] mainFrame] frameView] documentView];
    ASSERT(view);
    ASSERT([view isKindOfClass:[WebHTMLView class]]);
    return view;
}

- (BOOL)_isTopHTMLView
{
    return self == [self _topHTMLView];
}

- (BOOL)_insideAnotherHTMLView
{
    return self != [self _topHTMLView];
}

- (void)scrollPoint:(NSPoint)point
{
    // Since we can't subclass NSTextView to do what we want, we have to second guess it here.
    // If we get called during the handling of a key down event, we assume the call came from
    // NSTextView, and ignore it and use our own code to decide how to page up and page down
    // We are smarter about how far to scroll, and we have "superview scrolling" logic.
    NSEvent *event = [[self window] currentEvent];
    if ([event type] == NSKeyDown) {
        const unichar pageUp = NSPageUpFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageUp length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageUp:) with:nil];
            return;
        }
        const unichar pageDown = NSPageDownFunctionKey;
        if ([[event characters] rangeOfString:[NSString stringWithCharacters:&pageDown length:1]].length == 1) {
            [self tryToPerform:@selector(scrollPageDown:) with:nil];
            return;
        }
    }
    
    [super scrollPoint:point];
}

- (NSView *)hitTest:(NSPoint)point
{
    // WebHTMLView objects handle all events for objects inside them.
    // To get those events, we prevent hit testing from AppKit.

    // But there are three exceptions to this:
    //   1) For right mouse clicks and control clicks we don't yet have an implementation
    //      that works for nested views, so we let the hit testing go through the
    //      standard NSView code path (needs to be fixed, see bug 4361618).
    //   2) Java depends on doing a hit test inside it's mouse moved handling,
    //      so we let the hit testing go through the standard NSView code path
    //      when the current event is a mouse move (except when we are calling
    //      from _updateMouseoverWithEvent, so we have to use a global,
    //      forceWebHTMLViewHitTest, for that)
    //   3) The acceptsFirstMouse: and shouldDelayWindowOrderingForEvent: methods
    //      both need to figure out which view to check with inside the WebHTMLView.
    //      They use a global to change the behavior of hitTest: so they can get the
    //      right view. The global is forceNSViewHitTest and the method they use to
    //      do the hit testing is _hitViewForEvent:. (But this does not work correctly
    //      when there is HTML overlapping the view, see bug 4361626)
    //   4) NSAccessibilityHitTest relies on this for checking the cursor position.
    //      Our check for that is whether the event is NSFlagsChanged.  This works
    //      for VoiceOver's cntl-opt-f5 command (move focus to item under cursor)
    //      and Dictionary's cmd-cntl-D (open dictionary popup for item under cursor).
    //      This is of course a hack.

    BOOL captureHitsOnSubviews;
    if (forceNSViewHitTest)
        captureHitsOnSubviews = NO;
    else if (forceWebHTMLViewHitTest)
        captureHitsOnSubviews = YES;
    else {
        NSEvent *event = [[self window] currentEvent];
        captureHitsOnSubviews = !([event type] == NSMouseMoved
            || [event type] == NSRightMouseDown
            || ([event type] == NSLeftMouseDown && ([event modifierFlags] & NSControlKeyMask) != 0)
            || [event type] == NSFlagsChanged);
    }

    if (!captureHitsOnSubviews)
        return [super hitTest:point];
    if ([[self superview] mouse:point inRect:[self frame]])
        return self;
    return nil;
}

static WebHTMLView *lastHitView = nil;

- (void)_clearLastHitViewIfSelf
{
    if (lastHitView == self)
        lastHitView = nil;
}

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (void)_addTrackingRects:(NSRect *)rects owner:(id)owner userDataList:(void **)userDataList assumeInsideList:(BOOL *)assumeInsideList trackingNums:(NSTrackingRectTag *)trackingNums count:(int)count
{
    ASSERT(count == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(_private->trackingRectOwner == nil);
    _private->trackingRectOwner = owner;
    _private->trackingRectUserData = userDataList[0];
    trackingNums[0] = TRACKING_RECT_TAG;
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    if (tag == 0)
        return;
    
    if (_private && (tag == TRACKING_RECT_TAG)) {
        _private->trackingRectOwner = nil;
        return;
    }
    
    if (_private && (tag == _private->lastToolTipTag)) {
        [super removeTrackingRect:tag];
        _private->lastToolTipTag = 0;
        return;
    }
    
    // If any other tracking rect is being removed, we don't know how it was created
    // and it's possible there's a leak involved (see 3500217)
    ASSERT_NOT_REACHED();
}

- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count
{
    int i;
    for (i = 0; i < count; ++i) {
        int tag = tags[i];
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        if (_private != nil) {
            _private->trackingRectOwner = nil;
        }
    }
}

- (void)_sendToolTipMouseExited
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseExited:fakeEvent];
}

- (void)_sendToolTipMouseEntered
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_private->trackingRectUserData];
    [_private->trackingRectOwner mouseEntered:fakeEvent];
}

- (void)_setToolTip:(NSString *)string
{
    NSString *toolTip = [string length] == 0 ? nil : string;
    NSString *oldToolTip = _private->toolTip;
    if ((toolTip == nil || oldToolTip == nil) ? toolTip == oldToolTip : [toolTip isEqualToString:oldToolTip]) {
        return;
    }
    if (oldToolTip) {
        [self _sendToolTipMouseExited];
        [oldToolTip release];
    }
    _private->toolTip = [toolTip copy];
    if (toolTip) {
        // See radar 3500217 for why we remove all tooltips rather than just the single one we created.
        [self removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        _private->lastToolTipTag = [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return [[_private->toolTip copy] autorelease];
}

- (void)_updateMouseoverWithEvent:(NSEvent *)event
{
    if (_private->closed)
        return;

    NSView *contentView = [[event window] contentView];
    NSPoint locationForHitTest = [[contentView superview] convertPoint:[event locationInWindow] fromView:nil];
    
    forceWebHTMLViewHitTest = YES;
    NSView *hitView = [contentView hitTest:locationForHitTest];
    forceWebHTMLViewHitTest = NO;
    
    WebHTMLView *view = nil;
    if ([hitView isKindOfClass:[WebHTMLView class]]) 
        view = (WebHTMLView *)hitView; 

    if (view)
        [view retain];

    if (lastHitView != view && lastHitView && [lastHitView _frame]) {
        // If we are moving out of a view (or frame), let's pretend the mouse moved
        // all the way out of that view. But we have to account for scrolling, because
        // khtml doesn't understand our clipping.
        NSRect visibleRect = [[[[lastHitView _frame] frameView] _scrollView] documentVisibleRect];
        float yScroll = visibleRect.origin.y;
        float xScroll = visibleRect.origin.x;

        event = [NSEvent mouseEventWithType:NSMouseMoved
                         location:NSMakePoint(-1 - xScroll, -1 - yScroll )
                         modifierFlags:[[NSApp currentEvent] modifierFlags]
                         timestamp:[NSDate timeIntervalSinceReferenceDate]
                         windowNumber:[[view window] windowNumber]
                         context:[[NSApp currentEvent] context]
                         eventNumber:0 clickCount:0 pressure:0];
        if (FrameMac* lastHitCoreFrame = core([lastHitView _frame]))
            lastHitCoreFrame->eventHandler()->mouseMoved(event);
    }

    lastHitView = view;

    if (view) {
        if (FrameMac* coreFrame = core([view _frame]))
            coreFrame->eventHandler()->mouseMoved(event);

        NSPoint point = [view convertPoint:[event locationInWindow] fromView:nil];
        NSDictionary *element = [view elementAtPoint:point];

        // Have the web view send a message to the delegate so it can do status bar display.
        [[view _webView] _mouseDidMoveOverElement:element modifierFlags:[event modifierFlags]];

        // Set a tool tip; it won't show up right away but will if the user pauses.

        // First priority is a potential toolTip representing a spelling or grammar error
        NSString *newToolTip = [element objectForKey:WebElementSpellingToolTipKey];
        
        // Next priority is a toolTip from a URL beneath the mouse (if preference is set to show those).
        if ([newToolTip length] == 0 && _private->showsURLsInToolTips) {
            DOMHTMLElement *domElement = [element objectForKey:WebElementDOMNodeKey];
            
            // Get tooltip representing form action, if relevant
            if ([domElement isKindOfClass:[DOMHTMLInputElement class]]) {
                if ([[(DOMHTMLInputElement *)domElement type] isEqualToString:@"submit"])
                    newToolTip = [[(DOMHTMLInputElement *) domElement form] action];
            }
            
            // Get tooltip representing link's URL
            if ([newToolTip length] == 0)
                newToolTip = [[element objectForKey:WebElementLinkURLKey] _web_userVisibleString];
        }
        
        // Lastly we'll consider a tooltip for element with "title" attribute
        if ([newToolTip length] == 0)
            newToolTip = [element objectForKey:WebElementTitleKey];
        
        [view _setToolTip:newToolTip];

        [view release];
    }
}

+ (NSArray *)_insertablePasteboardTypes
{
    static NSArray *types = nil;
    if (!types) {
        types = [[NSArray alloc] initWithObjects:WebArchivePboardType, NSHTMLPboardType,
            NSFilenamesPboardType, NSTIFFPboardType, NSPICTPboardType, NSURLPboardType, 
            NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, NSColorPboardType, nil];
    }
    return types;
}

+ (NSArray *)_selectionPasteboardTypes
{
    // FIXME: We should put data for NSHTMLPboardType on the pasteboard but Microsoft Excel doesn't like our format of HTML (3640423).
    return [NSArray arrayWithObjects:WebArchivePboardType, NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, nil];
}

- (NSImage *)_dragImageForLinkElement:(NSDictionary *)element
{
    NSURL *linkURL = [element objectForKey: WebElementLinkURLKey];

    BOOL drawURLString = YES;
    BOOL clipURLString = NO, clipLabelString = NO;
    
    NSString *label = [element objectForKey: WebElementLinkLabelKey];
    NSString *urlString = [linkURL _web_userVisibleString];
    
    if (!label) {
        drawURLString = NO;
        label = urlString;
    }
    
    NSFont *labelFont = [[NSFontManager sharedFontManager] convertFont:[NSFont systemFontOfSize:DRAG_LINK_LABEL_FONT_SIZE]
                                                   toHaveTrait:NSBoldFontMask];
    NSFont *urlFont = [NSFont systemFontOfSize: DRAG_LINK_URL_FONT_SIZE];
    NSSize labelSize;
    labelSize.width = [label _web_widthWithFont: labelFont];
    labelSize.height = [labelFont ascender] - [labelFont descender];
    if (labelSize.width > MAX_DRAG_LABEL_WIDTH){
        labelSize.width = MAX_DRAG_LABEL_WIDTH;
        clipLabelString = YES;
    }
    
    NSSize imageSize, urlStringSize;
    imageSize.width = labelSize.width + DRAG_LABEL_BORDER_X * 2.0f;
    imageSize.height = labelSize.height + DRAG_LABEL_BORDER_Y * 2.0f;
    if (drawURLString) {
        urlStringSize.width = [urlString _web_widthWithFont: urlFont];
        urlStringSize.height = [urlFont ascender] - [urlFont descender];
        imageSize.height += urlStringSize.height;
        if (urlStringSize.width > MAX_DRAG_LABEL_WIDTH) {
            imageSize.width = MAX(MAX_DRAG_LABEL_WIDTH + DRAG_LABEL_BORDER_X * 2.0f, MIN_DRAG_LABEL_WIDTH_BEFORE_CLIP);
            clipURLString = YES;
        } else {
            imageSize.width = MAX(labelSize.width + DRAG_LABEL_BORDER_X * 2.0f, urlStringSize.width + DRAG_LABEL_BORDER_X * 2.0f);
        }
    }
    NSImage *dragImage = [[[NSImage alloc] initWithSize: imageSize] autorelease];
    [dragImage lockFocus];
    
    [[NSColor colorWithCalibratedRed: 0.7f green: 0.7f blue: 0.7f alpha: 0.8f] set];
    
    // Drag a rectangle with rounded corners/
    NSBezierPath *path = [NSBezierPath bezierPath];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0.0f, 0.0f, DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(0, imageSize.height - DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2.0f, imageSize.height - DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f)];
    [path appendBezierPathWithOvalInRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS * 2.0f, 0.0f, DRAG_LABEL_RADIUS * 2.0f, DRAG_LABEL_RADIUS * 2.0f)];
    
    [path appendBezierPathWithRect: NSMakeRect(DRAG_LABEL_RADIUS, 0.0f, imageSize.width - DRAG_LABEL_RADIUS * 2.0f, imageSize.height)];
    [path appendBezierPathWithRect: NSMakeRect(0.0f, DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 10.0f, imageSize.height - 2.0f * DRAG_LABEL_RADIUS)];
    [path appendBezierPathWithRect: NSMakeRect(imageSize.width - DRAG_LABEL_RADIUS - 20.0f, DRAG_LABEL_RADIUS, DRAG_LABEL_RADIUS + 20.0f, imageSize.height - 2.0f * DRAG_LABEL_RADIUS)];
    [path fill];
        
    NSColor *topColor = [NSColor colorWithCalibratedWhite:0.0f alpha:0.75f];
    NSColor *bottomColor = [NSColor colorWithCalibratedWhite:1.0f alpha:0.5f];
    if (drawURLString) {
        if (clipURLString)
            urlString = [WebStringTruncator centerTruncateString: urlString toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2.0f) withFont:urlFont];

        [urlString _web_drawDoubledAtPoint:NSMakePoint(DRAG_LABEL_BORDER_X, DRAG_LABEL_BORDER_Y - [urlFont descender]) 
             withTopColor:topColor bottomColor:bottomColor font:urlFont];
    }

    if (clipLabelString)
        label = [WebStringTruncator rightTruncateString: label toWidth:imageSize.width - (DRAG_LABEL_BORDER_X * 2.0f) withFont:labelFont];
    [label _web_drawDoubledAtPoint:NSMakePoint (DRAG_LABEL_BORDER_X, imageSize.height - DRAG_LABEL_BORDER_Y_OFFSET - [labelFont pointSize])
             withTopColor:topColor bottomColor:bottomColor font:labelFont];
    
    [dragImage unlockFocus];
    
    return dragImage;
}

- (BOOL)_startDraggingImage:(NSImage *)wcDragImage at:(NSPoint)wcDragLoc operation:(NSDragOperation)op event:(NSEvent *)mouseDraggedEvent sourceIsDHTML:(BOOL)srcIsDHTML DHTMLWroteData:(BOOL)dhtmlWroteData
{
    WebHTMLView *topHTMLView = [self _topHTMLView];
    if (self != topHTMLView) {
        [topHTMLView _setMouseDownEvent:_private->mouseDownEvent];
        BOOL result = [topHTMLView _startDraggingImage:wcDragImage at:wcDragLoc operation:op event:mouseDraggedEvent sourceIsDHTML:srcIsDHTML DHTMLWroteData:dhtmlWroteData];
        [topHTMLView _setMouseDownEvent:nil];
        return result;
    }

    NSPoint mouseDownPoint = [self convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    NSDictionary *element = [self elementAtPoint:mouseDownPoint allowShadowContent:YES];

    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    NSURL *imageURL = [element objectForKey:WebElementImageURLKey];
    BOOL isSelected = [[element objectForKey:WebElementIsSelectedKey] boolValue];

    [_private->draggingImageURL release];
    _private->draggingImageURL = nil;

    NSPoint mouseDraggedPoint = [self convertPoint:[mouseDraggedEvent locationInWindow] fromView:nil];
    _private->webCoreDragOp = op;     // will be DragNone if WebCore doesn't care
    NSImage *dragImage = nil;
    NSPoint dragLoc = { 0, 0 }; // quiet gcc 4.0 warning

    // We allow WebCore to override the drag image, even if its a link, image or text we're dragging.
    // This is in the spirit of the IE API, which allows overriding of pasteboard data and DragOp.
    // We could verify that ActionDHTML is allowed, although WebCore does claim to respect the action.
    if (wcDragImage) {
        dragImage = wcDragImage;
        // wcDragLoc is the cursor position relative to the lower-left corner of the image.
        // We add in the Y dimension because we are a flipped view, so adding moves the image down.
        if (linkURL)
            // see HACK below
            dragLoc = NSMakePoint(mouseDraggedPoint.x - wcDragLoc.x, mouseDraggedPoint.y + wcDragLoc.y);
        else
            dragLoc = NSMakePoint(mouseDownPoint.x - wcDragLoc.x, mouseDownPoint.y + wcDragLoc.y);
        _private->dragOffset = wcDragLoc;
    }
    
    WebView *webView = [self _webView];
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    BOOL startedDrag = YES;  // optimism - we almost always manage to start the drag

    // note per kwebster, the offset arg below is always ignored in positioning the image
    DOMNode *node = [element objectForKey:WebElementDOMNodeKey];
    WebHTMLView *innerHTMLView = (WebHTMLView *)[[[[node ownerDocument] webFrame] frameView] documentView];
    ASSERT([innerHTMLView isKindOfClass:[WebHTMLView class]]);
    if (imageURL != nil
            && [node isKindOfClass:[DOMElement class]]
            && [(DOMElement *)node image]
            && (_private->dragSourceActionMask & WebDragSourceActionImage)) {
        id source = self;
        if (!dhtmlWroteData) {
            // Select the image when it is dragged. This allows the image to be moved via MoveSelectionCommandImpl and this matches NSTextView's behavior.
            ASSERT(node != nil);
            [webView setSelectedDOMRange:[[node ownerDocument] _createRangeWithNode:node] affinity:NSSelectionAffinityDownstream];
            _private->draggingImageURL = [imageURL retain];
            
            WebArchive *archive;
            
            // If the image element comes from an ImageDocument, we don't want to 
            // create a web archive from the image element.
            if ([[self _bridge] canSaveAsWebArchive])
                archive = [node webArchive];
            else
                archive = [WebArchiver archiveMainResourceForFrame:[self _frame]];
            
            source = [pasteboard _web_declareAndWriteDragImageForElement:(DOMElement *)node
                                                                     URL:linkURL ? linkURL : imageURL
                                                                   title:[element objectForKey:WebElementImageAltStringKey]
                                                                 archive:archive
                                                                  source:self];
        }
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionImage fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil)
            [self _web_DragImageForElement:(DOMElement *)node
                                      rect:[self convertRect:[[element objectForKey:WebElementImageRectKey] rectValue] fromView:innerHTMLView]
                                     event:_private->mouseDownEvent
                                pasteboard:pasteboard
                                    source:source
                                    offset:&_private->dragOffset];
        else
            [self dragImage:dragImage
                         at:dragLoc
                     offset:NSZeroSize
                      event:_private->mouseDownEvent
                 pasteboard:pasteboard
                     source:source
                  slideBack:YES];
    } else if (linkURL && (_private->dragSourceActionMask & WebDragSourceActionLink)) {
        if (!dhtmlWroteData) {
            NSArray *types = [NSPasteboard _web_writableTypesForURL];
            [pasteboard declareTypes:types owner:self];
            [pasteboard _web_writeURL:linkURL andTitle:[element objectForKey:WebElementLinkLabelKey] types:types];            
        }
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionLink fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            dragImage = [self _dragImageForLinkElement:element];
            NSSize offset = NSMakeSize([dragImage size].width / 2, -DRAG_LABEL_BORDER_Y);
            dragLoc = NSMakePoint(mouseDraggedPoint.x - offset.width, mouseDraggedPoint.y - offset.height);
            _private->dragOffset.x = offset.width;
            _private->dragOffset.y = -offset.height;        // inverted because we are flipped
        }
        // HACK:  We should pass the mouseDown event instead of the mouseDragged!  This hack gets rid of
        // a flash of the image at the mouseDown location when the drag starts.
        [self dragImage:dragImage
                     at:dragLoc
                 offset:NSZeroSize
                  event:mouseDraggedEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else if (isSelected && (_private->dragSourceActionMask & WebDragSourceActionSelection)) {
        if (!dhtmlWroteData)
            [innerHTMLView _writeSelectionToPasteboard:pasteboard];
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionSelection fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            dragImage = [innerHTMLView _selectionDraggingImage];
            NSRect draggingRect = [self convertRect:[innerHTMLView _selectionDraggingRect] fromView:innerHTMLView];
            dragLoc = NSMakePoint(NSMinX(draggingRect), NSMaxY(draggingRect));
            _private->dragOffset.x = mouseDownPoint.x - dragLoc.x;
            _private->dragOffset.y = dragLoc.y - mouseDownPoint.y;        // inverted because we are flipped
        }
        [self dragImage:dragImage
                     at:dragLoc
                 offset:NSZeroSize
                  event:_private->mouseDownEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else if (srcIsDHTML) {
        ASSERT(_private->dragSourceActionMask & WebDragSourceActionDHTML);
        [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionDHTML fromPoint:mouseDownPoint withPasteboard:pasteboard];
        if (dragImage == nil) {
            // WebCore should have given us an image, but we'll make one up
            // FIXME: Oops! I removed this image from WebKit. Is this a dead code path?
            NSString *imagePath = [[NSBundle bundleForClass:[self class]] pathForResource:@"missing_image" ofType:@"tiff"];
            dragImage = [[[NSImage alloc] initWithContentsOfFile:imagePath] autorelease];
            NSSize imageSize = [dragImage size];
            dragLoc = NSMakePoint(mouseDownPoint.x - imageSize.width / 2, mouseDownPoint.y + imageSize.height / 2);
            _private->dragOffset.x = imageSize.width / 2;
            _private->dragOffset.y = imageSize.height / 2;
        }
        [self dragImage:dragImage
                     at:dragLoc
                 offset:NSZeroSize
                  event:_private->mouseDownEvent
             pasteboard:pasteboard
                 source:self
              slideBack:YES];
    } else {
        // Only way I know if to get here is if the original element clicked on in the mousedown is no longer
        // under the mousedown point, so linkURL, imageURL and isSelected are all false/nil.
        startedDrag = NO;
    }
    return startedDrag;
}

- (void)_handleAutoscrollForMouseDragged:(NSEvent *)event 
{ 
    [self autoscroll:event]; 
    [self _startAutoscrollTimer:event]; 
} 

- (BOOL)_mayStartDragAtEventLocation:(NSPoint)location
{
    WebHTMLView *topHTMLView = [self _topHTMLView];
    if (self != topHTMLView)
        return [topHTMLView _mayStartDragAtEventLocation:location];

    NSPoint mouseDownPoint = [self convertPoint:location fromView:nil];
    NSDictionary *mouseDownElement = [self elementAtPoint:mouseDownPoint allowShadowContent:YES];

    ASSERT([self _webView]);

    if ([mouseDownElement objectForKey:WebElementImageKey]
            && [mouseDownElement objectForKey:WebElementImageURLKey]
            && [[[self _webView] preferences] loadsImagesAutomatically]
            && (_private->dragSourceActionMask & WebDragSourceActionImage))
        return YES;
    
    if ([mouseDownElement objectForKey:WebElementLinkURLKey]
            && (_private->dragSourceActionMask & WebDragSourceActionLink)
            && [[mouseDownElement objectForKey:WebElementLinkIsLiveKey] boolValue])
        return YES;
    
    if ([[mouseDownElement objectForKey:WebElementIsSelectedKey] boolValue]
            && (_private->dragSourceActionMask & WebDragSourceActionSelection))
        return YES;
    
    return NO;
}

- (WebPluginController *)_pluginController
{
    return _private->pluginController;
}

- (void)_web_setPrintingModeRecursive
{
    [self _setPrinting:YES minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];
    [super _web_setPrintingModeRecursive];
}

- (void)_web_clearPrintingModeRecursive
{
    [self _setPrinting:NO minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];
    [super _web_clearPrintingModeRecursive];
}

- (void)_layoutIfNeeded
{
    ASSERT(!_private->subviewsSetAside);

    if ([[self _bridge] needsLayout])
        _private->needsLayout = YES;
    if (_private->needsToApplyStyles || _private->needsLayout)
        [self layout];
}

- (void)_web_layoutIfNeededRecursive
{
    [self _layoutIfNeeded];
    [super _web_layoutIfNeededRecursive];
}

- (void)_web_layoutIfNeededRecursive:(NSRect)displayRect testDirtyRect:(bool)testDirtyRect
{
    ASSERT(!_private->subviewsSetAside);

    displayRect = NSIntersectionRect(displayRect, [self bounds]);
    if (testDirtyRect) {
        NSRect dirtyRect = [self _dirtyRect];
        displayRect = NSIntersectionRect(displayRect, dirtyRect);
    }
    if (!NSIsEmptyRect(displayRect))
        [self _layoutIfNeeded];

    [super _web_layoutIfNeededRecursive:displayRect testDirtyRect:NO];
}

- (void)_startAutoscrollTimer: (NSEvent *)triggerEvent
{
    if (_private->autoscrollTimer == nil) {
        _private->autoscrollTimer = [[NSTimer scheduledTimerWithTimeInterval:AUTOSCROLL_INTERVAL
            target:self selector:@selector(_autoscroll) userInfo:nil repeats:YES] retain];
        _private->autoscrollTriggerEvent = [triggerEvent retain];
    }
}

// FIXME: _selectionRect is deprecated in favor of selectionRect, which is in protocol WebDocumentSelection.
// We can't remove this yet because it's still in use by Mail.
- (NSRect)_selectionRect
{
    return [self selectionRect];
}

- (void)_stopAutoscrollTimer
{
    NSTimer *timer = _private->autoscrollTimer;
    _private->autoscrollTimer = nil;
    [_private->autoscrollTriggerEvent release];
    _private->autoscrollTriggerEvent = nil;
    [timer invalidate];
    [timer release];
}

- (void)_autoscroll
{
    // Guarantee that the autoscroll timer is invalidated, even if we don't receive
    // a mouse up event.
    BOOL isStillDown = CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft);   
    if (!isStillDown){
        [self _stopAutoscrollTimer];
        return;
    }

    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
        location:[[self window] convertScreenToBase:[NSEvent mouseLocation]]
        modifierFlags:[[NSApp currentEvent] modifierFlags]
        timestamp:[NSDate timeIntervalSinceReferenceDate]
        windowNumber:[[self window] windowNumber]
        context:[[NSApp currentEvent] context]
        eventNumber:0 clickCount:0 pressure:0];
    [self mouseDragged:fakeEvent];
}

- (BOOL)_canEdit
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return false;
    return coreFrame->editor()->canEdit();
}

- (BOOL)_canEditRichly
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NO;
    return coreFrame->editor()->canEditRichly();
}

- (BOOL)_canAlterCurrentSelection
{
    return [self _hasSelectionOrInsertionPoint] && [self _isEditable];
}

- (BOOL)_hasSelection
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NO;
    return coreFrame->selectionController()->isRange();
}

- (BOOL)_hasSelectionOrInsertionPoint
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NO;
    return coreFrame->selectionController()->isCaretOrRange();
}

- (BOOL)_hasInsertionPoint
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NO;
    return coreFrame->selectionController()->isCaret();
}

- (BOOL)_isEditable
{
    FrameMac* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NO;
    return coreFrame->selectionController()->isContentEditable();
}

- (BOOL)_isSelectionInPasswordField
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NO;
    return coreFrame->selectionController()->isInPasswordField();
}

- (BOOL)_isSelectionUngrammatical
{
    if (Frame* coreFrame = core([self _frame]))
        return coreFrame->editor()->isSelectionUngrammatical();
    return NO;
}

- (BOOL)_isSelectionMisspelled
{
    if (Frame* coreFrame = core([self _frame]))
        return coreFrame->editor()->isSelectionMisspelled();
    return NO;
}

- (NSArray *)_guessesForMisspelledSelection
{
    ASSERT([[self selectedString] length] != 0);
    return [[NSSpellChecker sharedSpellChecker] guessesForWord:[self selectedString]];
}

- (void)_changeSpellingFromMenu:(id)sender
{
    ASSERT([[self selectedString] length] != 0);
    if ([self _shouldReplaceSelectionWithText:[sender title] givenAction:WebViewInsertActionPasted]) {
        [[self _bridge] replaceSelectionWithText:[sender title] selectReplacement:YES smartReplace:NO];
    }
}

- (void)_ignoreSpellingFromMenu:(id)sender
{
    ASSERT([[self selectedString] length] != 0);
    [[NSSpellChecker sharedSpellChecker] ignoreWord:[self selectedString] inSpellDocumentWithTag:[[self _webView] spellCheckerDocumentTag]];
}

- (void)_ignoreGrammarFromMenu:(id)sender
{
    // NSSpellChecker uses the same API for ignoring grammar as for ignoring spelling
    [self _ignoreSpellingFromMenu:(id)sender];
}

- (void)_learnSpellingFromMenu:(id)sender
{
    ASSERT([[self selectedString] length] != 0);
    [[NSSpellChecker sharedSpellChecker] learnWord:[self selectedString]];
}

- (BOOL)_transparentBackground
{
    return _private->transparentBackground;
}

- (void)_setTransparentBackground:(BOOL)f
{
    _private->transparentBackground = f;
}

- (NSImage *)_selectionDraggingImage
{
    if ([self _hasSelection]) {
        NSImage *dragImage = core([self _frame])->selectionImage();
        [dragImage _web_dissolveToFraction:WebDragImageAlpha];
        return dragImage;
    }
    return nil;
}

- (NSRect)_selectionDraggingRect
{
    // Mail currently calls this method. We can eliminate it when Mail no longer calls it.
    return [self selectionImageRect];
}

- (BOOL)_canIncreaseSelectionListLevel
{
    return [self _canEditRichly] && [[self _bridge] canIncreaseSelectionListLevel];
}

- (BOOL)_canDecreaseSelectionListLevel
{
    return [self _canEditRichly] && [[self _bridge] canDecreaseSelectionListLevel];
}

- (DOMNode *)_increaseSelectionListLevel
{
    if (![self _canEditRichly])
        return nil;
        
    WebFrameBridge *bridge = [self _bridge];
    return [bridge increaseSelectionListLevel];
}

- (DOMNode *)_increaseSelectionListLevelOrdered
{
    if (![self _canEditRichly])
        return nil;
        
    WebFrameBridge *bridge = [self _bridge];
    return [bridge increaseSelectionListLevelOrdered];
}

- (DOMNode *)_increaseSelectionListLevelUnordered
{
    if (![self _canEditRichly])
        return nil;
        
    WebFrameBridge *bridge = [self _bridge];
    return [bridge increaseSelectionListLevelUnordered];
}

- (void)_decreaseSelectionListLevel
{
    if (![self _canEditRichly])
        return;
        
    WebFrameBridge *bridge = [self _bridge];
    [bridge decreaseSelectionListLevel];
}

- (void)_setHighlighter:(id<WebHTMLHighlighter>)highlighter ofType:(NSString*)type
{
    if (!_private->highlighters)
        _private->highlighters = [[NSMutableDictionary alloc] init];
    [_private->highlighters setObject:highlighter forKey:type];
}

- (void)_removeHighlighterOfType:(NSString*)type
{
    [_private->highlighters removeObjectForKey:type];
}

- (BOOL)_web_firstResponderCausesFocusDisplay
{
    return [self _web_firstResponderIsSelfOrDescendantView] || [[self window] firstResponder] == [self _frameView];
}

- (void)_updateActiveState
{
    // This method does the job of updating the view based on the view's firstResponder-ness and
    // the window key-ness of the window containing this view. This involves four kinds of 
    // drawing updates right now. 
    // 
    // The four display attributes are as follows:
    // 
    // 1. The background color used to draw behind selected content (active | inactive color)
    // 2. Caret blinking (blinks | does not blink)
    // 3. The drawing of a focus ring around links in web pages.
    // 4. Changing the tint of controls from clear to aqua/graphite and vice versa
    //
    // Also, this is responsible for letting the bridge know if the window has gained or lost focus
    // so we can send focus and blur events.

    NSWindow *window = [self window];
    BOOL windowIsKey = [window isKeyWindow];
    BOOL windowOrSheetIsKey = windowIsKey || [[window attachedSheet] isKeyWindow];

    BOOL isActive = !_private->resigningFirstResponder && windowIsKey && (_private->descendantBecomingFirstResponder || [self _web_firstResponderCausesFocusDisplay]);

    if (Frame* coreFrame = core([self _frame])) {
        coreFrame->setWindowHasFocus(windowOrSheetIsKey);
        coreFrame->setIsActive(isActive);
    }
}

- (unsigned)markAllMatchesForText:(NSString *)string caseSensitive:(BOOL)caseFlag limit:(unsigned)limit
{
    return [[self _bridge] markAllMatchesForText:string caseSensitive:caseFlag limit:limit];
}

- (void)setMarkedTextMatchesAreHighlighted:(BOOL)newValue
{
    [[self _bridge] setMarkedTextMatchesAreHighlighted:newValue];
}

- (BOOL)markedTextMatchesAreHighlighted
{
    return [[self _bridge] markedTextMatchesAreHighlighted];
}

- (void)unmarkAllTextMatches
{
    return [[self _bridge] unmarkAllTextMatches];
}

- (NSArray *)rectsForTextMatches
{
    return [[self _bridge] rectsForTextMatches];
}

- (void)_writeSelectionToPasteboard:(NSPasteboard *)pasteboard
{
    ASSERT([self _hasSelection]);
    NSArray *types = [self pasteboardTypesForSelection];

    // Don't write RTFD to the pasteboard when the copied attributed string has no attachments.
    NSAttributedString *attributedString = [self selectedAttributedString];
    NSMutableArray *mutableTypes = nil;
    if (![attributedString containsAttachments]) {
        mutableTypes = [types mutableCopy];
        [mutableTypes removeObject:NSRTFDPboardType];
        types = mutableTypes;
    }

    [pasteboard declareTypes:types owner:nil];
    [self _writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard cachedAttributedString:attributedString];
    [mutableTypes release];
}

- (void)close
{
    if (_private->closed)
        return;
    [self _clearLastHitViewIfSelf];
    // FIXME: This is slow; should remove individual observers instead.
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_private->pluginController destroyAllPlugins];
    [_private->pluginController setDataSource:nil];
    // remove tooltips before clearing _private so removeTrackingRect: will work correctly
    [self removeAllToolTips];
    [_private clear];
    _private->closed = YES;
}

@end

@implementation NSView (WebHTMLViewFileInternal)

- (void)_web_setPrintingModeRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_setPrintingModeRecursive)];
}

- (void)_web_clearPrintingModeRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_clearPrintingModeRecursive)];
}

- (void)_web_layoutIfNeededRecursive
{
    [_subviews makeObjectsPerformSelector:@selector(_web_layoutIfNeededRecursive)];
}

- (void)_web_layoutIfNeededRecursive: (NSRect)rect testDirtyRect:(bool)testDirtyRect
{
    unsigned index, count;
    for (index = 0, count = [(NSArray *)_subviews count]; index < count; index++) {
        NSView *subview = [_subviews objectAtIndex:index];
        NSRect dirtiedSubviewRect = [subview convertRect: rect fromView: self];
        [subview _web_layoutIfNeededRecursive: dirtiedSubviewRect testDirtyRect:testDirtyRect];
    }
}

@end

@implementation NSMutableDictionary (WebHTMLViewFileInternal)

- (void)_web_setObjectIfNotNil:(id)object forKey:(id)key
{
    if (object == nil) {
        [self removeObjectForKey:key];
    } else {
        [self setObject:object forKey:key];
    }
}

@end

// The following is a workaround for
// <rdar://problem/3429631> window stops getting mouse moved events after first tooltip appears
// The trick is to define a category on NSToolTipPanel that implements setAcceptsMouseMovedEvents:.
// Since the category will be searched before the real class, we'll prevent the flag from being
// set on the tool tip panel.

@interface NSToolTipPanel : NSPanel
@end

@interface NSToolTipPanel (WebHTMLViewFileInternal)
@end

@implementation NSToolTipPanel (WebHTMLViewFileInternal)

- (void)setAcceptsMouseMovedEvents:(BOOL)flag
{
    // Do nothing, preventing the tool tip panel from trying to accept mouse-moved events.
}

@end

@interface NSArray (WebHTMLView)
- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object;
@end

@implementation WebHTMLView

+ (void)initialize
{
    [NSApp registerServicesMenuSendTypes:[[self class] _selectionPasteboardTypes] 
                             returnTypes:[[self class] _insertablePasteboardTypes]];
    _NSInitializeKillRing();
}

- (void)_resetCachedWebPreferences:(NSNotification *)ignored
{
    WebPreferences *preferences = [[self _webView] preferences];
    // Check for nil because we might not yet have an associated webView when this is called
    if (preferences == nil) {
        preferences = [WebPreferences standardPreferences];
    }
    _private->showsURLsInToolTips = [preferences showsURLsInToolTips];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
    
    // Make all drawing go through us instead of subviews.
    if (NSAppKitVersionNumber >= 711) {
        [self _setDrawsOwnDescendants:YES];
    }
    
    _private = [[WebHTMLViewPrivate alloc] init];

    _private->pluginController = [[WebPluginController alloc] initWithDocumentView:self];
    _private->needsLayout = YES;
    [self _resetCachedWebPreferences:nil];
    [[NSNotificationCenter defaultCenter] 
            addObserver:self selector:@selector(_resetCachedWebPreferences:) 
                   name:WebPreferencesChangedNotification object:nil];
    
    return self;
}

- (void)dealloc
{
    // We can't assert that close has already been called because
    // this view can be removed from it's superview, even though
    // it could be needed later, so close if needed.
    [self close];
    [_private release];
    _private = nil;
    [super dealloc];
}

- (void)finalize
{
    // We can't assert that close has already been called because
    // this view can be removed from it's superview, even though
    // it could be needed later, so close if needed.
    [self close];
    [super finalize];
}

- (IBAction)takeFindStringFromSelection:(id)sender
{
    if (![self _hasSelection]) {
        NSBeep();
        return;
    }

    [NSPasteboard _web_setFindPasteboardString:[self selectedString] withOwner:self];
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    [pasteboard declareTypes:types owner:nil];
    [self writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard];
    return YES;
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard *)pasteboard
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return NO;
    if (coreFrame->selectionController()->isContentRichlyEditable())
        [self _pasteWithPasteboard:pasteboard allowPlainText:YES];
    else
        [self _pasteAsPlainTextWithPasteboard:pasteboard];
    return YES;
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    if (sendType != nil && [[self pasteboardTypesForSelection] containsObject:sendType] && [self _hasSelection]) {
        return self;
    } else if (returnType != nil && [[[self class] _insertablePasteboardTypes] containsObject:returnType] && [self _isEditable]) {
        return self;
    }
    return [[self nextResponder] validRequestorForSendType:sendType returnType:returnType];
}

- (void)selectAll:(id)sender
{
    [self selectAll];
}

// jumpToSelection is the old name for what AppKit now calls centerSelectionInVisibleArea. Safari
// was using the old jumpToSelection selector in its menu. Newer versions of Safari will us the
// selector centerSelectionInVisibleArea. We'll leave this old selector in place for two reasons:
// (1) compatibility between older Safari and newer WebKit; (2) other WebKit-based applications
// might be using the jumpToSelection: selector, and we don't want to break them.
- (void)jumpToSelection:(id)sender
{
    [self centerSelectionInVisibleArea:sender];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item 
{
    SEL action = [item action];
    FrameMac* frame = core([self _frame]);

    if (action == @selector(changeSpelling:)
            || action == @selector(_changeSpellingFromMenu:)
            || action == @selector(checkSpelling:)
            || action == @selector(complete:)
            || action == @selector(deleteBackward:)
            || action == @selector(deleteBackwardByDecomposingPreviousCharacter:)
            || action == @selector(deleteForward:)
            || action == @selector(deleteToBeginningOfLine:)
            || action == @selector(deleteToBeginningOfParagraph:)
            || action == @selector(deleteToEndOfLine:)
            || action == @selector(deleteToEndOfParagraph:)
            || action == @selector(deleteToMark:)
            || action == @selector(deleteWordBackward:)
            || action == @selector(deleteWordForward:)
            || action == @selector(insertBacktab:)
            || action == @selector(insertLineBreak:)
            || action == @selector(insertNewline:)
            || action == @selector(insertNewlineIgnoringFieldEditor:)
            || action == @selector(insertParagraphSeparator:)
            || action == @selector(insertTab:)
            || action == @selector(insertTabIgnoringFieldEditor:)
            || action == @selector(moveBackward:)
            || action == @selector(moveBackwardAndModifySelection:)
            || action == @selector(moveDown:)
            || action == @selector(moveDownAndModifySelection:)
            || action == @selector(moveForward:)
            || action == @selector(moveForwardAndModifySelection:)
            || action == @selector(moveLeft:)
            || action == @selector(moveLeftAndModifySelection:)
            || action == @selector(moveParagraphBackwardAndModifySelection:)
            || action == @selector(moveParagraphForwardAndModifySelection:)
            || action == @selector(moveRight:)
            || action == @selector(moveRightAndModifySelection:)
            || action == @selector(moveToBeginningOfDocument:)
            || action == @selector(moveToBeginningOfDocumentAndModifySelection:)
            || action == @selector(moveToBeginningOfSentence:)
            || action == @selector(moveToBeginningOfSentenceAndModifySelection:)
            || action == @selector(moveToBeginningOfLine:)
            || action == @selector(moveToBeginningOfLineAndModifySelection:)
            || action == @selector(moveToBeginningOfParagraph:)
            || action == @selector(moveToBeginningOfParagraphAndModifySelection:)
            || action == @selector(moveToEndOfDocument:)
            || action == @selector(moveToEndOfDocumentAndModifySelection:)
            || action == @selector(moveToEndOfSentence:)
            || action == @selector(moveToEndOfSentenceAndModifySelection:)
            || action == @selector(moveToEndOfLine:)
            || action == @selector(moveToEndOfLineAndModifySelection:)
            || action == @selector(moveToEndOfParagraph:)
            || action == @selector(moveToEndOfParagraphAndModifySelection:)
            || action == @selector(moveUp:)
            || action == @selector(moveUpAndModifySelection:)
            || action == @selector(moveWordBackward:)
            || action == @selector(moveWordBackwardAndModifySelection:)
            || action == @selector(moveWordForward:)
            || action == @selector(moveWordForwardAndModifySelection:)
            || action == @selector(moveWordLeft:)
            || action == @selector(moveWordLeftAndModifySelection:)
            || action == @selector(moveWordRight:)
            || action == @selector(moveWordRightAndModifySelection:)
            || action == @selector(pageDown:)
            || action == @selector(pageDownAndModifySelection:)
            || action == @selector(pageUp:)
            || action == @selector(pageUpAndModifySelection:)
            || action == @selector(pasteFont:)
            || action == @selector(transpose:)
            || action == @selector(yank:)
            || action == @selector(yankAndSelect:))
        return [self _canEdit];
    
    if (action == @selector(showGuessPanel:)) {
#if !BUILDING_ON_TIGER
        // Match OS X AppKit behavior for post-Tiger. Don't change Tiger behavior.
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            BOOL panelShowing = [[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible];
            [menuItem setTitle:panelShowing ? UI_STRING("Hide Spelling and Grammar", "menu item title") : UI_STRING("Show Spelling and Grammar", "menu item title")];
        }
#endif
        return [self _canEdit];
    }
    
    if (action == @selector(changeBaseWritingDirection:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            NSWritingDirection writingDirection = static_cast<NSWritingDirection>([item tag]);
            if (writingDirection == NSWritingDirectionNatural) {
                [menuItem setState:NO];
                return NO;
            }
            DOMCSSStyleDeclaration* style = [self _emptyStyle];
            [style setDirection:writingDirection == NSWritingDirectionLeftToRight ? @"LTR" : @"RTL"];
            [menuItem setState:[[self _bridge] selectionHasStyle:style]];
        }
        return [self _canEdit];
    }
    
    if (action == @selector(toggleBaseWritingDirection:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            DOMCSSStyleDeclaration* rtl = [self _emptyStyle];
            [rtl setDirection:@"RTL"];
            // Take control of the title of the menu item, instead of just checking/unchecking it because otherwise
            // we don't know what the check would mean.
            [menuItem setTitle:[[self _bridge] selectionHasStyle:rtl] ? UI_STRING("Left to Right", "Left to Right context menu item") : UI_STRING("Right to Left", "Right to Left context menu item")];
        }
        return [self _canEdit];
    } 
    
    if (action == @selector(alignCenter:)
            || action == @selector(alignLeft:)
            || action == @selector(alignJustified:)
            || action == @selector(alignRight:)
            || action == @selector(changeAttributes:)
            || action == @selector(changeColor:)        
            || action == @selector(changeFont:)
            || action == @selector(indent:)
            || action == @selector(outdent:))
        return [self _canEditRichly];
    
    if (action == @selector(capitalizeWord:)
               || action == @selector(lowercaseWord:)
               || action == @selector(uppercaseWord:))
        return [self _hasSelection] && [self _isEditable];
    
    if (action == @selector(centerSelectionInVisibleArea:)
               || action == @selector(jumpToSelection:)
               || action == @selector(copyFont:)
               || action == @selector(setMark:))
        return [self _hasSelection] || ([self _isEditable] && [self _hasInsertionPoint]);
    
    if (action == @selector(changeDocumentBackgroundColor:))
        return [[self _webView] isEditable] && [self _canEditRichly];
    
    if (action == @selector(copy:))
        return (frame && frame->editor()->canDHTMLCopy()) || frame->editor()->canCopy();
    
    if (action == @selector(cut:))
        return (frame && frame->editor()->canDHTMLCut()) || frame->editor()->canCut();
    
    if (action == @selector(delete:))
        return (frame && frame->editor()->canDelete());
    
    if (action == @selector(_ignoreSpellingFromMenu:)
            || action == @selector(_learnSpellingFromMenu:)
            || action == @selector(takeFindStringFromSelection:))
        return [self _hasSelection];
    
    if (action == @selector(paste:) || action == @selector(pasteAsPlainText:))
        return (frame && frame->editor()->canDHTMLPaste()) || frame->editor()->canPaste();
    
    if (action == @selector(pasteAsRichText:))
        return frame && (frame->editor()->canDHTMLPaste()
            || (frame->editor()->canPaste() && frame->selectionController()->isContentRichlyEditable()));
    
    if (action == @selector(performFindPanelAction:))
        // FIXME: Not yet implemented.
        return NO;
    
    if (action == @selector(selectToMark:)
            || action == @selector(swapWithMark:))
        return [self _hasSelectionOrInsertionPoint] && [[self _bridge] markDOMRange] != nil;
    
    if (action == @selector(subscript:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            DOMCSSStyleDeclaration *style = [self _emptyStyle];
            [style setVerticalAlign:@"sub"];
            [menuItem setState:[[self _bridge] selectionHasStyle:style]];
        }
        return [self _canEditRichly];
    } 
    
    if (action == @selector(superscript:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            DOMCSSStyleDeclaration *style = [self _emptyStyle];
            [style setVerticalAlign:@"super"];
            [menuItem setState:[[self _bridge] selectionHasStyle:style]];
        }
        return [self _canEditRichly];
    } 
    
    if (action == @selector(underline:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            DOMCSSStyleDeclaration *style = [self _emptyStyle];
            [style setProperty:@"-khtml-text-decorations-in-effect" value:@"underline" priority:@""];
            [menuItem setState:[[self _bridge] selectionHasStyle:style]];
        }
        return [self _canEditRichly];
    } 
    
    if (action == @selector(unscript:)) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([menuItem isKindOfClass:[NSMenuItem class]]) {
            DOMCSSStyleDeclaration *style = [self _emptyStyle];
            [style setVerticalAlign:@"baseline"];
            [menuItem setState:[[self _bridge] selectionHasStyle:style]];
        }
        return [self _canEditRichly];
    } 
    
    if (action == @selector(_lookUpInDictionaryFromMenu:)) {
        return [self _hasSelection];
    } 
    
#if !BUILDING_ON_TIGER
    if (action == @selector(toggleGrammarChecking:)) {
        // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must validate 
        // the selector here because we implement it here, and we must implement it here because the AppKit 
        // code checks the first responder.
        BOOL checkMark = [self isGrammarCheckingEnabled];
        if ([(NSObject *)item isKindOfClass:[NSMenuItem class]]) {
            NSMenuItem *menuItem = (NSMenuItem *)item;
            [menuItem setState:checkMark ? NSOnState : NSOffState];
        }
        return YES;
    }
#endif
    
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    // Don't accept first responder when we first click on this view.
    // We have to pass the event down through WebCore first to be sure we don't hit a subview.
    // Do accept first responder at any other time, for example from keyboard events,
    // or from calls back from WebCore once we begin mouse-down event handling.
    NSEvent *event = [NSApp currentEvent];
    if ([event type] == NSLeftMouseDown
            && !_private->handlingMouseDownEvent
            && NSPointInRect([event locationInWindow], [self convertRect:[self visibleRect] toView:nil])) {
        return NO;
    }
    return YES;
}

- (BOOL)maintainsInactiveSelection
{
    // This method helps to determine whether the WebHTMLView should maintain
    // an inactive selection when it's not first responder.
    // Traditionally, these views have not maintained such selections,
    // clearing them when the view was not first responder. However,
    // to fix bugs like this one:
    // <rdar://problem/3672088>: "Editable WebViews should maintain a selection even 
    //                            when they're not firstResponder"
    // it was decided to add a switch to act more like an NSTextView.

    if ([[self _webView] maintainsInactiveSelection])
        return YES;

    // Predict the case where we are losing first responder status only to
    // gain it back again. Want to keep the selection in that case.
    id nextResponder = [[self window] _newFirstResponderAfterResigning];
    if ([nextResponder isKindOfClass:[NSScrollView class]]) {
        id contentView = [nextResponder contentView];
        if (contentView)
            nextResponder = contentView;
    }
    if ([nextResponder isKindOfClass:[NSClipView class]]) {
        id documentView = [nextResponder documentView];
        if (documentView)
            nextResponder = documentView;
    }
    if (nextResponder == self)
        return YES;

    FrameMac* coreFrame = core([self _frame]);
    bool selectionIsEditable = coreFrame && coreFrame->selectionController()->isContentEditable();
    bool nextResponderIsInWebView = [nextResponder isKindOfClass:[NSView class]]
        && [nextResponder isDescendantOf:[[[self _webView] mainFrame] frameView]];

    return selectionIsEditable && nextResponderIsInWebView;
}

- (void)addMouseMovedObserver
{
    if (!_private->dataSource || ![self _isTopHTMLView])
        return;

    // Unless the Dashboard asks us to do this for all windows, keep an observer going only for the key window.
    if (!([[self window] isKeyWindow] || [[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows]))
        return;

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(mouseMovedNotification:)
        name:WKMouseMovedNotification() object:nil];
    [self _frameOrBoundsChanged];
}

- (void)removeMouseMovedObserverUnconditionally
{
    [[NSNotificationCenter defaultCenter] removeObserver:self
        name:WKMouseMovedNotification() object:nil];
}

- (void)removeMouseMovedObserver
{
    // Don't remove the observer if we're running the Dashboard.
    if ([[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysSendMouseEventsToAllWindows])
        return;

    [[self _webView] _mouseDidMoveOverElement:nil modifierFlags:0];
    [self removeMouseMovedObserverUnconditionally];
}

- (void)addSuperviewObservers
{
    // We watch the bounds of our superview, so that we can do a layout when the size
    // of the superview changes. This is different from other scrollable things that don't
    // need this kind of thing because their layout doesn't change.
    
    // We need to pay attention to both height and width because our "layout" has to change
    // to extend the background the full height of the space and because some elements have
    // sizes that are based on the total size of the view.
    
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_frameOrBoundsChanged) 
            name:NSViewBoundsDidChangeNotification object:superview];

        // In addition to registering for frame/bounds change notifications, call -_frameOrBoundsChanged.
        // It will check the current size/scroll against the previous layout's size/scroll.  We need to
        // do this here to catch the case where the WebView is laid out at one size, removed from its
        // window, resized, and inserted into another window.  Our frame/bounds changed notifications
        // will not be sent in that situation, since we only watch for changes while in the view hierarchy.
        [self _frameOrBoundsChanged];
    }
}

- (void)removeSuperviewObservers
{
    NSView *superview = [self superview];
    if (superview && [self window]) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewFrameDidChangeNotification object:superview];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSViewBoundsDidChangeNotification object:superview];
    }
}

- (void)addWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidBecomeKey:)
            name:NSWindowDidBecomeKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidResignKey:)
            name:NSWindowDidResignKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowWillClose:)
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidBecomeKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidResignKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowWillCloseNotification object:window];
    }
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    [self removeSuperviewObservers];
}

- (void)viewDidMoveToSuperview
{
    // Do this here in case the text size multiplier changed when a non-HTML
    // view was installed.
    if ([self superview] != nil) {
        [self _updateTextSizeMultiplier];
        [self addSuperviewObservers];
    }
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (!_private)
        return;

    // FIXME: Some of these calls may not work because this view may be already removed from it's superview.
    [self removeMouseMovedObserverUnconditionally];
    [self removeWindowObservers];
    [self removeSuperviewObservers];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];

    [[self _pluginController] stopAllPlugins];
}

- (void)viewDidMoveToWindow
{
    // Don't do anything if we aren't initialized.  This happens
    // when decoding a WebView.  When WebViews are decoded their subviews
    // are created by initWithCoder: and so won't be normally
    // initialized.  The stub views are discarded by WebView.
    if (!_private)
        return;
        
    [self _stopAutoscrollTimer];
    if ([self window]) {
        _private->lastScrollPosition = [[self superview] bounds].origin;
        [self addWindowObservers];
        [self addSuperviewObservers];
        [self addMouseMovedObserver];

        // Schedule this update, rather than making the call right now.
        // The reason is that placing the caret in the just-installed view requires
        // the HTML/XML document to be available on the WebCore side, but it is not
        // at the time this code is running. However, it will be there on the next
        // crank of the run loop. Doing this helps to make a blinking caret appear 
        // in a new, empty window "automatic".
        [self performSelector:@selector(_updateActiveState) withObject:nil afterDelay:0];

        [[self _pluginController] startAllPlugins];

        _private->lastScrollPosition = NSZeroPoint;
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewWillMoveToHostWindow:) withObject:hostWindow];
}

- (void)viewDidMoveToHostWindow
{
    [[self subviews] _web_makePluginViewsPerformSelector:@selector(viewDidMoveToHostWindow) withObject:nil];
}


- (void)addSubview:(NSView *)view
{
    [super addSubview:view];

    if ([WebPluginController isPlugInView:view]) {
        [[self _pluginController] addPlugin:view];
    }
}

- (void)willRemoveSubview:(NSView *)subview
{
    if ([WebPluginController isPlugInView:subview])
        [[self _pluginController] destroyPlugin:subview];
    [super willRemoveSubview:subview];
}

- (void)reapplyStyles
{
    if (!_private->needsToApplyStyles) {
        return;
    }
    
#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    [[self _bridge] reapplyStylesForDeviceType:
        _private->printing ? WebCoreDevicePrinter : WebCoreDeviceScreen];
    
#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s apply style seconds = %f", [self URL], thisTime);
#endif

    _private->needsToApplyStyles = NO;
}

// Do a layout, but set up a new fixed width for the purposes of doing printing layout.
// minPageWidth==0 implies a non-printing layout
- (void)layoutToMinimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustingViewSize:(BOOL)adjustViewSize
{
    [self reapplyStyles];
    
    if (!_private->needsLayout) {
        return;
    }

#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif

    LOG(View, "%@ doing layout", self);

    if (minPageWidth > 0.0) {
        [[self _bridge] forceLayoutWithMinimumPageWidth:minPageWidth maximumPageWidth:maxPageWidth adjustingViewSize:adjustViewSize];
    } else {
        [[self _bridge] forceLayoutAdjustingViewSize:adjustViewSize];
    }
    _private->needsLayout = NO;
    
    if (!_private->printing) {
        // get size of the containing dynamic scrollview, so
        // appearance and disappearance of scrollbars will not show up
        // as a size change
        NSSize newLayoutFrameSize = [[[self superview] superview] frame].size;
        if (_private->laidOutAtLeastOnce && !NSEqualSizes(_private->lastLayoutFrameSize, newLayoutFrameSize)) {
            [[self _bridge] sendResizeEvent];
            if ([[self _bridge] needsLayout])
                [[self _bridge] forceLayoutAdjustingViewSize:NO];
        }
        _private->laidOutAtLeastOnce = YES;
        _private->lastLayoutSize = [(NSClipView *)[self superview] documentVisibleRect].size;
        _private->lastLayoutFrameSize = newLayoutFrameSize;
    }

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s layout seconds = %f", [self URL], thisTime);
#endif
}

- (void)layout
{
    [self layoutToMinimumPageWidth:0.0f maximumPageWidth:0.0f adjustingViewSize:NO];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    [_private->compController endRevertingChange:NO moveLeft:NO];

    _private->handlingMouseDownEvent = YES;
    BOOL handledEvent = NO;
    Frame* coreframe = core([self _frame]);
    if (coreframe)
        handledEvent = coreframe->eventHandler()->sendContextMenuEvent(event);
    _private->handlingMouseDownEvent = NO;
    
    if (handledEvent && coreframe) {
#ifdef WEBCORE_CONTEXT_MENUS
        if (Page* page = coreframe->page()) {
            NSArray* menuItems = page->contextMenuController()->contextMenu()->platformDescription();
            NSMenu* menu = nil;
            if (menuItems && [menuItems count] > 0) {
                menu = [[NSMenu alloc] init];
                
                unsigned i;
                for (i = 0; i < [menuItems count]; i++)
                    [menu addItem:[menuItems objectAtIndex:i]];
            }
            return [menu autorelease];
        }
#else
        return nil;
#endif
    }

    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSDictionary *element = [self elementAtPoint:point];
    return [[self _webView] _menuForElement:element defaultItems:nil];
}

- (BOOL)searchFor:(NSString *)string direction:(BOOL)forward caseSensitive:(BOOL)caseFlag wrap:(BOOL)wrapFlag
{
    if (![string length])
        return NO;

    return [[self _bridge] searchFor:string direction:forward caseSensitive:caseFlag wrap:wrapFlag];
}

- (void)clearFocus
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    Document* document = coreFrame->document();
    if (!document)
        return;
    
    document->setFocusedNode(0);
}

- (BOOL)isOpaque
{
    return [[self _webView] drawsBackground];
}

- (void)setNeedsDisplay:(BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    [super setNeedsDisplay: flag];
}

- (void)setNeedsLayout: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsLayout = flag;
}


- (void)setNeedsToApplyStyles: (BOOL)flag
{
    LOG(View, "%@ flag = %d", self, (int)flag);
    _private->needsToApplyStyles = flag;
}

- (void)drawSingleRect:(NSRect)rect
{
    [NSGraphicsContext saveGraphicsState];
    NSRectClip(rect);
        
    ASSERT([[self superview] isKindOfClass:[WebClipView class]]);

    [(WebClipView *)[self superview] setAdditionalClip:rect];

    NS_DURING {
        if ([self _transparentBackground]) {
            [[NSColor clearColor] set];
            NSRectFill (rect);
        }
        
        [[self _bridge] drawRect:rect];        
        WebView *webView = [self _webView];
        [[webView _UIDelegateForwarder] webView:webView didDrawRect:[webView convertRect:rect fromView:self]];
        [(WebClipView *)[self superview] resetAdditionalClip];

        [NSGraphicsContext restoreGraphicsState];
    } NS_HANDLER {
        [(WebClipView *)[self superview] resetAdditionalClip];
        [NSGraphicsContext restoreGraphicsState];
        LOG_ERROR("Exception caught while drawing: %@", localException);
        [localException raise];
    } NS_ENDHANDLER
}

- (void)drawRect:(NSRect)rect
{
    LOG(View, "%@ drawing", self);

    const NSRect *rects;
    WebNSInteger count;
    [self getRectsBeingDrawn:&rects count:&count];

    BOOL subviewsWereSetAside = _private->subviewsSetAside;
    if (subviewsWereSetAside)
        [self _restoreSubviews];

#ifdef _KWQ_TIMING
    double start = CFAbsoluteTimeGetCurrent();
#endif

    // If count == 0 here, use the rect passed in for drawing. This is a workaround for:
    // <rdar://problem/3908282> REGRESSION (Mail): No drag image dragging selected text in Blot and Mail
    // The reason for the workaround is that this method is called explicitly from the code
    // to generate a drag image, and at that time, getRectsBeingDrawn:count: will return a zero count.
    const int cRectThreshold = 10;
    const float cWastedSpaceThreshold = 0.75f;
    BOOL useUnionedRect = (count <= 1) || (count > cRectThreshold);
    if (!useUnionedRect) {
        // Attempt to guess whether or not we should use the unioned rect or the individual rects.
        // We do this by computing the percentage of "wasted space" in the union.  If that wasted space
        // is too large, then we will do individual rect painting instead.
        float unionPixels = (rect.size.width * rect.size.height);
        float singlePixels = 0;
        for (int i = 0; i < count; ++i)
            singlePixels += rects[i].size.width * rects[i].size.height;
        float wastedSpace = 1 - (singlePixels / unionPixels);
        if (wastedSpace <= cWastedSpaceThreshold)
            useUnionedRect = YES;
    }
    
    if (useUnionedRect)
        [self drawSingleRect:rect];
    else
        for (int i = 0; i < count; ++i)
            [self drawSingleRect:rects[i]];

#ifdef _KWQ_TIMING
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "%s draw seconds = %f", widget->part()->baseURL().URL().latin1(), thisTime);
#endif

    if (subviewsWereSetAside)
        [self _setAsideSubviews];
}

// Turn off the additional clip while computing our visibleRect.
- (NSRect)visibleRect
{
    if (!([[self superview] isKindOfClass:[WebClipView class]]))
        return [super visibleRect];
        
    WebClipView *clipView = (WebClipView *)[self superview];

    BOOL hasAdditionalClip = [clipView hasAdditionalClip];
    if (!hasAdditionalClip) {
        return [super visibleRect];
    }
    
    NSRect additionalClip = [clipView additionalClip];
    [clipView resetAdditionalClip];
    NSRect visibleRect = [super visibleRect];
    [clipView setAdditionalClip:additionalClip];
    return visibleRect;
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    NSWindow *keyWindow = [notification object];

    if (keyWindow == [self window])
        [self addMouseMovedObserver];

    if (keyWindow == [self window] || keyWindow == [[self window] attachedSheet])
        [self _updateActiveState];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    NSWindow *formerKeyWindow = [notification object];

    if (formerKeyWindow == [self window])
        [self removeMouseMovedObserver];

    if (formerKeyWindow == [self window] || formerKeyWindow == [[self window] attachedSheet]) {
        [self _updateActiveState];
        [_private->compController endRevertingChange:NO moveLeft:NO];
    }
}

- (void)windowWillClose:(NSNotification *)notification
{
    [_private->compController endRevertingChange:NO moveLeft:NO];
    [[self _pluginController] destroyAllPlugins];
}

- (void)scrollWheel:(NSEvent *)event
{
    [self retain];
    FrameMac* frame = core([self _frame]);
    if (!frame || !frame->eventHandler()->wheelEvent(event))
        [[self nextResponder] scrollWheel:event];    
    [self release];
}

- (BOOL)_isSelectionEvent:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    return [[[self elementAtPoint:point allowShadowContent:YES] objectForKey:WebElementIsSelectedKey] boolValue];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    NSView *hitView = [self _hitViewForEvent:event];
    WebHTMLView *hitHTMLView = [hitView isKindOfClass:[self class]] ? (WebHTMLView *)hitView : nil;
    
    if ([[self _webView] _dashboardBehavior:WebDashboardBehaviorAlwaysAcceptsFirstMouse])
        return YES;
    
    if (hitHTMLView) {
        bool result = false;
        if (FrameMac* coreFrame = core([hitHTMLView _frame])) {
            coreFrame->eventHandler()->setActivationEventNumber([event eventNumber]);
            [hitHTMLView _setMouseDownEvent:event];
            if ([hitHTMLView _isSelectionEvent:event])
                result = coreFrame->eventHandler()->eventMayStartDrag(event);
            [hitHTMLView _setMouseDownEvent:nil];
        }
        return result;
    }
    return [hitView acceptsFirstMouse:event];
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)event
{
    NSView *hitView = [self _hitViewForEvent:event];
    WebHTMLView *hitHTMLView = [hitView isKindOfClass:[self class]] ? (WebHTMLView *)hitView : nil;
    if (hitHTMLView) {
        bool result = false;
        if ([hitHTMLView _isSelectionEvent:event])
            if (FrameMac* coreFrame = core([hitHTMLView _frame])) {
                [hitHTMLView _setMouseDownEvent:event];
                result = coreFrame->eventHandler()->eventMayStartDrag(event);
                [hitHTMLView _setMouseDownEvent:nil];
            }
        return result;
    }
    return [hitView shouldDelayWindowOrderingForEvent:event];
}

- (void)mouseDown:(NSEvent *)event
{
    [self retain];

    _private->handlingMouseDownEvent = YES;

    // Record the mouse down position so we can determine drag hysteresis.
    [self _setMouseDownEvent:event];

    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        goto done;

    [_private->compController endRevertingChange:NO moveLeft:NO];

    // If the web page handles the context menu event and menuForEvent: returns nil, we'll get control click events here.
    // We don't want to pass them along to KHTML a second time.
    if (!([event modifierFlags] & NSControlKeyMask)) {
        _private->ignoringMouseDraggedEvents = NO;

        // Don't do any mouseover while the mouse is down.
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_updateMouseoverWithFakeEvent) object:nil];

        // Let WebCore get a chance to deal with the event. This will call back to us
        // to start the autoscroll timer if appropriate.
        if (Frame* coreframe = core([self _frame]))
            coreframe->eventHandler()->mouseDown(event);
    }

done:
    [_private->firstResponderTextViewAtMouseDownTime release];
    _private->firstResponderTextViewAtMouseDownTime = nil;

    _private->handlingMouseDownEvent = NO;
    
    [self release];
}

- (void)dragImage:(NSImage *)dragImage
               at:(NSPoint)at
           offset:(NSSize)offset
            event:(NSEvent *)event
       pasteboard:(NSPasteboard *)pasteboard
           source:(id)source
        slideBack:(BOOL)slideBack
{
    [self _stopAutoscrollTimer];

    WebHTMLView *topHTMLView = [self _topHTMLView];
    if (self != topHTMLView) {
        [topHTMLView dragImage:dragImage at:[self convertPoint:at toView:topHTMLView]
            offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
        return;
    }

    WebView *webView = [self _webView];
    
    [webView _setInitiatedDrag:YES];

    // Retain this view during the drag because it may be released before the drag ends.
    [self retain];

    id UIDelegate = [webView UIDelegate];
    // If a delegate takes over the drag but never calls draggedImage: endedAt:, we'll leak the WebHTMLView.
    if ([UIDelegate respondsToSelector:@selector(webView:dragImage:at:offset:event:pasteboard:source:slideBack:forView:)])
        [UIDelegate webView:webView dragImage:dragImage at:at offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack forView:self];
    else
        [super dragImage:dragImage at:at offset:offset event:event pasteboard:pasteboard source:source slideBack:slideBack];
}

- (void)mouseDragged:(NSEvent *)event
{
    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;

    [self retain];

    if (!_private->ignoringMouseDraggedEvents)
        if (Frame* coreframe = core([self _frame]))
            coreframe->eventHandler()->mouseDragged(event);

    [self release];
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
    ASSERT([self _isTopHTMLView]);

    if (_private->webCoreDragOp == NSDragOperationNone)
        return NSDragOperationGeneric | NSDragOperationCopy;
    return _private->webCoreDragOp;
}

- (void)draggedImage:(NSImage *)image movedTo:(NSPoint)screenLoc
{
    ASSERT([self _isTopHTMLView]);

    NSPoint windowImageLoc = [[self window] convertScreenToBase:screenLoc];
    NSPoint windowMouseLoc = NSMakePoint(windowImageLoc.x + _private->dragOffset.x, windowImageLoc.y + _private->dragOffset.y);
    [[self _bridge] dragSourceMovedTo:windowMouseLoc];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    ASSERT(![self _webView] || [self _isTopHTMLView]);

    NSPoint windowImageLoc = [[self window] convertScreenToBase:aPoint];
    NSPoint windowMouseLoc = NSMakePoint(windowImageLoc.x + _private->dragOffset.x, windowImageLoc.y + _private->dragOffset.y);
    [[self _bridge] dragSourceEndedAt:windowMouseLoc operation:operation];

    _private->initiatedDrag = NO;
    [[self _webView] _setInitiatedDrag:NO];
    
    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    _private->ignoringMouseDraggedEvents = YES;
    
    // Once the dragging machinery kicks in, we no longer get mouse drags or the up event.
    // WebCore expects to get balanced down/up's, so we must fake up a mouseup.
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                            location:windowMouseLoc
                                       modifierFlags:[[NSApp currentEvent] modifierFlags]
                                           timestamp:[NSDate timeIntervalSinceReferenceDate]
                                        windowNumber:[[self window] windowNumber]
                                             context:[[NSApp currentEvent] context]
                                         eventNumber:0 clickCount:0 pressure:0];
    [self mouseUp:fakeEvent]; // This will also update the mouseover state.
    
    // Balance the previous retain from when the drag started.
    [self release];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    ASSERT([self _isTopHTMLView]);
    ASSERT(_private->draggingImageURL);

    NSFileWrapper *wrapper = [[self _dataSource] _fileWrapperForURL:_private->draggingImageURL];
    if (wrapper == nil) {
        LOG_ERROR("Failed to create image file. Did the source image change while dragging? (<rdar://problem/4244861>)");
        return nil;
    }

    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[wrapper preferredFilename]];
    path = [[NSFileManager defaultManager] _webkit_pathWithUniqueFilenameForPath:path];
    if (![wrapper writeToFile:path atomically:NO updateFilenames:YES])
        LOG_ERROR("Failed to create image file via -[NSFileWrapper writeToFile:atomically:updateFilenames:]");

    return [NSArray arrayWithObject:[path lastPathComponent]];
}

- (void)mouseUp:(NSEvent *)event
{
    [self _setMouseDownEvent:nil];

    NSInputManager *currentInputManager = [NSInputManager currentInputManager];
    if ([currentInputManager wantsToHandleMouseEvents] && [currentInputManager handleMouseEvent:event])
        return;

    [self retain];

    [self _stopAutoscrollTimer];
    if (Frame* coreframe = core([self _frame]))
        coreframe->eventHandler()->mouseUp(event);
    [self _updateMouseoverWithFakeEvent];

    [self release];
}

- (void)mouseMovedNotification:(NSNotification *)notification
{
    [self _updateMouseoverWithEvent:[[notification userInfo] objectForKey:@"NSEvent"]];
}

// returning YES from this method is the way we tell AppKit that it is ok for this view
// to be in the key loop even when "tab to all controls" is not on.
- (BOOL)needsPanelToBecomeKey
{
    return YES;
}

- (NSView *)nextValidKeyView
{
    NSView *view = nil;
    BOOL lookInsideWebFrameViews = YES;
    if ([self isHiddenOrHasHiddenAncestor]) {
        lookInsideWebFrameViews = NO;
    } else if ([self _frame] == [[self _webView] mainFrame]) {
        // Check for case where first responder is last frame in a frameset, and we are
        // the top-level documentView.
        NSResponder *firstResponder = [[self window] firstResponder];
        if ((firstResponder != self) && [firstResponder isKindOfClass:[WebHTMLView class]] && ([(NSView *)firstResponder nextKeyView] == nil)) {
            lookInsideWebFrameViews = NO;
        }
    }
    
    if (lookInsideWebFrameViews) {
        view = [[self _bridge] nextKeyViewInsideWebFrameViews];
    }
    
    if (view == nil) {
        view = [super nextValidKeyView];
        // If there's no next view wired up, we must be in the last subframe, or we are
        // being called at an unusual time when the views have not yet been wired together.
        // There's no direct link to the next valid key view; get it from the bridge.
        // Note that view == self here when nextKeyView returns nil, due to AppKit oddness.
        // We'll check for both nil and self in case the AppKit oddness goes away.
        // WebFrameView has this same kind of logic for the previousValidKeyView case.
        if (view == nil || view == self) {
            view = [[self _bridge] nextValidKeyViewOutsideWebFrameViews];
        }
    }
        
    return view;
}

- (NSView *)previousValidKeyView
{
    NSView *view = nil;
    if (![self isHiddenOrHasHiddenAncestor])
        view = [[self _bridge] previousKeyViewInsideWebFrameViews];
    if (view == nil)
        view = [super previousValidKeyView];
    return view;
}

- (BOOL)becomeFirstResponder
{
    NSView *view = nil;
    if (![[self _webView] _isPerformingProgrammaticFocus] && !_private->willBecomeFirstResponderForNodeFocus) {
        switch ([[self window] keyViewSelectionDirection]) {
            case NSDirectSelection:
                break;
            case NSSelectingNext:
                view = [[self _bridge] nextKeyViewInsideWebFrameViews];
                break;
            case NSSelectingPrevious:
                view = [[self _bridge] previousKeyViewInsideWebFrameViews];
                break;
        }
    }
    _private->willBecomeFirstResponderForNodeFocus = NO;
    if (view)
        [[self window] makeFirstResponder:view];
    [self _updateActiveState];
    [self _updateFontPanel];
    _private->startNewKillRingSequence = YES;
    return YES;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        [_private->compController endRevertingChange:NO moveLeft:NO];
        _private->resigningFirstResponder = YES;
        if (![self maintainsInactiveSelection]) { 
            [self deselectAll];
            if (![[self _webView] _isPerformingProgrammaticFocus])
                [self clearFocus];
        }
        [self _updateActiveState];
        _private->resigningFirstResponder = NO;
        _private->willBecomeFirstResponderForNodeFocus = NO;
    }
    return resign;
}

- (void)setDataSource:(WebDataSource *)dataSource 
{
    ASSERT(dataSource);
    if (_private->dataSource == dataSource)
        return;
    ASSERT(!_private->dataSource);
    _private->closed = NO; // setting a data source reopens a closed view
    _private->dataSource = [dataSource retain];
    [_private->pluginController setDataSource:dataSource];
    [self addMouseMovedObserver];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

// This is an override of an NSControl method that wants to repaint the entire view when the window resigns/becomes
// key.  WebHTMLView is an NSControl only because it hosts NSCells that are painted by WebCore's Aqua theme
// renderer (and those cells must be hosted by an enclosing NSControl in order to paint properly).
- (void)updateCell:(NSCell*)cell
{
}

// Does setNeedsDisplay:NO as a side effect when printing is ending.
// pageWidth != 0 implies we will relayout to a new width
- (void)_setPrinting:(BOOL)printing minimumPageWidth:(float)minPageWidth maximumPageWidth:(float)maxPageWidth adjustViewSize:(BOOL)adjustViewSize
{
    WebFrame *frame = [self _frame];
    NSArray *subframes = [frame childFrames];
    unsigned n = [subframes count];
    unsigned i;
    for (i = 0; i != n; ++i) {
        WebFrame *subframe = [subframes objectAtIndex:i];
        WebFrameView *frameView = [subframe frameView];
        if ([[subframe dataSource] _isDocumentHTML]) {
            [(WebHTMLView *)[frameView documentView] _setPrinting:printing minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:adjustViewSize];
        }
    }

    if (printing != _private->printing) {
        [_private->pageRects release];
        _private->pageRects = nil;
        _private->printing = printing;
        [self setNeedsToApplyStyles:YES];
        [self setNeedsLayout:YES];
        [self layoutToMinimumPageWidth:minPageWidth maximumPageWidth:maxPageWidth adjustingViewSize:adjustViewSize];
        if (!printing) {
            // Can't do this when starting printing or nested printing won't work, see 3491427.
            [self setNeedsDisplay:NO];
        }
    }
}

- (BOOL)canPrintHeadersAndFooters
{
    return YES;
}

// This is needed for the case where the webview is embedded in the view that's being printed.
// It shouldn't be called when the webview is being printed directly.
- (void)adjustPageHeightNew:(float *)newBottom top:(float)oldTop bottom:(float)oldBottom limit:(float)bottomLimit
{
    // This helps when we print as part of a larger print process.
    // If the WebHTMLView itself is what we're printing, then we will never have to do this.
    BOOL wasInPrintingMode = _private->printing;
    if (!wasInPrintingMode)
        [self _setPrinting:YES minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];

    [[self _bridge] adjustPageHeightNew:newBottom top:oldTop bottom:oldBottom limit:bottomLimit];
    
    if (!wasInPrintingMode)
        [self _setPrinting:NO minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];
}

- (float)_availablePaperWidthForPrintOperation:(NSPrintOperation *)printOperation
{
    NSPrintInfo *printInfo = [printOperation printInfo];
    return [printInfo paperSize].width - [printInfo leftMargin] - [printInfo rightMargin];
}

- (float)_scaleFactorForPrintOperation:(NSPrintOperation *)printOperation
{
    float viewWidth = NSWidth([self bounds]);
    if (viewWidth < 1) {
        LOG_ERROR("%@ has no width when printing", self);
        return 1.0f;
    }

    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    float maxShrinkToFitScaleFactor = 1.0f / PrintingMaximumShrinkFactor;
    float shrinkToFitScaleFactor = [self _availablePaperWidthForPrintOperation:printOperation]/viewWidth;
    return userScaleFactor * MAX(maxShrinkToFitScaleFactor, shrinkToFitScaleFactor);
}

// FIXME 3491344: This is a secret AppKit-internal method that we need to override in order
// to get our shrink-to-fit to work with a custom pagination scheme. We can do this better
// if AppKit makes it SPI/API.
- (float)_provideTotalScaleFactorForPrintOperation:(NSPrintOperation *)printOperation 
{
    return [self _scaleFactorForPrintOperation:printOperation];
}

// This is used for Carbon printing. At some point we might want to make this public API.
- (void)setPageWidthForPrinting:(float)pageWidth
{
    [self _setPrinting:NO minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:NO];
    [self _setPrinting:YES minimumPageWidth:pageWidth maximumPageWidth:pageWidth adjustViewSize:YES];
}

- (void)_endPrintMode
{
    [self _setPrinting:NO minimumPageWidth:0.0f maximumPageWidth:0.0f adjustViewSize:YES];
    [[self window] setAutodisplay:YES];
}

- (void)_delayedEndPrintMode:(NSPrintOperation *)initiatingOperation
{
    ASSERT_ARG(initiatingOperation, initiatingOperation != nil);
    NSPrintOperation *currentOperation = [NSPrintOperation currentOperation];
    if (initiatingOperation == currentOperation) {
        // The print operation is still underway. We don't expect this to ever happen, hence the assert, but we're
        // being extra paranoid here since the printing code is so fragile. Delay the cleanup
        // further.
        ASSERT_NOT_REACHED();
        [self performSelector:@selector(_delayedEndPrintMode:) withObject:initiatingOperation afterDelay:0];
    } else if ([currentOperation view] == self) {
        // A new print job has started, but it is printing the same WebHTMLView again. We don't expect
        // this to ever happen, hence the assert, but we're being extra paranoid here since the printing code is so
        // fragile. Do nothing, because we don't want to break the print job currently in progress, and
        // the print job currently in progress is responsible for its own cleanup.
        ASSERT_NOT_REACHED();
    } else {
        // The print job that kicked off this delayed call has finished, and this view is not being
        // printed again. We expect that no other print job has started. Since this delayed call wasn't
        // cancelled, beginDocument and endDocument must not have been called, and we need to clean up
        // the print mode here.
        ASSERT(currentOperation == nil);
        [self _endPrintMode];
    }
}

// Return the number of pages available for printing
- (BOOL)knowsPageRange:(NSRangePointer)range
{
    // Must do this explicit display here, because otherwise the view might redisplay while the print
    // sheet was up, using printer fonts (and looking different).
    [self displayIfNeeded];
    [[self window] setAutodisplay:NO];
    
    // If we are a frameset just print with the layout we have onscreen, otherwise relayout
    // according to the paper size
    float minLayoutWidth = 0.0f;
    float maxLayoutWidth = 0.0f;
    if (!core([self _frame])->isFrameSet()) {
        float paperWidth = [self _availablePaperWidthForPrintOperation:[NSPrintOperation currentOperation]];
        minLayoutWidth = paperWidth * PrintingMinimumShrinkFactor;
        maxLayoutWidth = paperWidth * PrintingMaximumShrinkFactor;
    }
    [self _setPrinting:YES minimumPageWidth:minLayoutWidth maximumPageWidth:maxLayoutWidth adjustViewSize:YES]; // will relayout
    NSPrintOperation *printOperation = [NSPrintOperation currentOperation];
    // Certain types of errors, including invalid page ranges, can cause beginDocument and
    // endDocument to be skipped after we've put ourselves in print mode (see 4145905). In those cases
    // we need to get out of print mode without relying on any more callbacks from the printing mechanism.
    // If we get as far as beginDocument without trouble, then this delayed request will be cancelled.
    // If not cancelled, this delayed call will be invoked in the next pass through the main event loop,
    // which is after beginDocument and endDocument would be called.
    [self performSelector:@selector(_delayedEndPrintMode:) withObject:printOperation afterDelay:0];
    [[self _webView] _adjustPrintingMarginsForHeaderAndFooter];
    
    // There is a theoretical chance that someone could do some drawing between here and endDocument,
    // if something caused setNeedsDisplay after this point. If so, it's not a big tragedy, because
    // you'd simply see the printer fonts on screen. As of this writing, this does not happen with Safari.

    range->location = 1;
    float totalScaleFactor = [self _scaleFactorForPrintOperation:printOperation];
    float userScaleFactor = [printOperation _web_pageSetupScaleFactor];
    [_private->pageRects release];
    float fullPageHeight = floorf([self _calculatePrintHeight]/totalScaleFactor);
    NSArray *newPageRects = [[self _bridge] computePageRectsWithPrintWidthScaleFactor:userScaleFactor
                                                                          printHeight:fullPageHeight];
    
    // AppKit gets all messed up if you give it a zero-length page count (see 3576334), so if we
    // hit that case we'll pass along a degenerate 1 pixel square to print. This will print
    // a blank page (with correct-looking header and footer if that option is on), which matches
    // the behavior of IE and Camino at least.
    if ([newPageRects count] == 0)
        newPageRects = [NSArray arrayWithObject:[NSValue valueWithRect:NSMakeRect(0, 0, 1, 1)]];
    else if ([newPageRects count] > 1) {
        // If the last page is a short orphan, try adjusting the print height slightly to see if this will squeeze the
        // content onto one fewer page. If it does, use the adjusted scale. If not, use the original scale.
        float lastPageHeight = NSHeight([[newPageRects lastObject] rectValue]);
        if (lastPageHeight/fullPageHeight < LastPrintedPageOrphanRatio) {
            NSArray *adjustedPageRects = [[self _bridge] computePageRectsWithPrintWidthScaleFactor:userScaleFactor
                                                                                       printHeight:fullPageHeight*PrintingOrphanShrinkAdjustment];
            // Use the adjusted rects only if the page count went down
            if ([adjustedPageRects count] < [newPageRects count])
                newPageRects = adjustedPageRects;
        }
    }
    
    _private->pageRects = [newPageRects retain];
    
    range->length = [_private->pageRects count];
    
    return YES;
}

// Return the drawing rectangle for a particular page number
- (NSRect)rectForPage:(int)page
{
    return [[_private->pageRects objectAtIndex:page - 1] rectValue];
}

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));    
    [[self _webView] _drawHeaderAndFooter];
}

- (void)beginDocument
{
    NS_DURING
        // From now on we'll get a chance to call _endPrintMode in either beginDocument or
        // endDocument, so we can cancel the "just in case" pending call.
        [NSObject cancelPreviousPerformRequestsWithTarget:self
                                                 selector:@selector(_delayedEndPrintMode:)
                                                   object:[NSPrintOperation currentOperation]];
        [super beginDocument];
    NS_HANDLER
        // Exception during [super beginDocument] means that endDocument will not get called,
        // so we need to clean up our "print mode" here.
        [self _endPrintMode];
    NS_ENDHANDLER
}

- (void)endDocument
{
    [super endDocument];
    // Note sadly at this point [NSGraphicsContext currentContextDrawingToScreen] is still NO 
    [self _endPrintMode];
}

- (BOOL)_interceptEditingKeyEvent:(NSEvent *)event
{   
    // Use WebView's tabKeyCyclesThroughElements state to determine whether or not
    // to process tab key events. The idea here is that tabKeyCyclesThroughElements
    // will be YES when this WebView is being used in a browser, and we desire the
    // behavior where tab moves to the next element in tab order. If tabKeyCyclesThroughElements
    // is NO, it is likely that the WebView is being embedded as the whole view, as in Mail,
    // and tabs should input tabs as expected in a text editor. Using Option-Tab always cycles
    // through elements.

    if ([[self _webView] tabKeyCyclesThroughElements] && [event _web_isTabKeyEvent]) 
        return NO;

    if (![[self _webView] tabKeyCyclesThroughElements] && [event _web_isOptionTabKeyEvent])
        return NO;

    // Now process the key normally
    [self interpretKeyEvents:[NSArray arrayWithObject:event]];
    return YES;
}

- (void)keyDown:(NSEvent *)event
{
    BOOL eventWasSentToWebCore = (_private->keyDownEvent == event);

    [self retain];

    BOOL callSuper = NO;

    [_private->keyDownEvent release];
    _private->keyDownEvent = [event retain];

    if (!eventWasSentToWebCore && core([self _frame])->eventHandler()->keyEvent(event)) {
        // WebCore processed a key event, bail on any outstanding complete: UI
        [_private->compController endRevertingChange:YES moveLeft:NO];
    } else if (_private->compController && [_private->compController filterKeyDown:event]) {
        // Consumed by complete: popup window
    } else {
        // We're going to process a key event, bail on any outstanding complete: UI
        [_private->compController endRevertingChange:YES moveLeft:NO];
        BOOL handledKey = [self _canEdit] && [self _interceptEditingKeyEvent:event];
        if (!handledKey)
            callSuper = YES;
    }
    if (callSuper)
        [super keyDown:event];
    else
        [NSCursor setHiddenUntilMouseMoves:YES];

    [self release];
}

- (void)keyUp:(NSEvent *)event
{
    BOOL eventWasSentToWebCore = (_private->keyDownEvent == event);

    [self retain];
    if (eventWasSentToWebCore || !core([self _frame])->eventHandler()->keyEvent(event))
        [super keyUp:event];    
    [self release];
}

- (id)accessibilityAttributeValue:(NSString*)attributeName
{
    if ([attributeName isEqualToString: NSAccessibilityChildrenAttribute]) {
        id accTree = [[self _bridge] accessibilityTree];
        if (accTree)
            return [NSArray arrayWithObject:accTree];
        return nil;
    }
    return [super accessibilityAttributeValue:attributeName];
}

- (id)accessibilityFocusedUIElement
{
    id accTree = [[self _bridge] accessibilityTree];
    if (accTree)
        return [accTree accessibilityFocusedUIElement];
    return self;
}

- (id)accessibilityHitTest:(NSPoint)point
{
    id accTree = [[self _bridge] accessibilityTree];
    if (accTree) {
        NSPoint windowCoord = [[self window] convertScreenToBase:point];
        return [accTree accessibilityHitTest:[self convertPoint:windowCoord fromView:nil]];
    }
    return self;
}

- (id)_accessibilityParentForSubview:(NSView *)subview
{
    id accTree = [[self _bridge] accessibilityTree];
    if (!accTree)
        return self;
    id parent = [accTree _accessibilityParentForSubview:subview];
    if (!parent)
        return self;
    return parent;
}

- (void)centerSelectionInVisibleArea:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->revealSelection(RenderLayer::gAlignCenterAlways);
}

- (void)moveBackward:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveBackward");
}

- (void)moveBackwardAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveBackwardAndModifySelection");
}

- (void)moveDown:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveDown");
}

- (void)moveDownAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveDownAndModifySelection");
}

- (void)moveForward:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveForward");
}

- (void)moveForwardAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveForwardAndModifySelection");
}

- (void)moveLeft:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveLeft");
}

- (void)moveLeftAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveLeftAndModifySelection");
}

- (void)moveRight:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveRight");
}

- (void)moveRightAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveRightAndModifySelection");
}

- (void)moveToBeginningOfDocument:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToBeginningOfDocument");
}

- (void)moveToBeginningOfDocumentAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToBeginningOfDocumentAndModifySelection");
}

- (void)moveToBeginningOfSentence:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToBeginningOfSentence");
}

- (void)moveToBeginningOfSentenceAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToBeginningOfSentenceAndModifySelection");
}

- (void)moveToBeginningOfLine:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToBeginningOfLine");
}

- (void)moveToBeginningOfLineAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToBeginningOfLineAndModifySelection");
}

- (void)moveToBeginningOfParagraph:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToBeginningOfParagraph");
}

- (void)moveToBeginningOfParagraphAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToBeginningOfParagraphAndModifySelection");
}

- (void)moveToEndOfDocument:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToEndOfDocument");
}

- (void)moveToEndOfDocumentAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToEndOfDocumentAndModifySelection");
}

- (void)moveToEndOfSentence:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToEndOfSentence");
}

- (void)moveToEndOfSentenceAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToEndOfSentenceAndModifySelection");
}

- (void)moveToEndOfLine:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToEndOfLine");
}

- (void)moveToEndOfLineAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToEndOfLineAndModifySelection");
}

- (void)moveToEndOfParagraph:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToEndOfParagraph");
}

- (void)moveToEndOfParagraphAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveToEndOfParagraphAndModifySelection");
}

- (void)moveParagraphBackwardAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveParagraphBackwardAndModifySelection");
}

- (void)moveParagraphForwardAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveParagraphForwardAndModifySelection");
}

- (void)moveUp:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveUp");
}

- (void)moveUpAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveUpAndModifySelection");
}

- (void)moveWordBackward:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveWordBackward");
}

- (void)moveWordBackwardAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveWordBackwardAndModifySelection");
}

- (void)moveWordForward:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveWordForward");
}

- (void)moveWordForwardAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveWordForwardAndModifySelection");
}

- (void)moveWordLeft:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveWordLeft");
}

- (void)moveWordLeftAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveWordLeftAndModifySelection");
}

- (void)moveWordRight:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveWordRight");
}

- (void)moveWordRightAndModifySelection:(id)sender
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("MoveWordRightAndModifySelection");
}

- (void)pageUp:(id)sender
{
    WebFrameView *frameView = [self _frameView];
    if (!frameView)
        return;
    if ([self _canAlterCurrentSelection])
        [[self _bridge] alterCurrentSelection:SelectionController::MOVE verticalDistance:-[frameView _verticalPageScrollDistance]];
}

- (void)pageDown:(id)sender
{
    WebFrameView *frameView = [self _frameView];
    if (!frameView)
        return;
    if ([self _canAlterCurrentSelection])
        [[self _bridge] alterCurrentSelection:SelectionController::MOVE verticalDistance:[frameView _verticalPageScrollDistance]];
}

- (void)pageUpAndModifySelection:(id)sender
{
    WebFrameView *frameView = [self _frameView];
    if (!frameView)
        return;
    if ([self _canAlterCurrentSelection])
        [[self _bridge] alterCurrentSelection:SelectionController::EXTEND verticalDistance:-[frameView _verticalPageScrollDistance]];
}

- (void)pageDownAndModifySelection:(id)sender
{
    WebFrameView *frameView = [self _frameView];
    if (frameView == nil)
        return;
    if ([self _canAlterCurrentSelection])
        [[self _bridge] alterCurrentSelection:SelectionController::EXTEND verticalDistance:[frameView _verticalPageScrollDistance]];
}

- (void)_expandSelectionToGranularity:(TextGranularity)granularity
{
    if (![self _canAlterCurrentSelection])
        return;
    
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame || !coreFrame->selectionController()->isCaretOrRange())
        return;

    // NOTE: The enums *must* match the very similar ones declared in SelectionController.h
    Selection selection(coreFrame->selectionController()->selection());
    selection.expandUsingGranularity(static_cast<TextGranularity>(granularity));
    
    RefPtr<Range> range = selection.toRange();
    DOMRange *domRange = kit(range.get());
    
    if ([domRange collapsed])
        return;

    EAffinity affinity = coreFrame->selectionController()->affinity();
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldChangeSelectedDOMRange:[self _selectedRange] toDOMRange:domRange affinity:kit(affinity) stillSelecting:NO])
        selectRange(coreFrame->selectionController(), range.get(), affinity, true);
}

- (void)selectParagraph:(id)sender
{
    [self _expandSelectionToGranularity:ParagraphGranularity];
}

- (void)selectLine:(id)sender
{
    [self _expandSelectionToGranularity:LineGranularity];
}

- (void)selectSentence:(id)sender
{
    [self _expandSelectionToGranularity:SentenceGranularity];
}

- (void)selectWord:(id)sender
{
    [self _expandSelectionToGranularity:WordGranularity];
}

- (void)delete:(id)sender
{
#if USING_WEBCORE_DELETE
    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->editor()->performDelete();
#else
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame->editor()->canDelete()) {
        NSBeep();
        return;
    }
    [self _deleteSelection];
#endif
}

- (NSData *)_selectionStartFontAttributesAsRTF
{
    NSAttributedString *string = [[NSAttributedString alloc] initWithString:@"x"
        attributes:core([self _frame])->fontAttributesForSelectionStart()];
    NSData *data = [string RTFFromRange:NSMakeRange(0, [string length]) documentAttributes:nil];
    [string release];
    return data;
}

- (NSDictionary *)_fontAttributesFromFontPasteboard
{
    NSPasteboard *fontPasteboard = [NSPasteboard pasteboardWithName:NSFontPboard];
    if (fontPasteboard == nil)
        return nil;
    NSData *data = [fontPasteboard dataForType:NSFontPboardType];
    if (data == nil || [data length] == 0)
        return nil;
    // NSTextView does something more efficient by parsing the attributes only, but that's not available in API.
    NSAttributedString *string = [[[NSAttributedString alloc] initWithRTF:data documentAttributes:NULL] autorelease];
    if (string == nil || [string length] == 0)
        return nil;
    return [string fontAttributesInRange:NSMakeRange(0, 1)];
}

- (DOMCSSStyleDeclaration *)_emptyStyle
{
    return [[[self _frame] DOMDocument] createCSSStyleDeclaration];
}

- (NSString *)_colorAsString:(NSColor *)color
{
    NSColor *rgbColor = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
    // FIXME: If color is non-nil and rgbColor is nil, that means we got some kind
    // of fancy color that can't be converted to RGB. Changing that to "transparent"
    // might not be great, but it's probably OK.
    if (rgbColor == nil)
        return @"transparent";
    float r = [rgbColor redComponent];
    float g = [rgbColor greenComponent];
    float b = [rgbColor blueComponent];
    float a = [rgbColor alphaComponent];
    if (a == 0)
        return @"transparent";
    if (r == 0 && g == 0 && b == 0 && a == 1)
        return @"black";
    if (r == 1 && g == 1 && b == 1 && a == 1)
        return @"white";
    // FIXME: Lots more named colors. Maybe we could use the table in WebCore?
    if (a == 1)
        return [NSString stringWithFormat:@"rgb(%.0f,%.0f,%.0f)", r * 255, g * 255, b * 255];
    return [NSString stringWithFormat:@"rgba(%.0f,%.0f,%.0f,%f)", r * 255, g * 255, b * 255, a];
}

- (NSString *)_shadowAsString:(NSShadow *)shadow
{
    if (shadow == nil)
        return @"none";
    NSSize offset = [shadow shadowOffset];
    float blurRadius = [shadow shadowBlurRadius];
    if (offset.width == 0 && offset.height == 0 && blurRadius == 0)
        return @"none";
    NSColor *color = [shadow shadowColor];
    if (color == nil)
        return @"none";
    // FIXME: Handle non-integral values here?
    if (blurRadius == 0)
        return [NSString stringWithFormat:@"%@ %.0fpx %.0fpx", [self _colorAsString:color], offset.width, offset.height];
    return [NSString stringWithFormat:@"%@ %.0fpx %.0fpx %.0fpx", [self _colorAsString:color], offset.width, offset.height, blurRadius];
}

- (DOMCSSStyleDeclaration *)_styleFromFontAttributes:(NSDictionary *)dictionary
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSColor *color = [dictionary objectForKey:NSBackgroundColorAttributeName];
    [style setBackgroundColor:[self _colorAsString:color]];

    NSFont *font = [dictionary objectForKey:NSFontAttributeName];
    if (font == nil) {
        [style setFontFamily:@"Helvetica"];
        [style setFontSize:@"12px"];
        [style setFontWeight:@"normal"];
        [style setFontStyle:@"normal"];
    } else {
        NSFontManager *fm = [NSFontManager sharedFontManager];
        // FIXME: Need more sophisticated escaping code if we want to handle family names
        // with characters like single quote or backslash in their names.
        [style setFontFamily:[NSString stringWithFormat:@"'%@'", [font familyName]]];
        [style setFontSize:[NSString stringWithFormat:@"%0.fpx", [font pointSize]]];
        if ([fm weightOfFont:font] >= MIN_BOLD_WEIGHT)
            [style setFontWeight:@"bold"];
        else
            [style setFontWeight:@"normal"];
        if (([fm traitsOfFont:font] & NSItalicFontMask) != 0)
            [style setFontStyle:@"italic"];
        else
            [style setFontStyle:@"normal"];
    }

    color = [dictionary objectForKey:NSForegroundColorAttributeName];
    [style setColor:color ? [self _colorAsString:color] : (NSString *)@"black"];

    NSShadow *shadow = [dictionary objectForKey:NSShadowAttributeName];
    [style setTextShadow:[self _shadowAsString:shadow]];

    int strikethroughInt = [[dictionary objectForKey:NSStrikethroughStyleAttributeName] intValue];

    int superscriptInt = [[dictionary objectForKey:NSSuperscriptAttributeName] intValue];
    if (superscriptInt > 0)
        [style setVerticalAlign:@"super"];
    else if (superscriptInt < 0)
        [style setVerticalAlign:@"sub"];
    else
        [style setVerticalAlign:@"baseline"];
    int underlineInt = [[dictionary objectForKey:NSUnderlineStyleAttributeName] intValue];
    // FIXME: Underline wins here if we have both (see bug 3790443).
    if (strikethroughInt == NSUnderlineStyleNone && underlineInt == NSUnderlineStyleNone)
        [style setProperty:@"-khtml-text-decorations-in-effect" value:@"none" priority:@""];
    else if (underlineInt == NSUnderlineStyleNone)
        [style setProperty:@"-khtml-text-decorations-in-effect" value:@"line-through" priority:@""];
    else
        [style setProperty:@"-khtml-text-decorations-in-effect" value:@"underline" priority:@""];

    return style;
}

- (void)_applyStyleToSelection:(DOMCSSStyleDeclaration *)style withUndoAction:(EditAction)undoAction
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->applyStyleToSelection(core(style), undoAction);
}

- (void)_applyParagraphStyleToSelection:(DOMCSSStyleDeclaration *)style withUndoAction:(EditAction)undoAction
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->applyParagraphStyleToSelection(core(style), undoAction);
}

- (void)_toggleBold
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("ToggleBold");
}

- (void)_toggleItalic
{
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->execCommand("ToggleItalic");
}

- (BOOL)_handleStyleKeyEquivalent:(NSEvent *)event
{
    ASSERT([self _webView]);
    if (![[[self _webView] preferences] respectStandardStyleKeyEquivalents])
        return NO;
    
    if (![self _canEdit])
        return NO;
    
    NSString *string = [event charactersIgnoringModifiers];
    if ([string isEqualToString:@"b"]) {
        [self _toggleBold];
        return YES;
    }
    if ([string isEqualToString:@"i"]) {
        [self _toggleItalic];
        return YES;
    }
    
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    if ([self _handleStyleKeyEquivalent:event])
        return YES;
    
    BOOL eventWasSentToWebCore = (_private->keyDownEvent == event);
    BOOL ret = NO;

    [_private->keyDownEvent release];
    _private->keyDownEvent = [event retain];
    
    [self retain];
    
    // Pass command-key combos through WebCore if there is a key binding available for
    // this event. This lets web pages have a crack at intercepting command-modified keypresses.
    // But don't do it if we have already handled the event.
    if (!eventWasSentToWebCore 
            && event == [NSApp currentEvent]    // Pressing Esc results in a fake event being sent - don't pass it to WebCore
            && [self _web_firstResponderIsSelfOrDescendantView]
            && core([self _frame])
            && core([self _frame])->eventHandler()->keyEvent(event))
        ret = YES;
    else
        ret = [super performKeyEquivalent:event];
    
    [self release];
    
    return ret;
}

- (void)copyFont:(id)sender
{
    // Put RTF with font attributes on the pasteboard.
    // Maybe later we should add a pasteboard type that contains CSS text for "native" copy and paste font.
    NSPasteboard *fontPasteboard = [NSPasteboard pasteboardWithName:NSFontPboard];
    [fontPasteboard declareTypes:[NSArray arrayWithObject:NSFontPboardType] owner:nil];
    [fontPasteboard setData:[self _selectionStartFontAttributesAsRTF] forType:NSFontPboardType];
}

- (void)pasteFont:(id)sender
{
    // Read RTF with font attributes from the pasteboard.
    // Maybe later we should add a pasteboard type that contains CSS text for "native" copy and paste font.
    [self _applyStyleToSelection:[self _styleFromFontAttributes:[self _fontAttributesFromFontPasteboard]] withUndoAction:EditActionPasteFont];
}

- (void)pasteAsRichText:(id)sender
{
    // Since rich text always beats plain text when both are on the pasteboard, it's not
    // clear how this is different from plain old paste.
    [self _pasteWithPasteboard:[NSPasteboard generalPasteboard] allowPlainText:NO];
}

- (NSFont *)_originalFontA
{
    return [[NSFontManager sharedFontManager] fontWithFamily:@"Helvetica" traits:0 weight:STANDARD_WEIGHT size:10.0f];
}

- (NSFont *)_originalFontB
{
    return [[NSFontManager sharedFontManager] fontWithFamily:@"Times" traits:(NSBoldFontMask | NSItalicFontMask) weight:STANDARD_BOLD_WEIGHT size:12.0f];
}

- (void)_addToStyle:(DOMCSSStyleDeclaration *)style fontA:(NSFont *)a fontB:(NSFont *)b
{
    // Since there's no way to directly ask NSFontManager what style change it's going to do
    // we instead pass two "specimen" fonts to it and let it change them. We then deduce what
    // style change it was doing by looking at what happened to each of the two fonts.
    // So if it was making the text bold, both fonts will be bold after the fact.

    if (a == nil || b == nil)
        return;

    NSFontManager *fm = [NSFontManager sharedFontManager];

    NSFont *oa = [self _originalFontA];

    NSString *aFamilyName = [a familyName];
    NSString *bFamilyName = [b familyName];

    int aPointSize = (int)[a pointSize];
    int bPointSize = (int)[b pointSize];

    int aWeight = [fm weightOfFont:a];
    int bWeight = [fm weightOfFont:b];

    BOOL aIsBold = aWeight >= MIN_BOLD_WEIGHT;

    BOOL aIsItalic = ([fm traitsOfFont:a] & NSItalicFontMask) != 0;
    BOOL bIsItalic = ([fm traitsOfFont:b] & NSItalicFontMask) != 0;

    if ([aFamilyName isEqualToString:bFamilyName]) {
        NSString *familyNameForCSS = aFamilyName;

        // The family name may not be specific enough to get us the font specified.
        // In some cases, the only way to get exactly what we are looking for is to use
        // the Postscript name.
        
        // Find the font the same way the rendering code would later if it encountered this CSS.
        NSFontTraitMask traits = 0;
        if (aIsBold)
            traits |= NSBoldFontMask;
        if (aIsItalic)
            traits |= NSItalicFontMask;
        NSFont *foundFont = WebCoreFindFont(aFamilyName, traits, aPointSize);

        // If we don't find a font with the same Postscript name, then we'll have to use the
        // Postscript name to make the CSS specific enough.
        if (![[foundFont fontName] isEqualToString:[a fontName]]) {
            familyNameForCSS = [a fontName];
        }

        // FIXME: Need more sophisticated escaping code if we want to handle family names
        // with characters like single quote or backslash in their names.
        [style setFontFamily:[NSString stringWithFormat:@"'%@'", familyNameForCSS]];
    }

    int soa = (int)[oa pointSize];
    if (aPointSize == bPointSize)
        [style setFontSize:[NSString stringWithFormat:@"%dpx", aPointSize]];
    else if (aPointSize < soa)
        [style _setFontSizeDelta:@"-1px"];
    else if (aPointSize > soa)
        [style _setFontSizeDelta:@"1px"];

    if (aWeight == bWeight)
        [style setFontWeight:aIsBold ? @"bold" : @"normal"];

    if (aIsItalic == bIsItalic)
        [style setFontStyle:aIsItalic ? @"italic" :  @"normal"];
}

- (DOMCSSStyleDeclaration *)_styleFromFontManagerOperation
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSFontManager *fm = [NSFontManager sharedFontManager];

    NSFont *oa = [self _originalFontA];
    NSFont *ob = [self _originalFontB];    
    [self _addToStyle:style fontA:[fm convertFont:oa] fontB:[fm convertFont:ob]];

    return style;
}

- (void)changeFont:(id)sender
{
    [self _applyStyleToSelection:[self _styleFromFontManagerOperation] withUndoAction:EditActionSetFont];
}

- (DOMCSSStyleDeclaration *)_styleForAttributeChange:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    NSShadow *shadow = [[NSShadow alloc] init];
    [shadow setShadowOffset:NSMakeSize(1, 1)];

    NSDictionary *oa = [NSDictionary dictionaryWithObjectsAndKeys:
        [self _originalFontA], NSFontAttributeName,
        nil];
    NSDictionary *ob = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSColor blackColor], NSBackgroundColorAttributeName,
        [self _originalFontB], NSFontAttributeName,
        [NSColor whiteColor], NSForegroundColorAttributeName,
        shadow, NSShadowAttributeName,
        [NSNumber numberWithInt:NSUnderlineStyleSingle], NSStrikethroughStyleAttributeName,
        [NSNumber numberWithInt:1], NSSuperscriptAttributeName,
        [NSNumber numberWithInt:NSUnderlineStyleSingle], NSUnderlineStyleAttributeName,
        nil];

    [shadow release];

#if 0

NSObliquenessAttributeName        /* float; skew to be applied to glyphs, default 0: no skew */
    // font-style, but that is just an on-off switch

NSExpansionAttributeName          /* float; log of expansion factor to be applied to glyphs, default 0: no expansion */
    // font-stretch?

NSKernAttributeName               /* float, amount to modify default kerning, if 0, kerning off */
    // letter-spacing? probably not good enough

NSUnderlineColorAttributeName     /* NSColor, default nil: same as foreground color */
NSStrikethroughColorAttributeName /* NSColor, default nil: same as foreground color */
    // text-decoration-color?

NSLigatureAttributeName           /* int, default 1: default ligatures, 0: no ligatures, 2: all ligatures */
NSBaselineOffsetAttributeName     /* float, in points; offset from baseline, default 0 */
NSStrokeWidthAttributeName        /* float, in percent of font point size, default 0: no stroke; positive for stroke alone, negative for stroke and fill (a typical value for outlined text would be 3.0) */
NSStrokeColorAttributeName        /* NSColor, default nil: same as foreground color */
    // need extensions?

#endif
    
    NSDictionary *a = [sender convertAttributes:oa];
    NSDictionary *b = [sender convertAttributes:ob];

    NSColor *ca = [a objectForKey:NSBackgroundColorAttributeName];
    NSColor *cb = [b objectForKey:NSBackgroundColorAttributeName];
    if (ca == cb) {
        [style setBackgroundColor:[self _colorAsString:ca]];
    }

    [self _addToStyle:style fontA:[a objectForKey:NSFontAttributeName] fontB:[b objectForKey:NSFontAttributeName]];

    ca = [a objectForKey:NSForegroundColorAttributeName];
    cb = [b objectForKey:NSForegroundColorAttributeName];
    if (ca == cb) {
        [style setColor:[self _colorAsString:ca]];
    }

    NSShadow *sha = [a objectForKey:NSShadowAttributeName];
    if (sha)
        [style setTextShadow:[self _shadowAsString:sha]];
    else if ([b objectForKey:NSShadowAttributeName] == nil)
        [style setTextShadow:@"none"];

    int sa = [[a objectForKey:NSStrikethroughStyleAttributeName] intValue];
    int sb = [[b objectForKey:NSStrikethroughStyleAttributeName] intValue];
    if (sa == sb) {
        if (sa == NSUnderlineStyleNone)
            [style setProperty:@"-khtml-text-decorations-in-effect" value:@"none" priority:@""]; 
            // we really mean "no line-through" rather than "none"
        else
            [style setProperty:@"-khtml-text-decorations-in-effect" value:@"line-through" priority:@""];
            // we really mean "add line-through" rather than "line-through"
    }

    sa = [[a objectForKey:NSSuperscriptAttributeName] intValue];
    sb = [[b objectForKey:NSSuperscriptAttributeName] intValue];
    if (sa == sb) {
        if (sa > 0)
            [style setVerticalAlign:@"super"];
        else if (sa < 0)
            [style setVerticalAlign:@"sub"];
        else
            [style setVerticalAlign:@"baseline"];
    }

    int ua = [[a objectForKey:NSUnderlineStyleAttributeName] intValue];
    int ub = [[b objectForKey:NSUnderlineStyleAttributeName] intValue];
    if (ua == ub) {
        if (ua == NSUnderlineStyleNone)
            [style setProperty:@"-khtml-text-decorations-in-effect" value:@"none" priority:@""];
            // we really mean "no underline" rather than "none"
        else
            [style setProperty:@"-khtml-text-decorations-in-effect" value:@"underline" priority:@""];
            // we really mean "add underline" rather than "underline"
    }

    return style;
}

- (void)changeAttributes:(id)sender
{
    [self _applyStyleToSelection:[self _styleForAttributeChange:sender] withUndoAction:EditActionChangeAttributes];
}

- (DOMCSSStyleDeclaration *)_styleFromColorPanelWithSelector:(SEL)selector
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];

    ASSERT([style respondsToSelector:selector]);
    [style performSelector:selector withObject:[self _colorAsString:[[NSColorPanel sharedColorPanel] color]]];
    
    return style;
}

- (EditAction)_undoActionFromColorPanelWithSelector:(SEL)selector
{
    if (selector == @selector(setBackgroundColor:))
        return EditActionSetBackgroundColor;    
    return EditActionSetColor;
}

- (void)_changeCSSColorUsingSelector:(SEL)selector inRange:(DOMRange *)range
{
    DOMCSSStyleDeclaration *style = [self _styleFromColorPanelWithSelector:selector];
    WebView *webView = [self _webView];
    if ([[webView _editingDelegateForwarder] webView:webView shouldApplyStyle:style toElementsInDOMRange:range])
        core([self _frame])->editor()->applyStyle(core(style), [self _undoActionFromColorPanelWithSelector:selector]);
}

- (void)changeDocumentBackgroundColor:(id)sender
{
    // Mimicking NSTextView, this method sets the background color for the
    // entire document. There is no NSTextView API for setting the background
    // color on the selected range only. Note that this method is currently
    // never called from the UI (see comment in changeColor:).
    // FIXME: this actually has no effect when called, probably due to 3654850. _documentRange seems
    // to do the right thing because it works in startSpeaking:, and I know setBackgroundColor: does the
    // right thing because I tested it with [self _selectedRange].
    // FIXME: This won't actually apply the style to the entire range here, because it ends up calling
    // [bridge applyStyle:], which operates on the current selection. To make this work right, we'll
    // need to save off the selection, temporarily set it to the entire range, make the change, then
    // restore the old selection.
    [self _changeCSSColorUsingSelector:@selector(setBackgroundColor:) inRange:[self _documentRange]];
}

- (void)changeColor:(id)sender
{
    // FIXME: in NSTextView, this method calls changeDocumentBackgroundColor: when a
    // private call has earlier been made by [NSFontFontEffectsBox changeColor:], see 3674493. 
    // AppKit will have to be revised to allow this to work with anything that isn't an 
    // NSTextView. However, this might not be required for Tiger, since the background-color 
    // changing box in the font panel doesn't work in Mail (3674481), though it does in TextEdit.
    [self _applyStyleToSelection:[self _styleFromColorPanelWithSelector:@selector(setColor:)] 
                  withUndoAction:EditActionSetColor];
}

- (void)_alignSelectionUsingCSSValue:(NSString *)CSSAlignmentValue withUndoAction:(EditAction)undoAction
{
    if (![self _canEditRichly])
        return;
        
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setTextAlign:CSSAlignmentValue];
    [self _applyStyleToSelection:style withUndoAction:undoAction];
}

- (void)alignCenter:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"center" withUndoAction:EditActionCenter];
}

- (void)alignJustified:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"justify" withUndoAction:EditActionJustify];
}

- (void)alignLeft:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"left" withUndoAction:EditActionAlignLeft];
}

- (void)alignRight:(id)sender
{
    [self _alignSelectionUsingCSSValue:@"right" withUndoAction:EditActionAlignRight];
}

- (void)insertTab:(id)sender
{
    [self insertText:@"\t"];
}

- (void)insertBacktab:(id)sender
{
    // Doing nothing matches normal NSTextView behavior. If we ever use WebView for a field-editor-type purpose
    // we might add code here.
}

- (void)insertNewline:(id)sender
{
    if (![self _canEdit])
        return;
        
    // Perhaps we should make this delegate call sensitive to the real DOM operation we actually do.
    WebFrameBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:@"\n" givenAction:WebViewInsertActionTyped]) {
        if ([self _canEditRichly])
            [bridge insertParagraphSeparator];
        else
            [bridge insertLineBreak];
    }
}

- (void)insertLineBreak:(id)sender
{
    if (![self _canEdit])
        return;
        
    // Perhaps we should make this delegate call sensitive to the real DOM operation we actually do.
    WebFrameBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:@"\n" givenAction:WebViewInsertActionTyped])
        [bridge insertLineBreak];
}

- (void)insertParagraphSeparator:(id)sender
{
    if (![self _canEdit])
        return;

    // Perhaps we should make this delegate call sensitive to the real DOM operation we actually do.
    WebFrameBridge *bridge = [self _bridge];
    if ([self _shouldReplaceSelectionWithText:@"\n" givenAction:WebViewInsertActionTyped]) {
        if ([self _canEditRichly])
            [bridge insertParagraphSeparator];
        else
            [bridge insertLineBreak];
    }
}

- (void)_changeWordCaseWithSelector:(SEL)selector
{
    if (![self _canEdit])
        return;

    WebFrameBridge *bridge = [self _bridge];
    [self selectWord:nil];
    NSString *word = [[bridge selectedString] performSelector:selector];
    // FIXME: Does this need a different action context other than "typed"?
    if ([self _shouldReplaceSelectionWithText:word givenAction:WebViewInsertActionTyped])
        [bridge replaceSelectionWithText:word selectReplacement:NO smartReplace:NO];
}

- (void)uppercaseWord:(id)sender
{
    [self _changeWordCaseWithSelector:@selector(uppercaseString)];
}

- (void)lowercaseWord:(id)sender
{
    [self _changeWordCaseWithSelector:@selector(lowercaseString)];
}

- (void)capitalizeWord:(id)sender
{
    [self _changeWordCaseWithSelector:@selector(capitalizedString)];
}

- (BOOL)_deleteWithDirection:(SelectionController::EDirection)direction granularity:(TextGranularity)granularity killRing:(BOOL)killRing isTypingAction:(BOOL)isTypingAction
{
    // Delete the selection, if there is one.
    // If not, make a selection using the passed-in direction and granularity.

    if (![self _canEdit])
        return NO;

    DOMRange *range = [self _selectedRange];
    WebDeletionAction deletionAction = deleteSelectionAction;

    BOOL smartDeleteOK = NO;
    
    if ([self _hasSelection]) {
        smartDeleteOK = YES;
        if (isTypingAction)
            deletionAction = deleteKeyAction;
    } else {
        switch (direction) {
            case SelectionController::FORWARD:
            case SelectionController::RIGHT:
                deletionAction = forwardDeleteKeyAction;
                break;
            case SelectionController::BACKWARD:
            case SelectionController::LEFT:
                deletionAction = deleteKeyAction;
                break;
        }
    }

    if (range == nil)
        return NO;
    [self _deleteRange:range killRing:killRing prepend:NO smartDeleteOK:smartDeleteOK deletionAction:deletionAction granularity:granularity];
    return YES;
}

- (void)deleteForward:(id)sender
{
    if (![self _isEditable])
        return;
    [self _deleteWithDirection:SelectionController::FORWARD granularity:CharacterGranularity killRing:NO isTypingAction:YES];
}

- (void)deleteBackward:(id)sender
{
    if (![self _isEditable])
        return;
    [self _deleteWithDirection:SelectionController::BACKWARD granularity:CharacterGranularity killRing:NO isTypingAction:YES];
}

- (void)deleteBackwardByDecomposingPreviousCharacter:(id)sender
{
    LOG_ERROR("unimplemented, doing deleteBackward instead");
    [self deleteBackward:sender];
}

- (void)deleteWordForward:(id)sender
{
    [self _deleteWithDirection:SelectionController::FORWARD granularity:WordGranularity killRing:YES isTypingAction:NO];
}

- (void)deleteWordBackward:(id)sender
{
    [self _deleteWithDirection:SelectionController::BACKWARD granularity:WordGranularity killRing:YES isTypingAction:NO];
}

- (void)deleteToBeginningOfLine:(id)sender
{
    [self _deleteWithDirection:SelectionController::BACKWARD granularity:LineBoundary killRing:YES isTypingAction:NO];
}

- (void)deleteToEndOfLine:(id)sender
{
    // To match NSTextView, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph (like deleteToEndOfParagraph does below).
    if (![self _deleteWithDirection:SelectionController::FORWARD granularity:LineBoundary killRing:YES isTypingAction:NO])
        [self _deleteWithDirection:SelectionController::FORWARD granularity:CharacterGranularity killRing:YES isTypingAction:NO];
}

- (void)deleteToBeginningOfParagraph:(id)sender
{
    [self _deleteWithDirection:SelectionController::BACKWARD granularity:ParagraphBoundary killRing:YES isTypingAction:NO];
}

- (void)deleteToEndOfParagraph:(id)sender
{
    // Despite the name of the method, this should delete the newline if the caret is at the end of a paragraph.
    // If deletion to the end of the paragraph fails, we delete one character forward, which will delete the newline.
    if (![self _deleteWithDirection:SelectionController::FORWARD granularity:ParagraphBoundary killRing:YES isTypingAction:NO])
        [self _deleteWithDirection:SelectionController::FORWARD granularity:CharacterGranularity killRing:YES isTypingAction:NO];
}

- (void)complete:(id)sender
{
    if (![self _canEdit])
        return;
    if (!_private->compController)
        _private->compController = [[WebTextCompleteController alloc] initWithHTMLView:self];
    [_private->compController doCompletion];
}

- (void)checkSpelling:(id)sender
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    core([self _frame])->editor()->advanceToNextMisspelling();
}

- (void)showGuessPanel:(id)sender
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    NSPanel *spellingPanel = [checker spellingPanel];
#if !BUILDING_ON_TIGER
    // Post-Tiger, this menu item is a show/hide toggle, to match AppKit. Leave Tiger behavior alone
    // to match rest of OS X.
    if ([spellingPanel isVisible]) {
        [spellingPanel orderOut:sender];
        return;
    }
#endif
    
    core([self _frame])->editor()->advanceToNextMisspelling(true);
    [spellingPanel orderFront:sender];
}

- (void)_changeSpellingToWord:(NSString *)newWord
{
    if (![self _canEdit])
        return;

    // Don't correct to empty string.  (AppKit checked this, we might as well too.)
    if (![NSSpellChecker sharedSpellChecker]) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    if ([newWord isEqualToString:@""])
        return;

    if ([self _shouldReplaceSelectionWithText:newWord givenAction:WebViewInsertActionPasted])
        [[self _bridge] replaceSelectionWithText:newWord selectReplacement:YES smartReplace:NO];
}

- (void)changeSpelling:(id)sender
{
    [self _changeSpellingToWord:[[sender selectedCell] stringValue]];
}

- (void)ignoreSpelling:(id)sender
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }
    
    NSString *stringToIgnore = [sender stringValue];
    unsigned int length = [stringToIgnore length];
    if (stringToIgnore && length > 0) {
        [checker ignoreWord:stringToIgnore inSpellDocumentWithTag:[[self _webView] spellCheckerDocumentTag]];
        // FIXME: Need to clear misspelling marker if the currently selected word is the one we are to ignore?
    }
}

- (void)performFindPanelAction:(id)sender
{
    // Implementing this will probably require copying all of NSFindPanel.h and .m.
    // We need *almost* the same thing as AppKit, but not quite.
    LOG_ERROR("unimplemented");
}

- (void)startSpeaking:(id)sender
{
    WebFrameBridge *bridge = [self _bridge];
    DOMRange *range = [self _selectedRange];
    if (!range || [range collapsed])
        range = [self _documentRange];
    [NSApp speakString:[bridge stringForRange:range]];
}

- (void)stopSpeaking:(id)sender
{
    [NSApp stopSpeaking:sender];
}

- (void)insertNewlineIgnoringFieldEditor:(id)sender
{
    [self insertNewline:sender];
}

- (void)insertTabIgnoringFieldEditor:(id)sender
{
    [self insertTab:sender];
}

- (void)subscript:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setVerticalAlign:@"sub"];
    [self _applyStyleToSelection:style withUndoAction:EditActionSubscript];
}

- (void)superscript:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setVerticalAlign:@"super"];
    [self _applyStyleToSelection:style withUndoAction:EditActionSuperscript];
}

- (void)unscript:(id)sender
{
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setVerticalAlign:@"baseline"];
    [self _applyStyleToSelection:style withUndoAction:EditActionUnscript];
}

- (void)underline:(id)sender
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    // Despite the name, this method is actually supposed to toggle underline.
    // FIXME: This currently clears overline, line-through, and blink as an unwanted side effect.
    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setProperty:@"-khtml-text-decorations-in-effect" value:@"underline" priority:@""];
    if (coreFrame->editor()->selectionStartHasStyle(core(style)))
        [style setProperty:@"-khtml-text-decorations-in-effect" value:@"none" priority:@""];
    [self _applyStyleToSelection:style withUndoAction:EditActionUnderline];
}

- (void)yank:(id)sender
{
    if (![self _canEdit])
        return;
    
    NSString* yankee = _NSYankFromKillRing();
    if ([yankee length])
        [self insertText:yankee];
    else
        [self deleteBackward:nil];

    _NSSetKillRingToYankedState();
}

- (void)yankAndSelect:(id)sender
{
    if (![self _canEdit])
        return;

    NSString* yankee = _NSYankPreviousFromKillRing();
    if ([yankee length])
        [self _insertText:yankee selectInsertedText:YES];
    else
        [self deleteBackward:nil];
        
    _NSSetKillRingToYankedState();
}

- (void)setMark:(id)sender
{
    [[self _bridge] setMarkDOMRange:[self _selectedRange]];
}

static DOMRange *unionDOMRanges(DOMRange *a, DOMRange *b)
{
    ASSERT(a);
    ASSERT(b);
    DOMRange *s = [a compareBoundaryPoints:DOM_START_TO_START sourceRange:b] <= 0 ? a : b;
    DOMRange *e = [a compareBoundaryPoints:DOM_END_TO_END sourceRange:b] <= 0 ? b : a;
    DOMRange *r = [[[a startContainer] ownerDocument] createRange];
    [r setStart:[s startContainer] offset:[s startOffset]];
    [r setEnd:[e endContainer] offset:[e endOffset]];
    return r;
}

- (void)deleteToMark:(id)sender
{
    if (![self _canEdit])
        return;

    DOMRange *mark = [[self _bridge] markDOMRange];
    if (mark == nil) {
        [self delete:sender];
    } else {
        DOMRange *selection = [self _selectedRange];
        DOMRange *r;
        NS_DURING
            r = unionDOMRanges(mark, selection);
        NS_HANDLER
            r = selection;
        NS_ENDHANDLER
        [self _deleteRange:r killRing:YES prepend:YES smartDeleteOK:NO deletionAction:deleteSelectionAction granularity:CharacterGranularity];
    }
    [self setMark:sender];
}

- (void)selectToMark:(id)sender
{
    WebFrameBridge *bridge = [self _bridge];
    DOMRange *mark = [bridge markDOMRange];
    if (mark == nil) {
        NSBeep();
        return;
    }
    DOMRange *selection = [self _selectedRange];
    Frame* coreFrame = core([self _frame]);
    NS_DURING
        if (coreFrame)
            selectRange(coreFrame->selectionController(), core(unionDOMRanges(mark, selection)), DOWNSTREAM, true);
    NS_HANDLER
        NSBeep();
    NS_ENDHANDLER
}

- (void)swapWithMark:(id)sender
{
    WebFrameBridge *bridge = [self _bridge];
    DOMRange *mark = [bridge markDOMRange];

    if (mark == nil) {
        NSBeep();
        return;
    }

    DOMRange *selection = [self _selectedRange];
    Frame* coreFrame = core([self _frame]);
    NS_DURING
        if (coreFrame)
            selectRange(coreFrame->selectionController(), core(mark), DOWNSTREAM, true);
    NS_HANDLER
        NSBeep();
        return;
    NS_ENDHANDLER
    [bridge setMarkDOMRange:selection];
}

- (void)transpose:(id)sender
{
    if (![self _canEdit])
        return;

    WebFrameBridge *bridge = [self _bridge];
    DOMRange *r = [bridge rangeOfCharactersAroundCaret];
    if (!r)
        return;
    NSString *characters = [bridge stringForRange:r];
    if ([characters length] != 2)
        return;
    NSString *transposed = [[characters substringFromIndex:1] stringByAppendingString:[characters substringToIndex:1]];
    WebView *webView = [self _webView];
    if (![[webView _editingDelegateForwarder] webView:webView shouldChangeSelectedDOMRange:[self _selectedRange]
            toDOMRange:r affinity:NSSelectionAffinityDownstream stillSelecting:NO])
        return;

    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        selectRange(coreFrame->selectionController(), core(r), DOWNSTREAM, true);
    if ([self _shouldReplaceSelectionWithText:transposed givenAction:WebViewInsertActionTyped])
        [bridge replaceSelectionWithText:transposed selectReplacement:NO smartReplace:NO];
}

- (void)toggleBaseWritingDirection:(id)sender
{
    if (![self _canEdit])
        return;
    
    NSString *direction = @"RTL";
    switch ([[self _bridge] baseWritingDirectionForSelectionStart]) {
        case NSWritingDirectionLeftToRight:
            break;
        case NSWritingDirectionRightToLeft:
            direction = @"LTR";
            break;
        // The writingDirectionForSelectionStart method will never return "natural". It
        // will always return a concrete direction. So, keep the compiler happy, and assert not reached.
        case NSWritingDirectionNatural:
            ASSERT_NOT_REACHED();
            break;
    }

    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setDirection:direction];
    [self _applyParagraphStyleToSelection:style withUndoAction:EditActionSetWritingDirection];
}

- (void)changeBaseWritingDirection:(id)sender
{
    if (![self _canEdit])
        return;
    
    NSWritingDirection writingDirection = static_cast<NSWritingDirection>([sender tag]);
    
    // We disable the menu item that performs this action because we can't implement
    // NSWritingDirectionNatural's behavior using CSS.
    ASSERT(writingDirection != NSWritingDirectionNatural);

    DOMCSSStyleDeclaration *style = [self _emptyStyle];
    [style setDirection:writingDirection == NSWritingDirectionLeftToRight ? @"LTR" : @"RTL"];
    [self _applyParagraphStyleToSelection:style withUndoAction:EditActionSetWritingDirection];
}

- (void)indent:(id)sender
{
    core([self _frame])->editor()->indent();
}

- (void)outdent:(id)sender
{
    core([self _frame])->editor()->outdent();
}

#if 0

// CSS does not have a way to specify an outline font, which may make this difficult to implement.
// Maybe a special case of text-shadow?
- (void)outline:(id)sender;

// This is part of table support, which may be in NSTextView for Tiger.
// It's probably simple to do the equivalent thing for WebKit.
- (void)insertTable:(id)sender;

// This could be important.
- (void)toggleTraditionalCharacterShape:(id)sender;

// I'm not sure what the equivalents of these in the web world are.
- (void)insertLineSeparator:(id)sender;
- (void)insertPageBreak:(id)sender;

// These methods are not implemented in NSTextView yet at the time of this writing.
- (void)changeCaseOfLetter:(id)sender;
- (void)transposeWords:(id)sender;

#endif

// Super-hack alert.
// Workaround for bug 3789278.

// Returns a selector only if called while:
//   1) first responder is self
//   2) handling a key down event
//   3) not yet inside keyDown: method
//   4) key is an arrow key
// The selector is the one that gets sent by -[NSWindow _processKeyboardUIKey] for this key.
- (SEL)_arrowKeyDownEventSelectorIfPreprocessing
{
    NSWindow *w = [self window];
    if ([w firstResponder] != self)
        return NULL;
    NSEvent *e = [w currentEvent];
    if ([e type] != NSKeyDown)
        return NULL;
    if (e == _private->keyDownEvent)
        return NULL;
    NSString *s = [e charactersIgnoringModifiers];
    if ([s length] == 0)
        return NULL;
    switch ([s characterAtIndex:0]) {
        case NSDownArrowFunctionKey:
            return @selector(moveDown:);
        case NSLeftArrowFunctionKey:
            return @selector(moveLeft:);
        case NSRightArrowFunctionKey:
            return @selector(moveRight:);
        case NSUpArrowFunctionKey:
            return @selector(moveUp:);
        default:
            return NULL;
    }
}

// Returns NO instead of YES if called on the selector that the
// _arrowKeyDownEventSelectorIfPreprocessing method returns.
// This should only happen inside -[NSWindow _processKeyboardUIKey],
// and together with the change below should cause that method
// to return NO rather than handling the key.
// Also set a 1-shot flag for the nextResponder check below.
- (BOOL)respondsToSelector:(SEL)selector
{
    if (![super respondsToSelector:selector])
        return NO;
    SEL arrowKeySelector = [self _arrowKeyDownEventSelectorIfPreprocessing];
    if (selector != arrowKeySelector)
        return YES;
    _private->nextResponderDisabledOnce = YES;
    return NO;
}

// Returns nil instead of the next responder if called when the
// one-shot flag is set, and _arrowKeyDownEventSelectorIfPreprocessing
// returns something other than NULL. This should only happen inside
// -[NSWindow _processKeyboardUIKey] and together with the change above
// should cause that method to return NO rather than handling the key.
- (NSResponder *)nextResponder
{
    BOOL disabled = _private->nextResponderDisabledOnce;
    _private->nextResponderDisabledOnce = NO;
    if (disabled && [self _arrowKeyDownEventSelectorIfPreprocessing] != NULL)
        return nil;
    return [super nextResponder];
}

@end

@implementation WebHTMLView (WebTextSizing)

- (IBAction)_makeTextSmaller:(id)sender
{
    [self _updateTextSizeMultiplier];
}

- (IBAction)_makeTextLarger:(id)sender
{
    [self _updateTextSizeMultiplier];
}

- (IBAction)_makeTextStandardSize:(id)sender
{
    [self _updateTextSizeMultiplier];
}

- (BOOL)_tracksCommonSizeFactor
{
    return YES;
}

- (void)_textSizeMultiplierChanged
{
    [self _updateTextSizeMultiplier];
}

// never sent because we track the common size factor
- (BOOL)_canMakeTextSmaller
{
    ASSERT_NOT_REACHED();
    return NO;
}

- (BOOL)_canMakeTextLarger
{
    ASSERT_NOT_REACHED();
    return NO;
}

- (BOOL)_canMakeTextStandardSize
{
    ASSERT_NOT_REACHED();
    return NO;
}

@end

@implementation NSArray (WebHTMLView)

- (void)_web_makePluginViewsPerformSelector:(SEL)selector withObject:(id)object
{
    NSEnumerator *enumerator = [self objectEnumerator];
    WebNetscapePluginEmbeddedView *view;
    while ((view = [enumerator nextObject]) != nil)
        if ([view isKindOfClass:[WebNetscapePluginEmbeddedView class]])
            [view performSelector:selector withObject:object];
}

@end

@implementation WebHTMLView (WebInternal)

- (void)_selectionChanged
{
    [self _updateSelectionForInputManager];
    [self _updateFontPanel];
    _private->startNewKillRingSequence = YES;
}

- (void)_formControlIsBecomingFirstResponder:(NSView *)formControl
{
    if (![formControl isDescendantOf:self])
        return;
    _private->descendantBecomingFirstResponder = YES;
    [self _updateActiveState];
    _private->descendantBecomingFirstResponder = NO;
}

- (void)_formControlIsResigningFirstResponder:(NSView *)formControl
{
    // set resigningFirstResponder so _updateActiveState behaves the same way it does when
    // the WebHTMLView itself is resigningFirstResponder; don't use the primary selection feedback.
    // If the first responder is in the process of being set on the WebHTMLView itself, it will
    // get another chance at _updateActiveState in its own becomeFirstResponder method.
    _private->resigningFirstResponder = YES;
    [self _updateActiveState];
    _private->resigningFirstResponder = NO;
}

- (void)_updateFontPanel
{
    // FIXME: NSTextView bails out if becoming or resigning first responder, for which it has ivar flags. Not
    // sure if we need to do something similar.
    
    if (![self _canEdit])
        return;
    
    NSWindow *window = [self window];
    // FIXME: is this first-responder check correct? What happens if a subframe is editable and is first responder?
    if ([NSApp keyWindow] != window || [window firstResponder] != self)
        return;
    
    BOOL multiple = NO;
    NSFont *font = [[self _bridge] fontForSelection:&multiple];

    // FIXME: for now, return a bogus font that distinguishes the empty selection from the non-empty
    // selection. We should be able to remove this once the rest of this code works properly.
    if (font == nil)
        font = [self _hasSelection] ? [NSFont menuFontOfSize:23] : [NSFont toolTipsFontOfSize:17];
    ASSERT(font != nil);

    NSFontManager *fm = [NSFontManager sharedFontManager];
    [fm setSelectedFont:font isMultiple:multiple];

    // FIXME: we don't keep track of selected attributes, or set them on the font panel. This
    // appears to have no effect on the UI. E.g., underlined text in Mail or TextEdit is
    // not reflected in the font panel. Maybe someday this will change.
}

- (unsigned)_delegateDragSourceActionMask
{
    ASSERT(_private->mouseDownEvent != nil);
    WebHTMLView *topHTMLView = [self _topHTMLView];
    if (self != topHTMLView) {
        [topHTMLView _setMouseDownEvent:_private->mouseDownEvent];
        unsigned result = [topHTMLView _delegateDragSourceActionMask];
        [topHTMLView _setMouseDownEvent:nil];
        return result;
    }

    WebView *webView = [self _webView];
    NSPoint point = [webView convertPoint:[_private->mouseDownEvent locationInWindow] fromView:nil];
    _private->dragSourceActionMask = [[webView _UIDelegateForwarder] webView:webView dragSourceActionMaskForPoint:point];
    return _private->dragSourceActionMask;
}

- (BOOL)_canSmartCopyOrDelete
{
    return [[self _webView] smartInsertDeleteEnabled] && [[self _bridge] selectionGranularity] == WordGranularity;
}

- (DOMRange *)_smartDeleteRangeForProposedRange:(DOMRange *)proposedRange
{
    if (proposedRange == nil || [self _canSmartCopyOrDelete] == NO)
        return nil;
    
    return [[self _bridge] smartDeleteRangeForProposedRange:proposedRange];
}

- (void)_smartInsertForString:(NSString *)pasteString replacingRange:(DOMRange *)rangeToReplace beforeString:(NSString **)beforeString afterString:(NSString **)afterString
{
    if (!pasteString || !rangeToReplace || ![[self _webView] smartInsertDeleteEnabled]) {
        if (beforeString)
            *beforeString = nil;
        if (afterString)
            *afterString = nil;
        return;
    }
    
    [[self _bridge] smartInsertForString:pasteString replacingRange:rangeToReplace beforeString:beforeString afterString:afterString];
}

- (BOOL)_textViewWasFirstResponderAtMouseDownTime:(NSTextView *)textView
{
    return textView == _private->firstResponderTextViewAtMouseDownTime;
}

- (void)_pauseNullEventsForAllNetscapePlugins
{
    NSArray *subviews = [self subviews];
    unsigned int subviewCount = [subviews count];
    unsigned int subviewIndex;
    
    for (subviewIndex = 0; subviewIndex < subviewCount; subviewIndex++) {
        NSView *subview = [subviews objectAtIndex:subviewIndex];
        if ([subview isKindOfClass:[WebBaseNetscapePluginView class]])
            [(WebBaseNetscapePluginView *)subview stopNullEvents];
    }
}

- (void)_resumeNullEventsForAllNetscapePlugins
{
    NSArray *subviews = [self subviews];
    unsigned int subviewCount = [subviews count];
    unsigned int subviewIndex;
    
    for (subviewIndex = 0; subviewIndex < subviewCount; subviewIndex++) {
        NSView *subview = [subviews objectAtIndex:subviewIndex];
        if ([subview isKindOfClass:[WebBaseNetscapePluginView class]])
            [(WebBaseNetscapePluginView *)subview restartNullEvents];
    }
}

- (void)_willMakeFirstResponderForNodeFocus
{
    _private->willBecomeFirstResponderForNodeFocus = YES;
}

- (id<WebHTMLHighlighter>)_highlighterForType:(NSString*)type
{
    return [_private->highlighters objectForKey:type];
}

- (WebFrame *)_frame
{
    return [_private->dataSource webFrame];
}

- (void)_setInitiatedDrag:(BOOL)flag
{
    _private->initiatedDrag = flag;
}

- (BOOL)_initiatedDrag
{
    return _private->initiatedDrag;
}

- (void)copy:(id)sender
{
#if USING_WEBCORE_COPY
    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->editor()->copy();
#else
    Frame* coreFrame = core([self _frame]);
    if (coreFrame && coreFrame->editor()->tryDHTMLCopy())
        return; // DHTML did the whole operation
    if (!coreFrame->editor()->canCopy()) {
        NSBeep();
        return;
    }
    [self _writeSelectionToPasteboard:[NSPasteboard generalPasteboard]];
#endif
}

- (void)cut:(id)sender
{
#if USING_WEBCORE_CUT
    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->editor()->cut();
#else
    FrameMac* coreFrame = core([self _frame]);
    if (coreFrame && coreFrame->editor()->tryDHTMLCut())
        return; // DHTML did the whole operation
    if (!coreFrame->editor()->canCut()) {
        NSBeep();
        return;
    }
    DOMRange *range = [self _selectedRange];
    if ([self _shouldDeleteRange:range]) {
        [self _writeSelectionToPasteboard:[NSPasteboard generalPasteboard]];
        if (coreFrame)
            coreFrame->editor()->deleteSelectionWithSmartDelete([self _canSmartCopyOrDelete]);
    }
#endif
}

- (void)paste:(id)sender
{
#if USING_WEBCORE_PASTE
    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->editor()->paste();
#else
    FrameMac* coreFrame = core([self _frame]);
    if (coreFrame && coreFrame->editor()->tryDHTMLPaste())
        return; // DHTML did the whole operation
    if (!coreFrame->editor()->canPaste())
        return;
    if (coreFrame && coreFrame->selectionController()->isContentRichlyEditable())
        [self _pasteWithPasteboard:[NSPasteboard generalPasteboard] allowPlainText:YES];
    else
        [self _pasteAsPlainTextWithPasteboard:[NSPasteboard generalPasteboard]];
#endif
}

- (void)pasteAsPlainText:(id)sender
{
    if (![self _canEdit])
        return;
    [self _pasteAsPlainTextWithPasteboard:[NSPasteboard generalPasteboard]];
}

#if !BUILDING_ON_TIGER

- (BOOL)isGrammarCheckingEnabled
{
    // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must implement the method here because
    // the AppKit code checks the first responder.
    return [[self _webView] isGrammarCheckingEnabled];
}

- (void)setGrammarCheckingEnabled:(BOOL)flag
{
    // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must implement the method here because
    // the AppKit code checks the first responder.
    [[self _webView] setGrammarCheckingEnabled:flag];
}

- (void)toggleGrammarChecking:(id)sender
{
    // FIXME 4799134: WebView is the bottleneck for this grammar-checking logic, but we must implement the method here because
    // the AppKit code checks the first responder.
    [[self _webView] toggleGrammarChecking:sender];
}

#endif /* !BUILDING_ON_TIGER */

- (void)_lookUpInDictionaryFromMenu:(id)sender
{
    // This should only be called when there's a selection, but play it safe.
    if (![self _hasSelection])
        return;
    
    // Soft link to dictionary-display function to avoid linking another framework (ApplicationServices/LangAnalysis)
    static bool lookedForFunction = false;
    typedef OSStatus (*ServiceWindowShowFunction)(id inWordString, NSRect inWordBoundary, UInt16 inLineDirection);
    static ServiceWindowShowFunction dictionaryServiceWindowShow;
    if (!lookedForFunction) {
        const struct mach_header *frameworkImageHeader = 0;
        dictionaryServiceWindowShow = reinterpret_cast<ServiceWindowShowFunction>(
            _NSSoftLinkingGetFrameworkFuncPtr(@"ApplicationServices", @"LangAnalysis", "_DCMDictionaryServiceWindowShow", &frameworkImageHeader));
        lookedForFunction = true;
    }
    if (!dictionaryServiceWindowShow) {
        LOG_ERROR("Couldn't find _DCMDictionaryServiceWindowShow"); 
        return;
    }

    // FIXME: must check for right-to-left here
    NSWritingDirection writingDirection = NSWritingDirectionLeftToRight;

    NSAttributedString *attrString = [self selectedAttributedString];
    // FIXME: the dictionary API expects the rect for the first line of selection. Passing
    // the rect for the entire selection, as we do here, positions the pop-up window near
    // the bottom of the selection rather than at the selected word.
    NSRect rect = [self convertRect:core([self _frame])->visibleSelectionRect() toView:nil];
    rect.origin = [[self window] convertBaseToScreen:rect.origin];
    NSData *data = [attrString RTFFromRange:NSMakeRange(0, [attrString length]) documentAttributes:nil];
    dictionaryServiceWindowShow(data, rect, (writingDirection == NSWritingDirectionRightToLeft) ? 1 : 0);
}

@end

@implementation WebHTMLView (WebNSTextInputSupport)

- (NSArray *)validAttributesForMarkedText
{
    static NSArray *validAttributes;
    if (!validAttributes)
        validAttributes = [[NSArray allocWithZone:[self zone]] initWithObjects:
            NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName,
            NSMarkedClauseSegmentAttributeName, NSTextInputReplacementRangeAttributeName, nil];
    // NSText also supports the following attributes, but it's
    // hard to tell which are really required for text input to
    // work well; I have not seen any input method make use of them yet.
    //     NSFontAttributeName, NSForegroundColorAttributeName,
    //     NSBackgroundColorAttributeName, NSLanguageAttributeName.
    return validAttributes;
}

- (WebNSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
    NSWindow *window = [self window];
    WebFrameBridge *bridge = [self _bridge];

    if (window)
        thePoint = [window convertScreenToBase:thePoint];
    thePoint = [self convertPoint:thePoint fromView:nil];

    DOMRange *range = [bridge characterRangeAtPoint:thePoint];
    if (!range)
        return NSNotFound;
    
    return [bridge convertDOMRangeToNSRange:range].location;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
{
    WebFrameBridge *bridge = [self _bridge];
    
    // Just to match NSTextView's behavior. Regression tests cannot detect this;
    // to reproduce, use a test application from http://bugzilla.opendarwin.org/show_bug.cgi?id=4682
    // (type something; try ranges (1, -1) and (2, -1).
    if ((theRange.location + theRange.length < theRange.location) && (theRange.location + theRange.length != 0))
        theRange.length = 0;
    
    DOMRange *range = [bridge convertNSRangeToDOMRange:theRange];
    if (!range)
        return NSMakeRect(0,0,0,0);
    
    ASSERT([range startContainer]);
    ASSERT([range endContainer]);
    
    NSRect resultRect = [bridge firstRectForDOMRange:range];
    resultRect = [self convertRect:resultRect toView:nil];

    NSWindow *window = [self window];
    if (window)
        resultRect.origin = [window convertBaseToScreen:resultRect.origin];
    
    return resultRect;
}

- (NSRange)selectedRange
{
    return [[self _bridge] selectedNSRange];
}

- (NSRange)markedRange
{
    if (![self hasMarkedText])
        return NSMakeRange(NSNotFound,0);
    return [[self _bridge] markedTextNSRange];
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)nsRange
{
    WebFrameBridge *bridge = [self _bridge];
    DOMRange *domRange = [bridge convertNSRangeToDOMRange:nsRange];
    if (!domRange)
        return nil;
    return [NSAttributedString _web_attributedStringFromRange:core(domRange)];
}

// test for 10.4 because of <rdar://problem/4243463>
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
- (long)conversationIdentifier
{
    return (long)self;
}
#else
- (NSInteger)conversationIdentifier
{
    return (NSInteger)self;
}
#endif

- (BOOL)hasMarkedText
{
    return [[self _bridge] markedTextDOMRange] != nil;
}

- (void)unmarkText
{
    [[self _bridge] setMarkedTextDOMRange:nil customAttributes:nil ranges:nil];
}

- (void)_selectMarkedText
{
    if ([self hasMarkedText]) {
        WebFrameBridge *bridge = [self _bridge];
        DOMRange *markedTextRange = [bridge markedTextDOMRange];
        Frame* coreFrame = core([self _frame]);
        if (coreFrame)
            selectRange(coreFrame->selectionController(), core(markedTextRange), DOWNSTREAM, false);
    }
}

- (void)_selectRangeInMarkedText:(NSRange)range
{
    ASSERT([self hasMarkedText]);

    WebFrameBridge *bridge = [self _bridge];
    DOMRange *selectedRange = [[[self _frame] DOMDocument] createRange];
    DOMRange *markedTextRange = [bridge markedTextDOMRange];
    
    ASSERT([markedTextRange startContainer] == [markedTextRange endContainer]);

    unsigned selectionStart = [markedTextRange startOffset] + range.location;
    unsigned selectionEnd = selectionStart + range.length;

    [selectedRange setStart:[markedTextRange startContainer] offset:selectionStart];
    [selectedRange setEnd:[markedTextRange startContainer] offset:selectionEnd];

    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        selectRange(coreFrame->selectionController(), core(selectedRange), DOWNSTREAM, false);
}

- (void)_extractAttributes:(NSArray **)a ranges:(NSArray **)r fromAttributedString:(NSAttributedString *)string
{
    int length = [[string string] length];
    int i = 0;
    NSMutableArray *attributes = [NSMutableArray array];
    NSMutableArray *ranges = [NSMutableArray array];
    while (i < length) {
        NSRange effectiveRange;
        NSDictionary *attrs = [string attributesAtIndex:i longestEffectiveRange:&effectiveRange inRange:NSMakeRange(i,length - i)];
        [attributes addObject:attrs];
        [ranges addObject:[NSValue valueWithRange:effectiveRange]];
        i = effectiveRange.location + effectiveRange.length;
    }
    *a = attributes;
    *r = ranges;
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
{
    WebFrameBridge *bridge = [self _bridge];

    if (![self _isEditable])
        return;

    BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]]; // Otherwise, NSString

    if (isAttributedString) {
        unsigned markedTextLength = [(NSString *)string length];
        NSString *rangeString = [string attribute:NSTextInputReplacementRangeAttributeName atIndex:0 longestEffectiveRange:NULL inRange:NSMakeRange(0, markedTextLength)];
        // The AppKit adds a 'secret' property to the string that contains the replacement
        // range.  The replacement range is the range of the the text that should be replaced
        // with the new string.
        if (rangeString)
            [[self _bridge] selectNSRange:NSRangeFromString(rangeString)];
    }

    _private->ignoreMarkedTextSelectionChange = YES;

    // if we had marked text already, we need to make sure to replace
    // that, instead of the selection/caret
    [self _selectMarkedText];

    NSString *text = string;
    NSArray *attributes = nil;
    NSArray *ranges = nil;
    if (isAttributedString) {
        text = [string string];
        [self _extractAttributes:&attributes ranges:&ranges fromAttributedString:string];
    }

    [bridge replaceMarkedTextWithText:text];
    [bridge setMarkedTextDOMRange:[self _selectedRange] customAttributes:attributes ranges:ranges];
    if ([self hasMarkedText])
        [self _selectRangeInMarkedText:newSelRange];

    _private->ignoreMarkedTextSelectionChange = NO;
}

- (void)doCommandBySelector:(SEL)aSelector
{
    WebView *webView = [self _webView];
    if (![[webView _editingDelegateForwarder] webView:webView doCommandBySelector:aSelector])
        [super doCommandBySelector:aSelector];
}

- (void)_discardMarkedText
{
    if (![self hasMarkedText])
        return;

    _private->ignoreMarkedTextSelectionChange = YES;

    [self _selectMarkedText];
    [self unmarkText];
    [[NSInputManager currentInputManager] markedTextAbandoned:self];
    // FIXME: Should we be calling the delegate here?
    if (Frame* coreFrame = core([self _frame]))
        coreFrame->editor()->deleteSelectionWithSmartDelete(false);

    _private->ignoreMarkedTextSelectionChange = NO;
}

- (void)_insertText:(NSString *)text selectInsertedText:(BOOL)selectText
{
    if (text == nil || [text length] == 0 || (![self _isEditable] && ![self hasMarkedText]))
        return;

    if (![self _shouldReplaceSelectionWithText:text givenAction:WebViewInsertActionTyped]) {
        [self _discardMarkedText];
        return;
    }

    _private->ignoreMarkedTextSelectionChange = YES;

    // If we had marked text, we replace that, instead of the selection/caret.
    [self _selectMarkedText];

    [[self _bridge] insertText:text selectInsertedText:selectText];

    _private->ignoreMarkedTextSelectionChange = NO;

    // Inserting unmarks any marked text.
    [self unmarkText];
}

- (void)insertText:(id)string
{
    // We don't yet support inserting an attributed string but input methods don't appear to require this.
    NSString *text;
    if ([string isKindOfClass:[NSAttributedString class]])
        text = [string string];
    else
        text = string;
    [self _insertText:text selectInsertedText:NO];
}

- (BOOL)_selectionIsInsideMarkedText
{
    WebFrameBridge *bridge = [self _bridge];
    DOMRange *selection = [self _selectedRange];
    DOMRange *markedTextRange = [bridge markedTextDOMRange];

    ASSERT([markedTextRange startContainer] == [markedTextRange endContainer]);

    if ([selection startContainer] != [markedTextRange startContainer]) 
        return NO;

    if ([selection endContainer] != [markedTextRange startContainer])
        return NO;

    if ([selection startOffset] < [markedTextRange startOffset])
        return NO;

    if ([selection endOffset] > [markedTextRange endOffset])
        return NO;

    return YES;
}

- (void)_updateSelectionForInputManager
{
    if (![self hasMarkedText] || _private->ignoreMarkedTextSelectionChange)
        return;

    if ([self _selectionIsInsideMarkedText]) {
        DOMRange *selection = [self _selectedRange];
        DOMRange *markedTextDOMRange = [[self _bridge] markedTextDOMRange];

        unsigned markedSelectionStart = [selection startOffset] - [markedTextDOMRange startOffset];
        unsigned markedSelectionLength = [selection endOffset] - [selection startOffset];
        NSRange newSelectionRange = NSMakeRange(markedSelectionStart, markedSelectionLength);

        [[NSInputManager currentInputManager] markedTextSelectionChanged:newSelectionRange client:self];
    } else {
        [self unmarkText];
        [[NSInputManager currentInputManager] markedTextAbandoned:self];
    }
}

@end

/*
    This class runs the show for handing the complete: NSTextView operation.  It counts on its HTML view
    to call endRevertingChange: whenever the current completion needs to be aborted.
 
    The class is in one of two modes:  PopupWindow showing, or not.  It is shown when a completion yields
    more than one match.  If a completion yields one or zero matches, it is not shown, and **there is no
    state carried across to the next completion**.
 */

@implementation WebTextCompleteController

- (id)initWithHTMLView:(WebHTMLView *)view
{
    self = [super init];
    if (!self)
        return nil;
    _view = view;
    return self;
}

- (void)dealloc
{
    [_popupWindow release];
    [_completions release];
    [_originalString release];
    [super dealloc];
}

- (void)_insertMatch:(NSString *)match
{
    // FIXME: 3769654 - We should preserve case of string being inserted, even in prefix (but then also be
    // able to revert that).  Mimic NSText.
    WebFrameBridge *bridge = [_view _bridge];
    NSString *newText = [match substringFromIndex:prefixLength];
    [bridge replaceSelectionWithText:newText selectReplacement:YES smartReplace:NO];
}

// mostly lifted from NSTextView_KeyBinding.m
- (void)_buildUI
{
    NSRect scrollFrame = NSMakeRect(0, 0, 100, 100);
    NSRect tableFrame = NSZeroRect;    
    tableFrame.size = [NSScrollView contentSizeForFrameSize:scrollFrame.size hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder];
    // Added cast to work around problem with multiple Foundation initWithIdentifier: methods with different parameter types.
    NSTableColumn *column = [(NSTableColumn *)[NSTableColumn alloc] initWithIdentifier:[NSNumber numberWithInt:0]];
    [column setWidth:tableFrame.size.width];
    [column setEditable:NO];
    
    _tableView = [[NSTableView alloc] initWithFrame:tableFrame];
    [_tableView setAutoresizingMask:NSViewWidthSizable];
    [_tableView addTableColumn:column];
    [column release];
    [_tableView setDrawsGrid:NO];
    [_tableView setCornerView:nil];
    [_tableView setHeaderView:nil];
    [_tableView setAutoresizesAllColumnsToFit:YES];
    [_tableView setDelegate:self];
    [_tableView setDataSource:self];
    [_tableView setTarget:self];
    [_tableView setDoubleAction:@selector(tableAction:)];
    
    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:scrollFrame];
    [scrollView setBorderType:NSNoBorder];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [scrollView setDocumentView:_tableView];
    [_tableView release];
    
    _popupWindow = [[NSWindow alloc] initWithContentRect:scrollFrame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    [_popupWindow setAlphaValue:0.88f];
    [_popupWindow setContentView:scrollView];
    [scrollView release];
    [_popupWindow setHasShadow:YES];
    [_popupWindow setOneShot:YES];
    //[_popupWindow _setForceActiveControls:YES];   // AK secret - no known problem from leaving this out
    [_popupWindow setReleasedWhenClosed:NO];
}

// mostly lifted from NSTextView_KeyBinding.m
- (void)_placePopupWindow:(NSPoint)topLeft
{
    int numberToShow = [_completions count];
    if (numberToShow > 20) {
        numberToShow = 20;
    }

    NSRect windowFrame;
    NSPoint wordStart = topLeft;
    windowFrame.origin = [[_view window] convertBaseToScreen:[_view convertPoint:wordStart toView:nil]];
    windowFrame.size.height = numberToShow * [_tableView rowHeight] + (numberToShow + 1) * [_tableView intercellSpacing].height;
    windowFrame.origin.y -= windowFrame.size.height;
    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont systemFontOfSize:12.0f], NSFontAttributeName, nil];
    float maxWidth = 0.0f;
    int maxIndex = -1;
    int i;
    for (i = 0; i < numberToShow; i++) {
        float width = ceilf([[_completions objectAtIndex:i] sizeWithAttributes:attributes].width);
        if (width > maxWidth) {
            maxWidth = width;
            maxIndex = i;
        }
    }
    windowFrame.size.width = 100;
    if (maxIndex >= 0) {
        maxWidth = ceilf([NSScrollView frameSizeForContentSize:NSMakeSize(maxWidth, 100.0f) hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSNoBorder].width);
        maxWidth = ceilf([NSWindow frameRectForContentRect:NSMakeRect(0.0f, 0.0f, maxWidth, 100.0f) styleMask:NSBorderlessWindowMask].size.width);
        maxWidth += 5.0f;
        windowFrame.size.width = MAX(maxWidth, windowFrame.size.width);
        maxWidth = MIN(400.0f, windowFrame.size.width);
    }
    [_popupWindow setFrame:windowFrame display:NO];
    
    [_tableView reloadData];
    [_tableView selectRow:0 byExtendingSelection:NO];
    [_tableView scrollRowToVisible:0];
    [self _reflectSelection];
    [_popupWindow setLevel:NSPopUpMenuWindowLevel];
    [_popupWindow orderFront:nil];    
    [[_view window] addChildWindow:_popupWindow ordered:NSWindowAbove];
}

- (void)doCompletion
{
    if (!_popupWindow) {
        NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
        if (!checker) {
            LOG_ERROR("No NSSpellChecker");
            return;
        }

        // Get preceeding word stem
        WebFrameBridge *bridge = [_view _bridge];
        DOMRange *selection = kit(core([_view _frame])->selectionController()->toRange().get());
        DOMRange *wholeWord = [bridge rangeByAlteringCurrentSelection:SelectionController::EXTEND
            direction:SelectionController::BACKWARD granularity:WordGranularity];
        DOMRange *prefix = [wholeWord cloneRange];
        [prefix setEnd:[selection startContainer] offset:[selection startOffset]];

        // Reject some NOP cases
        if ([prefix collapsed]) {
            NSBeep();
            return;
        }
        NSString *prefixStr = [bridge stringForRange:prefix];
        NSString *trimmedPrefix = [prefixStr stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if ([trimmedPrefix length] == 0) {
            NSBeep();
            return;
        }
        prefixLength = [prefixStr length];

        // Lookup matches
        [_completions release];
        _completions = [checker completionsForPartialWordRange:NSMakeRange(0, [prefixStr length]) inString:prefixStr language:nil inSpellDocumentWithTag:[[_view _webView] spellCheckerDocumentTag]];
        [_completions retain];
    
        if (!_completions || [_completions count] == 0) {
            NSBeep();
        } else if ([_completions count] == 1) {
            [self _insertMatch:[_completions objectAtIndex:0]];
        } else {
            ASSERT(!_originalString);       // this should only be set IFF we have a popup window
            _originalString = [[bridge stringForRange:selection] retain];
            [self _buildUI];
            NSRect wordRect = [bridge caretRectAtNode:[wholeWord startContainer] offset:[wholeWord startOffset] affinity:NSSelectionAffinityDownstream];
            // +1 to be under the word, not the caret
            // FIXME - 3769652 - Wrong positioning for right to left languages.  We should line up the upper
            // right corner with the caret instead of upper left, and the +1 would be a -1.
            NSPoint wordLowerLeft = { NSMinX(wordRect)+1, NSMaxY(wordRect) };
            [self _placePopupWindow:wordLowerLeft];
        }
    } else {
        [self endRevertingChange:YES moveLeft:NO];
    }
}

- (void)endRevertingChange:(BOOL)revertChange moveLeft:(BOOL)goLeft
{
    if (_popupWindow) {
        // tear down UI
        [[_view window] removeChildWindow:_popupWindow];
        [_popupWindow orderOut:self];
        // Must autorelease because event tracking code may be on the stack touching UI
        [_popupWindow autorelease];
        _popupWindow = nil;

        if (revertChange) {
            WebFrameBridge *bridge = [_view _bridge];
            [bridge replaceSelectionWithText:_originalString selectReplacement:YES smartReplace:NO];
        } else if (goLeft) {
            [_view moveBackward:nil];
        } else {
            [_view moveForward:nil];
        }
        [_originalString release];
        _originalString = nil;
    }
    // else there is no state to abort if the window was not up
}

// WebHTMLView gives us a crack at key events it sees.  Return whether we consumed the event.
// The features for the various keys mimic NSTextView.
- (BOOL)filterKeyDown:(NSEvent *)event
{
    if (_popupWindow) {
        NSString *string = [event charactersIgnoringModifiers];
        unichar c = [string characterAtIndex:0];
        if (c == NSUpArrowFunctionKey) {
            int selectedRow = [_tableView selectedRow];
            if (0 < selectedRow) {
                [_tableView selectRow:selectedRow-1 byExtendingSelection:NO];
                [_tableView scrollRowToVisible:selectedRow-1];
            }
            return YES;
        } else if (c == NSDownArrowFunctionKey) {
            int selectedRow = [_tableView selectedRow];
            if (selectedRow < (int)[_completions count]-1) {
                [_tableView selectRow:selectedRow+1 byExtendingSelection:NO];
                [_tableView scrollRowToVisible:selectedRow+1];
            }
            return YES;
        } else if (c == NSRightArrowFunctionKey || c == '\n' || c == '\r' || c == '\t') {
            [self endRevertingChange:NO moveLeft:NO];
            return YES;
        } else if (c == NSLeftArrowFunctionKey) {
            [self endRevertingChange:NO moveLeft:YES];
            return YES;
        } else if (c == 0x1b || c == NSF5FunctionKey) {
            [self endRevertingChange:YES moveLeft:NO];
            return YES;
        } else if (c == ' ' || ispunct(c)) {
            [self endRevertingChange:NO moveLeft:NO];
            return NO;  // let the char get inserted
        }
    }
    return NO;
}

- (void)_reflectSelection
{
    int selectedRow = [_tableView selectedRow];
    ASSERT(selectedRow >= 0 && selectedRow < (int)[_completions count]);
    [self _insertMatch:[_completions objectAtIndex:selectedRow]];
}

- (void)tableAction:(id)sender
{
    [self _reflectSelection];
    [self endRevertingChange:NO moveLeft:NO];
}

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [_completions count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
    return [_completions objectAtIndex:row];
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    [self _reflectSelection];
}

@end

@implementation WebHTMLView (WebDocumentPrivateProtocols)

- (NSRect)selectionRect
{
    if ([self _hasSelection])
        return core([self _frame])->selectionRect();
    return NSZeroRect;
}

- (NSView *)selectionView
{
    return self;
}

- (NSImage *)selectionImageForcingWhiteText:(BOOL)forceWhiteText
{
    if ([self _hasSelection])
        return core([self _frame])->selectionImage(forceWhiteText);
    return nil;
}

- (NSRect)selectionImageRect
{
    if ([self _hasSelection])
        return core([self _frame])->visibleSelectionRect();
    return NSZeroRect;
}

- (NSArray *)pasteboardTypesForSelection
{
    if ([self _canSmartCopyOrDelete]) {
        NSMutableArray *types = [[[[self class] _selectionPasteboardTypes] mutableCopy] autorelease];
        [types addObject:WebSmartPastePboardType];
        return types;
    } else {
        return [[self class] _selectionPasteboardTypes];
    }
}

- (void)writeSelectionWithPasteboardTypes:(NSArray *)types toPasteboard:(NSPasteboard *)pasteboard
{
    [self _writeSelectionWithPasteboardTypes:types toPasteboard:pasteboard cachedAttributedString:nil];
}

- (void)selectAll
{
    Frame* coreFrame = core([self _frame]);
    if (coreFrame)
        coreFrame->selectionController()->selectAll();
}

- (void)deselectAll
{
    Frame* coreFrame = core([self _frame]);
    if (!coreFrame)
        return;
    coreFrame->selectionController()->clear();
}

- (NSString *)string
{
    return [[self _bridge] stringForRange:[self _documentRange]];
}

- (NSAttributedString *)_attributeStringFromDOMRange:(DOMRange *)range
{
    NSAttributedString *attributedString;
#if !LOG_DISABLED        
    double start = CFAbsoluteTimeGetCurrent();
#endif    
    attributedString = [[[NSAttributedString alloc] _initWithDOMRange:range] autorelease];
#if !LOG_DISABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "creating attributed string from selection took %f seconds.", duration);
#endif
    return attributedString;
}

- (NSAttributedString *)attributedString
{
    DOMDocument *document = [[self _frame] DOMDocument];
    NSAttributedString *attributedString = [self _attributeStringFromDOMRange:[document _documentRange]];
    if (!attributedString) {
        Document* coreDocument = core(document);
        Range range(coreDocument, coreDocument, 0, 0, 0);
        attributedString = [NSAttributedString _web_attributedStringFromRange:&range];
    }
    return attributedString;
}

- (NSString *)selectedString
{
    return [[self _bridge] selectedString];
}

- (NSAttributedString *)selectedAttributedString
{
    NSAttributedString *attributedString = [self _attributeStringFromDOMRange:[self _selectedRange]];
    if (!attributedString) {
        FrameMac* coreFrame = core([self _frame]);
        if (coreFrame) {
            RefPtr<Range> range = coreFrame->selectionController()->selection().toRange();
            attributedString = [NSAttributedString _web_attributedStringFromRange:range.get()];
        }
    }
    return attributedString;
}

- (BOOL)supportsTextEncoding
{
    return YES;
}

@end

@implementation WebHTMLView (WebDocumentInternalProtocols)

- (BOOL)_canProcessDragWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    ASSERT([self _isTopHTMLView]);

    NSPasteboard *pasteboard = [draggingInfo draggingPasteboard];
    NSMutableSet *types = [NSMutableSet setWithArray:[pasteboard types]];
    [types intersectSet:[NSSet setWithArray:[WebHTMLView _insertablePasteboardTypes]]];
    if ([types count] == 0)
        return NO;
    if ([types count] == 1
            && [types containsObject:NSFilenamesPboardType]
            && ![self _imageExistsAtPaths:[pasteboard propertyListForType:NSFilenamesPboardType]])
        return NO;

    NSPoint point = [self convertPoint:[draggingInfo draggingLocation] fromView:nil];
    NSDictionary *element = [self elementAtPoint:point allowShadowContent:YES];
    ASSERT(element);
    WebFrame *innerFrame = (WebFrame *)[element objectForKey:WebElementFrameKey];
    ASSERT(innerFrame);
    ASSERT([innerFrame isKindOfClass:[WebFrame class]]);
    WebHTMLView* innerView = (WebHTMLView *)[[innerFrame frameView] documentView];
    if ([[element objectForKey:WebElementDOMNodeKey] isContentEditable]) {
        // Can't drag onto the selection being dragged.
        if ([innerView _initiatedDrag] && [[element objectForKey:WebElementIsSelectedKey] boolValue])
            return NO;
        return YES;
    }

    return NO;
}

- (BOOL)_isMoveDrag:(id <NSDraggingInfo>)draggingInfo
{
    FrameMac* coreFrame = core([self _frame]);
    return _private->initiatedDrag
        && coreFrame
        && coreFrame->selectionController()->isContentEditable()
        && !([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask)
        && ![[[draggingInfo draggingPasteboard] types] containsObject:NSURLPboardType];
}

- (BOOL)_isNSColorDrag:(id <NSDraggingInfo>)draggingInfo
{
    return [[[draggingInfo draggingPasteboard] types] containsObject:NSColorPboardType];
}

- (NSDragOperation)draggingUpdatedWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask
{
    ASSERT([self _isTopHTMLView]);

    NSDragOperation operation = NSDragOperationNone;
    if (actionMask & WebDragDestinationActionDHTML)
        operation = [[self _bridge] dragOperationForDraggingInfo:draggingInfo];
    _private->webCoreHandlingDrag = (operation != NSDragOperationNone);

    if ((actionMask & WebDragDestinationActionEdit) && !_private->webCoreHandlingDrag && [self _canProcessDragWithDraggingInfo:draggingInfo]) {
        if ([self _isNSColorDrag:draggingInfo])
            operation = NSDragOperationGeneric;
        else {
            WebView *webView = [self _webView];
            [webView moveDragCaretToPoint:[webView convertPoint:[draggingInfo draggingLocation] fromView:nil]];
            NSPoint point = [self convertPoint:[draggingInfo draggingLocation] fromView:nil];
            NSDictionary *element = [self elementAtPoint:point allowShadowContent:YES];
            ASSERT(element);
            WebFrame *innerFrame = (WebFrame *)[element objectForKey:WebElementFrameKey];
            ASSERT(innerFrame);
            ASSERT([innerFrame isKindOfClass:[WebFrame class]]);
            WebHTMLView* innerView = (WebHTMLView *)[[innerFrame frameView] documentView];
            operation = [innerView _isMoveDrag:draggingInfo] ? NSDragOperationMove : NSDragOperationCopy;
        }
    } else
        [[self _webView] removeDragCaret];

    return operation;
}

- (void)draggingCancelledWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    ASSERT([self _isTopHTMLView]);

    [[self _bridge] dragExitedWithDraggingInfo:draggingInfo];
    [[self _webView] removeDragCaret];
}

- (BOOL)concludeDragForDraggingInfo:(id <NSDraggingInfo>)draggingInfo actionMask:(unsigned int)actionMask
{
    ASSERT([self _isTopHTMLView]);

    WebView *webView = [self _webView];
    WebFrameBridge *bridge = [self _bridge];
    if (_private->webCoreHandlingDrag) {
        ASSERT(actionMask & WebDragDestinationActionDHTML);
        [[webView _UIDelegateForwarder] webView:webView willPerformDragDestinationAction:WebDragDestinationActionDHTML forDraggingInfo:draggingInfo];
        [bridge concludeDragForDraggingInfo:draggingInfo];
        return YES;
    }

    if (!(actionMask & WebDragDestinationActionEdit))
        return NO;

    NSPoint point = [self convertPoint:[draggingInfo draggingLocation] fromView:nil];
    NSDictionary *element = [self elementAtPoint:point allowShadowContent:YES];
    ASSERT(element);
    WebFrame *innerFrame = (WebFrame *)[element objectForKey:WebElementFrameKey];
    ASSERT(innerFrame);
    ASSERT([innerFrame isKindOfClass:[WebFrame class]]);
    WebHTMLView* innerView = (WebHTMLView *)[[innerFrame frameView] documentView];
    WebFrameBridge *innerBridge = [innerFrame _bridge];

    if ([self _isNSColorDrag:draggingInfo]) {
        NSColor *color = [NSColor colorFromPasteboard:[draggingInfo draggingPasteboard]];
        if (!color)
            return NO;
        Frame* coreFrame = core(innerFrame);
        if (!coreFrame)
            return NO;
        DOMRange *innerRange = kit(coreFrame->selectionController()->toRange().get());
        DOMCSSStyleDeclaration *style = [self _emptyStyle];
        [style setProperty:@"color" value:[self _colorAsString:color] priority:@""];
        if (![[webView _editingDelegateForwarder] webView:webView
                shouldApplyStyle:style toElementsInDOMRange:innerRange])
            return NO;
        [[webView _UIDelegateForwarder] webView:webView
            willPerformDragDestinationAction:WebDragDestinationActionEdit
            forDraggingInfo:draggingInfo];
        coreFrame->editor()->applyStyle(core(style), EditActionSetColor);
        return YES;
    }

    BOOL didInsert = NO;
    if ([self _canProcessDragWithDraggingInfo:draggingInfo]) {
        NSPasteboard *pasteboard = [draggingInfo draggingPasteboard];
        if ([innerView _isMoveDrag:draggingInfo] || [innerBridge isDragCaretRichlyEditable]) { 
            DOMRange *range = [innerBridge dragCaretDOMRange];
            BOOL chosePlainText;
            DOMDocumentFragment *fragment = [self _documentFragmentFromPasteboard:pasteboard
                inContext:range allowPlainText:YES chosePlainText:&chosePlainText];
            if (fragment && [self _shouldInsertFragment:fragment replacingDOMRange:range givenAction:WebViewInsertActionDropped]) {
                [[webView _UIDelegateForwarder] webView:webView willPerformDragDestinationAction:WebDragDestinationActionEdit forDraggingInfo:draggingInfo];
                if ([innerView _isMoveDrag:draggingInfo]) {
                    BOOL smartMove = [innerBridge selectionGranularity] == WordGranularity && [self _canSmartReplaceWithPasteboard:pasteboard];
                    [innerBridge moveSelectionToDragCaret:fragment smartMove:smartMove];
                } else {
                    [innerBridge setSelectionToDragCaret];
                    [innerBridge replaceSelectionWithFragment:fragment selectReplacement:YES smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard] matchStyle:chosePlainText];
                }
                didInsert = YES;
            }
        } else {
            NSString *text = [self _plainTextFromPasteboard:pasteboard];
            if (text && [self _shouldInsertText:text replacingDOMRange:[innerBridge dragCaretDOMRange] givenAction:WebViewInsertActionDropped]) {
                [[webView _UIDelegateForwarder] webView:webView willPerformDragDestinationAction:WebDragDestinationActionEdit forDraggingInfo:draggingInfo];
                [innerBridge setSelectionToDragCaret];
                [innerBridge replaceSelectionWithText:text selectReplacement:YES smartReplace:[self _canSmartReplaceWithPasteboard:pasteboard]];
                didInsert = YES;
            }
        }
    }
    [webView removeDragCaret];
    return didInsert;
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    return [self elementAtPoint:point allowShadowContent:NO];
}

- (NSDictionary *)elementAtPoint:(NSPoint)point allowShadowContent:(BOOL)allow;
{
    Frame* coreframe = core([self _frame]);
    if (coreframe) 
        return [[[WebElementDictionary alloc] initWithHitTestResult:coreframe->eventHandler()->hitTestResultAtPoint(IntPoint(point), allow)] autorelease];
    return nil;
}

@end
