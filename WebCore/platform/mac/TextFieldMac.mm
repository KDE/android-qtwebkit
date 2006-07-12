/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "TextField.h"

#import "Color.h"
#import "IntSize.h"
#import "BlockExceptions.h"
#import "Font.h"
#import "Logging.h"
#import "WebCoreTextField.h"
#import "WebCoreFrameBridge.h"
#import "FontData.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "WebCoreViewFactory.h"
#import "WidgetClient.h"

@interface NSSearchFieldCell (SearchFieldSecrets)
- (void)_addStringToRecentSearches:(NSString *)string;
@end

namespace WebCore {

NSControlSize ControlSizeForFont(const Font& f)
{
    const int fontSize = f.pixelSize();
    if (fontSize >= 16)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

TextField::TextField(Type type)
    : m_type(type)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    id view = nil;
    switch (type) {
        case Normal:
            view = [WebCoreTextField alloc];
            break;
        case Password:
            view = [WebCoreSecureTextField alloc];
            break;
        case Search:
            view = [WebCoreSearchField alloc];
            break;
    }
    ASSERT(view);
    [view initWithWidget:this];
    m_controller = [view controller];
    setView((NSView *)view);
    [view release];
    [view setSelectable:YES]; // must do this explicitly so setEditable:NO does not make it NO
    END_BLOCK_OBJC_EXCEPTIONS;
}

TextField::~TextField()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_controller detachWidget];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void TextField::setCursorPosition(int pos)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSRange tempRange = {pos, 0}; // 4213314
    [m_controller setSelectedRange:tempRange];
    END_BLOCK_OBJC_EXCEPTIONS;
}

int TextField::cursorPosition() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [m_controller selectedRange].location;
    END_BLOCK_OBJC_EXCEPTIONS;
    return 0;
}

void TextField::setFont(const Font &font)
{
    Widget::setFont(font);
    if (m_type == Search) {
        const NSControlSize size = ControlSizeForFont(font);    
        NSControl * const searchField = static_cast<NSControl *>(getView());
        [[searchField cell] setControlSize:size];
        [searchField setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
    }
    else {
        NSTextField *textField = (NSTextField *)getView();
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [textField setFont:font.getNSFont()];
        END_BLOCK_OBJC_EXCEPTIONS;
    }
}

void TextField::setColors(const Color& background, const Color& foreground)
{
    NSTextField *textField = (NSTextField *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // Below we've added a special case that maps any completely transparent color to white.  This is a workaround for the following
    // AppKit problems: <rdar://problem/3142730> and <rdar://problem/3036580>.  Without this special case we have black
    // backgrounds on some text fields as described in <rdar://problem/3854383>.  Text fields will still not be able to display
    // transparent and translucent backgrounds, which will need to be fixed in the future.  See  <rdar://problem/3865114>.
        
    [textField setTextColor:nsColor(foreground)];

    Color bg = background;
    if (!bg.isValid() || bg.alpha() == 0)
        bg = Color::white;
    [textField setBackgroundColor:nsColor(bg)];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void TextField::setText(const String& s)
{
    NSTextField *textField = (NSTextField *)getView();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textField setStringValue:s];
    END_BLOCK_OBJC_EXCEPTIONS;
}

String TextField::text() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return String([m_controller string]);
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

void TextField::setMaxLength(int len)
{
    [m_controller setMaximumLength:len];
}

bool TextField::isReadOnly() const
{
    NSTextField *textField = (NSTextField *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return ![textField isEditable];
    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

void TextField::setReadOnly(bool flag)
{
    NSTextField *textField = (NSTextField *)getView();
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [textField setEditable:!flag];
    END_BLOCK_OBJC_EXCEPTIONS;
}

int TextField::maxLength() const
{
    return [m_controller maximumLength];
}


void TextField::selectAll()
{
    if (!hasFocus()) {
        // Do the makeFirstResponder ourselves explicitly (by calling setFocus)
        // so WebHTMLView will know it's programmatic and not the user clicking.
        setFocus();
    } else {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        NSTextField *textField = (NSTextField *)getView();
        [textField selectText:nil];
        END_BLOCK_OBJC_EXCEPTIONS;
    }
}

int TextField::selectionStart() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([m_controller hasSelection]) {
        return [m_controller selectedRange].location;
    }
    END_BLOCK_OBJC_EXCEPTIONS;
    return -1;
}

String TextField::selectedText() const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSRange range = [m_controller selectedRange];
    NSString *str = [m_controller string];
    return [str substringWithRange:range];
    END_BLOCK_OBJC_EXCEPTIONS;
    return String();
}

void TextField::setSelection(int start, int length)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSRange tempRange = {start, length}; // 4213314
    [m_controller setSelectedRange:tempRange];
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool TextField::hasSelectedText() const
{
    return [m_controller hasSelection];
}

bool TextField::edited() const
{
    return [m_controller edited];
}

void TextField::setEdited(bool flag)
{
    [m_controller setEdited:flag];
}

static const NSSize textFieldMargins = { 8, 6 };

IntSize TextField::sizeForCharacterWidth(int numCharacters) const
{
    // Figure out how big a text field needs to be for a given number of characters
    // (using "0" as the nominal character).

    NSTextField *textField = (NSTextField *)getView();

    ASSERT(numCharacters > 0);

    // We empirically determined these dimensions.
    // It would be better to get this info from AppKit somehow, but bug 3711080 shows we can't yet.
    // Note: baselinePosition below also has the height computation.
    NSSize size = textFieldMargins;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    RenderWidget *client = static_cast<RenderWidget *>(Widget::client());
    FontPlatformData font([textField font], client->view()->printingMode());
    Font renderer(font);

    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    size.height += [layoutManager defaultLineHeightForFont:font.font];
    [layoutManager release];

    TextStyle style;
    style.disableRoundingHacks();

    const UniChar zero = '0';
    TextRun run(&zero, 1);

    size.width += ceilf(renderer.floatWidth(run, style) * numCharacters);

    return IntSize(size);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntSize(0, 0);
}

int TextField::baselinePosition(int height) const
{
    NSTextField *textField = (NSTextField *)getView();

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSFont *font = [textField font];
    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    float lineHeight = [layoutManager defaultLineHeightForFont:font];
    [layoutManager release];
    NSRect bounds = NSMakeRect(0, 0, 100, textFieldMargins.height + lineHeight); // bounds width is arbitrary, height same as what sizeForCharacterWidth returns
    NSRect drawingRect = [[textField cell] drawingRectForBounds:bounds];
    return static_cast<int>(ceilf(drawingRect.origin.y - bounds.origin.y + lineHeight + [font descender]));
    END_BLOCK_OBJC_EXCEPTIONS;

    return 0;
}

void TextField::setAlignment(HorizontalAlignment alignment)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSTextField *textField = (NSTextField *)getView();
    [textField setAlignment:TextAlignment(alignment)];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void TextField::setWritingDirection(TextDirection direction)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [m_controller setBaseWritingDirection:(direction == RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight)];
    END_BLOCK_OBJC_EXCEPTIONS;
}

Widget::FocusPolicy TextField::focusPolicy() const
{
    FocusPolicy policy = Widget::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
}

bool TextField::checksDescendantsForFocus() const
{
    return true;
}

NSTextAlignment TextAlignment(HorizontalAlignment a)
{
    switch (a) {
        case AlignLeft:
            return NSLeftTextAlignment;
        case AlignRight:
            return NSRightTextAlignment;
        case AlignHCenter:
            return NSCenterTextAlignment;
    }
    LOG_ERROR("unsupported alignment");
    return NSLeftTextAlignment;
}

void TextField::setLiveSearch(bool liveSearch)
{
    if (m_type != Search)
        return;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    [[searchField cell] setSendsWholeSearchString:!liveSearch];
}

void TextField::setAutoSaveName(const String& name)
{
    if (m_type != Search)
        return;
    
    String autosave;
    if (!name.isEmpty())
        autosave = "com.apple.WebKit.searchField:" + name;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    [searchField setRecentsAutosaveName:autosave];
}

void TextField::setMaxResults(int maxResults)
{
    if (m_type != Search)
        return;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    id searchCell = [searchField cell];
    if (maxResults == -1) {
        [searchCell setSearchButtonCell:nil];
        [searchCell setSearchMenuTemplate:nil];
    }
    else {
        NSMenu* cellMenu = [searchCell searchMenuTemplate];
        NSButtonCell* buttonCell = [searchCell searchButtonCell];
        if (!buttonCell)
            [searchCell resetSearchButtonCell];
        if (cellMenu && !maxResults)
            [searchCell setSearchMenuTemplate:nil];
        else if (!cellMenu && maxResults)
            [searchCell setSearchMenuTemplate:[[WebCoreViewFactory sharedFactory] cellMenuForSearchField]];
    }
    
    [searchCell setMaximumRecents:maxResults];
}

void TextField::setPlaceholderString(const String& placeholder)
{
    NSTextField *textField = (NSTextField *)getView();
    [[textField cell] setPlaceholderString:placeholder];
}

void TextField::addSearchResult()
{
    if (m_type != Search)
        return;
    
    NSSearchField *searchField = (NSSearchField *)getView();
    [[searchField cell] _addStringToRecentSearches:[searchField stringValue]];
}

}
