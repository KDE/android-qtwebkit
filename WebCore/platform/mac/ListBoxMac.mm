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

#import "config.h"
#import "ListBox.h"

#import "BlockExceptions.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "FontData.h"
#import "WebCoreWidgetHolder.h"
#import "WidgetClient.h"
#import <wtf/Assertions.h>

#import "GraphicsContext.h"

using namespace WebCore;

const int minLines = 4; /* ensures we have a scroll bar */
const float bottomMargin = 1;
const float leftMargin = 2;
const float rightMargin = 2;

@interface WebCoreListBoxScrollView : NSScrollView <WebCoreWidgetHolder>
@end

@interface WebCoreTableView : NSTableView <WebCoreWidgetHolder>
{
@public
    ListBox *_box;
    BOOL processingMouseEvent;
    BOOL clickedDuringMouseEvent;
    BOOL inNextValidKeyView;
    NSWritingDirection _direction;
    BOOL isSystemFont;
    UCTypeSelectRef typeSelectSelector;
}
- (id)initWithListBox:(ListBox *)b;
- (void)detach;
- (void)_webcore_setKeyboardFocusRingNeedsDisplay;
- (Widget *)widget;
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;
- (void)fontChanged;
@end

static Font* itemScreenRenderer = 0;
static Font* itemPrinterRenderer = 0;
static Font* groupLabelScreenRenderer = 0;
static Font* groupLabelPrinterRenderer = 0;

static NSFont *itemFont()
{
    static NSFont *font = [[NSFont systemFontOfSize:[NSFont smallSystemFontSize]] retain];
    return font;
}

static Font* itemTextRenderer()
{
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        if (itemScreenRenderer == nil) {
            FontPlatformData font(itemFont());
            itemScreenRenderer = new Font(font);
        }
        return itemScreenRenderer;
    } else {
        if (itemPrinterRenderer == nil) {
            FontPlatformData font(itemFont(), true);
            itemPrinterRenderer = new Font(font);
        }
        return itemPrinterRenderer;
    }
}

static Font* groupLabelTextRenderer()
{
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        if (groupLabelScreenRenderer == nil) {
            FontPlatformData font([NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]]);
            groupLabelScreenRenderer = new Font(font);
        }
        return groupLabelScreenRenderer;
    } else {
        if (groupLabelPrinterRenderer == nil) {
            FontPlatformData font([NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]], true);
            groupLabelPrinterRenderer = new Font(font);
        }
        return groupLabelPrinterRenderer;
    }
}

ListBox::ListBox()
    : _changingSelection(false)
    , _enabled(true)
    , _widthGood(false)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSScrollView *scrollView = [[WebCoreListBoxScrollView alloc] initWithFrame:NSZeroRect];
    setView(scrollView);
    [scrollView release];
    
    [scrollView setBorderType:NSBezelBorder];
    [scrollView setHasVerticalScroller:YES];
    [[scrollView verticalScroller] setControlSize:NSSmallControlSize];

    // Another element might overlap this one, so we have to do the slower-style scrolling.
    [[scrollView contentView] setCopiesOnScroll:NO];
    
    // In WebHTMLView, we set a clip. This is not typical to do in an
    // NSView, and while correct for any one invocation of drawRect:,
    // it causes some bad problems if that clip is cached between calls.
    // The cached graphics state, which clip views keep around, does
    // cache the clip in this undesirable way. Consequently, we want to 
    // release the GState for all clip views for all views contained in 
    // a WebHTMLView. Here we do it for list boxes used in forms.
    // See these bugs for more information:
    // <rdar://problem/3226083>: REGRESSION (Panther): white box overlaying select lists at nvidia.com drivers page
    [[scrollView contentView] releaseGState];

    WebCoreTableView *tableView = [[WebCoreTableView alloc] initWithListBox:this];
    [scrollView setDocumentView:tableView];
    [tableView release];
    [scrollView setVerticalLineScroll:[tableView rowHeight]];
        
    END_BLOCK_OBJC_EXCEPTIONS;
}

ListBox::~ListBox()
{
    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    WebCoreTableView *tableView = [scrollView documentView];
    [tableView detach];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ListBox::clear()
{
    _items.clear();
    _widthGood = false;
}

void ListBox::setSelectionMode(SelectionMode mode)
{
    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    NSTableView *tableView = [scrollView documentView];
    [tableView setAllowsMultipleSelection:mode != Single];
    END_BLOCK_OBJC_EXCEPTIONS;
}

void ListBox::appendItem(const DeprecatedString &text, ListBoxItemType type, bool enabled)
{
    _items.append(ListBoxItem(text, type, enabled));
    _widthGood = false;
}

void ListBox::doneAppendingItems()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    NSTableView *tableView = [scrollView documentView];
    [tableView reloadData];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void ListBox::setSelected(int index, bool selectIt)
{
    ASSERT(index >= 0);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    NSTableView *tableView = [scrollView documentView];
    _changingSelection = true;
    if (selectIt) {
        [tableView selectRow:index byExtendingSelection:[tableView allowsMultipleSelection]];
        [tableView scrollRowToVisible:index];
    } else {
        [tableView deselectRow:index];
    }

    END_BLOCK_OBJC_EXCEPTIONS;

    _changingSelection = false;
}

bool ListBox::isSelected(int index) const
{
    ASSERT(index >= 0);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    NSTableView *tableView = [scrollView documentView];
    return [tableView isRowSelected:index]; 

    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

void ListBox::setEnabled(bool enabled)
{
    if (enabled != _enabled) {
        // You would think this would work, but not until AppKit bug 2177792 is fixed.
        //BEGIN_BLOCK_OBJC_EXCEPTIONS;
        //NSTableView *tableView = [(NSScrollView *)getView() documentView];
        //[tableView setEnabled:enabled];
        //END_BLOCK_OBJC_EXCEPTIONS;

        _enabled = enabled;

        NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
        NSTableView *tableView = [scrollView documentView];
        [tableView reloadData];
    }
}

bool ListBox::isEnabled()
{
    return _enabled;
}

IntSize ListBox::sizeForNumberOfLines(int lines) const
{
    NSSize size = {0,0};

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    WebCoreTableView *tableView = [scrollView documentView];
    
    if (!_widthGood) {
        float width = 0;
        DeprecatedValueListConstIterator<ListBoxItem> i = const_cast<const DeprecatedValueList<ListBoxItem> &>(_items).begin();
        DeprecatedValueListConstIterator<ListBoxItem> e = const_cast<const DeprecatedValueList<ListBoxItem> &>(_items).end();
        if (i != e) {
            WebCore::TextStyle style;
            style.disableRoundingHacks();
            style.setRTL([tableView baseWritingDirection] == NSWritingDirectionRightToLeft);

            const Font* renderer;
            const Font* groupLabelRenderer;
            
            bool needToDeleteLabel = false;
            if (tableView->isSystemFont) {        
                renderer = itemTextRenderer();
                groupLabelRenderer = groupLabelTextRenderer();
            } else {
                renderer = &font();
                FontDescription boldDesc = font().fontDescription();
                boldDesc.setWeight(cBoldWeight);
                groupLabelRenderer = new Font(boldDesc, font().letterSpacing(), font().wordSpacing());
                groupLabelRenderer->update();
                needToDeleteLabel = true;
            }
            
            do {
                const DeprecatedString &s = (*i).string;
                TextRun run(reinterpret_cast<const UniChar *>(s.unicode()), s.length(), 0, s.length());
                float textWidth = (((*i).type == ListBoxGroupLabel) ? groupLabelRenderer : renderer)->floatWidth(run, style);
                width = max(width, textWidth);
                ++i;
            
            } while (i != e);
            
            if (needToDeleteLabel)
                delete groupLabelRenderer;
        }
        _width = ceilf(width);
        _widthGood = true;
    }
    
    NSSize contentSize = { _width, [tableView rowHeight] * MAX(minLines, lines) };
    size = [NSScrollView frameSizeForContentSize:contentSize hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSBezelBorder];
    size.width += [NSScroller scrollerWidthForControlSize:NSSmallControlSize] - [NSScroller scrollerWidth] + leftMargin + rightMargin;

    return IntSize(size);
    END_BLOCK_OBJC_EXCEPTIONS;
    return IntSize(0, 0);
}

Widget::FocusPolicy ListBox::focusPolicy() const
{
    FocusPolicy policy = Widget::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
}

bool ListBox::checksDescendantsForFocus() const
{
    return true;
}

void ListBox::setWritingDirection(TextDirection d)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    WebCoreTableView *tableView = [scrollView documentView];
    NSWritingDirection direction = d == RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    if ([tableView baseWritingDirection] != direction) {
        [tableView setBaseWritingDirection:direction];
        [tableView reloadData];
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

void ListBox::clearCachedTextRenderers()
{
    delete itemScreenRenderer;
    itemScreenRenderer = 0;
    
    delete itemPrinterRenderer;
    itemPrinterRenderer = 0;
    
    delete groupLabelScreenRenderer;
    groupLabelScreenRenderer = 0;
    
    delete groupLabelPrinterRenderer;
    groupLabelPrinterRenderer = 0;
}

void ListBox::setFont(const Font& font)
{
    Widget::setFont(font);

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    WebCoreTableView *tableView = [scrollView documentView];
    [tableView fontChanged];
}

@implementation WebCoreListBoxScrollView

- (Widget *)widget
{
    WebCoreTableView *tableView = [self documentView];
    
    assert([tableView isKindOfClass:[WebCoreTableView class]]);
    
    return [tableView widget];
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    NSTableColumn *column = [[[self documentView] tableColumns] objectAtIndex:0];
    [column setWidth:[self contentSize].width];
    [column setMinWidth:[self contentSize].width];
    [column setMaxWidth:[self contentSize].width];
}

- (BOOL)becomeFirstResponder
{
    WebCoreTableView *documentView = [self documentView];
    Widget *widget = [documentView widget];
    [FrameMac::bridgeForWidget(widget) makeFirstResponder:documentView];
    return YES;
}

- (BOOL)autoforwardsScrollWheelEvents
{
    return YES;
}

@end

static Boolean ListBoxTypeSelectCallback(UInt32 index, void *listDataPtr, void *refcon, CFStringRef *outString, UCTypeSelectOptions *tsOptions)
{
    WebCoreTableView *self = static_cast<WebCoreTableView *>(refcon);   
    ListBox *box = static_cast<ListBox *>([self widget]);
    
    if (!box)
        return false;
    
    if (index > box->count())
        return false;
    
    if (outString)
        *outString = box->itemAtIndex(index).string.getCFString();
    
    if (tsOptions)
        *tsOptions = kUCTSOptionsNoneMask;
    
    return true;
}

@implementation WebCoreTableView

- (id)initWithListBox:(ListBox *)b
{
    [super init];

    _box = b;

    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:nil];

    [column setEditable:NO];

    [self addTableColumn:column];

    [column release];
    
    [self setAllowsMultipleSelection:NO];
    [self setHeaderView:nil];
    [self setIntercellSpacing:NSMakeSize(0, 0)];
    
    [self setDataSource:self];
    [self setDelegate:self];

    return self;
}

- (void)finalize
{
    if (typeSelectSelector)
        UCTypeSelectReleaseSelector(&typeSelectSelector);
    
    [super finalize];
}

- (void)dealloc
{
    if (typeSelectSelector)
        UCTypeSelectReleaseSelector(&typeSelectSelector);
    
    [super dealloc];
}

- (void)detach
{
    _box = 0;
    [self setDelegate:nil];
    [self setDataSource:nil];
}

- (void)mouseDown:(NSEvent *)event
{
    if (!_box) {
        [super mouseDown:event];
        return;
    }

    processingMouseEvent = YES;
    NSView *outerView = [_box->getOuterView() retain];
    Widget::beforeMouseDown(outerView);
    [super mouseDown:event];
    Widget::afterMouseDown(outerView);
    [outerView release];
    processingMouseEvent = NO;

    if (clickedDuringMouseEvent) {
    clickedDuringMouseEvent = false;
    } else if (_box) {
    _box->sendConsumedMouseUp();
    }
}

- (void)keyDown:(NSEvent *)event
{
    if (!_box)  {
        return;
    }
    WebCoreFrameBridge *bridge = FrameMac::bridgeForWidget(_box);
    if (![bridge interceptKeyEvent:event toView:self]) {
    [super keyDown:event];
    }
}

- (void)keyUp:(NSEvent *)event
{
    if (!_box)  {
        return;
    }
    
    WebCoreFrameBridge *bridge = FrameMac::bridgeForWidget(_box);
    if (![bridge interceptKeyEvent:event toView:self]) {
        [super keyUp:event];
        NSString *string = [event characters];
       
        if ([string length] == 0)
           return;
       
        // type select should work with any graphic character as defined in D13a of the unicode standard.
        const uint32_t graphicCharacterMask = U_GC_L_MASK | U_GC_M_MASK | U_GC_N_MASK | U_GC_P_MASK | U_GC_S_MASK | U_GC_ZS_MASK;
        unichar pressedCharacter = [string characterAtIndex:0];
        
        if (!(U_GET_GC_MASK(pressedCharacter) & graphicCharacterMask)) {
            if (typeSelectSelector)
                UCTypeSelectFlushSelectorData(typeSelectSelector);
            return;            
        }
        
        OSStatus err = noErr;
        if (!typeSelectSelector)
            err = UCTypeSelectCreateSelector(0, 0, kUCCollateStandardOptions, &typeSelectSelector);
        
        if (err || !typeSelectSelector)
            return;
        
        Boolean updateSelector = false;
        // the timestamp and what the AddKey function want for time are the same thing.
        err = UCTypeSelectAddKeyToSelector(typeSelectSelector, (CFStringRef)string, [event timestamp], &updateSelector);
        
        if (err || !updateSelector)
            return;
  
        UInt32 closestItem = 0;
        
        err = UCTypeSelectFindItem(typeSelectSelector, [self numberOfRowsInTableView:self], 0, self, ListBoxTypeSelectCallback, &closestItem); 

        if (err)
            return;
        
        [self selectRowIndexes:[NSIndexSet indexSetWithIndex:closestItem] byExtendingSelection:NO];
        [self scrollRowToVisible:closestItem];
        
    }
}

- (BOOL)becomeFirstResponder
{
    if (!_box) {
        return NO;
    }

    BOOL become = [super becomeFirstResponder];
    
    if (become) {
        if (_box && _box->client() && !FrameMac::currentEventIsMouseDownInWidget(_box))
            _box->client()->scrollToVisible(_box);
        [self _webcore_setKeyboardFocusRingNeedsDisplay];
        if (_box && _box->client()) {
            _box->client()->focusIn(_box);
            [FrameMac::bridgeForWidget(_box) formControlIsBecomingFirstResponder:self];
        }
    }

    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign && _box && _box->client()) {
        _box->client()->focusOut(_box);
        [FrameMac::bridgeForWidget(_box) formControlIsResigningFirstResponder:self];
    }
    return resign;
}

- (BOOL)canBecomeKeyView
{
    // Simplified method from NSView; overridden to replace NSView's way of checking
    // for full keyboard access with ours.
    return ([self window] != nil) && ![self isHiddenOrHasHiddenAncestor] && [self acceptsFirstResponder];
}

- (NSView *)nextKeyView
{
    return _box && inNextValidKeyView
        ? FrameMac::nextKeyViewForWidget(_box, SelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
    return _box && inNextValidKeyView
        ? FrameMac::nextKeyViewForWidget(_box, SelectingPrevious)
        : [super previousKeyView];
}

- (NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _box ? _box->count() : 0;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)column row:(int)row
{
    return nil;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    if (_box && _box->client())
        _box->client()->selectionChanged(_box);
    if (_box && !_box->changingSelection()) {
        if (processingMouseEvent) {
            clickedDuringMouseEvent = true;
            _box->sendConsumedMouseUp();
        }
        if (_box && _box->client())
            _box->client()->clicked(_box);
    }
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(int)row
{
    if (!_box)
        return NO;
    
    const ListBoxItem &item = _box->itemAtIndex(row);
    
    return item.type == ListBoxOption && item.enabled;
}

- (BOOL)selectionShouldChangeInTableView:(NSTableView *)aTableView
{
    return _box && _box->isEnabled();
}

- (void)drawRow:(int)row clipRect:(NSRect)clipRect
{
    if (!_box) {
        return;
    }

    const ListBoxItem &item = _box->itemAtIndex(row);

    NSColor *color;
    if (_box->isEnabled() && item.enabled) {
        if ([self isRowSelected:row] && [[self window] firstResponder] == self && ([[self window] isKeyWindow] || ![[self window] canBecomeKeyWindow])) {
            color = [NSColor alternateSelectedControlTextColor];
        } else {
            color = [NSColor controlTextColor];
        }
    } else {
        color = [NSColor disabledControlTextColor];
    }

    bool rtl = _direction == NSWritingDirectionRightToLeft;

    bool deleteRenderer = false;
    const Font* renderer;
    if (isSystemFont) {
        renderer = (item.type == ListBoxGroupLabel) ? groupLabelTextRenderer() : itemTextRenderer();
    } else {
        if (item.type == ListBoxGroupLabel) {
            deleteRenderer = true;
            FontDescription boldDesc = _box->font().fontDescription();
            boldDesc.setWeight(cBoldWeight);
            renderer = new Font(boldDesc, _box->font().letterSpacing(), _box->font().wordSpacing());
            renderer->update();
        }
        else
            renderer = &_box->font();
    }
   
    WebCore::TextStyle style;
    style.setRTL(rtl);
    style.disableRoundingHacks();

    TextRun run(reinterpret_cast<const UniChar *>(item.string.unicode()), item.string.length());
    
    NSRect cellRect = [self frameOfCellAtColumn:0 row:row];
    FloatPoint point;
    if (!rtl)
        point.setX(NSMinX(cellRect) + leftMargin);
    else
        point.setX(NSMaxX(cellRect) - rightMargin - renderer->floatWidth(run, style));
    point.setY(NSMaxY(cellRect) - renderer->descent() - bottomMargin);

    CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    GraphicsContext graphicsContext(context);
    CGFloat red, green, blue, alpha;
    [[color colorUsingColorSpaceName:NSDeviceRGBColorSpace] getRed:&red green:&green blue:&blue alpha:&alpha];
    graphicsContext.setPen(makeRGBA((int)(red * 255), (int)(green * 255), (int)(blue * 255), (int)(alpha * 255)));
    renderer->drawText(&graphicsContext, run, style, point);
}

- (void)_webcore_setKeyboardFocusRingNeedsDisplay
{
    [self setKeyboardFocusRingNeedsDisplayInRect:[self bounds]];
}

- (Widget *)widget
{
    return _box;
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction
{
    _direction = direction;
}

- (NSWritingDirection)baseWritingDirection
{
    return _direction;
}

- (void)fontChanged
{
    NSFont *font = _box->font().getNSFont();
    isSystemFont = [[font fontName] isEqualToString:[itemFont() fontName]] && [font pointSize] == [itemFont() pointSize];
    [self setRowHeight:ceilf([font ascender] - [font descender] + bottomMargin)];
    [self setNeedsDisplay:YES];
}

@end
