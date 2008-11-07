/**
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "RenderMenuList.h"

#include "CSSStyleSelector.h"
#include "Document.h"
#include "FontSelector.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLSelectElement.h"
#include "PopupMenu.h"
#include "RenderBR.h"
#include "RenderScrollbar.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "NodeRenderStyle.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderMenuList::RenderMenuList(HTMLSelectElement* element)
    : RenderFlexibleBox(element)
    , m_buttonText(0)
    , m_innerBlock(0)
    , m_optionsChanged(true)
    , m_optionsWidth(0)
    , m_popup(0)
    , m_popupIsVisible(false)
{
}

RenderMenuList::~RenderMenuList()
{
    if (m_popup)
        m_popup->disconnectClient();
    m_popup = 0;
}

// this static cast is safe because RenderMenuLists are only created for HTMLSelectElements
HTMLSelectElement* RenderMenuList::selectElement()
{
    return static_cast<HTMLSelectElement*>(node());
}

void RenderMenuList::createInnerBlock()
{
    if (m_innerBlock) {
        ASSERT(firstChild() == m_innerBlock);
        ASSERT(!m_innerBlock->nextSibling());
        return;
    }

    // Create an anonymous block.
    ASSERT(!firstChild());
    m_innerBlock = createAnonymousBlock();
    adjustInnerStyle();
    RenderFlexibleBox::addChild(m_innerBlock);
}

void RenderMenuList::adjustInnerStyle()
{
    m_innerBlock->style()->setBoxFlex(1.0f);
    
    m_innerBlock->style()->setPaddingLeft(Length(theme()->popupInternalPaddingLeft(style()), Fixed));
    m_innerBlock->style()->setPaddingRight(Length(theme()->popupInternalPaddingRight(style()), Fixed));
    m_innerBlock->style()->setPaddingTop(Length(theme()->popupInternalPaddingTop(style()), Fixed));
    m_innerBlock->style()->setPaddingBottom(Length(theme()->popupInternalPaddingBottom(style()), Fixed));
        
    if (PopupMenu::itemWritingDirectionIsNatural()) {
        // Items in the popup will not respect the CSS text-align and direction properties,
        // so we must adjust our own style to match.
        m_innerBlock->style()->setTextAlign(LEFT);
        TextDirection direction = (m_buttonText && m_buttonText->text()->defaultWritingDirection() == WTF::Unicode::RightToLeft) ? RTL : LTR;
        m_innerBlock->style()->setDirection(direction);
    }
}

void RenderMenuList::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    createInnerBlock();
    m_innerBlock->addChild(newChild, beforeChild);
}

void RenderMenuList::removeChild(RenderObject* oldChild)
{
    if (oldChild == m_innerBlock || !m_innerBlock) {
        RenderFlexibleBox::removeChild(oldChild);
        m_innerBlock = 0;
    } else
        m_innerBlock->removeChild(oldChild);
}

void RenderMenuList::styleDidChange(RenderStyle::Diff diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (m_buttonText)
        m_buttonText->setStyle(style());
    if (m_innerBlock) // RenderBlock handled updating the anonymous block's style.
        adjustInnerStyle();

    setReplaced(isInline());

    bool fontChanged = !oldStyle || oldStyle->font() != style()->font();
    if (fontChanged)
        updateOptionsWidth();
}

void RenderMenuList::updateOptionsWidth()
{
    float maxOptionWidth = 0;
    const Vector<HTMLElement*>& listItems = static_cast<HTMLSelectElement*>(node())->listItems();
    int size = listItems.size();    
    for (int i = 0; i < size; ++i) {
        HTMLElement* element = listItems[i];
        if (element->hasTagName(optionTag)) {
            String text = static_cast<HTMLOptionElement*>(element)->optionText();
            if (!text.isEmpty())
                maxOptionWidth = max(maxOptionWidth, style()->font().floatWidth(text));
        }
    }

    int width = static_cast<int>(ceilf(maxOptionWidth));
    if (m_optionsWidth == width)
        return;

    m_optionsWidth = width;
    setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderMenuList::updateFromElement()
{
    if (m_optionsChanged) {
        updateOptionsWidth();
        m_optionsChanged = false;
    }

    if (m_popupIsVisible)
        m_popup->updateFromElement();
    else
        setTextFromOption(static_cast<HTMLSelectElement*>(node())->selectedIndex());
}

void RenderMenuList::setTextFromOption(int optionIndex)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    int size = listItems.size();

    int i = select->optionToListIndex(optionIndex);
    String text = "";
    if (i >= 0 && i < size) {
        HTMLElement* element = listItems[i];
        if (element->hasTagName(optionTag))
            text = static_cast<HTMLOptionElement*>(listItems[i])->optionText();
    }
    setText(text.stripWhiteSpace());
}

void RenderMenuList::setText(const String& s)
{
    if (s.isEmpty()) {
        if (!m_buttonText || !m_buttonText->isBR()) {
            if (m_buttonText)
                m_buttonText->destroy();
            m_buttonText = new (renderArena()) RenderBR(document());
            m_buttonText->setStyle(style());
            addChild(m_buttonText);
        }
    } else {
        if (m_buttonText && !m_buttonText->isBR())
            m_buttonText->setText(s.impl());
        else {
            if (m_buttonText)
                m_buttonText->destroy();
            m_buttonText = new (renderArena()) RenderText(document(), s.impl());
            m_buttonText->setStyle(style());
            addChild(m_buttonText);
        }
        adjustInnerStyle();
    }
}

String RenderMenuList::text() const
{
    return m_buttonText ? m_buttonText->text() : 0;
}

IntRect RenderMenuList::controlClipRect(int tx, int ty) const
{
    // Clip to the intersection of the content box and the content box for the inner box
    // This will leave room for the arrows which sit in the inner box padding,
    // and if the inner box ever spills out of the outer box, that will get clipped too.
    IntRect outerBox(tx + borderLeft() + paddingLeft(), 
                   ty + borderTop() + paddingTop(),
                   contentWidth(), 
                   contentHeight());
    
    IntRect innerBox(tx + m_innerBlock->xPos() + m_innerBlock->paddingLeft(), 
                   ty + m_innerBlock->yPos() + m_innerBlock->paddingTop(),
                   m_innerBlock->contentWidth(), 
                   m_innerBlock->contentHeight());

    return intersection(outerBox, innerBox);
}

void RenderMenuList::calcPrefWidths()
{
    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;
    
    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = calcContentBoxWidth(style()->width().value());
    else
        m_maxPrefWidth = max(m_optionsWidth, theme()->minimumMenuListSize(style())) + m_innerBlock->paddingLeft() + m_innerBlock->paddingRight();

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPrefWidth = max(m_maxPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minPrefWidth = max(m_minPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPrefWidth = min(m_maxPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minPrefWidth = min(m_minPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;

    setPrefWidthsDirty(false);
}

void RenderMenuList::showPopup()
{
    if (m_popupIsVisible)
        return;

    // Create m_innerBlock here so it ends up as the first child.
    // This is important because otherwise we might try to create m_innerBlock
    // inside the showPopup call and it would fail.
    createInnerBlock();
    if (!m_popup)
        m_popup = PopupMenu::create(this);
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    m_popupIsVisible = true;

    // Compute the top left taking transforms into account, but use
    // the actual width of the element to size the popup.
    FloatPoint absTopLeft = localToAbsolute(FloatPoint(), false, true);
    IntRect absBounds = absoluteBoundingBoxRect();
    absBounds.setLocation(roundedIntPoint(absTopLeft));
    m_popup->show(absBounds, document()->view(),
        select->optionToListIndex(select->selectedIndex()));
}

void RenderMenuList::hidePopup()
{
    if (m_popup)
        m_popup->hide();
    m_popupIsVisible = false;
}

void RenderMenuList::valueChanged(unsigned listIndex, bool fireOnChange)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    select->setSelectedIndex(select->listToOptionIndex(listIndex), true, fireOnChange);
}

String RenderMenuList::itemText(unsigned listIndex) const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    HTMLElement* element = select->listItems()[listIndex];
    if (element->hasTagName(optgroupTag))
        return static_cast<HTMLOptGroupElement*>(element)->groupLabelText();
    else if (element->hasTagName(optionTag))
        return static_cast<HTMLOptionElement*>(element)->optionText();
    return String();
}

bool RenderMenuList::itemIsEnabled(unsigned listIndex) const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    HTMLElement* element = select->listItems()[listIndex];
    if (!element->hasTagName(optionTag))
        return false;
    bool groupEnabled = true;
    if (element->parentNode() && element->parentNode()->hasTagName(optgroupTag))
        groupEnabled = element->parentNode()->isEnabled();
    return element->isEnabled() && groupEnabled;
}

PopupMenuStyle RenderMenuList::itemStyle(unsigned listIndex) const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    HTMLElement* element = select->listItems()[listIndex];
    
    RenderStyle* style = element->renderStyle() ? element->renderStyle() : element->computedStyle();
    return style ? PopupMenuStyle(style->color(), itemBackgroundColor(listIndex), style->font(), style->visibility() == VISIBLE) : menuStyle();
}

Color RenderMenuList::itemBackgroundColor(unsigned listIndex) const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    HTMLElement* element = select->listItems()[listIndex];

    Color backgroundColor;
    if (element->renderStyle())
        backgroundColor = element->renderStyle()->backgroundColor();
    // If the item has an opaque background color, return that.
    if (!backgroundColor.hasAlpha())
        return backgroundColor;

    // Otherwise, the item's background is overlayed on top of the menu background.
    backgroundColor = style()->backgroundColor().blend(backgroundColor);
    if (!backgroundColor.hasAlpha())
        return backgroundColor;

    // If the menu background is not opaque, then add an opaque white background behind.
    return Color(Color::white).blend(backgroundColor);
}

PopupMenuStyle RenderMenuList::menuStyle() const
{

    RenderStyle* s = m_innerBlock ? m_innerBlock->style() : style();
    return PopupMenuStyle(s->color(), s->backgroundColor(), s->font(), s->visibility() == VISIBLE);
}

HostWindow* RenderMenuList::hostWindow() const
{
    return document()->view()->hostWindow();
}

PassRefPtr<Scrollbar> RenderMenuList::createScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
{
    RefPtr<Scrollbar> widget;
    bool hasCustomScrollbarStyle = style()->hasPseudoStyle(RenderStyle::SCROLLBAR);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(client, orientation, this);
    else
        widget = Scrollbar::createNativeScrollbar(client, orientation, controlSize);
    return widget.release();
}

int RenderMenuList::clientInsetLeft() const
{
    return 0;
}

int RenderMenuList::clientInsetRight() const
{
    return 0;
}

int RenderMenuList::clientPaddingLeft() const
{
    return paddingLeft();
}

int RenderMenuList::clientPaddingRight() const
{
    return paddingRight();
}

int RenderMenuList::listSize() const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    return select->listItems().size();
}

int RenderMenuList::selectedIndex() const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    return select->optionToListIndex(select->selectedIndex());
}

bool RenderMenuList::itemIsSeparator(unsigned listIndex) const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    HTMLElement* element = select->listItems()[listIndex];
    return element->hasTagName(hrTag);
}

bool RenderMenuList::itemIsLabel(unsigned listIndex) const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    HTMLElement* element = select->listItems()[listIndex];
    return element->hasTagName(optgroupTag);
}

bool RenderMenuList::itemIsSelected(unsigned listIndex) const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    HTMLElement* element = select->listItems()[listIndex];
    return element->hasTagName(optionTag)&& static_cast<HTMLOptionElement*>(element)->selected();
}

void RenderMenuList::setTextFromItem(unsigned listIndex)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    setTextFromOption(select->listToOptionIndex(listIndex));
}

FontSelector* RenderMenuList::fontSelector() const
{
    return document()->styleSelector()->fontSelector();
}

}
