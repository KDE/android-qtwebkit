/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef POPUPMENU_H
#define POPUPMENU_H

#include "Shared.h"

#include "IntRect.h"
#include <wtf/PassRefPtr.h>

#if PLATFORM(MAC)
#include "RetainPtr.h"
#ifdef __OBJC__
@class NSPopUpButtonCell;
#else
class NSPopUpButtonCell;
#endif
#elif PLATFORM(WIN)
typedef struct HWND__* HWND;
typedef struct HDC__* HDC;
typedef struct HBITMAP__* HBITMAP;
#endif

namespace WebCore {

class FrameView;
class HTMLOptionElement;
class HTMLOptGroupElement;
class RenderMenuList;

class PopupMenu : public Shared<PopupMenu> {
public:
    static PassRefPtr<PopupMenu> create(RenderMenuList* menuList) { return new PopupMenu(menuList); }
    ~PopupMenu();
    
    void disconnectMenuList() { m_menuList = 0; }

    void show(const IntRect&, FrameView*, int index);
    void hide();
    
    RenderMenuList* menuList() const { return m_menuList; }

#if PLATFORM(WIN)
    bool up();
    bool down();

    int itemHeight() const { return m_itemHeight; }
    const IntRect& windowRect() const { return m_windowRect; }
    IntRect clientRect() const;

    int listIndexAtPoint(const IntPoint& point) { return (point.y() + m_scrollOffset) / m_itemHeight; }
    bool setFocusedIndex(int index, bool fireOnChange = false);
    int focusedIndex() const { return m_focusedIndex; }

    void paint(const IntRect& damageRect, HDC hdc = 0);

    HWND popupHandle() const { return m_popup; }

    void setWasClicked(bool b = true) { m_wasClicked = b; }
    bool wasClicked() const { return m_wasClicked; }

    void setScrollOffset(int offset) { m_scrollOffset = offset; }
    int scrollOffset() const { return m_scrollOffset; }

    void incrementWheelDelta(int delta);
    void reduceWheelDelta(int delta);
    int wheelDelta() const { return m_wheelDelta; }
#endif

private:
    RenderMenuList* m_menuList;
    
#if PLATFORM(MAC)
    void clear();
    void populate();
    void addSeparator();
    void addGroupLabel(HTMLOptGroupElement*);
    void addOption(HTMLOptionElement*);

    RetainPtr<NSPopUpButtonCell> m_popup;
#elif PLATFORM(WIN)
    void calculatePositionAndSize(const IntRect&, FrameView*);
    void invalidateItem(int index);

    HWND m_popup;
    HDC m_DC;
    HBITMAP m_bmp;
    bool m_wasClicked;
    IntRect m_windowRect;
    int m_itemHeight;
    int m_focusedIndex;
    int m_scrollOffset;
    int m_wheelDelta;
#endif

    PopupMenu(RenderMenuList* menuList);
};

}

#endif
