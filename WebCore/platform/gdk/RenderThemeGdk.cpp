/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * All rights reserved.
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
 *
 */

#include "config.h"
#include "RenderThemeGdk.h"

#include "GraphicsContext.h"
#include "NotImplementedGdk.h"
#include "RenderObject.h"

#define THEME_COLOR 204
#define THEME_FONT  210

// Generic state constants
#define TS_NORMAL    1
#define TS_HOVER     2
#define TS_ACTIVE    3
#define TS_DISABLED  4
#define TS_FOCUSED   5

// Button constants
#define BP_BUTTON    1
#define BP_RADIO     2
#define BP_CHECKBOX  3

// Textfield constants
#define TFP_TEXTFIELD 1
#define TFS_READONLY  6

namespace WebCore {

RenderTheme* theme()
{
    static RenderThemeGdk gdkTheme;
    return &gdkTheme;
}

void RenderThemeGdk::close()
{
}

void RenderThemeGdk::addIntrinsicMargins(RenderStyle* style) const
{
    // Cut out the intrinsic margins completely if we end up using a small font size
    if (style->fontSize() < 11)
        return;

    // Intrinsic margin value.
    const int m = 2;

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(m, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(m, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(m, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(m, Fixed));
    }
}

bool RenderThemeGdk::supportsFocus(EAppearance appearance)
{
    switch (appearance) {
        case PushButtonAppearance:
        case ButtonAppearance:
        case TextFieldAppearance:
            return true;
        default:
            return false;
    }

    return false;
}

unsigned RenderThemeGdk::determineState(RenderObject* o)
{
    unsigned result = TS_NORMAL;
    if (!isEnabled(o))
        result = TS_DISABLED;
    else if (isReadOnlyControl(o))
        result = TFS_READONLY; // Readonly is supported on textfields.
    else if (supportsFocus(o->style()->appearance()) && isFocused(o))
        result = TS_FOCUSED;
    else if (isPressed(o))
        result = TS_ACTIVE;
    else if (isHovered(o))
        result = TS_HOVER;
    if (isChecked(o))
        result += 4; // 4 unchecked states, 4 checked states.
    return result;
}

ThemeData RenderThemeGdk::getThemeData(RenderObject* o)
{
    ThemeData result;
    switch (o->style()->appearance()) {
        case PushButtonAppearance:
        case ButtonAppearance:
            result.m_part = BP_BUTTON;
            result.m_state = determineState(o);
            break;
        case CheckboxAppearance:
            result.m_part = BP_CHECKBOX;
            result.m_state = determineState(o);
            break;
        case RadioAppearance:
            result.m_part = BP_RADIO;
            result.m_state = determineState(o);
            break;
        case TextFieldAppearance:
            result.m_part = TFP_TEXTFIELD;
            result.m_state = determineState(o);
            break;
        default:
            // FIXME: much more?
            break;
    }

    return result;
}

void RenderThemeGdk::setCheckboxSize(RenderStyle*) const 
{ 
    notImplementedGdk(); 
}

bool RenderThemeGdk::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    // FIXME: is it the right thing to do?
    return paintButton(o, i, r); 
}

void RenderThemeGdk::setRadioSize(RenderStyle*) const 
{ 
    notImplementedGdk(); 
}

bool RenderThemeGdk::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{ 
    // FIXME: is it the right thing to do?
    return paintButton(o, i, r); 
}

bool RenderThemeGdk::paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) 
{ 
    // FIXME: should use theme-aware drawing
    return true;
}

void RenderThemeGdk::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle*, Element* e) const 
{ 
    notImplementedGdk(); 
}

bool RenderThemeGdk::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    // FIXME: should use theme-aware drawing
    return true;
}

bool RenderThemeGdk::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

void RenderThemeGdk::adjustButtonStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    addIntrinsicMargins(style);
}

void RenderThemeGdk::systemFont(int propId, FontDescription&) const
{
}

}
