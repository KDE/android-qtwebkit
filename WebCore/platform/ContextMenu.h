/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef ContextMenu_h
#define ContextMenu_h

#include "HitTestResult.h"
#include "PlatformString.h"

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSMutableArray;
#else
class NSMutableArray;
#endif
#elif PLATFORM(WIN)
typedef struct HMENU__* HMENU;
#endif

namespace WebCore {

// This enum needs to be in sync with WebMenuItemTag, which is defined in WebUIDelegate.h
enum ContextMenuAction {
    WebMenuItemTagNoAction=0, // This item is not actually in WebUIDelegate.h
    WebMenuItemTagOpenLinkInNewWindow=1,
    WebMenuItemTagDownloadLinkToDisk,
    WebMenuItemTagCopyLinkToClipboard,
    WebMenuItemTagOpenImageInNewWindow,
    WebMenuItemTagDownloadImageToDisk,
    WebMenuItemTagCopyImageToClipboard,
    WebMenuItemTagOpenFrameInNewWindow,
    WebMenuItemTagCopy,
    WebMenuItemTagGoBack,
    WebMenuItemTagGoForward,
    WebMenuItemTagStop,
    WebMenuItemTagReload,
    WebMenuItemTagCut,
    WebMenuItemTagPaste,
    WebMenuItemTagSpellingGuess,
    WebMenuItemTagNoGuessesFound,
    WebMenuItemTagIgnoreSpelling,
    WebMenuItemTagLearnSpelling,
    WebMenuItemTagOther,
    WebMenuItemTagSearchInSpotlight,
    WebMenuItemTagSearchWeb,
    WebMenuItemTagLookUpInDictionary,
    WebMenuItemTagOpenWithDefaultApplication,
    WebMenuItemPDFActualSize,
    WebMenuItemPDFZoomIn,
    WebMenuItemPDFZoomOut,
    WebMenuItemPDFAutoSize,
    WebMenuItemPDFSinglePage,
    WebMenuItemPDFFacingPages,
    WebMenuItemPDFContinuous,
    WebMenuItemPDFNextPage,
    WebMenuItemPDFPreviousPage,
};

enum ContextMenuItemType {
    ActionType,
    SeparatorType,
    SubmenuType
};

struct ContextMenuItem {
    ContextMenuItem()
        : type(SeparatorType)
        , action(WebMenuItemTagNoAction)
        , title(String())
    {
    }

    ContextMenuItem(ContextMenuItemType theType, ContextMenuAction theAction, const String& theTitle)
        : type(theType)
        , action(theAction)
        , title(theTitle)
    {
    }

    // FIXME: Need to support submenus (perhaps a Vector<ContextMenuItem>*?)
    // FIXME: Do we need a keyboard accelerator here?

    ContextMenuItemType type;
    ContextMenuAction action;
    String title;
};

#if PLATFORM(MAC)
typedef NSMutableArray* PlatformMenuDescription;
#elif PLATFORM(WIN)
typedef HMENU PlatformMenuDescription;
#endif

class ContextMenu : Noncopyable
{
public:
    ContextMenu(HitTestResult result)
        : m_hitTestResult(result)
        , m_menu(0)
    {
    }

    void populate();

    // FIXME: Implement these
    void show() {}
    void hide() {}

    void insertItem(unsigned position, ContextMenuItem);
    void appendItem(ContextMenuItem item);

    unsigned itemCount();

    HitTestResult hitTestResult() const { return m_hitTestResult; }

    PlatformMenuDescription platformMenuDescription() { return m_menu; }
    void setPlatformMenuDescription(PlatformMenuDescription menu);

private:
    HitTestResult m_hitTestResult;
    PlatformMenuDescription m_menu;
};

}

#endif // ContextMenu_h
