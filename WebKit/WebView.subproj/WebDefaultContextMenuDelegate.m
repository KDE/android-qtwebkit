/*
      WebDefaultContextMenuDelegate.m
      Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDefaultContextMenuDelegate.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/DOM.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebDOMOperations.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebLocalizableStrings.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <WebCore/WebCoreBridge.h>

#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURLRequestPrivate.h>

@implementation WebDefaultUIDelegate (WebContextMenu)

- (NSMenuItem *)menuItemWithTag:(int)tag
{
    NSMenuItem *menuItem = [[[NSMenuItem alloc] init] autorelease];
    [menuItem setTag:tag];
    
    NSString *title = nil;
    SEL action = NULL;
    
    switch(tag) {
        case WebMenuItemTagOpenLinkInNewWindow:
            title = UI_STRING("Open Link in New Window", "Open in New Window context menu item");
            action = @selector(openLinkInNewWindow:);
            [menuItem setTarget:self];
            break;
        case WebMenuItemTagDownloadLinkToDisk:
            title = UI_STRING("Download Linked File", "Download Linked File context menu item");
            action = @selector(downloadLinkToDisk:);
            [menuItem setTarget:self];
            break;
        case WebMenuItemTagCopyLinkToClipboard:
            title = UI_STRING("Copy Link", "Copy Link context menu item");
            action = @selector(copyLinkToClipboard:);
            [menuItem setTarget:self];
            break;
        case WebMenuItemTagOpenImageInNewWindow:
            title = UI_STRING("Open Image in New Window", "Open Image in New Window context menu item");
            action = @selector(openImageInNewWindow:);
            [menuItem setTarget:self];
            break;
        case WebMenuItemTagDownloadImageToDisk:
            title = UI_STRING("Download Image", "Download Image context menu item");
            action = @selector(downloadImageToDisk:);
            [menuItem setTarget:self];
            break;
        case WebMenuItemTagCopyImageToClipboard:
            title = UI_STRING("Copy Image", "Copy Image context menu item");
            action = @selector(copyImageToClipboard:);
            [menuItem setTarget:self];
            break;
        case WebMenuItemTagOpenFrameInNewWindow:
            title = UI_STRING("Open Frame in New Window", "Open Frame in New Window context menu item");
            action = @selector(openFrameInNewWindow:);
            [menuItem setTarget:self];
            break;
        case WebMenuItemTagCopy:
            title = UI_STRING("Copy", "Copy context menu item");
            action = @selector(copy:);
            break;
        case WebMenuItemTagGoBack:
            title = UI_STRING("Back", "Back context menu item");
            action = @selector(goBack:);
            break;
        case WebMenuItemTagGoForward:
            title = UI_STRING("Forward", "Forward context menu item");
            action = @selector(goForward:);
            break;
        case WebMenuItemTagStop:
            title = UI_STRING("Stop", "Stop context menu item");
            action = @selector(stopLoading:);
            break;
        case WebMenuItemTagReload:
            title = UI_STRING("Reload", "Reload context menu item");
            action = @selector(reload:);
            break;
        case WebMenuItemTagCut:
            title = UI_STRING("Cut", "Cut context menu item");
            action = @selector(cut:);
            break;
        case WebMenuItemTagPaste:
            title = UI_STRING("Paste", "Paste context menu item");
            action = @selector(paste:);
            break;
        case WebMenuItemTagSpellingGuess:
            action = @selector(_changeSpellingFromMenu:);
            break;
        case WebMenuItemTagNoGuessesFound:
            title = UI_STRING("No Guesses Found", "No Guesses Found context menu item");
            break;
        case WebMenuItemTagIgnoreSpelling:
            title = UI_STRING("Ignore Spelling", "Ignore Spelling context menu item");
            action = @selector(_ignoreSpellingFromMenu:);
            break;
        case WebMenuItemTagLearnSpelling:
            title = UI_STRING("Learn Spelling", "Learn Spelling context menu item");
            action = @selector(_learnSpellingFromMenu:);
            break;
        default:
            return nil;
    }

    if (title != nil) {
        [menuItem setTitle:title];
    }
    [menuItem setAction:action];
    
    return menuItem;
}

- (NSArray *)contextMenuItemsForElement:(NSDictionary *)element
{
    NSMutableArray *menuItems = [NSMutableArray array];
    
    NSURL *linkURL = [element objectForKey:WebElementLinkURLKey];
    
    if (linkURL) {
        if([WebView _canHandleRequest:[NSURLRequest requestWithURL:linkURL]]) {
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagOpenLinkInNewWindow]];
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagDownloadLinkToDisk]];
        }
        [menuItems addObject:[self menuItemWithTag:WebMenuItemTagCopyLinkToClipboard]];
    }
    
    WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
    NSURL *imageURL = [element objectForKey:WebElementImageURLKey];
    
    if (imageURL) {
        if (linkURL) {
            [menuItems addObject:[NSMenuItem separatorItem]];
        }
        [menuItems addObject:[self menuItemWithTag:WebMenuItemTagOpenImageInNewWindow]];
        [menuItems addObject:[self menuItemWithTag:WebMenuItemTagDownloadImageToDisk]];
        if ([imageURL isFileURL] || [[webFrame dataSource] _fileWrapperForURL:imageURL]) {
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagCopyImageToClipboard]];
        }
    }
    
    if (!imageURL && !linkURL) {
        if ([[element objectForKey:WebElementIsSelectedKey] boolValue]) {
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagCopy]];
        } else {
            WebView *wv = [webFrame webView];
            if ([wv canGoBack]) {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagGoBack]];
            }
            if ([wv canGoForward]) {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagGoForward]];
            }
            if ([wv isLoading]) {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagStop]];
            } else {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagReload]];
            }
            
            if (webFrame != [wv mainFrame]) {
                [menuItems addObject:[self menuItemWithTag:WebMenuItemTagOpenFrameInNewWindow]];
            }
        }
    }
    
    // Attach element as the represented object to each item.
    [menuItems makeObjectsPerformSelector:@selector(setRepresentedObject:) withObject:element];
    
    return menuItems;
}

- (NSArray *)editingContextMenuItemsForElement:(NSDictionary *)element
{
    NSMenuItem *item;
    NSMutableArray *menuItems = [NSMutableArray array];
    WebHTMLView *HTMLView = (WebHTMLView *)[[[element objectForKey:WebElementFrameKey] frameView] documentView];
    ASSERT([HTMLView isKindOfClass:[WebHTMLView class]]);
    
    // Add spelling related context menu items.
    if ([HTMLView _isSelectionMisspelled]) {
        NSArray *guesses = [HTMLView _guessesForMisspelledSelection];
        unsigned count = [guesses count];
        if (count > 0) {
            NSEnumerator *enumerator = [guesses objectEnumerator];
            NSString *guess;
            while ((guess = [enumerator nextObject]) != nil) {
                item = [self menuItemWithTag:WebMenuItemTagSpellingGuess];
                [item setTitle:guess];
                [menuItems addObject:item];
            }
        } else {
            [menuItems addObject:[self menuItemWithTag:WebMenuItemTagNoGuessesFound]];
        }
        [menuItems addObject:[NSMenuItem separatorItem]];
        [menuItems addObject:[self menuItemWithTag:WebMenuItemTagIgnoreSpelling]];
        [menuItems addObject:[self menuItemWithTag:WebMenuItemTagLearnSpelling]];
        [menuItems addObject:[NSMenuItem separatorItem]];
    }
    
    // Load our NSTextView-like context menu nib.
    if (defaultMenu == nil) {
        static NSNib *textViewMenuNib = nil;
        if (textViewMenuNib == nil) {
            textViewMenuNib = [[NSNib alloc] initWithNibNamed:@"WebViewEditingContextMenu" bundle:[NSBundle bundleForClass:[self class]]];
        }
        [textViewMenuNib instantiateNibWithOwner:self topLevelObjects:nil];
    }
    
    // Add tags to the menu items because this is what the WebUIDelegate expects.
    NSEnumerator *enumerator = [[defaultMenu itemArray] objectEnumerator];
    while ((item = [enumerator nextObject]) != nil) {
        item = [item copy];
        SEL action = [item action];
        int tag;
        if (action == @selector(cut:)) {
            tag = WebMenuItemTagCut;
        } else if (action == @selector(copy:)) {
            tag = WebMenuItemTagCopy;
        } else if (action == @selector(paste:)) {
            tag = WebMenuItemTagPaste;
        } else {
            tag = WebMenuItemTagOther;
        }
        [item setTag:tag];
        [menuItems addObject:item];
        [item release];
    }
    
    return menuItems;
}

- (NSArray *)webView:(WebView *)wv contextMenuItemsForElement:(NSDictionary *)element  defaultMenuItems:(NSArray *)defaultMenuItems
{
    WebHTMLView *HTMLView = (WebHTMLView *)[[[element objectForKey:WebElementFrameKey] frameView] documentView];
    if ([HTMLView isKindOfClass:[WebHTMLView class]] && [HTMLView _isEditable]) {
        return [self editingContextMenuItemsForElement:element];
    } else {
        return [self contextMenuItemsForElement:element];
    }
}

- (void)openNewWindowWithURL:(NSURL *)URL element:(NSDictionary *)element
{
    WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
    WebView *webView = [webFrame webView];
    
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL];
    NSString *referrer = [[webFrame _bridge] referrer];
    if (referrer) {
	[request setHTTPReferrer:referrer];
    }
    
    [webView _openNewWindowWithRequest:request];
}

- (void)downloadURL:(NSURL *)URL element:(NSDictionary *)element
{
    WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
    WebView *webView = [webFrame webView];
    [webView _downloadURL:URL];
}

- (void)openLinkInNewWindow:(id)sender
{
    NSDictionary *element = [sender representedObject];
    [self openNewWindowWithURL:[element objectForKey:WebElementLinkURLKey] element:element];
}

- (void)downloadLinkToDisk:(id)sender
{
    NSDictionary *element = [sender representedObject];
    [self downloadURL:[element objectForKey:WebElementLinkURLKey] element:element];
}

- (void)copyLinkToClipboard:(id)sender
{
    NSDictionary *element = [sender representedObject];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *types = [NSPasteboard _web_writableTypesForURL];
    [pasteboard declareTypes:types owner:self];    
    [[[element objectForKey:WebElementFrameKey] webView] _writeLinkElement:element 
                                                       withPasteboardTypes:types
                                                              toPasteboard:pasteboard];
}

- (void)openImageInNewWindow:(id)sender
{
    NSDictionary *element = [sender representedObject];
    [self openNewWindowWithURL:[element objectForKey:WebElementImageURLKey] element:element];
}

- (void)downloadImageToDisk:(id)sender
{
    NSDictionary *element = [sender representedObject];
    [self downloadURL:[element objectForKey:WebElementImageURLKey] element:element];
}

- (void)copyImageToClipboard:(id)sender
{
    NSDictionary *element = [sender representedObject];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *types = [NSPasteboard _web_writableTypesForImage];
    [pasteboard declareTypes:types owner:self];
    [[[element objectForKey:WebElementFrameKey] webView] _writeImageElement:element 
                                                        withPasteboardTypes:types 
                                                               toPasteboard:pasteboard];
}

- (void)openFrameInNewWindow:(id)sender
{
    NSDictionary *element = [sender representedObject];
    WebFrame *webFrame = [element objectForKey:WebElementFrameKey];
    [self openNewWindowWithURL:[[webFrame dataSource] _URL] element:element];
}

@end
