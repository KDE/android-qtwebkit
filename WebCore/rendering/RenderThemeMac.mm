/*
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#import "config.h"
#import "RenderThemeMac.h"

#import "cssstyleselector.h"
#import "CSSValueKeywords.h"
#import "Document.h"
#import "FoundationExtras.h"
#import "FrameView.h"
#import "GraphicsContext.h"
#import "Image.h"
#import "LocalCurrentGraphicsContext.h"
#import "RenderPopupMenuMac.h"
#import "RenderView.h"
#import "WebCoreSystemInterface.h"

// The methods in this file are specific to the Mac OS X platform.

namespace WebCore {

enum {
    topMargin,
    rightMargin,
    bottomMargin,
    leftMargin
};

enum {
    topPadding,
    rightPadding,
    bottomPadding,
    leftPadding
};
    
RenderTheme* theme()
{
    static RenderThemeMac macTheme;
    return &macTheme;
}

RenderThemeMac::RenderThemeMac()
    : checkbox(nil)
    , radio(nil)
    , button(nil)
    , resizeCornerImage(0)
{
}

Color RenderThemeMac::platformActiveSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor selectedTextBackgroundColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color((int)(255 * [color redComponent]), (int)(255 * [color greenComponent]), (int)(255 * [color blueComponent]));
}

Color RenderThemeMac::platformInactiveSelectionBackgroundColor() const
{
    NSColor* color = [[NSColor secondarySelectedControlColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    return Color((int)(255 * [color redComponent]), (int)(255 * [color greenComponent]), (int)(255 * [color blueComponent]));
}

void RenderThemeMac::systemFont(int propId, FontDescription& fontDescription) const
{
    static FontDescription systemFont;
    static FontDescription smallSystemFont;
    static FontDescription menuFont;
    static FontDescription labelFont;
    static FontDescription miniControlFont;
    static FontDescription smallControlFont;
    static FontDescription controlFont;
    
    FontDescription* cachedDesc;
    NSFont* font = nil;
    switch (propId) {
        case CSS_VAL_SMALL_CAPTION:
            cachedDesc = &smallSystemFont;
            if (!smallSystemFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
            break;
        case CSS_VAL_MENU:
            cachedDesc = &menuFont;
            if (!menuFont.isAbsoluteSize())
                font = [NSFont menuFontOfSize:[NSFont systemFontSize]];
            break;
        case CSS_VAL_STATUS_BAR:
            cachedDesc = &labelFont;
            if (!labelFont.isAbsoluteSize())
                font = [NSFont labelFontOfSize:[NSFont labelFontSize]];
            break;
        case CSS_VAL__WEBKIT_MINI_CONTROL:
            cachedDesc = &miniControlFont;
            if (!miniControlFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSMiniControlSize]];
            break;
        case CSS_VAL__WEBKIT_SMALL_CONTROL:
            cachedDesc = &smallControlFont;
            if (!smallControlFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]];
            break;
        case CSS_VAL__WEBKIT_CONTROL:
            cachedDesc = &controlFont;
            if (!controlFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSRegularControlSize]];
            break;
        default:
            cachedDesc = &systemFont;
            if (!systemFont.isAbsoluteSize())
                font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
    }
    
    if (font) {
        cachedDesc->setIsAbsoluteSize(true);
        cachedDesc->setGenericFamily(FontDescription::NoFamily);
        cachedDesc->firstFamily().setFamily([font familyName]);
        cachedDesc->setSpecifiedSize([font pointSize]);
        NSFontTraitMask traits = [[NSFontManager sharedFontManager] traitsOfFont:font];
        cachedDesc->setBold(traits & NSBoldFontMask);
        cachedDesc->setItalic(traits & NSItalicFontMask);
    }
    fontDescription = *cachedDesc;
}

bool RenderThemeMac::isControlStyled(const RenderStyle* style, const BorderData& border, 
                                     const BackgroundLayer& background, const Color& backgroundColor) const
{
    if (style->appearance() == TextFieldAppearance || style->appearance() == TextAreaAppearance)
        return style->border() != border;
    return RenderTheme::isControlStyled(style, border, background, backgroundColor);
}

void RenderThemeMac::paintResizeControl(GraphicsContext* c, const IntRect& r)
{
    if (!resizeCornerImage)
        resizeCornerImage = Image::loadPlatformResource("textAreaResizeCorner");

    IntPoint imagePoint(r.right() - resizeCornerImage->width(), r.bottom() - resizeCornerImage->height());
    c->drawImage(resizeCornerImage, imagePoint);
}

void RenderThemeMac::adjustRepaintRect(const RenderObject* o, IntRect& r)
{
    switch (o->style()->appearance()) {
        case CheckboxAppearance: {
            // Since we query the prototype cell, we need to update its state to match.
            setCheckboxCellState(o, r);
        
            // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
            // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
            r = inflateRect(r, checkboxSizes()[[checkbox controlSize]], checkboxMargins());
            break;
        }
        case RadioAppearance: {
            // Since we query the prototype cell, we need to update its state to match.
            setRadioCellState(o, r);
        
            // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
            // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
            r = inflateRect(r, radioSizes()[[radio controlSize]], radioMargins());
            break;
        }
        case PushButtonAppearance:
        case ButtonAppearance: {
            // Since we query the prototype cell, we need to update its state to match.
            setButtonCellState(o, r);
        
            // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
            // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
            if ([button bezelStyle] == NSRoundedBezelStyle)
                r = inflateRect(r, buttonSizes()[[button controlSize]], buttonMargins());
            break;
        }
        case MenulistAppearance: {
            setPopupButtonCellState(o, r); 
            r = inflateRect(r, popupButtonSizes()[[popupButton controlSize]], popupButtonMargins());
            break;
        }
        default:
            break;
    }
}

IntRect RenderThemeMac::inflateRect(const IntRect& r, const IntSize& size, const int* margins) const
{
    // Only do the inflation if the available width/height are too small.  Otherwise try to
    // fit the glow/check space into the available box's width/height.
    int widthDelta = r.width() - (size.width() + margins[leftMargin] + margins[rightMargin]);
    int heightDelta = r.height() - (size.height() + margins[topMargin] + margins[bottomMargin]);
    IntRect result(r);
    if (widthDelta < 0) {
        result.setX(result.x() - margins[leftMargin]);
        result.setWidth(result.width() - widthDelta);
    }
    if (heightDelta < 0) {
        result.setY(result.y() - margins[topMargin]);
        result.setHeight(result.height() - heightDelta);
    }
    return result;
}

void RenderThemeMac::updateCheckedState(NSCell* cell, const RenderObject* o)
{
    bool oldIndeterminate = [cell state] == NSMixedState;
    bool indeterminate = isIndeterminate(o);
    bool checked = isChecked(o);
    
    if (oldIndeterminate != indeterminate) {
        [cell setState:indeterminate ? NSMixedState : (checked ? NSOnState : NSOffState)];
        return;
    }

    bool oldChecked = [cell state] == NSOnState;
    if (checked != oldChecked)
        [cell setState:checked ? NSOnState : NSOffState];
}

void RenderThemeMac::updateEnabledState(NSCell* cell, const RenderObject* o)
{
    bool oldEnabled = [cell isEnabled];
    bool enabled = isEnabled(o);
    if (enabled != oldEnabled)
        [cell setEnabled:enabled];
}

void RenderThemeMac::updateFocusedState(NSCell* cell, const RenderObject* o)
{
    // FIXME: Need to add a key window test here, or the element will look
    // focused even when in the background.
    bool oldFocused = [cell showsFirstResponder];
    bool focused = (o->element() && o->document()->focusNode() == o->element()) && (o->style()->outlineStyleIsAuto());
    if (focused != oldFocused)
        [cell setShowsFirstResponder:focused];
}

void RenderThemeMac::updatePressedState(NSCell* cell, const RenderObject* o)
{
    bool oldPressed = [cell isHighlighted];
    bool pressed = (o->element() && o->element()->active());
    if (pressed != oldPressed)
        [cell setHighlighted:pressed];
}

short RenderThemeMac::baselinePosition(const RenderObject* o) const
{
    if (o->style()->appearance() == CheckboxAppearance || o->style()->appearance() == RadioAppearance)
        return o->marginTop() + o->height() - 2; // The baseline is 2px up from the bottom of the checkbox/radio in AppKit.
    return RenderTheme::baselinePosition(o);
}

bool RenderThemeMac::controlSupportsTints(const RenderObject* o) const
{
    if (!isEnabled(o))
        return false;
    
    // Checkboxes only have tint when checked.
    if (o->style()->appearance() == CheckboxAppearance)
        return isChecked(o);
    
    // For now assume other controls have tint if enabled.
    return true;
}

NSControlSize RenderThemeMac::controlSizeForFont(RenderStyle* style) const
{
    int fontSize = style->fontSize();
    if (fontSize >= 16)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

void RenderThemeMac::setControlSize(NSCell* cell, const IntSize* sizes, const IntSize& minSize)
{
    NSControlSize size;
    if (minSize.width() >= sizes[NSRegularControlSize].width() &&
        minSize.height() >= sizes[NSRegularControlSize].height())
        size = NSRegularControlSize;
    else if (minSize.width() >= sizes[NSSmallControlSize].width() &&
             minSize.height() >= sizes[NSSmallControlSize].height())
        size = NSSmallControlSize;
    else
        size = NSMiniControlSize;
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:size];
}

IntSize RenderThemeMac::sizeForFont(RenderStyle* style, const IntSize* sizes) const
{
    return sizes[controlSizeForFont(style)];
}

void RenderThemeMac::setSizeFromFont(RenderStyle* style, const IntSize* sizes) const
{
    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
    IntSize size = sizeForFont(style, sizes);
    if (style->width().isIntrinsicOrAuto() && size.width() > 0)
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto() && size.height() > 0)
        style->setHeight(Length(size.height(), Fixed));
}

void RenderThemeMac::setFontFromControlSize(CSSStyleSelector* selector, RenderStyle* style, NSControlSize controlSize) const
{
    FontDescription fontDescription;
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::SerifFamily);
    
    NSFont* font = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSize]];
    fontDescription.firstFamily().setFamily([font familyName]);
    fontDescription.setComputedSize([font pointSize]);
    fontDescription.setSpecifiedSize([font pointSize]);

    if (style->setFontDescription(fontDescription))
        style->font().update();
}

bool RenderThemeMac::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    // Determine the width and height needed for the control and prepare the cell for painting.
    setCheckboxCellState(o, r);
    
    // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
    // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
    IntRect inflatedRect = inflateRect(r, checkboxSizes()[[checkbox controlSize]], checkboxMargins());
    [checkbox drawWithFrame:NSRect(inflatedRect) inView:o->view()->frameView()->getDocumentView()];
    [checkbox setControlView: nil];
    
    return false;
}

const IntSize* RenderThemeMac::checkboxSizes() const
{
    static const IntSize sizes[3] = { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10) };
    return sizes;
}

const int* RenderThemeMac::checkboxMargins() const
{
    static const int margins[3][4] = 
    {
        { 3, 4, 4, 2 },
        { 4, 3, 3, 3 },
        { 4, 3, 3, 3 },
    };
    return margins[[checkbox controlSize]];
}

void RenderThemeMac::setCheckboxCellState(const RenderObject* o, const IntRect& r)
{
    if (!checkbox) {
        checkbox = HardRetainWithNSRelease([[NSButtonCell alloc] init]);
        [checkbox setButtonType:NSSwitchButton];
        [checkbox setTitle:nil];
        [checkbox setAllowsMixedState:YES];
    }
    
    // Set the control size based off the rectangle we're painting into.
    setControlSize(checkbox, checkboxSizes(), r.size());
    
    // Update the various states we respond to.
    updateCheckedState(checkbox, o);
    updateEnabledState(checkbox, o);
    updatePressedState(checkbox, o);
    updateFocusedState(checkbox, o);
}

void RenderThemeMac::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;
    
    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, checkboxSizes());
}

bool RenderThemeMac::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    // Determine the width and height needed for the control and prepare the cell for painting.
    setRadioCellState(o, r);
    
    // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
    // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
    IntRect inflatedRect = inflateRect(r, radioSizes()[[radio controlSize]], radioMargins());
    [radio drawWithFrame:NSRect(inflatedRect) inView:o->view()->frameView()->getDocumentView()];
    [radio setControlView: nil];
    
    return false;
}

const IntSize* RenderThemeMac::radioSizes() const
{
    static const IntSize sizes[3] = { IntSize(14, 15), IntSize(12, 13), IntSize(10, 10) };
    return sizes;
}

const int* RenderThemeMac::radioMargins() const
{
    static const int margins[3][4] = 
    {
        { 2, 2, 4, 2 },
        { 3, 2, 3, 2 },
        { 1, 0, 2, 0 },
    };
    return margins[[radio controlSize]];
}

void RenderThemeMac::setRadioCellState(const RenderObject* o, const IntRect& r)
{
    if (!radio) {
        radio = HardRetainWithNSRelease([[NSButtonCell alloc] init]);
        [radio setButtonType:NSRadioButton];
        [radio setTitle:nil];
    }
    
    // Set the control size based off the rectangle we're painting into.
    setControlSize(radio, radioSizes(), r.size());
    
    // Update the various states we respond to.
    updateCheckedState(radio, o);
    updateEnabledState(radio, o);
    updatePressedState(radio, o);
    updateFocusedState(radio, o);
}


void RenderThemeMac::setRadioSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;
    
    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, radioSizes());
}

void RenderThemeMac::setButtonPaddingFromControlSize(RenderStyle* style, NSControlSize size) const
{
    // Just use 8px.  AppKit wants to use 11px for mini buttons, but that padding is just too large
    // for real-world Web sites (creating a huge necessary minimum width for buttons whose space is
    // by definition constrained, since we select mini only for small cramped environments.
    // This also guarantees the HTML4 <button> will match our rendering by default, since we're using a consistent
    // padding.
    const int padding = 8;
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setPaddingTop(Length(0, Fixed));
    style->setPaddingBottom(Length(0, Fixed));
}

void RenderThemeMac::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    // There are three appearance constants for buttons.
    // (1) Push-button is the constant for the default Aqua system button.  Push buttons will not scale vertically and will not allow
    // custom fonts or colors.  <input>s use this constant.  This button will allow custom colors and font weights/variants but won't
    // scale vertically.
    // (2) square-button is the constant for the square button.  This button will allow custom fonts and colors and will scale vertically.
    // (3) Button is the constant that means "pick the best button as appropriate."  <button>s use this constant.  This button will
    // also scale vertically and allow custom fonts and colors.  It will attempt to use Aqua if possible and will make this determination
    // solely on the rectangle of the control.
    
    // Determine our control size based off our font.
    NSControlSize controlSize = controlSizeForFont(style);

    if (style->appearance() == PushButtonAppearance) {
        // Ditch the border.
        style->resetBorder();

        // Height is locked to auto.
        style->setHeight(Length(Auto));
        
        // White-space is locked to pre
        style->setWhiteSpace(PRE);

        // Set the button's vertical size.
        setButtonSize(style);

        // Add in the padding that we'd like to use.
        setButtonPaddingFromControlSize(style, controlSize);

        // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
        // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
        // system font for the control size instead.
        setFontFromControlSize(selector, style, controlSize);
    } else {
        // Set a min-height so that we can't get smaller than the mini button.
        style->setMinHeight(Length(15, Fixed));
        
        // Reset the top and bottom borders.
        style->resetBorderTop();
        style->resetBorderBottom();
    }
}

const IntSize* RenderThemeMac::buttonSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
    return sizes;
}

const int* RenderThemeMac::buttonMargins() const
{
    static const int margins[3][4] = 
    {
        { 4, 6, 7, 6 },
        { 4, 5, 6, 5 },
        { 0, 1, 1, 1 },
    };
    return margins[[button controlSize]];
}

void RenderThemeMac::setButtonSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;
    
    // Use the font size to determine the intrinsic width of the control.
    setSizeFromFont(style, buttonSizes());
}

void RenderThemeMac::setButtonCellState(const RenderObject* o, const IntRect& r)
{
    if (!button) {
        button = HardRetainWithNSRelease([[NSButtonCell alloc] init]);
        [button setTitle:nil];
        [button setButtonType:NSMomentaryPushInButton];
    }

    // Set the control size based off the rectangle we're painting into.
    if (o->style()->appearance() == SquareButtonAppearance ||
        r.height() > buttonSizes()[NSRegularControlSize].height()) {
        // Use the square button
        if ([button bezelStyle] != NSShadowlessSquareBezelStyle)
            [button setBezelStyle:NSShadowlessSquareBezelStyle];
    } else if ([button bezelStyle] != NSRoundedBezelStyle)
        [button setBezelStyle:NSRoundedBezelStyle];
            
    setControlSize(button, buttonSizes(), r.size());
    
    // Update the various states we respond to.
    updateCheckedState(button, o);
    updateEnabledState(button, o);
    updatePressedState(button, o);
    updateFocusedState(button, o);
}

bool RenderThemeMac::paintButton(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext LocalCurrentGraphicsContext(paintInfo.p);

    // Determine the width and height needed for the control and prepare the cell for painting.
    setButtonCellState(o, r);
    
    // We inflate the rect as needed to account for padding included in the cell to accommodate the button
    // shadow.  We don't consider this part of the bounds of the control in WebKit.
    IntSize size = buttonSizes()[[button controlSize]];
    size.setWidth(r.width());
    IntRect inflatedRect = r;
    if ([button bezelStyle] == NSRoundedBezelStyle) {
        // Center the button within the available space.
        if (inflatedRect.height() > size.height()) {
            inflatedRect.setY(inflatedRect.y() + (inflatedRect.height() - size.height()) / 2);
            inflatedRect.setHeight(size.height());
        }
        
        // Now inflate it to account for the shadow.
        inflatedRect = inflateRect(inflatedRect, size, buttonMargins());
    }

    [button drawWithFrame:NSRect(inflatedRect) inView:o->view()->frameView()->getDocumentView()];
    [button setControlView:nil];

    return false;
}

bool RenderThemeMac::paintTextField(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext LocalCurrentGraphicsContext(paintInfo.p);
    wkDrawBezeledTextFieldCell(r, isEnabled(o) && !isReadOnlyControl(o));
    return false;
}

void RenderThemeMac::adjustTextFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

bool RenderThemeMac::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& paintInfo, const IntRect& r)
{
    LocalCurrentGraphicsContext LocalCurrentGraphicsContext(paintInfo.p);
    wkDrawBezeledTextArea(r, isEnabled(o) && !isReadOnlyControl(o));
    return false;
}

void RenderThemeMac::adjustTextAreaStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
}

const int* RenderThemeMac::popupButtonMargins() const
{
    static const int margins[3][4] = 
    {
        { 0, 3, 1, 3 },
        { 0, 3, 2, 3 },
        { 0, 1, 0, 1 }
    };
    return margins[[popupButton controlSize]];
}

const IntSize* RenderThemeMac::popupButtonSizes() const
{
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
    return sizes;
}

const int* RenderThemeMac::popupButtonPadding(NSControlSize size) const
{
    static const int padding[3][4] = 
    {
        { 2, 26, 3, 8 },
        { 2, 23, 3, 8 },
        { 2, 22, 3, 10 }
    };
    return padding[size];
}

void RenderThemeMac::setPopupPaddingFromControlSize(RenderStyle* style, NSControlSize size) const
{
    style->setPaddingLeft(Length(popupButtonPadding(size)[leftPadding], Fixed));
    style->setPaddingRight(Length(popupButtonPadding(size)[rightPadding], Fixed));
    style->setPaddingTop(Length(popupButtonPadding(size)[topPadding], Fixed));
    style->setPaddingBottom(Length(popupButtonPadding(size)[bottomPadding], Fixed));
}

bool RenderThemeMac::paintMenuList(RenderObject* o, const RenderObject::PaintInfo&, const IntRect& r)
{
    setPopupButtonCellState(o, r);
    
    IntRect inflatedRect = r;
    IntSize size = popupButtonSizes()[[popupButton controlSize]];
    size.setWidth(r.width());

    // Now inflate it to account for the shadow.
    inflatedRect = inflateRect(inflatedRect, size, popupButtonMargins());
        
    [popupButton drawWithFrame:inflatedRect inView:o->view()->frameView()->getDocumentView()];
    [popupButton setControlView:nil];
    return false;
}

const float baseFontSize = 11.0;
const float baseArrowHeight = 4.0;
const float baseArrowWidth = 5.0;
const float baseSpaceBetweenArrows = 2.0;
const int arrowPaddingLeft = 6;
const int arrowPaddingRight = 6;
const int paddingBeforeSeparator = 4;
const int baseBorderRadius = 5;
const int styledPopupPaddingLeft = 8;
const int styledPopupPaddingTop = 1;
const int styledPopupPaddingBottom = 2;

static void TopGradientInterpolate( void *info, const CGFloat *inData, CGFloat *outData )
{
    static float dark[4] = { 1, 1, 1, 0.4 };
    static float light[4] = { 1, 1, 1, 0.15 };
    float a = inData[0];
    int i = 0;
    for( i = 0; i < 4; i++ )
        outData[i] = ( 1.0 - a ) * dark[i] + a * light[i];
}

static void BottomGradientInterpolate( void *info, const CGFloat *inData, CGFloat *outData )
{
    static float dark[4] = { 1, 1, 1, 0 };
    static float light[4] = { 1, 1, 1, 0.3 };
    float a = inData[0];
    int i = 0;
    for( i = 0; i < 4; i++ )
        outData[i] = ( 1.0 - a ) * dark[i] + a * light[i];
}

static void MainGradientInterpolate( void *info, const CGFloat *inData, CGFloat *outData )
{
    static float dark[4] = { 0, 0, 0, 0.15 };
    static float light[4] = { 0, 0, 0, 0 };
    float a = inData[0];
    int i = 0;
    for( i = 0; i < 4; i++ )
        outData[i] = ( 1.0 - a ) * dark[i] + a * light[i];
}

void RenderThemeMac::paintMenuListButtonGradients(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    CGContextRef context = i.p->platformContext();
        
    i.p->save();

    int radius = o->style()->borderTopLeftRadius().width();
    
    CGColorSpaceRef cspace = CGColorSpaceCreateDeviceRGB();    

    FloatRect topGradient(r.x(), r.y(), r.width(), r.height() / 2.0);
    struct CGFunctionCallbacks topCallbacks = { 0, TopGradientInterpolate, NULL };
    CGFunctionRef topFunction = CGFunctionCreate( NULL, 1, NULL, 4, NULL, &topCallbacks );
    CGShadingRef topShading = CGShadingCreateAxial( cspace, CGPointMake(topGradient.x(),  topGradient.y()), 
                            CGPointMake(topGradient.x(), topGradient.bottom()), topFunction, false, false );

    FloatRect bottomGradient(r.x() + radius, r.y() + r.height() / 2.0, r.width() - 2 * radius, r.height() / 2.0);
    struct CGFunctionCallbacks bottomCallbacks = { 0, BottomGradientInterpolate, NULL };
    CGFunctionRef bottomFunction = CGFunctionCreate( NULL, 1, NULL, 4, NULL, &bottomCallbacks );
    CGShadingRef bottomShading = CGShadingCreateAxial( cspace, CGPointMake(bottomGradient.x(),  bottomGradient.y()), 
                            CGPointMake(bottomGradient.x(), bottomGradient.bottom()), bottomFunction, false, false );

    struct CGFunctionCallbacks mainCallbacks = { 0, MainGradientInterpolate, NULL };
    CGFunctionRef mainFunction = CGFunctionCreate( NULL, 1, NULL, 4, NULL, &mainCallbacks );
    CGShadingRef mainShading = CGShadingCreateAxial( cspace, CGPointMake(r.x(),  r.y()), 
                            CGPointMake(r.x(), r.bottom()), mainFunction, false, false );

    CGShadingRef leftShading = CGShadingCreateAxial( cspace, CGPointMake(r.x(),  r.y()), 
                            CGPointMake(r.x() + radius, r.y()), mainFunction, false, false );                        

    CGShadingRef rightShading = CGShadingCreateAxial( cspace, CGPointMake(r.right(),  r.y()), 
                            CGPointMake(r.right() - radius, r.y()), mainFunction, false, false );                             
    i.p->save();
    CGContextClipToRect(context, r);
    i.p->addRoundedRectClip(r, 
        o->style()->borderTopLeftRadius(), o->style()->borderTopRightRadius(),
        o->style()->borderBottomLeftRadius(), o->style()->borderBottomRightRadius());
    CGContextDrawShading(context, mainShading);  
    i.p->restore();      

    i.p->save();
    CGContextClipToRect(context, topGradient);
    i.p->addRoundedRectClip(enclosingIntRect(topGradient), 
        o->style()->borderTopLeftRadius(), o->style()->borderTopRightRadius(),
        IntSize(), IntSize());
    CGContextDrawShading(context, topShading);  
    i.p->restore();      

    i.p->save();
    CGContextClipToRect(context, bottomGradient);
    i.p->addRoundedRectClip(enclosingIntRect(bottomGradient), 
        IntSize(), IntSize(),
        o->style()->borderBottomLeftRadius(), o->style()->borderBottomRightRadius());
    CGContextDrawShading(context, bottomShading); 
    i.p->restore();
    
    i.p->save();
    CGContextClipToRect(context, r);
    i.p->addRoundedRectClip(r, 
        o->style()->borderTopLeftRadius(), o->style()->borderTopRightRadius(),
        o->style()->borderBottomLeftRadius(), o->style()->borderBottomRightRadius());
    CGContextDrawShading(context, leftShading);
    CGContextDrawShading(context, rightShading);
    i.p->restore();
    
    i.p->restore();
}

bool RenderThemeMac::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{        
    i.p->save();
        
    IntRect bounds = IntRect(r.x() + o->style()->borderLeftWidth(), 
                             r.y() + o->style()->borderTopWidth(), 
                             r.width() - o->style()->borderLeftWidth() - o->style()->borderRightWidth(),
                             r.height() - o->style()->borderTopWidth() - o->style()->borderBottomWidth());
    // Draw the gradients to give the styled popup menu a button appearance
    paintMenuListButtonGradients(o, i, bounds);
                
    float fontScale = o->style()->fontSize() / baseFontSize;
    float centerY = bounds.y() + bounds.height() / 2.0;
    float arrowHeight = baseArrowHeight * fontScale;
    float arrowWidth = baseArrowWidth * fontScale;    
    float leftEdge = bounds.right() - arrowPaddingRight - arrowWidth;
    float spaceBetweenArrows = baseSpaceBetweenArrows * fontScale;
    
    i.p->setFillColor(o->style()->color());
    i.p->setPen(Pen(o->style()->color()));
    
    FloatPoint arrow1[3];
    arrow1[0] = FloatPoint(leftEdge, centerY - spaceBetweenArrows / 2.0);
    arrow1[1] = FloatPoint(leftEdge + arrowWidth, centerY - spaceBetweenArrows / 2.0);
    arrow1[2] = FloatPoint(leftEdge + arrowWidth / 2.0, centerY - spaceBetweenArrows / 2.0 - arrowHeight);

    // Draw the top arrow
    i.p->drawConvexPolygon(3, arrow1, true);

    FloatPoint arrow2[3];
    arrow2[0] = FloatPoint(leftEdge, centerY + spaceBetweenArrows / 2.0);
    arrow2[1] = FloatPoint(leftEdge + arrowWidth, centerY + spaceBetweenArrows / 2.0);
    arrow2[2] = FloatPoint(leftEdge + arrowWidth / 2.0, centerY + spaceBetweenArrows / 2.0 + arrowHeight);   
    
    // Draw the bottom arrow
    i.p->drawConvexPolygon(3, arrow2, true); 
    
    Color leftSeparatorColor(0, 0, 0, 40);
    Color rightSeparatorColor(255, 255, 255, 40);
    int separatorSpace = 2;
    int leftEdgeOfSeparator = int(leftEdge - arrowPaddingLeft); // FIXME: Round?
    
    // Draw the separator to the left of the arrows
    i.p->setPen(Pen(leftSeparatorColor));
    i.p->drawLine(IntPoint(leftEdgeOfSeparator, bounds.y()), IntPoint(leftEdgeOfSeparator, bounds.bottom()));

    i.p->setPen(Pen(rightSeparatorColor));
    i.p->drawLine(IntPoint(leftEdgeOfSeparator + separatorSpace, bounds.y()), IntPoint(leftEdgeOfSeparator + separatorSpace, bounds.bottom()));

    i.p->restore();
    return false;
}

void RenderThemeMac::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    NSControlSize controlSize = controlSizeForFont(style);

    style->resetBorder();
    
    // Height is locked to auto.
    style->setHeight(Length(Auto));
    
    // White-space is locked to pre
    style->setWhiteSpace(PRE);

    // Set the foreground color to black when we have the aqua look
    style->setColor(Color::black);

    // Set the button's vertical size.
    setButtonSize(style);

    // Add in the padding that we'd like to use.
    setPopupPaddingFromControlSize(style, controlSize);

    // Our font is locked to the appropriate system font size for the control.  To clarify, we first use the CSS-specified font to figure out
    // a reasonable control size, but once that control size is determined, we throw that font away and use the appropriate
    // system font for the control size instead.
    setFontFromControlSize(selector, style, controlSize);
}

void RenderThemeMac::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{        
    float fontScale = style->fontSize() / baseFontSize;
    float arrowWidth = baseArrowWidth * fontScale;
    
    // We're overriding the padding to allow for the arrow control.  WinIE doesn't honor padding on selects, so
    // this shouldn't cause problems on the web.  If IE7 changes that, we should reconsider this.
    style->setPaddingLeft(Length(styledPopupPaddingLeft, Fixed));
    style->setPaddingRight(Length(int(ceilf(arrowWidth + arrowPaddingLeft + arrowPaddingRight + paddingBeforeSeparator)), Fixed));
    style->setPaddingTop(Length(styledPopupPaddingTop, Fixed));
    style->setPaddingBottom(Length(styledPopupPaddingBottom, Fixed));
    
    if (style->hasBorderRadius())
        style->setBorderRadius(IntSize(int(baseBorderRadius + fontScale - 1), int(baseBorderRadius + fontScale - 1))); // FIXME: Round up?
    
    const int minHeight = 15;
    style->setMinHeight(Length(minHeight, Fixed));
}

void RenderThemeMac::setPopupButtonCellState(const RenderObject* o, const IntRect& r)
{
    if (!popupButton) {
        popupButton = HardRetainWithNSRelease([[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO]);
        [popupButton setUsesItemFromMenu:NO];
    }

    // Set the control size based off the rectangle we're painting into.
    setControlSize(popupButton, popupButtonSizes(), r.size());

    // Update the various states we respond to.
    updateCheckedState(popupButton, o);
    updateEnabledState(popupButton, o);
    updatePressedState(popupButton, o);
    updateFocusedState(popupButton, o);
}

RenderPopupMenu* RenderThemeMac::createPopupMenu(RenderArena* arena, Document* doc, RenderMenuList* menuList)
{
    return new (arena) RenderPopupMenuMac(doc, menuList);
}

int RenderThemeMac::minimumMenuListSize(RenderStyle* style) const
{
    int fontSize = style->fontSize();
    if (fontSize >= 13)
        return 9;
    if (fontSize >= 11)
        return 5;
    return 0;
}

}
