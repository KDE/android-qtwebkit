/*
 * Copyright (C) 2005, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
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

#ifndef EventNames_h
#define EventNames_h

#include "AtomicString.h"

namespace WebCore { namespace EventNames {

#define DOM_EVENT_NAMES_FOR_EACH(macro) \
    \
    macro(abort) \
    macro(beforecopy) \
    macro(beforecut) \
    macro(beforepaste) \
    macro(beforeunload) \
    macro(blur) \
    macro(change) \
    macro(click) \
    macro(contextmenu) \
    macro(copy) \
    macro(cut) \
    macro(dblclick) \
    macro(drag) \
    macro(dragend) \
    macro(dragenter) \
    macro(dragleave) \
    macro(dragover) \
    macro(dragstart) \
    macro(drop) \
    macro(error) \
    macro(focus) \
    macro(input) \
    macro(keydown) \
    macro(keypress) \
    macro(keyup) \
    macro(load) \
    macro(mousedown) \
    macro(mousemove) \
    macro(mouseout) \
    macro(mouseover) \
    macro(mouseup) \
    macro(mousewheel) \
    macro(overflowchanged) \
    macro(paste) \
    macro(readystatechange) \
    macro(reset) \
    macro(resize) \
    macro(scroll) \
    macro(search) \
    macro(select) \
    macro(selectstart) \
    macro(submit) \
    macro(textInput) \
    macro(unload) \
    macro(zoom) \
    \
    macro(DOMActivate) \
    macro(DOMAttrModified) \
    macro(DOMCharacterDataModified) \
    macro(DOMFocusIn) \
    macro(DOMFocusOut) \
    macro(DOMNodeInserted) \
    macro(DOMNodeInsertedIntoDocument) \
    macro(DOMNodeRemoved) \
    macro(DOMNodeRemovedFromDocument) \
    macro(DOMSubtreeModified) \
    \
    macro(webkitBeforeTextInserted) \
    macro(webkitEditableContentChanged) \
    \
// end of DOM_EVENT_NAMES_FOR_EACH

#ifndef DOM_EVENT_NAMES_HIDE_GLOBALS
    #define DOM_EVENT_NAMES_DECLARE(name) extern const AtomicString name##Event;
    DOM_EVENT_NAMES_FOR_EACH(DOM_EVENT_NAMES_DECLARE)
    #undef DOM_EVENT_NAMES_DECLARE
#endif

    void init();

} }

#endif
