/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               http://www.torchmobile.com/
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

#if ENABLE(WML)
#include "WMLDocument.h"

#include "Page.h"
#include "WMLCardElement.h"
#include "WMLPageState.h"

namespace WebCore {

WMLDocument::WMLDocument(Frame* frame)
    : Document(frame, false) 
{
    clearXMLVersion();

    Page* page = this->page();
    ASSERT(page);

    if (page && !page->wmlPageState()) {
        RefPtr<WMLPageState> pageState = new WMLPageState(page);
        page->setWMLPageState(pageState);
    }
}

WMLDocument::~WMLDocument()
{
}

void WMLDocument::finishedParsing()
{
    WMLPageState* wmlPageState = wmlPageStateForDocument(document());
    if (!wmlPageState || !wmlPageState->isDeckAccessible()) {
        // FIXME: Error reporting
        Document::finishedParsing();
        return;
    }

    // FIXME: Notify the existance of templates to all cards of the current deck
    // WMLTemplateElement::registerTemplatesInDocument(document()));

    // Set destination card
    WMLCardElement* card = WMLCardElement::setActiveCardInDocument(document(), KURL());
    if (!card) {
        // FIXME: Error reporting
        Document::finishedParsing();
        return;
    }
 
    // FIXME: shadow the deck-level do if needed
    // FIXME: handle the intrinsic event

    wmlPageState->setNeedCheckDeckAccess(false);
    Document::finishedParsing();
}

WMLPageState* wmlPageStateForDocument(Document* doc)
{
    ASSERT(doc);
    if (!doc)
        return 0;

    Page* page = doc->page();
    ASSERT(page);

    if (!page)
        return 0;

    WMLPageState* wmlPageState = page->wmlPageState();
    ASSERT(wmlPageState);

    return wmlPageState;
}

}

#endif
