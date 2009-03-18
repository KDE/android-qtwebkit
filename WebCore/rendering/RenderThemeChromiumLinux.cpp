/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008, 2009 Google Inc.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderThemeChromiumLinux.h"

#include "ChromiumBridge.h"
#include "CSSValueKeywords.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "RenderObject.h"
#include "ScrollbarTheme.h"
#include "TransformationMatrix.h"
#include "UserAgentStyleSheets.h"

#include "SkShader.h"
#include "SkGradientShader.h"

namespace WebCore {

enum PaddingType {
    TopPadding,
    RightPadding,
    BottomPadding,
    LeftPadding
};

static const int styledMenuListInternalPadding[4] = { 1, 4, 1, 4 };

// The default variable-width font size.  We use this as the default font
// size for the "system font", and as a base size (which we then shrink) for
// form control fonts.
static const float DefaultFontSize = 16.0;

static bool supportsFocus(ControlPart appearance)
{
    // This causes WebKit to draw the focus rings for us.
    return false;
}

static void setSizeIfAuto(RenderStyle* style, const IntSize& size)
{
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(size.height(), Fixed));
}

// We aim to match IE here.
// -IE uses a font based on the encoding as the default font for form controls.
// -Gecko uses MS Shell Dlg (actually calls GetStockObject(DEFAULT_GUI_FONT),
// which returns MS Shell Dlg)
// -Safari uses Lucida Grande.
//
// FIXME: The only case where we know we don't match IE is for ANSI encodings.
// IE uses MS Shell Dlg there, which we render incorrectly at certain pixel
// sizes (e.g. 15px). So, for now we just use Arial.
static const char* defaultGUIFont(Document* document)
{
    return "Arial";
}

RenderTheme* theme()
{
    static RenderThemeChromiumLinux theme;
    return &theme;
}

RenderThemeChromiumLinux::RenderThemeChromiumLinux()
{
}

// Use the Windows style sheets to match their metrics.
String RenderThemeChromiumLinux::extraDefaultStyleSheet()
{
    return String(themeChromiumWinUserAgentStyleSheet, sizeof(themeChromiumWinUserAgentStyleSheet));
}

String RenderThemeChromiumLinux::extraQuirksStyleSheet()
{
    return String(themeWinQuirksUserAgentStyleSheet, sizeof(themeWinQuirksUserAgentStyleSheet));
}

bool RenderThemeChromiumLinux::supportsFocusRing(const RenderStyle* style) const
{
    return supportsFocus(style->appearance());
}

Color RenderThemeChromiumLinux::platformActiveSelectionBackgroundColor() const
{
    return Color(0x1e, 0x90, 0xff);
}

Color RenderThemeChromiumLinux::platformInactiveSelectionBackgroundColor() const
{
    return Color(0xc8, 0xc8, 0xc8);
}

Color RenderThemeChromiumLinux::platformActiveSelectionForegroundColor() const
{
    return Color(0, 0, 0);
}

Color RenderThemeChromiumLinux::platformInactiveSelectionForegroundColor() const
{
    return Color(0x32, 0x32, 0x32);
}

Color RenderThemeChromiumLinux::platformTextSearchHighlightColor() const
{
    return Color(0xff, 0xff, 0x96);
}

double RenderThemeChromiumLinux::caretBlinkInterval() const
{
    // Disable the blinking caret in layout test mode, as it introduces
    // a race condition for the pixel tests. http://b/1198440
    if (ChromiumBridge::layoutTestMode())
        return 0;

    // We cache the interval so we don't have to repeatedly request it from gtk.
    return 0.5;
}

void RenderThemeChromiumLinux::systemFont(int propId, Document* document, FontDescription& fontDescription) const
{
    float fontSize = DefaultFontSize;

    switch (propId) {
    case CSSValueWebkitMiniControl:
    case CSSValueWebkitSmallControl:
    case CSSValueWebkitControl:
        // Why 2 points smaller? Because that's what Gecko does. Note that we
        // are assuming a 96dpi screen, which is the default that we use on
        // Windows.
        static const float pointsPerInch = 72.0f;
        static const float pixelsPerInch = 96.0f;
        fontSize -= (2.0f / pointsPerInch) * pixelsPerInch;
        break;
    }

    fontDescription.firstFamily().setFamily(defaultGUIFont(NULL));
    fontDescription.setSpecifiedSize(fontSize);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setGenericFamily(FontDescription::NoFamily);
    fontDescription.setWeight(FontWeightNormal);
    fontDescription.setItalic(false);
}

int RenderThemeChromiumLinux::minimumMenuListSize(RenderStyle* style) const
{
    return 0;
}

bool RenderThemeChromiumLinux::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    static Image* const checkedImage = Image::loadPlatformResource("linuxCheckboxOn").releaseRef();
    static Image* const uncheckedImage = Image::loadPlatformResource("linuxCheckboxOff").releaseRef();

    Image* image = this->isChecked(o) ? checkedImage : uncheckedImage;
    i.context->drawImage(image, rect);
    return false;
}

void RenderThemeChromiumLinux::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME:  A hard-coded size of 13 is used.  This is wrong but necessary
    // for now.  It matches Firefox.  At different DPI settings on Windows,
    // querying the theme gives you a larger size that accounts for the higher
    // DPI.  Until our entire engine honors a DPI setting other than 96, we
    // can't rely on the theme's metrics.
    const IntSize size(13, 13);
    setSizeIfAuto(style, size);
}

bool RenderThemeChromiumLinux::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    static Image* const checkedImage = Image::loadPlatformResource("linuxRadioOn").releaseRef();
    static Image* const uncheckedImage = Image::loadPlatformResource("linuxRadioOff").releaseRef();

    Image* image = this->isChecked(o) ? checkedImage : uncheckedImage;
    i.context->drawImage(image, rect);
    return false;
}

void RenderThemeChromiumLinux::setRadioSize(RenderStyle* style) const
{
    // Use same sizing for radio box as checkbox.
    setCheckboxSize(style);
}

static void paintButtonLike(RenderTheme* theme, RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect) {
    SkCanvas* const canvas = i.context->platformContext()->canvas();
    SkPaint paint;
    SkRect skrect;
    const int right = rect.x() + rect.width();
    const int bottom = rect.y() + rect.height();

    // If the button is too small, fallback to drawing a single, solid color
    if (rect.width() < 5 || rect.height() < 5) {
        paint.setARGB(0xff, 0xe9, 0xe9, 0xe9);
        skrect.set(rect.x(), rect.y(), right, bottom);
        canvas->drawRect(skrect, paint);
        return;
    }

    const int borderAlpha = theme->isHovered(o) ? 0x80 : 0x55;
    paint.setARGB(borderAlpha, 0, 0, 0);
    canvas->drawLine(rect.x() + 1, rect.y(), right - 1, rect.y(), paint);
    canvas->drawLine(right - 1, rect.y() + 1, right - 1, bottom - 1, paint);
    canvas->drawLine(rect.x() + 1, bottom - 1, right - 1, bottom - 1, paint);
    canvas->drawLine(rect.x(), rect.y() + 1, rect.x(), bottom - 1, paint);

    paint.setARGB(0xff, 0, 0, 0);
    SkPoint p[2];
    const int lightEnd = theme->isPressed(o) ? 1 : 0;
    const int darkEnd = !lightEnd;
    p[lightEnd].set(SkIntToScalar(rect.x()), SkIntToScalar(rect.y()));
    p[darkEnd].set(SkIntToScalar(rect.x()), SkIntToScalar(bottom - 1));
    SkColor colors[2];
    colors[0] = SkColorSetARGB(0xff, 0xf8, 0xf8, 0xf8);
    colors[1] = SkColorSetARGB(0xff, 0xdd, 0xdd, 0xdd);

    SkShader* s = SkGradientShader::CreateLinear(
        p, colors, NULL, 2, SkShader::kClamp_TileMode, NULL);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setShader(s);
    s->unref();

    skrect.set(rect.x() + 1, rect.y() + 1, right - 1, bottom - 1);
    canvas->drawRect(skrect, paint);

    paint.setShader(NULL);
    paint.setARGB(0xff, 0xce, 0xce, 0xce);
    canvas->drawPoint(rect.x() + 1, rect.y() + 1, paint);
    canvas->drawPoint(right - 2, rect.y() + 1, paint);
    canvas->drawPoint(rect.x() + 1, bottom - 2, paint);
    canvas->drawPoint(right - 2, bottom - 2, paint);
}

bool RenderThemeChromiumLinux::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    paintButtonLike(this, o, i, rect);
    return false;
}

bool RenderThemeChromiumLinux::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return true;
}

bool RenderThemeChromiumLinux::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return true;
}

bool RenderThemeChromiumLinux::paintSearchFieldResultsDecoration(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return true;
}

bool RenderThemeChromiumLinux::paintSearchFieldResultsButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return true;
}

bool RenderThemeChromiumLinux::paintSearchFieldCancelButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return true;
}

void RenderThemeChromiumLinux::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    // Height is locked to auto on all browsers.
    style->setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeChromiumLinux::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    SkCanvas* const canvas = i.context->platformContext()->canvas();
    const int right = rect.x() + rect.width();
    const int middle = rect.y() + rect.height() / 2;

    paintButtonLike(this, o, i, rect);

    SkPaint paint;
    paint.setARGB(0xff, 0, 0, 0);
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);

    SkPath path;
    path.moveTo(right - 13, middle - 3);
    path.rLineTo(6, 0);
    path.rLineTo(-3, 6);
    path.close();
    canvas->drawPath(path, paint);

    return false;
}

void RenderThemeChromiumLinux::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustMenuListStyle(selector, style, e);
}

// Used to paint styled menulists (i.e. with a non-default border)
bool RenderThemeChromiumLinux::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMenuList(o, i, rect);
}

int RenderThemeChromiumLinux::popupInternalPaddingLeft(RenderStyle* style) const
{
    return menuListInternalPadding(style, LeftPadding);
}

int RenderThemeChromiumLinux::popupInternalPaddingRight(RenderStyle* style) const
{
    return menuListInternalPadding(style, RightPadding);
}

int RenderThemeChromiumLinux::popupInternalPaddingTop(RenderStyle* style) const
{
    return menuListInternalPadding(style, TopPadding);
}

int RenderThemeChromiumLinux::popupInternalPaddingBottom(RenderStyle* style) const
{
    return menuListInternalPadding(style, BottomPadding);
}

int RenderThemeChromiumLinux::buttonInternalPaddingLeft() const
{
    return 3;
}

int RenderThemeChromiumLinux::buttonInternalPaddingRight() const
{
    return 3;
}

int RenderThemeChromiumLinux::buttonInternalPaddingTop() const
{
    return 1;
}

int RenderThemeChromiumLinux::buttonInternalPaddingBottom() const
{
    return 1;
}

bool RenderThemeChromiumLinux::controlSupportsTints(const RenderObject* o) const
{
    return isEnabled(o);
}

Color RenderThemeChromiumLinux::activeListBoxSelectionBackgroundColor() const
{
    return Color(0x28, 0x28, 0x28);
}

Color RenderThemeChromiumLinux::activeListBoxSelectionForegroundColor() const
{
    return Color(0, 0, 0);
}

Color RenderThemeChromiumLinux::inactiveListBoxSelectionBackgroundColor() const
{
    return Color(0xc8, 0xc8, 0xc8);
}

Color RenderThemeChromiumLinux::inactiveListBoxSelectionForegroundColor() const
{
    return Color(0x32, 0x32, 0x32);
}

int RenderThemeChromiumLinux::menuListInternalPadding(RenderStyle* style, int paddingType) const
{
    // This internal padding is in addition to the user-supplied padding.
    // Matches the FF behavior.
    int padding = styledMenuListInternalPadding[paddingType];

    // Reserve the space for right arrow here. The rest of the padding is
    // set by adjustMenuListStyle, since PopMenuWin.cpp uses the padding from
    // RenderMenuList to lay out the individual items in the popup.
    // If the MenuList actually has appearance "NoAppearance", then that means
    // we don't draw a button, so don't reserve space for it.
    const int bar_type = style->direction() == LTR ? RightPadding : LeftPadding;
    if (paddingType == bar_type && style->appearance() != NoControlPart)
        padding += ScrollbarTheme::nativeTheme()->scrollbarThickness();

    return padding;
}

}  // namespace WebCore
