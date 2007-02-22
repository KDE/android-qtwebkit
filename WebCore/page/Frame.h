// -*- c-basic-offset: 4 -*-
 /* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Trolltech ASA
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

#ifndef Frame_h
#define Frame_h

#include "Color.h"
#include "EditAction.h"
#include "DragImage.h"
#include "RenderLayer.h"
#include "TextGranularity.h"
#include <wtf/unicode/Unicode.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

class NPObject;

namespace KJS {
    class Interpreter;
    class JSObject;
    
    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

#if PLATFORM(MAC)
#ifdef __OBJC__
@class WebCoreFrameBridge;
@class WebScriptObject;
@class NSArray;
@class NSDictionary;
@class NSFont;
@class NSImage;
@class NSMenu;
@class NSMutableDictionary;
@class NSString;
@class WebCoreFrameBridge;
#else
class WebCoreFrameBridge;
class WebScriptObject;
class NSArray;
class NSDictionary;
class NSFont;
class NSImage;
class NSMenu;
class NSMutableDictionary;
class NSString;
class WebCoreFrameBridge;
typedef unsigned int NSDragOperation;
typedef int NSWritingDirection;
#endif
#endif

namespace WebCore {

class CSSComputedStyleDeclaration;
class CSSMutableStyleDeclaration;
class CSSStyleDeclaration;
class CommandByName;
class DOMWindow;
class Document;
class Editor;
class Element;
class EventHandler;
class FloatRect;
class FrameLoader;
class FrameLoaderClient;
class HTMLFrameOwnerElement;
class HTMLTableCellElement;
class FramePrivate;
class FrameTree;
class FrameView;
class GraphicsContext;
class HTMLFormElement;
class IntRect;
class KJSProxy;
class KURL;
class Node;
class Page;
class Range;
class RenderObject;
class RenderPart;
class RenderStyle;
class Selection;
class SelectionController;
class Settings;
class Widget;

struct FrameLoadRequest;

template <typename T> class Timer;

struct MarkedTextUnderline {
    MarkedTextUnderline() 
        : startOffset(0), endOffset(0), thick(false) { }
    MarkedTextUnderline(unsigned s, unsigned e, const Color& c, bool t) 
        : startOffset(s), endOffset(e), color(c), thick(t) { }
    unsigned startOffset;
    unsigned endOffset;
    Color color;
    bool thick;
};

class Frame : public Shared<Frame> {
public:
    Frame(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);
    virtual void setView(FrameView*);
    virtual ~Frame();
    
#if PLATFORM(MAC)    
    void setBridge(WebCoreFrameBridge*);
    WebCoreFrameBridge* bridge() const;
#endif

    Page* page() const;
    HTMLFrameOwnerElement* ownerElement() const;

    void pageDestroyed();
    void disconnectOwnerElement();

    Document* document() const;
    FrameView* view() const;

    CommandByName* command() const;
    DOMWindow* domWindow() const;
    Editor* editor() const;
    EventHandler* eventHandler() const;
    FrameLoader* loader() const;
    SelectionController* selectionController() const;
    FrameTree* tree() const;

    RenderObject* renderer() const; // root renderer for the document contained in this frame
    RenderPart* ownerRenderer(); // renderer for the element that contains this frame

    friend class FrameLoader;
    friend class FramePrivate;

    DragImageRef dragImageForSelection();
    
private:
    FramePrivate* d;
    
// === undecided, may or may not belong here

public:
    static Frame* frameForWidget(const Widget*);

    void setSettings(Settings*);
    const Settings* settings() const;
    void reparseConfiguration();

    // should move to FrameView
    void paint(GraphicsContext*, const IntRect&);
    void setPaintRestriction(PaintRestriction pr);

    void setUserStyleSheetLocation(const KURL&);
    void setUserStyleSheet(const String& styleSheetData);

    void setZoomFactor(int percent);
    int zoomFactor() const;

    bool inViewSourceMode() const;
    void setInViewSourceMode(bool = true) const;

    void setJSStatusBarText(const String&);
    void setJSDefaultStatusBarText(const String&);
    String jsStatusBarText() const;
    String jsDefaultStatusBarText() const;

    void keepAlive(); // Used to keep the frame alive when running a script that might destroy it.
#ifndef NDEBUG
    static void cancelAllKeepAlive();
#endif

    KJS::Bindings::Instance* createScriptInstanceForWidget(Widget*);
    KJS::Bindings::RootObject* bindingRootObject();
    
    PassRefPtr<KJS::Bindings::RootObject> createRootObject(void* nativeHandle, PassRefPtr<KJS::Interpreter>);

#if PLATFORM(MAC)
    WebScriptObject* windowScriptObject();
#endif
    NPObject* windowScriptNPObject();
    
    void setDocument(Document*);

    KJSProxy* scriptProxy();

    bool isFrameSet() const;

    void adjustPageHeight(float* newBottom, float oldTop, float oldBottom, float bottomLimit);

    void forceLayout();
    void forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth);

    void sendResizeEvent();
    void sendScrollEvent();

    void clearTimers();
    static void clearTimers(FrameView*);

    bool isActive() const;
    void setIsActive(bool flag);
    void setWindowHasFocus(bool flag);

    // Convenience, to avoid repeating the code to dig down to get this.
    UChar backslashAsCurrencySymbol() const;

    void setNeedsReapplyStyles();
    String documentTypeString() const;

    bool prohibitsScrolling() const;
    void setProhibitsScrolling(const bool);

private:
    void cleanupScriptObjects();
    void cleanupPlatformScriptObjects();

    void lifeSupportTimerFired(Timer<Frame>*);
    
// === to be moved into Chrome

public:
    void focusWindow();
    void unfocusWindow();
    void print();
    bool shouldClose();
    void scheduleClose();

// === to be moved into Editor

public:
    String selectedText() const;  
    bool findString(const String&, bool forward, bool caseFlag, bool wrapFlag, bool startInSelection);

    const Selection& mark() const; // Mark, to be used as emacs uses it.
    void setMark(const Selection&);

    void transpose();

    void computeAndSetTypingStyle(CSSStyleDeclaration* , EditAction = EditActionUnspecified);
    enum TriState { falseTriState, trueTriState, mixedTriState };
    TriState selectionHasStyle(CSSStyleDeclaration*) const;
    String selectionStartStylePropertyValue(int stylePropertyID) const;
    void applyEditingStyleToBodyElement() const;
    void removeEditingStyleFromBodyElement() const;
    void applyEditingStyleToElement(Element*) const;
    void removeEditingStyleFromElement(Element*) const;

    Range* markedTextRange() const;
#if PLATFORM(MAC)
    void issuePasteCommand();
#endif
    void issueTransposeCommand();
    void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
    bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity, bool stillSelecting) const;

    RenderStyle* styleForSelectionStart(Node*& nodeToRemove) const;

    const Vector<MarkedTextUnderline>& markedTextUnderlines() const;  
    bool markedTextUsesUnderlines() const;
  
    unsigned markAllMatchesForText(const String&, bool caseFlag, unsigned limit);
    bool markedTextMatchesAreHighlighted() const;
    void setMarkedTextMatchesAreHighlighted(bool flag);

    CSSComputedStyleDeclaration* selectionComputedStyle(Node*& nodeToRemove) const;

    void textFieldDidBeginEditing(Element*);
    void textFieldDidEndEditing(Element*);
    void textDidChangeInTextField(Element*);
    bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
    void textWillBeDeletedInTextField(Element* input);
    void textDidChangeInTextArea(Element*);

// === to be moved into SelectionController

public:
    TextGranularity selectionGranularity() const;
    void setSelectionGranularity(TextGranularity) const;

    bool shouldChangeSelection(const Selection&) const;
    bool shouldDeleteSelection(const Selection&) const;
    void clearCaretRectIfNeeded();
    void setFocusedNodeIfNeeded();
    void selectionLayoutChanged();
    void notifyRendererOfSelectionChange(bool userTriggered);

    void invalidateSelection();

    void setCaretVisible(bool = true);
    void paintCaret(GraphicsContext*, const IntRect&) const;  
    void paintDragCaret(GraphicsContext*, const IntRect&) const;

    bool isContentEditable() const; // if true, everything in frame is editable

    void setSecureKeyboardEntry(bool);
    bool isSecureKeyboardEntry();

    CSSMutableStyleDeclaration* typingStyle() const;
    void setTypingStyle(CSSMutableStyleDeclaration*);
    void clearTypingStyle();

    IntRect selectionRect() const;
    FloatRect visibleSelectionRect() const;

    HTMLFormElement* currentForm() const;

    void revealSelection(const RenderLayer::ScrollAlignment& = RenderLayer::gAlignCenterIfNeeded) const;
    void revealCaret(const RenderLayer::ScrollAlignment& = RenderLayer::gAlignCenterIfNeeded) const;
    void setSelectionFromNone();

private:
    void caretBlinkTimerFired(Timer<Frame>*);

// === to be moved into the Platform directory

public:
    bool isCharacterSmartReplaceExempt(UChar, bool);

// === to be deleted

public:
    SelectionController* dragCaretController() const;

    String searchForLabelsAboveCell(RegularExpression*, HTMLTableCellElement*);
    String searchForLabelsBeforeElement(const Vector<String>& labels, Element*);
    String matchLabelsAgainstElement(const Vector<String>& labels, Element*);

#if PLATFORM(MAC)
// === undecided, may or may not belong here

public:
    NSString* searchForNSLabelsAboveCell(RegularExpression*, HTMLTableCellElement*);
    NSString* searchForLabelsBeforeElement(NSArray* labels, Element*);
    NSString* matchLabelsAgainstElement(NSArray* labels, Element*);

    NSMutableDictionary* dashboardRegionsDictionary();
    void dashboardRegionsChanged();

    void willPopupMenu(NSMenu*);

    NSImage* selectionImage(bool forceWhiteText = false) const;
    NSImage* snapshotDragImage(Node*, NSRect* imageRect, NSRect* elementRect) const;

private:    
    NSImage* imageFromRect(NSRect) const;

// === to be moved into Chrome

public:

    FloatRect customHighlightLineRect(const AtomicString& type, const FloatRect& lineRect);
    void paintCustomHighlight(const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect, bool text, bool line);

// === to be moved into Editor

public:

    NSDictionary* fontAttributesForSelectionStart() const;
    NSWritingDirection baseWritingDirectionForSelectionStart() const;

    void setMarkedTextRange(const Range* , NSArray* attributes, NSArray* ranges);
#endif
};

} // namespace WebCore

#endif // Frame_h
