/**
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#include "css/cssstyleselector.h"
#include "rendering/render_style.h"
#include "css/css_stylesheetimpl.h"
#include "css/css_ruleimpl.h"
#include "css/css_valueimpl.h"
#include "css/csshelper.h"
#include "rendering/render_object.h"
#include "html/html_documentimpl.h"
#include "html/html_elementimpl.h"
#include "xml/dom_elementimpl.h"
#include "dom/css_rule.h"
#include "dom/css_value.h"
#include "khtml_factory.h"
#include "khtmlpart_p.h"
using namespace khtml;
using namespace DOM;

#include "css/cssproperties.h"
#include "css/cssvalues.h"

#include "misc/khtmllayout.h"
#include "khtml_settings.h"
#include "misc/htmlhashes.h"
#include "misc/helper.h"
#include "misc/loader.h"

#include "rendering/font.h"

#include "khtmlview.h"
#include "khtml_part.h"

#include <kstandarddirs.h>
#include <kcharsets.h>
#include <kglobal.h>
#include <qfile.h>
#include <qfontdatabase.h>
#include <qfontinfo.h>
#include <qvaluelist.h>
#include <qstring.h>
#include <kdebug.h>
#include <kurl.h>
#include <qdatetime.h>
#include <assert.h>
#include <qpaintdevicemetrics.h>
#include <qintcache.h>
#include <stdlib.h>

// #define STYLE_SHARING_STATS 1

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) \
{\
    style->set##Prop(parentStyle->prop());\
    return;\
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
else if (isInitial) \
{\
    style->set##Prop(RenderStyle::initial##Prop());\
    return;\
}

#define HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(prop, Prop, Value) \
HANDLE_INHERIT(prop, Prop) \
else if (isInitial) \
{\
    style->set##Prop(RenderStyle::initial##Value());\
    return;\
}

#define HANDLE_BACKGROUND_INHERIT_AND_INITIAL(prop, Prop) \
if (isInherit) { \
    BackgroundLayer* currChild = style->accessBackgroundLayers(); \
    BackgroundLayer* prevChild = 0; \
    const BackgroundLayer* currParent = parentStyle->backgroundLayers(); \
    while (currParent && currParent->is##Prop##Set()) { \
        if (!currChild) { \
            /* Need to make a new layer.*/ \
            currChild = new BackgroundLayer(); \
            prevChild->setNext(currChild); \
        } \
        currChild->set##Prop(currParent->prop()); \
        prevChild = currChild; \
        currChild = prevChild->next(); \
        currParent = currParent->next(); \
    } \
    \
    while (currChild) { \
        /* Reset any remaining layers to not have the property set. */ \
        currChild->clear##Prop(); \
        currChild = currChild->next(); \
    } \
    return; \
} \
if (isInitial) { \
    BackgroundLayer* currChild = style->accessBackgroundLayers(); \
    currChild->set##Prop(RenderStyle::initial##Prop()); \
    for (currChild = currChild->next(); currChild; currChild = currChild->next()) \
        currChild->clear##Prop(); \
    return; \
}

#define HANDLE_BACKGROUND_VALUE(prop, Prop, value) { \
HANDLE_BACKGROUND_INHERIT_AND_INITIAL(prop, Prop) \
if (!value->isPrimitiveValue() && !value->isValueList()) \
    return; \
BackgroundLayer* currChild = style->accessBackgroundLayers(); \
BackgroundLayer* prevChild = 0; \
if (value->isPrimitiveValue()) { \
    map##Prop(currChild, value); \
    currChild = currChild->next(); \
} \
else { \
    /* Walk each value and put it into a layer, creating new layers as needed. */ \
    CSSValueListImpl* valueList = static_cast<CSSValueListImpl*>(value); \
    for (unsigned int i = 0; i < valueList->length(); i++) { \
        if (!currChild) { \
            /* Need to make a new layer to hold this value */ \
            currChild = new BackgroundLayer(); \
            prevChild->setNext(currChild); \
        } \
        map##Prop(currChild, valueList->item(i)); \
        prevChild = currChild; \
        currChild = currChild->next(); \
    } \
} \
while (currChild) { \
    /* Reset all remaining layers to not have the property set. */ \
    currChild->clear##Prop(); \
    currChild = currChild->next(); \
} }

#define HANDLE_INHERIT_COND(propID, prop, Prop) \
if (id == propID) \
{\
    style->set##Prop(parentStyle->prop());\
    return;\
}

#define HANDLE_INITIAL_COND(propID, Prop) \
if (id == propID) \
{\
    style->set##Prop(RenderStyle::initial##Prop());\
    return;\
}

#define HANDLE_INITIAL_COND_WITH_VALUE(propID, Prop, Value) \
if (id == propID) \
{\
    style->set##Prop(RenderStyle::initial##Value());\
    return;\
}

namespace khtml {

CSSRuleSet *CSSStyleSelector::defaultStyle = 0;
CSSRuleSet *CSSStyleSelector::defaultQuirksStyle = 0;
CSSRuleSet *CSSStyleSelector::defaultPrintStyle = 0;
CSSStyleSheetImpl *CSSStyleSelector::defaultSheet = 0;
RenderStyle* CSSStyleSelector::styleNotYetAvailable = 0;
CSSStyleSheetImpl *CSSStyleSelector::quirksSheet = 0;

static CSSStyleSelector::Encodedurl *encodedurl = 0;
static PseudoState pseudoState;

CSSStyleSelector::CSSStyleSelector( DocumentImpl* doc, QString userStyleSheet, StyleSheetListImpl *styleSheets,
                                    bool _strictParsing )
{
    init();

    view = doc->view();
    strictParsing = _strictParsing;
    settings = view ? view->part()->settings() : 0;
    if(!defaultStyle) loadDefaultStyle(settings);
    m_medium = view ? view->mediaType() : QString("all");

    m_userStyle = 0;
    m_userSheet = 0;
    paintDeviceMetrics = doc->paintDeviceMetrics();

    // FIXME: This sucks! The user sheet is reparsed every time!
    if (!userStyleSheet.isEmpty()) {
        m_userSheet = new DOM::CSSStyleSheetImpl(doc);
        m_userSheet->parseString(DOMString(userStyleSheet), strictParsing);

        m_userStyle = new CSSRuleSet();
        m_userStyle->addRulesFromSheet( m_userSheet, m_medium );
    }

    // add stylesheets from document
    m_authorStyle = new CSSRuleSet();

    QPtrListIterator<StyleSheetImpl> it(styleSheets->styleSheets);
    for (; it.current(); ++it)
        if (it.current()->isCSSStyleSheet())
            m_authorStyle->addRulesFromSheet(static_cast<CSSStyleSheetImpl*>(it.current()), m_medium);

    //kdDebug( 6080 ) << "number of style sheets in document " << authorStyleSheets.count() << endl;
    //kdDebug( 6080 ) << "CSSStyleSelector: author style has " << authorStyle->count() << " elements"<< endl;
}

CSSStyleSelector::CSSStyleSelector( CSSStyleSheetImpl *sheet )
{
    init();

    if(!defaultStyle) loadDefaultStyle();
    KHTMLView *view = sheet->doc()->view();
    m_medium =  view ? view->mediaType() : QString("all");

    m_authorStyle = new CSSRuleSet();
    m_authorStyle->addRulesFromSheet( sheet, m_medium );
}

void CSSStyleSelector::init()
{
    element = 0;
    settings = 0;
    paintDeviceMetrics = 0;
    m_matchedRuleCount = m_matchedDeclCount = m_tmpRuleCount = 0;
}

void CSSStyleSelector::setEncodedURL(const KURL& url)
{
    KURL u = url;

    u.setQuery( QString::null );
    u.setRef( QString::null );
    encodedurl.file = u.url();
    int pos = encodedurl.file.findRev('/');
    encodedurl.path = encodedurl.file;
    if ( pos > 0 ) {
	encodedurl.path.truncate( pos );
	encodedurl.path += '/';
    }
    u.setPath( QString::null );
    encodedurl.host = u.url();

    //kdDebug() << "CSSStyleSelector::CSSStyleSelector encoded url " << encodedurl.path << endl;
}

CSSStyleSelector::~CSSStyleSelector()
{
    delete m_authorStyle;
    delete m_userStyle;
    delete m_userSheet;
}

void CSSStyleSelector::loadDefaultStyle(const KHTMLSettings *s)
{
    if(defaultStyle) return;

    {
        QFile f(locate( "data", "khtml/css/html4.css" ) );
        f.open(IO_ReadOnly);

        QCString file( f.size()+1 );
        int readbytes = f.readBlock( file.data(), f.size() );
        f.close();
        if ( readbytes >= 0 )
            file[readbytes] = '\0';

        QString style = QString::fromLatin1( file.data() );
        if(s)
            style += s->settingsToCSS();
        DOMString str(style);

        defaultSheet = new DOM::CSSStyleSheetImpl((DOM::CSSStyleSheetImpl * ) 0);
        defaultSheet->parseString( str );

        // Collect only strict-mode rules.
        defaultStyle = new CSSRuleSet();
        defaultStyle->addRulesFromSheet( defaultSheet, "screen" );
        
        defaultPrintStyle = new CSSRuleSet();
        defaultPrintStyle->addRulesFromSheet( defaultSheet, "print" );
    }
    {
        QFile f(locate( "data", "khtml/css/quirks.css" ) );
        f.open(IO_ReadOnly);

        QCString file( f.size()+1 );
        int readbytes = f.readBlock( file.data(), f.size() );
        f.close();
        if ( readbytes >= 0 )
            file[readbytes] = '\0';

        QString style = QString::fromLatin1( file.data() );
        DOMString str(style);

        quirksSheet = new DOM::CSSStyleSheetImpl((DOM::CSSStyleSheetImpl * ) 0);
        quirksSheet->parseString( str );

        // Collect only quirks-mode rules.
        defaultQuirksStyle = new CSSRuleSet();
        defaultQuirksStyle->addRulesFromSheet( quirksSheet, "screen" );
    }

    //kdDebug() << "CSSStyleSelector: default style has " << defaultStyle->count() << " elements"<< endl;
}

void CSSStyleSelector::addMatchedRule(CSSRuleData* rule)
{
    if (m_matchedRules.size() <= m_matchedRuleCount)
        m_matchedRules.resize(2*m_matchedRules.size()+1);
    m_matchedRules[m_matchedRuleCount++] = rule;
}

void CSSStyleSelector::addMatchedDeclaration(CSSMutableStyleDeclarationImpl* decl)
{
    if (m_matchedDecls.size() <= m_matchedDeclCount)
        m_matchedDecls.resize(2*m_matchedDecls.size()+1);
    m_matchedDecls[m_matchedDeclCount++] = decl;
}

void CSSStyleSelector::matchRules(CSSRuleSet* rules, int& firstRuleIndex, int& lastRuleIndex)
{
    m_matchedRuleCount = 0;
    firstRuleIndex = lastRuleIndex = -1;
    if (!rules || !element) return;
    
    // We need to collect the rules for id, class, tag, and everything else into a buffer and
    // then sort the buffer.
    if (element->hasID())
        matchRulesForList(rules->getIDRules(element->getIDAttribute().implementation()),
                          firstRuleIndex, lastRuleIndex);
    if (element->hasClass()) {
        for (const AtomicStringList* singleClass = element->getClassList();
             singleClass; singleClass = singleClass->next())
            matchRulesForList(rules->getClassRules(singleClass->string().implementation()),
                                                   firstRuleIndex, lastRuleIndex);
    }
    matchRulesForList(rules->getTagRules((void*)(int)localNamePart(element->id())),
                      firstRuleIndex, lastRuleIndex);
    matchRulesForList(rules->getUniversalRules(), firstRuleIndex, lastRuleIndex);
    
    // If we didn't match any rules, we're done.
    if (m_matchedRuleCount == 0) return;
    
    // Sort the set of matched rules.
    sortMatchedRules(0, m_matchedRuleCount);
    
    // Now transfer the set of matched rules over to our list of decls.
    for (unsigned i = 0; i < m_matchedRuleCount; i++)
        addMatchedDeclaration(m_matchedRules[i]->rule()->declaration());
}

void CSSStyleSelector::matchRulesForList(CSSRuleDataList* rules,
                                         int& firstRuleIndex, int& lastRuleIndex)
{
    if (!rules) return;
    for (CSSRuleData* d = rules->first(); d; d = d->next()) {
        CSSStyleRuleImpl* rule = d->rule();
        Q_UINT16 cssTagId = localNamePart(element->id());
        Q_UINT16 tag = localNamePart(d->selector()->tag);
        if ((cssTagId == tag || tag == anyLocalName) && checkSelector(d->selector(), element)) {
            // If the rule has no properties to apply, then ignore it.
            CSSMutableStyleDeclarationImpl* decl = rule->declaration();
            if (!decl || !decl->length()) continue;
            
            // If we're matching normal rules, set a pseudo bit if 
            // we really just matched a pseudo-element.
            if (dynamicPseudo != RenderStyle::NOPSEUDO && pseudoStyle == RenderStyle::NOPSEUDO)
                style->setHasPseudoStyle(dynamicPseudo);
            else {
                // Update our first/last rule indices in the matched rules array.
                lastRuleIndex = m_matchedDeclCount + m_matchedRuleCount;
                if (firstRuleIndex == -1) firstRuleIndex = m_matchedDeclCount + m_matchedRuleCount;

                // Add this rule to our list of matched rules.
                addMatchedRule(d);
            }
        }
    }
}

bool operator >(CSSRuleData& r1, CSSRuleData& r2)
{
    int spec1 = r1.selector()->specificity();
    int spec2 = r2.selector()->specificity();
    return (spec1 == spec2) ? r1.position() > r2.position() : spec1 > spec2; 
}
bool operator <=(CSSRuleData& r1, CSSRuleData& r2)
{
    return !(r1 > r2);
}

void CSSStyleSelector::sortMatchedRules(uint start, uint end)
{
    if (start >= end || (end-start == 1))
        return; // Sanity check.
    
    if (end - start <= 6) {
        // Apply a bubble sort for smaller lists.
        for (uint i = end-1; i > start; i--) {
            bool sorted = true;
            for (uint j = start; j < i; j++) {
                CSSRuleData* elt = m_matchedRules[j];
                CSSRuleData* elt2 = m_matchedRules[j+1];
                if (*elt > *elt2) {
                    sorted = false;
                    m_matchedRules[j] = elt2;
                    m_matchedRules[j+1] = elt;
                }
            }
            if (sorted)
                return;
        }
    }
    else {
        // Peform a merge sort for larger lists.
        uint mid = (start+end)/2;
        sortMatchedRules(start, mid);
        sortMatchedRules(mid, end);
        
        CSSRuleData* elt = m_matchedRules[mid-1];
        CSSRuleData* elt2 = m_matchedRules[mid];
        
        // Handle the fast common case (of equal specificity).  The list may already
        // be completely sorted.
        if (*elt <= *elt2)
            return;
        
        // We have to merge sort.  Ensure our merge buffer is big enough to hold
        // all the items.
        m_tmpRules.resize(end - start);
        uint i1 = start;
        uint i2 = mid;
        
        elt = m_matchedRules[i1];
        elt2 = m_matchedRules[i2];
        
        while (i1 < mid || i2 < end) {
            if (i1 < mid && (i2 == end || *elt <= *elt2)) {
                m_tmpRules[m_tmpRuleCount++] = elt;
                i1++;
                if (i1 < mid)
                    elt = m_matchedRules[i1];
            }
            else {
                m_tmpRules[m_tmpRuleCount++] = elt2;
                i2++;
                if (i2 < end)
                    elt2 = m_matchedRules[i2];
            }
        }
        
        for (uint i = start; i < end; i++)
            m_matchedRules[i] = m_tmpRules[i-start];
        
        m_tmpRuleCount = 0;
    }    
}

void CSSStyleSelector::initElementAndPseudoState(ElementImpl* e)
{
    element = e;
    if (element && element->isHTMLElement())
        htmlElement = static_cast<HTMLElementImpl*>(element);
    else
        htmlElement = 0;
    ::encodedurl = &encodedurl;
    pseudoState = PseudoUnknown;
}

void CSSStyleSelector::initForStyleResolve(ElementImpl* e, RenderStyle* defaultParent)
{
    // set some variables we will need
    pseudoStyle = RenderStyle::NOPSEUDO;

    parentNode = e->parentNode();
    if (defaultParent)
        parentStyle = defaultParent;
    else
        parentStyle = (parentNode && parentNode->renderer()) ? parentNode->renderer()->style() : 0;
    view = element->getDocument()->view();
    isXMLDoc = !element->getDocument()->isHTMLDocument();
    part = element->getDocument()->part();
    settings = part ? part->settings() : 0;
    paintDeviceMetrics = element->getDocument()->paintDeviceMetrics();
    
    style = 0;
    
    m_matchedRuleCount = 0;
    m_matchedDeclCount = 0;
    m_tmpRuleCount = 0;
    
    fontDirty = false;
}

// modified version of the one in kurl.cpp
static void cleanpath(QString &path)
{
    int pos;
    while ( (pos = path.find( "/../" )) != -1 ) {
        int prev = 0;
        if ( pos > 0 )
            prev = path.findRev( "/", pos -1 );
        // don't remove the host, i.e. http://foo.org/../foo.html
        if (prev < 0 || (prev > 3 && path.findRev("://", prev-1) == prev-2))
            path.remove( pos, 3);
        else
            // matching directory found ?
            path.remove( prev, pos- prev + 3 );
    }
    pos = 0;
    
    // Don't remove "//" from an anchor identifier. -rjw
    // Set refPos to -2 to mean "I haven't looked for the anchor yet".
    // We don't want to waste a function call on the search for the the anchor
    // in the vast majority of cases where there is no "//" in the path.
    int refPos = -2;
    while ( (pos = path.find( "//", pos )) != -1) {
        if (refPos == -2)
            refPos = path.find("#", 0);
        if (refPos > 0 && pos >= refPos)
            break;
        
        if ( pos == 0 || path[pos-1] != ':' )
            path.remove( pos, 1 );
        else
            pos += 2;
    }
    while ( (pos = path.find( "/./" )) != -1)
        path.remove( pos, 2 );
    //kdDebug() << "checkPseudoState " << path << endl;
}

static void checkPseudoState( DOM::ElementImpl *e, bool checkVisited = true )
{
    if (!e->hasAnchor()) {
        pseudoState = PseudoNone;
        return;
    }
    
    const AtomicString& attr = e->getAttribute(ATTR_HREF);
    if (attr.isNull()) {
        pseudoState = PseudoNone;
        return;
    }
    
    if (!checkVisited) {
        pseudoState = PseudoAnyLink;
        return;
    }
    
    QConstString cu(attr.unicode(), attr.length());
    QString u = cu.string();
    if ( !u.contains("://") ) {
        if ( u[0] == '/' )
            u.prepend(encodedurl->host);
        else if ( u[0] == '#' )
            u.prepend(encodedurl->file);
        else
            u.prepend(encodedurl->path);
        cleanpath( u );
    }
    //completeURL( attr.string() );
    pseudoState = KHTMLFactory::vLinks()->contains( u ) ? PseudoVisited : PseudoLink;
}

#ifdef STYLE_SHARING_STATS
static int fraction = 0;
static int total = 0;
#endif

const int siblingThreshold = 10;

NodeImpl* CSSStyleSelector::locateCousinList(ElementImpl* parent)
{
    if (parent && parent->isHTMLElement()) {
        HTMLElementImpl* p = static_cast<HTMLElementImpl*>(parent);
        if (p->renderer() && !p->inlineStyleDecl() && !p->hasID()) {
            DOM::NodeImpl* r = p->previousSibling();
            int subcount = 0;
            RenderStyle* st = p->renderer()->style();
            while (r) {
                if (r->renderer() && r->renderer()->style() == st)
                    return r->lastChild();
                if (subcount++ == siblingThreshold)
                    return 0;
                r = r->previousSibling();
            }
            if (!r)
                r = locateCousinList(static_cast<ElementImpl*>(parent->parentNode()));
            while (r) {
                if (r->renderer() && r->renderer()->style() == st)
                    return r->lastChild();
                if (subcount++ == siblingThreshold)
                    return 0;
                r = r->previousSibling();
            }
        }
    }
    return 0;
}

bool CSSStyleSelector::canShareStyleWithElement(NodeImpl* n)
{
    if (n->isHTMLElement()) {
        bool mouseInside = element->renderer() ? element->renderer()->mouseInside() : false;
        HTMLElementImpl* s = static_cast<HTMLElementImpl*>(n);
        if (s->renderer() && (s->id() == element->id()) && !s->hasID() &&
            (s->hasClass() == element->hasClass()) && !s->inlineStyleDecl() &&
            (s->hasMappedAttributes() == htmlElement->hasMappedAttributes()) &&
            (s->hasAnchor() == element->hasAnchor()) && 
            !s->renderer()->style()->affectedByAttributeSelectors() &&
            (s->renderer()->mouseInside() == mouseInside) &&
            (s->active() == element->active()) &&
            (s->focused() == element->focused())) {
            bool classesMatch = true;
            if (s->hasClass()) {
                const AtomicString& class1 = element->getAttribute(ATTR_CLASS);
                const AtomicString& class2 = s->getAttribute(ATTR_CLASS);
                classesMatch = (class1 == class2);
            }
            
            if (classesMatch) {
                bool mappedAttrsMatch = true;
                if (s->hasMappedAttributes())
                    mappedAttrsMatch = s->htmlAttributes()->mapsEquivalent(htmlElement->htmlAttributes());
                if (mappedAttrsMatch) {
                    bool anchorsMatch = true;
                    if (s->hasAnchor()) {
                        // We need to check to see if the visited state matches.
                        QColor linkColor = element->getDocument()->linkColor();
                        QColor visitedColor = element->getDocument()->visitedLinkColor();
                        if (pseudoState == PseudoUnknown)
                            checkPseudoState(element, s->renderer()->style()->pseudoState() != PseudoAnyLink ||
                                             linkColor != visitedColor);
                        anchorsMatch = (pseudoState == s->renderer()->style()->pseudoState());
                    }
                    
                    if (anchorsMatch)
                        return true;
                }
            }
        }
    }
    return false;
}

RenderStyle* CSSStyleSelector::locateSharedStyle()
{
    if (htmlElement && !htmlElement->inlineStyleDecl() && !htmlElement->hasID() &&
        !htmlElement->getDocument()->usesSiblingRules()) {
        // Check previous siblings.
        int count = 0;
        DOM::NodeImpl* n;
        for (n = element->previousSibling(); n && !n->isElementNode(); n = n->previousSibling());
        while (n) {
            if (canShareStyleWithElement(n))
                return n->renderer()->style();
            if (count++ == siblingThreshold)
                return 0;
            for (n = n->previousSibling(); n && !n->isElementNode(); n = n->previousSibling());
        }
        if (!n) 
            n = locateCousinList(static_cast<ElementImpl*>(element->parentNode()));
        while (n) {
            if (canShareStyleWithElement(n))
                return n->renderer()->style();
            if (count++ == siblingThreshold)
                return 0;
            for (n = n->previousSibling(); n && !n->isElementNode(); n = n->previousSibling());
        }        
    }
    return 0;
}


RenderStyle* CSSStyleSelector::styleForElement(ElementImpl* e, RenderStyle* defaultParent, bool allowSharing)
{
    if (!e->getDocument()->haveStylesheetsLoaded()) {
        if (!styleNotYetAvailable) {
            styleNotYetAvailable = ::new RenderStyle();
            styleNotYetAvailable->setDisplay(NONE);
            styleNotYetAvailable->ref();
        }
        return styleNotYetAvailable;
    }
    
    initElementAndPseudoState(e);
    if (allowSharing) {
        style = locateSharedStyle();
#ifdef STYLE_SHARING_STATS
        fraction += style != 0;
        total++;
        printf("Sharing %d out of %d\n", fraction, total);
#endif
        if (style)
            return style;
    }
    initForStyleResolve(e, defaultParent);

    style = new (e->getDocument()->renderArena()) RenderStyle();
    if (parentStyle)
        style->inheritFrom(parentStyle);
    else
        parentStyle = style;
    
    // 1. First we match rules from the user agent sheet.
    int firstUARule = -1, lastUARule = -1;
    matchRules(defaultStyle, firstUARule, lastUARule);
    
    // 2. In quirks mode, we match rules from the quirks user agent sheet.
    if (!strictParsing)
        matchRules(defaultQuirksStyle, firstUARule, lastUARule);
    
    // 3. If our medium is print, then we match rules from the print sheet.
    if (m_medium == "print")
        matchRules(defaultPrintStyle, firstUARule, lastUARule);
    
    // 4. Now we check user sheet rules.
    int firstUserRule = -1, lastUserRule = -1;
    matchRules(m_userStyle, firstUserRule, lastUserRule);

    // 5. Now check author rules, beginning first with presentational attributes
    // mapped from HTML.
    int firstAuthorRule = -1, lastAuthorRule = -1;
    if (htmlElement) {
        // Ask if the HTML element has mapped attributes.
        if (htmlElement->hasMappedAttributes()) {
            // Walk our attribute list and add in each decl.
            const HTMLNamedAttrMapImpl* map = htmlElement->htmlAttributes();
            for (uint i = 0; i < map->length(); i++) {
                HTMLAttributeImpl* attr = map->attributeItem(i);
                if (attr->decl()) {
                    if (firstAuthorRule == -1) firstAuthorRule = m_matchedDeclCount;
                    lastAuthorRule = m_matchedDeclCount;
                    addMatchedDeclaration(attr->decl());
                }
            }
        }

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        CSSMutableStyleDeclarationImpl* attributeDecl = htmlElement->additionalAttributeStyleDecl();
        if (attributeDecl) {
            if (firstAuthorRule == -1) firstAuthorRule = m_matchedDeclCount;
            lastAuthorRule = m_matchedDeclCount;
            addMatchedDeclaration(attributeDecl);
        }
    }
    
    // 6. Check the rules in author sheets next.
    matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);
    
    // 7. Now check our inline style attribute.
    if (htmlElement) {
        CSSMutableStyleDeclarationImpl* inlineDecl = htmlElement->inlineStyleDecl();
        if (inlineDecl) {
            if (firstAuthorRule == -1) firstAuthorRule = m_matchedDeclCount;
            lastAuthorRule = m_matchedDeclCount;
            addMatchedDeclaration(inlineDecl);
        }
    }
    
    // Now we have all of the matched rules in the appropriate order.  Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    applyDeclarations(true, false, 0, m_matchedDeclCount-1);
    applyDeclarations(true, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(true, true, firstUserRule, lastUserRule);
    applyDeclarations(true, true, firstUARule, lastUARule);
    
    // If our font got dirtied, go ahead and update it now.
    if (fontDirty) {
        checkForTextSizeAdjust();
        checkForGenericFamilyChange(style, parentStyle);
        style->htmlFont().update(paintDeviceMetrics);
        fontDirty = false;
    }
    
    // Now do the normal priority properties.
    applyDeclarations(false, false, 0, m_matchedDeclCount-1);
    applyDeclarations(false, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(false, true, firstUserRule, lastUserRule);
    applyDeclarations(false, true, firstUARule, lastUARule);
    
    // If our font got dirtied by one of the non-essential font props, 
    // go ahead and update it a second time.
    if (fontDirty) {
        checkForTextSizeAdjust();
        checkForGenericFamilyChange(style, parentStyle);
        style->htmlFont().update(paintDeviceMetrics);
        fontDirty = false;
    }
    
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style, e);

    // If we are a link, cache the determined pseudo-state.
    if (e->hasAnchor())
        style->setPseudoState(pseudoState);

    // Now return the style.
    return style;
}

RenderStyle* CSSStyleSelector::pseudoStyleForElement(RenderStyle::PseudoId pseudo, 
                                                     ElementImpl* e, RenderStyle* parentStyle)
{
    if (!e)
        return 0;

    initElementAndPseudoState(e);
    initForStyleResolve(e, parentStyle);
    pseudoStyle = pseudo;
    
    // Since we don't use pseudo-elements in any of our quirk/print user agent rules, don't waste time walking
    // those rules.
    
    // Check UA, user and author rules.
    int firstUARule = -1, lastUARule = -1, firstUserRule = -1, lastUserRule = -1, firstAuthorRule = -1, lastAuthorRule = -1;
    matchRules(defaultStyle, firstUARule, lastUARule);
    matchRules(m_userStyle, firstUserRule, lastUserRule);
    matchRules(m_authorStyle, firstAuthorRule, lastAuthorRule);
    
    if (m_matchedDeclCount == 0)
        return 0;
    
    style = new (e->getDocument()->renderArena()) RenderStyle();
    if (parentStyle)
        style->inheritFrom(parentStyle);
    else
        parentStyle = style;
    style->noninherited_flags._styleType = pseudoStyle;
    
    // High-priority properties.
    applyDeclarations(true, false, 0, m_matchedDeclCount-1);
    applyDeclarations(true, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(true, true, firstUserRule, lastUserRule);
    applyDeclarations(true, true, firstUARule, lastUARule);
    
    // If our font got dirtied, go ahead and update it now.
    if (fontDirty) {
        checkForTextSizeAdjust();
        checkForGenericFamilyChange(style, parentStyle);
        style->htmlFont().update(paintDeviceMetrics);
        fontDirty = false;
    }
    
    // Now do the normal priority properties.
    applyDeclarations(false, false, 0, m_matchedDeclCount-1);
    applyDeclarations(false, true, firstAuthorRule, lastAuthorRule);
    applyDeclarations(false, true, firstUserRule, lastUserRule);
    applyDeclarations(false, true, firstUARule, lastUARule);
    
    // If our font got dirtied by one of the non-essential font props, 
    // go ahead and update it a second time.
    if (fontDirty) {
        checkForTextSizeAdjust();
        checkForGenericFamilyChange(style, parentStyle);
        style->htmlFont().update(paintDeviceMetrics);
        fontDirty = false;
    }
    
    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(style, 0);

    // Now return the style.
    return style;
}

void CSSStyleSelector::adjustRenderStyle(RenderStyle* style, DOM::ElementImpl *e)
{
    // Cache our original display.
    style->setOriginalDisplay(style->display());

    if (style->display() != NONE) {
        // If we have a <td> that specifies a float property, in quirks mode we just drop the float
        // property.
        // Sites also commonly use display:inline/block on <td>s and <table>s.  In quirks mode we force
        // these tags to retain their display types.
        if (!strictParsing && e) {
            if (e->id() == ID_TD) {
                style->setDisplay(TABLE_CELL);
                style->setFloating(FNONE);
            }
            else if (e->id() == ID_TABLE)
                style->setDisplay(style->isDisplayInlineType() ? INLINE_TABLE : TABLE);
        }

        // Frames and framesets never honor position:relative or position:absolute.  This is necessary to
        // fix a crash where a site tries to position these objects.  They also never honor display.
        if (e && (e->id() == ID_FRAME || e->id() == ID_FRAMESET)) {
            style->setPosition(STATIC);
            style->setDisplay(BLOCK);
        }

        // Table headers with a text-align of auto will change the text-align to center.
        if (e && e->id() == ID_TH && style->textAlign() == TAAUTO)
            style->setTextAlign(CENTER);
        
        // Mutate the display to BLOCK or TABLE for certain cases, e.g., if someone attempts to
        // position or float an inline, compact, or run-in.  Cache the original display, since it
        // may be needed for positioned elements that have to compute their static normal flow
        // positions.  We also force inline-level roots to be block-level.
        if (style->display() != BLOCK && style->display() != TABLE && style->display() != BOX &&
            (style->position() == ABSOLUTE || style->position() == FIXED || style->floating() != FNONE ||
             (e && e->getDocument()->documentElement() == e))) {
            if (style->display() == INLINE_TABLE)
                style->setDisplay(TABLE);
            else if (style->display() == INLINE_BOX)
                style->setDisplay(BOX);
            else if (style->display() == LIST_ITEM) {
                // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk,
                // but only in quirks mode.
                if (!strictParsing && style->floating() != FNONE)
                    style->setDisplay(BLOCK);
            }
            else
                style->setDisplay(BLOCK);
        }
        
        // After performing the display mutation, check table rows.  We do not honor position:relative on
        // table rows.  This has been established in CSS2.1 (and caused a crash in containingBlock() on
        // some sites).
        if (style->display() == TABLE_ROW && style->position() == RELATIVE)
            style->setPosition(STATIC);
    }

    // Make sure our z-index value is only applied if the object is positioned,
    // relatively positioned, or transparent.
    if (style->position() == STATIC && style->opacity() == 1.0f) {
        if (e && e->getDocument()->documentElement() == e)
            style->setZIndex(0); // The root has a z-index of 0 if not positioned or transparent.
        else
            style->setHasAutoZIndex(); // Everyone else gets an auto z-index.
    }

    // Auto z-index becomes 0 for transparent objects.  This prevents cases where
    // objects that should be blended as a single unit end up with a non-transparent object
    // wedged in between them.
    if (style->opacity() < 1.0f && style->hasAutoZIndex())
        style->setZIndex(0);
    
    // Finally update our text decorations in effect, but don't allow text-decoration to percolate through
    // tables, inline blocks, inline tables, or run-ins.
    if (style->display() == TABLE || style->display() == INLINE_TABLE || style->display() == RUN_IN
        || style->display() == INLINE_BLOCK || style->display() == INLINE_BOX)
        style->setTextDecorationsInEffect(style->textDecoration());
    else
        style->addToTextDecorationsInEffect(style->textDecoration());
    
    // Cull out any useless layers and also repeat patterns into additional layers.
    style->adjustBackgroundLayers();

    // Only use slow repaints if we actually have a background image.
    // FIXME: We only need to invalidate the fixed regions when scrolling.  It's total overkill to
    // prevent the entire view from blitting on a scroll.
    if (style->hasFixedBackgroundImage() && view)
        view->useSlowRepaints();
}

static bool subject;

bool CSSStyleSelector::checkSelector(CSSSelector* sel, ElementImpl *e)
{
    dynamicPseudo = RenderStyle::NOPSEUDO;
    
    NodeImpl *n = e;

    // we have the subject part of the selector
    subject = true;

    // We track whether or not the rule contains only :hover and :active in a simple selector. If
    // so, we can't allow that to apply to every element on the page.  We assume the author intended
    // to apply the rules only to links.
    bool onlyHoverActive = (sel->tag == anyQName &&
                            (sel->match == CSSSelector::Pseudo &&
                              (sel->pseudoType() == CSSSelector::PseudoHover ||
                               sel->pseudoType() == CSSSelector::PseudoActive)));
    bool affectedByHover = style ? style->affectedByHoverRules() : false;
    bool affectedByActive = style ? style->affectedByActiveRules() : false;
    bool havePseudo = pseudoStyle != RenderStyle::NOPSEUDO;
    
    // first selector has to match
    if (!checkOneSelector(sel, e)) return false;

    // check the subselectors
    CSSSelector::Relation relation = sel->relation;
    while((sel = sel->tagHistory))
    {
        if (!n->isElementNode()) return false;
        if (relation != CSSSelector::SubSelector) {
            subject = false;
            if (havePseudo && dynamicPseudo != pseudoStyle)
                return false;
        }
        
        switch(relation)
        {
        case CSSSelector::Descendant:
        {
            // FIXME: This match needs to know how to backtrack and be non-deterministic.
            bool found = false;
            while(!found)
            {
		n = n->parentNode();
                if(!n || !n->isElementNode()) return false;
                ElementImpl *elem = static_cast<ElementImpl *>(n);
                if (checkOneSelector(sel, elem)) found = true;
            }
            break;
        }
        case CSSSelector::Child:
        {
            n = n->parentNode();
            if (!strictParsing)
                while (n && n->implicitNode()) n = n->parentNode();
            if(!n || !n->isElementNode()) return false;
            ElementImpl *elem = static_cast<ElementImpl *>(n);
            if (!checkOneSelector(sel, elem)) return false;
            break;
        }
        case CSSSelector::Sibling:
        {
            n = n->previousSibling();
	    while( n && !n->isElementNode() )
		n = n->previousSibling();
            if( !n ) return false;
            ElementImpl *elem = static_cast<ElementImpl *>(n);
            if (!checkOneSelector(sel, elem)) return false;
            break;
        }
        case CSSSelector::SubSelector:
	{
            if (onlyHoverActive)
                onlyHoverActive = (sel->match == CSSSelector::Pseudo &&
                                   (sel->pseudoType() == CSSSelector::PseudoHover ||
                                    sel->pseudoType() == CSSSelector::PseudoActive));
            
	    //kdDebug() << "CSSOrderedRule::checkSelector" << endl;
	    ElementImpl *elem = static_cast<ElementImpl *>(n);
	    // a selector is invalid if something follows :first-xxx
	    if (dynamicPseudo != RenderStyle::NOPSEUDO)
		return false;
	    if (!checkOneSelector(sel, elem)) return false;
	    //kdDebug() << "CSSOrderedRule::checkSelector: passed" << endl;
	    break;
	}
        }
        relation = sel->relation;
    }

    if (subject && havePseudo && dynamicPseudo != pseudoStyle)
        return false;
    
    // disallow *:hover, *:active, and *:hover:active except for links
    if (onlyHoverActive && subject) {
        if (pseudoState == PseudoUnknown)
            checkPseudoState(e);

        if (pseudoState == PseudoNone) {
            if (!affectedByHover && style->affectedByHoverRules())
                style->setAffectedByHoverRules(false);
            if (!affectedByActive && style->affectedByActiveRules())
                style->setAffectedByActiveRules(false);
            return false;
        }
    }

    return true;
}

bool CSSStyleSelector::checkOneSelector(DOM::CSSSelector *sel, DOM::ElementImpl *e)
{
    if(!e)
        return false;

    if (sel->tag != anyQName) {
        int eltID = e->id();
        Q_UINT16 localName = localNamePart(eltID);
        Q_UINT16 ns = namespacePart(eltID);
        Q_UINT16 selLocalName = localNamePart(sel->tag);
        Q_UINT16 selNS = namespacePart(sel->tag);
        
        if (localName <= ID_LAST_TAG && e->isHTMLElement())
            ns = xhtmlNamespace; // FIXME: Really want to move away from this complicated hackery and just
                                 // switch tags and attr names over to AtomicStrings.
        
        if ((selLocalName != anyLocalName && localName != selLocalName) ||
            (selNS != anyNamespace && ns != selNS))
            return false;
    }

    if (sel->attr) {
        if (sel->match == CSSSelector::Class) {
            if (!e->hasClass())
                return false;
            for (const AtomicStringList* c = e->getClassList(); c; c = c->next())
                if (c->string() == sel->value)
                    return true;
            return false;
        }
        else if (sel->match == CSSSelector::Id)
            return e->hasID() && e->getIDAttribute() == sel->value;
        else if (style && (e != element || !htmlElement || !htmlElement->isMappedAttribute(sel->attr)))
            style->setAffectedByAttributeSelectors();

        const AtomicString& value = e->getAttribute(sel->attr);
        if (value.isNull()) return false; // attribute is not set

        switch(sel->match) {
        case CSSSelector::Exact:
	    if ((isXMLDoc && sel->value != value) ||
                (!isXMLDoc && !equalsIgnoreCase(sel->value, value)))
                return false;
            break;
        case CSSSelector::List:
        {
            int spacePos = value.find(' ', 0);
            if (spacePos == -1) {
                // There is no list, just a single item.  We can avoid
                // allocing QStrings and just treat this as an exact
                // match check.
                if ((isXMLDoc && sel->value != value) ||
                    (!isXMLDoc && !equalsIgnoreCase(sel->value, value)))
                    return false;
                break;
            }

            // The selector's value can't contain a space, or it's totally bogus.
            spacePos = sel->value.find(' ');
            if (spacePos != -1)
                return false;

            QString str = value.string();
            QString selStr = sel->value.string();
            int startSearchAt = 0;
            while (true) {
                int foundPos = str.find(selStr, startSearchAt, isXMLDoc);
                if (foundPos == -1) return false;
                if (foundPos == 0 || str[foundPos-1] == ' ') {
                    uint endStr = foundPos + selStr.length();
                    if (endStr == str.length() || str[endStr] == ' ')
                        break; // We found a match.
                }
                
                // No match.  Keep looking.
                startSearchAt = foundPos + 1;
            }

            break;
        }
        case CSSSelector::Contain:
        {
            //kdDebug( 6080 ) << "checking for contains match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
            int pos = str.find(selStr, 0, isXMLDoc);
            if(pos == -1) return false;
            break;
        }
        case CSSSelector::Begin:
        {
            //kdDebug( 6080 ) << "checking for beginswith match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
            int pos = str.find(selStr, 0, isXMLDoc);
            if(pos != 0) return false;
            break;
        }
        case CSSSelector::End:
        {
            //kdDebug( 6080 ) << "checking for endswith match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
	    if (isXMLDoc && !str.endsWith(selStr)) return false;
	    if (!isXMLDoc) {
	        int pos = str.length() - selStr.length();
		if (pos < 0 || pos != str.find(selStr, pos, false) )
		    return false;
	    }
            break;
        }
        case CSSSelector::Hyphen:
        {
            //kdDebug( 6080 ) << "checking for hyphen match" << endl;
            QString str = value.string();
            QString selStr = sel->value.string();
            if(str.length() < selStr.length()) return false;
            // Check if str begins with selStr:
            if(str.find(selStr, 0, isXMLDoc) != 0) return false;
            // It does. Check for exact match or following '-':
            if(str.length() != selStr.length()
                && str[selStr.length()] != '-') return false;
            break;
        }
        default:
            break;
        }
    }
    if(sel->match == CSSSelector::Pseudo)
    {
        // Pseudo elements. We need to check first child here. No dynamic pseudo
        // elements for the moment
//	kdDebug() << "CSSOrderedRule::pseudo " << value << endl;
	switch (sel->pseudoType()) {
            case CSSSelector::PseudoEmpty:
                if (!e->firstChild())
                    return true;
                break;
            case CSSSelector::PseudoFirstChild: {
                // first-child matches the first child that is an element!
                if (e->parentNode()) {
                    DOM::NodeImpl* n = e->previousSibling();
                    while ( n && !n->isElementNode() )
                        n = n->previousSibling();
                    if ( !n )
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoLastChild: {
                // last-child matches the last child that is an element!
                if (e->parentNode()) {
                    DOM::NodeImpl* n = e->nextSibling();
                    while ( n && !n->isElementNode() )
                        n = n->nextSibling();
                    if ( !n )
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoOnlyChild: {
                // If both first-child and last-child apply, then only-child applies.
                if (e->parentNode()) {
                    DOM::NodeImpl* n = e->previousSibling();
                    while ( n && !n->isElementNode() )
                        n = n->previousSibling();
                    if ( !n ) {
                        n = e->nextSibling();
                        while ( n && !n->isElementNode() )
                            n = n->nextSibling();
                        if ( !n )
                            return true;
                    }
                }
                break;
            }
            case CSSSelector::PseudoFirstLine:
                if ( subject ) {
                    dynamicPseudo=RenderStyle::FIRST_LINE;
                    return true;
                }
                break;
            case CSSSelector::PseudoFirstLetter:
                if ( subject ) {
                    dynamicPseudo=RenderStyle::FIRST_LETTER;
                    return true;
                }
                break;
            case CSSSelector::PseudoTarget:
                if (e == e->getDocument()->getCSSTarget())
                    return true;
                break;
            case CSSSelector::PseudoAnyLink:
                if (pseudoState == PseudoUnknown)
                    checkPseudoState(e, false);
                if (pseudoState == PseudoAnyLink || pseudoState == PseudoLink || pseudoState == PseudoVisited)
                    return true;
                break;
            case CSSSelector::PseudoLink:
                if ( pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink )
                    checkPseudoState( e );
                if ( pseudoState == PseudoLink )
                    return true;
                break;
            case CSSSelector::PseudoVisited:
                if ( pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink )
                    checkPseudoState( e );
                if ( pseudoState == PseudoVisited )
                    return true;
                break;
            case CSSSelector::PseudoHover: {
                // If we're in quirks mode, then hover should never match anchors with no
                // href.  This is important for sites like wsj.com.
                if (strictParsing || e->id() != ID_A || e->hasAnchor()) {
                    if (element == e && style)
                        style->setAffectedByHoverRules(true);
                    if (e->renderer()) {
                        if (element != e)
                            e->renderer()->style()->setAffectedByHoverRules(true);
                        if (e->renderer()->mouseInside())
                            return true;
                    }
                }
                break;
            }
            case CSSSelector::PseudoDrag: {
                if (element == e && style)
                    style->setAffectedByDragRules(true);
                if (e->renderer()) {
                    if (element != e)
                        e->renderer()->style()->setAffectedByDragRules(true);
                    if (e->renderer()->isDragging())
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoFocus:
                if (e && e->focused()) {
                    return true;
                }
                break;
            case CSSSelector::PseudoActive:
                // If we're in quirks mode, then :active should never match anchors with no
                // href. 
                if (strictParsing || e->id() != ID_A || e->hasAnchor()) {
                    if (element == e && style)
                        style->setAffectedByActiveRules(true);
                    else if (e->renderer())
                        e->renderer()->style()->setAffectedByActiveRules(true);
                    if (e->active())
                        return true;
                }
                break;
            case CSSSelector::PseudoRoot:
                if (e == e->getDocument()->documentElement())
                    return true;
                break;
            case CSSSelector::PseudoNot: {
                // check the simple selector
                for (CSSSelector* subSel = sel->simpleSelector; subSel;
                     subSel = subSel->tagHistory) {
                    // :not cannot nest.  I don't really know why this is a restriction in CSS3,
                    // but it is, so let's honor it.
                    if (subSel->simpleSelector)
                        break;
                    if (!checkOneSelector(subSel, e))
                        return true;
                }
                break;
            }
            case CSSSelector::PseudoSelection:
                dynamicPseudo = RenderStyle::SELECTION;
                return true;
            case CSSSelector::PseudoBefore:
                dynamicPseudo = RenderStyle::BEFORE;
                return true;
            case CSSSelector::PseudoAfter:
                dynamicPseudo = RenderStyle::AFTER;
                return true;
                
            case CSSSelector::PseudoNotParsed:
                assert(false);
                break;
            case CSSSelector::PseudoLang:
                /* not supported for now */
            case CSSSelector::PseudoOther:
                break;
        }
	return false;
    }
    // ### add the rest of the checks...
    return true;
}

// -----------------------------------------------------------------

CSSRuleSet::CSSRuleSet()
{
    m_idRules.setAutoDelete(true);
    m_classRules.setAutoDelete(true);
    m_tagRules.setAutoDelete(true);
    m_universalRules = 0;
    m_ruleCount = 0;
}

void CSSRuleSet::addToRuleSet(void* hash, QPtrDict<CSSRuleDataList>& dict,
                              CSSStyleRuleImpl* rule, CSSSelector* sel)
{
    if (!hash) return;
    CSSRuleDataList* rules = dict.find(hash);
    if (!rules) {
        rules = new CSSRuleDataList(m_ruleCount++, rule, sel);
        dict.insert(hash, rules);
    }
    else
        rules->append(m_ruleCount++, rule, sel);
}

void CSSRuleSet::addRule(CSSStyleRuleImpl* rule, CSSSelector* sel)
{
    if (sel->match == CSSSelector::Id) {
        addToRuleSet(sel->value.implementation(), m_idRules, rule, sel);
        return;
    }
    if (sel->match == CSSSelector::Class) {
        addToRuleSet(sel->value.implementation(), m_classRules, rule, sel);
        return;
    }
     
    Q_UINT16 localName = localNamePart(sel->tag);
    if (localName != anyLocalName) {
        addToRuleSet((void*)(int)localName, m_tagRules, rule, sel);
        return;
    }
    
    // Just put it in the universal rule set.
    if (!m_universalRules)
        m_universalRules = new CSSRuleDataList(m_ruleCount++, rule, sel);
    else
        m_universalRules->append(m_ruleCount++, rule, sel);
}

void CSSRuleSet::addRulesFromSheet(CSSStyleSheetImpl *sheet, const DOMString &medium)
{
    if (!sheet || !sheet->isCSSStyleSheet()) return;

    // No media implies "all", but if a media list exists it must
    // contain our current medium
    if (sheet->media() && !sheet->media()->contains(medium))
        return; // the style sheet doesn't apply

    int len = sheet->length();

    for (int i = 0; i < len; i++) {
        StyleBaseImpl *item = sheet->item(i);
        if (item->isStyleRule()) {
            CSSStyleRuleImpl* rule = static_cast<CSSStyleRuleImpl*>(item);
            for (CSSSelector* s = rule->selector(); s; s = s->next())
                addRule(rule, s);
        }
        else if(item->isImportRule()) {
            CSSImportRuleImpl *import = static_cast<CSSImportRuleImpl *>(item);

            //kdDebug( 6080 ) << "@import: Media: "
            //                << import->media()->mediaText().string() << endl;

            if (!import->media() || import->media()->contains(medium))
                addRulesFromSheet(import->styleSheet(), medium);
        }
        else if(item->isMediaRule()) {
            CSSMediaRuleImpl *r = static_cast<CSSMediaRuleImpl*>(item);
            CSSRuleListImpl *rules = r->cssRules();

            //DOMString mediaText = media->mediaText();
            //kdDebug( 6080 ) << "@media: Media: "
            //                << r->media()->mediaText().string() << endl;

            if ((!r->media() || r->media()->contains(medium)) && rules) {
                // Traverse child elements of the @media rule.
                for (unsigned j = 0; j < rules->length(); j++) {
                    //kdDebug( 6080 ) << "*** Rule #" << j << endl;

                    CSSRuleImpl *childItem = rules->item(j);
                    if (childItem->isStyleRule()) {
                        // It is a StyleRule, so append it to our list
                        CSSStyleRuleImpl* rule = static_cast<CSSStyleRuleImpl*>(childItem);
                        for (CSSSelector* s = rule->selector(); s; s = s->next())
                            addRule(rule, s);
                        
                    }
                    else
                    {
                        //kdDebug( 6080 ) << "Ignoring child rule of "
                        //    "ImportRule: rule is not a StyleRule!" << endl;
                    }
                }   // for rules
            }   // if rules
            else
            {
                //kdDebug( 6080 ) << "CSSMediaRule not rendered: "
                //                << "rule empty or wrong medium!" << endl;
            }
        }
        // ### include other rules
    }
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

static Length convertToLength( CSSPrimitiveValueImpl *primitiveValue, RenderStyle *style, QPaintDeviceMetrics *paintDeviceMetrics, bool *ok = 0 )
{
    Length l;
    if ( !primitiveValue ) {
	if ( ok )
	    *ok = false;
    } else {
	int type = primitiveValue->primitiveType();
	if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
	    l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
	else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
	    l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)), Percent);
	else if(type == CSSPrimitiveValue::CSS_NUMBER)
	    l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)*100), Percent);
        else if (type == CSSPrimitiveValue::CSS_HTML_RELATIVE)
            l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_HTML_RELATIVE)), Relative);
	else if ( ok )
	    *ok = false;
    }
    return l;
}


// color mapping code
struct colorMap {
    int css_value;
    QRgb color;
};

static const colorMap cmap[] = {
    { CSS_VAL_AQUA, 0xFF00FFFF },
    { CSS_VAL_BLACK, 0xFF000000 },
    { CSS_VAL_BLUE, 0xFF0000FF },
    { CSS_VAL_FUCHSIA, 0xFFFF00FF },
    { CSS_VAL_GRAY, 0xFF808080 },
    { CSS_VAL_GREEN, 0xFF008000  },
    { CSS_VAL_LIME, 0xFF00FF00 },
    { CSS_VAL_MAROON, 0xFF800000 },
    { CSS_VAL_NAVY, 0xFF000080 },
    { CSS_VAL_OLIVE, 0xFF808000  },
    { CSS_VAL_ORANGE, 0xFFFFA500 },
    { CSS_VAL_PURPLE, 0xFF800080 },
    { CSS_VAL_RED, 0xFFFF0000 },
    { CSS_VAL_SILVER, 0xFFC0C0C0 },
    { CSS_VAL_TEAL, 0xFF008080  },
    { CSS_VAL_WHITE, 0xFFFFFFFF },
    { CSS_VAL_YELLOW, 0xFFFFFF00 },
#if !APPLE_CHANGES
    { CSS_VAL_INVERT, invertedColor },
#endif
    { CSS_VAL_TRANSPARENT, transparentColor },
    { CSS_VAL_GREY, 0xFF808080 },
#if APPLE_CHANGES
    { CSS_VAL_ACTIVEBORDER, 0xFFE0E0E0 },
    { CSS_VAL_ACTIVECAPTION, 0xFF000000 },
    { CSS_VAL_APPWORKSPACE, 0xFF000000 },
    { CSS_VAL_BUTTONFACE, 0xFFC0C0C0 },
    { CSS_VAL_BUTTONHIGHLIGHT, 0xFFE0E0E0 },
    { CSS_VAL_BUTTONSHADOW, 0xFFFFFFFF },
    { CSS_VAL_BUTTONTEXT, 0xFF000000 },
    { CSS_VAL_CAPTIONTEXT, 0xFF000000 },
    { CSS_VAL_GRAYTEXT, 0xFF000000 },
    { CSS_VAL_HIGHLIGHT, 0xFFFFFFFF },
    { CSS_VAL_HIGHLIGHTTEXT, 0xFFFFFFFF },
    { CSS_VAL_INACTIVEBORDER, 0xFFFFFFFF },
    { CSS_VAL_INACTIVECAPTION, 0xFFFFFFFF },
    { CSS_VAL_INACTIVECAPTIONTEXT, 0xFF000000 },
    { CSS_VAL_INFOBACKGROUND, 0xFF000000 },
    { CSS_VAL_INFOTEXT, 0xFF000000 },
    { CSS_VAL_MENU, 0xFFFFFFFF },
    { CSS_VAL_MENUTEXT, 0xFFFFFFFF },
    { CSS_VAL_SCROLLBAR, 0xFFFFFFFF },
    { CSS_VAL_TEXT, 0xFF000000 },
    { CSS_VAL_THREEDDARKSHADOW, 0xFF404040 },
    { CSS_VAL_THREEDFACE, 0xFFC0C0C0 },
    { CSS_VAL_THREEDHIGHLIGHT, 0xFFE0E0E0 },
    { CSS_VAL_THREEDLIGHTSHADOW, 0xFFC0C0C0 },
    { CSS_VAL_THREEDSHADOW, 0xFFFFFFFF },
    { CSS_VAL_WINDOW, 0xFFFFFFFF },
    { CSS_VAL_WINDOWFRAME, 0xFFFFFFFF },
    { CSS_VAL_WINDOWTEXT, 0xFF000000 },
#endif
    { 0, 0 }
};

#if !APPLE_CHANGES

struct uiColors {
    int css_value;
    const char * configGroup;
    const char * configEntry;
QPalette::ColorGroup group;
QColorGroup::ColorRole role;
};

const char * const wmgroup = "WM";
const char * const generalgroup = "General";

/* Mapping system settings to CSS 2
* Tried hard to get an appropriate mapping - schlpbch
*/
static const uiColors uimap[] = {
    // Active window border.
    { CSS_VAL_ACTIVEBORDER, wmgroup, "background", QPalette::Active, QColorGroup::Light },
    // Active window caption.
    { CSS_VAL_ACTIVECAPTION, wmgroup, "background", QPalette::Active, QColorGroup::Text },
    // Text in caption, size box, and scrollbar arrow box.
    { CSS_VAL_CAPTIONTEXT, wmgroup, "activeForeground", QPalette::Active, QColorGroup::Text },
    // Face color for three-dimensional display elements.
    { CSS_VAL_BUTTONFACE, wmgroup, 0, QPalette::Inactive, QColorGroup::Button },
    // Dark shadow for three-dimensional display elements (for edges facing away from the light source).
    { CSS_VAL_BUTTONHIGHLIGHT, wmgroup, 0, QPalette::Inactive, QColorGroup::Light },
    // Shadow color for three-dimensional display elements.
    { CSS_VAL_BUTTONSHADOW, wmgroup, 0, QPalette::Inactive, QColorGroup::Shadow },
    // Text on push buttons.
    { CSS_VAL_BUTTONTEXT, wmgroup, "buttonForeground", QPalette::Inactive, QColorGroup::ButtonText },
    // Dark shadow for three-dimensional display elements.
    { CSS_VAL_THREEDDARKSHADOW, wmgroup, 0, QPalette::Inactive, QColorGroup::Dark },
    // Face color for three-dimensional display elements.
    { CSS_VAL_THREEDFACE, wmgroup, 0, QPalette::Inactive, QColorGroup::Button },
    // Highlight color for three-dimensional display elements.
    { CSS_VAL_THREEDHIGHLIGHT, wmgroup, 0, QPalette::Inactive, QColorGroup::Light },
    // Light color for three-dimensional display elements (for edges facing the light source).
    { CSS_VAL_THREEDLIGHTSHADOW, wmgroup, 0, QPalette::Inactive, QColorGroup::Midlight },
    // Dark shadow for three-dimensional display elements.
    { CSS_VAL_THREEDSHADOW, wmgroup, 0, QPalette::Inactive, QColorGroup::Shadow },

    // Inactive window border.
    { CSS_VAL_INACTIVEBORDER, wmgroup, "background", QPalette::Disabled, QColorGroup::Background },
    // Inactive window caption.
    { CSS_VAL_INACTIVECAPTION, wmgroup, "inactiveBackground", QPalette::Disabled, QColorGroup::Background },
    // Color of text in an inactive caption.
    { CSS_VAL_INACTIVECAPTIONTEXT, wmgroup, "inactiveForeground", QPalette::Disabled, QColorGroup::Text },
    { CSS_VAL_GRAYTEXT, wmgroup, 0, QPalette::Disabled, QColorGroup::Text },

    // Menu background
    { CSS_VAL_MENU, generalgroup, "background", QPalette::Inactive, QColorGroup::Background },
    // Text in menus
    { CSS_VAL_MENUTEXT, generalgroup, "foreground", QPalette::Inactive, QColorGroup::Background },

    // Text of item(s) selected in a control.
    { CSS_VAL_HIGHLIGHT, generalgroup, "selectBackground", QPalette::Inactive, QColorGroup::Background },

    // Text of item(s) selected in a control.
    { CSS_VAL_HIGHLIGHTTEXT, generalgroup, "selectForeground", QPalette::Inactive, QColorGroup::Background },

    // Background color of multiple document interface.
    { CSS_VAL_APPWORKSPACE, generalgroup, "background", QPalette::Inactive, QColorGroup::Text },

    // Scroll bar gray area.
    { CSS_VAL_SCROLLBAR, generalgroup, "background", QPalette::Inactive, QColorGroup::Background },

    // Window background.
    { CSS_VAL_WINDOW, generalgroup, "windowBackground", QPalette::Inactive, QColorGroup::Background },
    // Window frame.
    { CSS_VAL_WINDOWFRAME, generalgroup, "windowBackground", QPalette::Inactive, QColorGroup::Background },
    // WindowText
    { CSS_VAL_WINDOWTEXT, generalgroup, "windowForeground", QPalette::Inactive, QColorGroup::Text },
    { CSS_VAL_TEXT, generalgroup, 0, QPalette::Inactive, QColorGroup::Text },
    { 0, 0, 0, QPalette::NColorGroups, QColorGroup::NColorRoles }
};

#endif // !APPLE_CHANGES

static QColor colorForCSSValue( int css_value )
{
    // try the regular ones first
    const colorMap *col = cmap;
    while ( col->css_value && col->css_value != css_value )
        ++col;
    if ( col->css_value )
        return col->color;

#if APPLE_CHANGES
    return QColor();
#else
    const uiColors *uicol = uimap;
    while ( uicol->css_value && uicol->css_value != css_value )
        ++uicol;
    if ( !uicol->css_value ) {
        if ( css_value == CSS_VAL_INFOBACKGROUND )
            return QToolTip::palette().inactive().background();
        else if ( css_value == CSS_VAL_INFOTEXT )
            return QToolTip::palette().inactive().foreground();
        else if ( css_value == CSS_VAL_BACKGROUND ) {
            KConfig bckgrConfig("kdesktoprc", true, false); // No multi-screen support
            bckgrConfig.setGroup("Desktop0");
            // Desktop background.
            return bckgrConfig.readColorEntry("Color1", &qApp->palette().disabled().background());
        }
        return khtml::invalidColor;
    }
    
    const QPalette &pal = qApp->palette();
    QColor c = pal.color( uicol->group, uicol->role );
    if ( uicol->configEntry ) {
        KConfig *globalConfig = KGlobal::config();
        globalConfig->setGroup( uicol->configGroup );
        c = globalConfig->readColorEntry( uicol->configEntry, &c );
    }
    
    return c;
#endif
}

void CSSStyleSelector::applyDeclarations(bool applyFirst, bool isImportant,
                                         int startIndex, int endIndex)
{
    if (startIndex == -1) return;
    for (int i = startIndex; i <= endIndex; i++) {
        CSSMutableStyleDeclarationImpl* decl = m_matchedDecls[i];
        QValueListConstIterator<CSSProperty> end;
        for (QValueListConstIterator<CSSProperty> it = decl->valuesIterator(); it != end; ++it) {
            const CSSProperty& current = *it;
            // give special priority to font-xxx, color properties
            if (isImportant == current.isImportant()) {
                bool first;
                switch(current.id())
                {
                    case CSS_PROP_BACKGROUND:
                    case CSS_PROP_BACKGROUND_IMAGE:
                    case CSS_PROP_COLOR:
                    case CSS_PROP_DIRECTION:
                    case CSS_PROP_DISPLAY:
                    case CSS_PROP_FONT:
                    case CSS_PROP_FONT_SIZE:
                    case CSS_PROP_FONT_STYLE:
                    case CSS_PROP_FONT_FAMILY:
                    case CSS_PROP_FONT_WEIGHT:
#if APPLE_CHANGES
                    case CSS_PROP__APPLE_TEXT_SIZE_ADJUST:
#endif
                        // these have to be applied first, because other properties use the computed
                        // values of these porperties.
                        first = true;
                        break;
                    default:
                        first = false;
                        break;
                }
                
                if (first == applyFirst)
                    applyProperty(current.id(), current.value());
            }
        }
    }
}

void CSSStyleSelector::applyProperty( int id, DOM::CSSValueImpl *value )
{
    //kdDebug( 6080 ) << "applying property " << prop->m_id << endl;

    CSSPrimitiveValueImpl *primitiveValue = 0;
    if(value->isPrimitiveValue()) primitiveValue = static_cast<CSSPrimitiveValueImpl *>(value);

    Length l;
    bool apply = false;

    bool isInherit = (parentNode && value->cssValueType() == CSSValue::CSS_INHERIT);
    bool isInitial = (value->cssValueType() == CSSValue::CSS_INITIAL) ||
                     (!parentNode && value->cssValueType() == CSSValue::CSS_INHERIT);

    // These properties are used to set the correct margins/padding on RTL lists.
    if (id == CSS_PROP__KHTML_MARGIN_START)
        id = style->direction() == LTR ? CSS_PROP_MARGIN_LEFT : CSS_PROP_MARGIN_RIGHT;
    else if (id == CSS_PROP__KHTML_PADDING_START)
        id = style->direction() == LTR ? CSS_PROP_PADDING_LEFT : CSS_PROP_PADDING_RIGHT;

    // What follows is a list that maps the CSS properties into their corresponding front-end
    // RenderStyle values.  Shorthands (e.g. border, background) occur in this list as well and
    // are only hit when mapping "inherit" or "initial" into front-end values.
    switch(id)
    {
// ident only properties
    case CSS_PROP_BACKGROUND_ATTACHMENT:
        HANDLE_BACKGROUND_VALUE(backgroundAttachment, BackgroundAttachment, value)
        break;
    case CSS_PROP_BACKGROUND_REPEAT:
        HANDLE_BACKGROUND_VALUE(backgroundRepeat, BackgroundRepeat, value)
        break;
    case CSS_PROP_BORDER_COLLAPSE:
        HANDLE_INHERIT_AND_INITIAL(borderCollapse, BorderCollapse)
        if(!primitiveValue) break;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_COLLAPSE:
            style->setBorderCollapse(true);
            break;
        case CSS_VAL_SEPARATE:
            style->setBorderCollapse(false);
            break;
        default:
            return;
        }
        break;
        
    case CSS_PROP_BORDER_TOP_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderTopStyle, BorderTopStyle, BorderStyle)
        if (!primitiveValue) return;
        style->setBorderTopStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_BORDER_RIGHT_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderRightStyle, BorderRightStyle, BorderStyle)
        if (!primitiveValue) return;
        style->setBorderRightStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_BORDER_BOTTOM_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderBottomStyle, BorderBottomStyle, BorderStyle)
        if (!primitiveValue) return;
        style->setBorderBottomStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_BORDER_LEFT_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(borderLeftStyle, BorderLeftStyle, BorderStyle)
        if (!primitiveValue) return;
        style->setBorderLeftStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_OUTLINE_STYLE:
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(outlineStyle, OutlineStyle, BorderStyle)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_AUTO)
            style->setOutlineStyle(DOTTED, true);
        else
            style->setOutlineStyle((EBorderStyle)(primitiveValue->getIdent() - CSS_VAL_NONE));
        break;
    case CSS_PROP_CAPTION_SIDE:
    {
        HANDLE_INHERIT_AND_INITIAL(captionSide, CaptionSide)
        if(!primitiveValue) break;
        ECaptionSide c = RenderStyle::initialCaptionSide();
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_LEFT:
            c = CAPLEFT; break;
        case CSS_VAL_RIGHT:
            c = CAPRIGHT; break;
        case CSS_VAL_TOP:
            c = CAPTOP; break;
        case CSS_VAL_BOTTOM:
            c = CAPBOTTOM; break;
        default:
            return;
        }
        style->setCaptionSide(c);
        return;
    }
    case CSS_PROP_CLEAR:
    {
        HANDLE_INHERIT_AND_INITIAL(clear, Clear)
        if(!primitiveValue) break;
        EClear c;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_NONE:
            c = CNONE; break;
        case CSS_VAL_LEFT:
            c = CLEFT; break;
        case CSS_VAL_RIGHT:
            c = CRIGHT; break;
        case CSS_VAL_BOTH:
            c = CBOTH; break;
        default:
            return;
        }
        style->setClear(c);
        return;
    }
    case CSS_PROP_DIRECTION:
    {
        HANDLE_INHERIT_AND_INITIAL(direction, Direction)
        if(!primitiveValue) break;
        style->setDirection( (EDirection) (primitiveValue->getIdent() - CSS_VAL_LTR) );
        return;
    }
    case CSS_PROP_DISPLAY:
    {
        HANDLE_INHERIT_AND_INITIAL(display, Display)
        if(!primitiveValue) break;
	int id = primitiveValue->getIdent();
	EDisplay d;
	if (id == CSS_VAL_NONE)
	    d = NONE;
	else
	    d = EDisplay(primitiveValue->getIdent() - CSS_VAL_INLINE);

        style->setDisplay(d);
        //kdDebug( 6080 ) << "setting display to " << d << endl;

        break;
    }

    case CSS_PROP_EMPTY_CELLS:
    {
        HANDLE_INHERIT_AND_INITIAL(emptyCells, EmptyCells)
        if (!primitiveValue) break;
        int id = primitiveValue->getIdent();
        if (id == CSS_VAL_SHOW)
            style->setEmptyCells(SHOW);
        else if (id == CSS_VAL_HIDE)
            style->setEmptyCells(HIDE);
        break;
    }
    case CSS_PROP_FLOAT:
    {
        HANDLE_INHERIT_AND_INITIAL(floating, Floating)
        if(!primitiveValue) return;
        EFloat f;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_LEFT:
            f = FLEFT; break;
        case CSS_VAL_RIGHT:
            f = FRIGHT; break;
        case CSS_VAL_NONE:
        case CSS_VAL_CENTER:  //Non standart CSS-Value
            f = FNONE; break;
        default:
            return;
        }
        
        style->setFloating(f);
        break;
    }

        break;
    case CSS_PROP_FONT_STRETCH:
        break; /* Not supported. */

    case CSS_PROP_FONT_STYLE:
    {
        FontDef fontDef = style->htmlFont().fontDef;
        if (isInherit)
            fontDef.italic = parentStyle->htmlFont().fontDef.italic;
	else if (isInitial)
            fontDef.italic = false;
        else {
	    if(!primitiveValue) return;
	    switch(primitiveValue->getIdent()) {
		case CSS_VAL_OBLIQUE:
		// ### oblique is the same as italic for the moment...
		case CSS_VAL_ITALIC:
		    fontDef.italic = true;
		    break;
		case CSS_VAL_NORMAL:
		    fontDef.italic = false;
		    break;
		default:
		    return;
	    }
	}
        if (style->setFontDef( fontDef ))
	fontDirty = true;
        break;
    }


    case CSS_PROP_FONT_VARIANT:
    {
        FontDef fontDef = style->htmlFont().fontDef;
        if (isInherit) 
            fontDef.smallCaps = parentStyle->htmlFont().fontDef.smallCaps;
        else if (isInitial)
            fontDef.smallCaps = false;
        else {
            if(!primitiveValue) return;
            int id = primitiveValue->getIdent();
            if ( id == CSS_VAL_NORMAL )
                fontDef.smallCaps = false;
            else if ( id == CSS_VAL_SMALL_CAPS )
                fontDef.smallCaps = true;
            else
                return;
        }
        if (style->setFontDef( fontDef ))
            fontDirty = true;
        break;        
    }

    case CSS_PROP_FONT_WEIGHT:
    {
        FontDef fontDef = style->htmlFont().fontDef;
        if (isInherit)
            fontDef.weight = parentStyle->htmlFont().fontDef.weight;
        else if (isInitial)
            fontDef.weight = QFont::Normal;
        else {
            if(!primitiveValue) return;
            if(primitiveValue->getIdent())
            {
                switch(primitiveValue->getIdent()) {
                    // ### we just support normal and bold fonts at the moment...
                    // setWeight can actually accept values between 0 and 99...
                    case CSS_VAL_BOLD:
                    case CSS_VAL_BOLDER:
                    case CSS_VAL_600:
                    case CSS_VAL_700:
                    case CSS_VAL_800:
                    case CSS_VAL_900:
                        fontDef.weight = QFont::Bold;
                        break;
                    case CSS_VAL_NORMAL:
                    case CSS_VAL_LIGHTER:
                    case CSS_VAL_100:
                    case CSS_VAL_200:
                    case CSS_VAL_300:
                    case CSS_VAL_400:
                    case CSS_VAL_500:
                        fontDef.weight = QFont::Normal;
                        break;
                    default:
                        return;
                }
            }
            else
            {
                // ### fix parsing of 100-900 values in parser, apply them here
            }
        }
        if (style->setFontDef( fontDef ))
            fontDirty = true;
        break;
    }
        
    case CSS_PROP_LIST_STYLE_POSITION:
    {
        HANDLE_INHERIT_AND_INITIAL(listStylePosition, ListStylePosition)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent())
            style->setListStylePosition( (EListStylePosition) (primitiveValue->getIdent() - CSS_VAL_OUTSIDE) );
        return;
    }

    case CSS_PROP_LIST_STYLE_TYPE:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleType, ListStyleType)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent())
        {
            EListStyleType t;
	    int id = primitiveValue->getIdent();
	    if ( id == CSS_VAL_NONE) { // important!!
	      t = LNONE;
	    } else {
	      t = EListStyleType(id - CSS_VAL_DISC);
	    }
            style->setListStyleType(t);
        }
        return;
    }

    case CSS_PROP_OVERFLOW:
    {
        HANDLE_INHERIT_AND_INITIAL(overflow, Overflow)
        if (!primitiveValue) return;
        EOverflow o;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_VISIBLE:
            o = OVISIBLE; break;
        case CSS_VAL_HIDDEN:
            o = OHIDDEN; break;
        case CSS_VAL_SCROLL:
            o = OSCROLL; break;
        case CSS_VAL_AUTO:
            o = OAUTO; break;
        case CSS_VAL_MARQUEE:
            o = OMARQUEE; break;
        case CSS_VAL_OVERLAY:
            o = OOVERLAY; break;
        default:
            return;
        }
        style->setOverflow(o);
        return;
    }

    case CSS_PROP_PAGE_BREAK_BEFORE:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakBefore, PageBreakBefore, PageBreak)
        if (!primitiveValue) return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_AUTO:
                style->setPageBreakBefore(PBAUTO);
                break;
            case CSS_VAL_LEFT:
            case CSS_VAL_RIGHT:
            case CSS_VAL_ALWAYS:
                style->setPageBreakBefore(PBALWAYS); // CSS2.1: "Conforming user agents may map left/right to always."
                break;
            case CSS_VAL_AVOID:
                style->setPageBreakBefore(PBAVOID);
                break;
        }
        break;
    }

    case CSS_PROP_PAGE_BREAK_AFTER:
    {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakAfter, PageBreakAfter, PageBreak)
        if (!primitiveValue) return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_AUTO:
                style->setPageBreakAfter(PBAUTO);
                break;
            case CSS_VAL_LEFT:
            case CSS_VAL_RIGHT:
            case CSS_VAL_ALWAYS:
                style->setPageBreakAfter(PBALWAYS); // CSS2.1: "Conforming user agents may map left/right to always."
                break;
            case CSS_VAL_AVOID:
                style->setPageBreakAfter(PBAVOID);
                break;
        }
        break;
    }

    case CSS_PROP_PAGE_BREAK_INSIDE: {
        HANDLE_INHERIT_AND_INITIAL_WITH_VALUE(pageBreakInside, PageBreakInside, PageBreak)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_AUTO)
            style->setPageBreakInside(PBAUTO);
        else if (primitiveValue->getIdent() == CSS_VAL_AVOID)
            style->setPageBreakInside(PBAVOID);
        return;
    }
        
    case CSS_PROP_PAGE:
        break; /* FIXME: Not even sure what this is...  -dwh */

    case CSS_PROP_POSITION:
    {
        HANDLE_INHERIT_AND_INITIAL(position, Position)
        if(!primitiveValue) return;
        EPosition p;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_STATIC:
            p = STATIC; break;
        case CSS_VAL_RELATIVE:
            p = RELATIVE; break;
        case CSS_VAL_ABSOLUTE:
            p = ABSOLUTE; break;
        case CSS_VAL_FIXED:
            {
                if ( view )
		    view->useSlowRepaints();
                p = FIXED;
                break;
            }
        default:
            return;
        }
        style->setPosition(p);
        return;
    }

    case CSS_PROP_TABLE_LAYOUT: {
        HANDLE_INHERIT_AND_INITIAL(tableLayout, TableLayout)

        if ( !primitiveValue->getIdent() )
            return;

        ETableLayout l = RenderStyle::initialTableLayout();
        switch( primitiveValue->getIdent() ) {
            case CSS_VAL_FIXED:
                l = TFIXED;
                // fall through
            case CSS_VAL_AUTO:
                style->setTableLayout( l );
            default:
                break;
        }
        break;
    }
        
    case CSS_PROP_UNICODE_BIDI: {
        HANDLE_INHERIT_AND_INITIAL(unicodeBidi, UnicodeBidi)
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_NORMAL:
                style->setUnicodeBidi(UBNormal); 
                break;
            case CSS_VAL_EMBED:
                style->setUnicodeBidi(Embed); 
                break;
            case CSS_VAL_BIDI_OVERRIDE:
                style->setUnicodeBidi(Override);
                break;
            default:
                return;
        }
	break;
    }
    case CSS_PROP_TEXT_TRANSFORM: {
        HANDLE_INHERIT_AND_INITIAL(textTransform, TextTransform)
        
        if(!primitiveValue->getIdent()) return;

        ETextTransform tt;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_CAPITALIZE:  tt = CAPITALIZE;  break;
        case CSS_VAL_UPPERCASE:   tt = UPPERCASE;   break;
        case CSS_VAL_LOWERCASE:   tt = LOWERCASE;   break;
        case CSS_VAL_NONE:
        default:                  tt = TTNONE;      break;
        }
        style->setTextTransform(tt);
        break;
        }

    case CSS_PROP_VISIBILITY:
    {
        HANDLE_INHERIT_AND_INITIAL(visibility, Visibility)

        switch( primitiveValue->getIdent() ) {
        case CSS_VAL_HIDDEN:
            style->setVisibility( HIDDEN );
            break;
        case CSS_VAL_VISIBLE:
            style->setVisibility( VISIBLE );
            break;
        case CSS_VAL_COLLAPSE:
            style->setVisibility( COLLAPSE );
        default:
            break;
        }
        break;
    }
    case CSS_PROP_WHITE_SPACE:
        HANDLE_INHERIT_AND_INITIAL(whiteSpace, WhiteSpace)

        if(!primitiveValue->getIdent()) return;

        EWhiteSpace s;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL__KHTML_NOWRAP:
            s = KHTML_NOWRAP;
            break;
        case CSS_VAL_NOWRAP:
            s = NOWRAP;
            break;
        case CSS_VAL_PRE:
            s = PRE;
            break;
        case CSS_VAL_NORMAL:
        default:
            s = NORMAL;
            break;
        }
        style->setWhiteSpace(s);
        break;

    case CSS_PROP_BACKGROUND_POSITION:
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(backgroundXPosition, BackgroundXPosition);
        HANDLE_BACKGROUND_INHERIT_AND_INITIAL(backgroundYPosition, BackgroundYPosition);
        break;
    case CSS_PROP_BACKGROUND_POSITION_X: {
        HANDLE_BACKGROUND_VALUE(backgroundXPosition, BackgroundXPosition, value)
        break;
    }
    case CSS_PROP_BACKGROUND_POSITION_Y: {
        HANDLE_BACKGROUND_VALUE(backgroundYPosition, BackgroundYPosition, value)
        break;
    }
    case CSS_PROP_BORDER_SPACING: {
        if (isInherit) {
            style->setHorizontalBorderSpacing(parentStyle->horizontalBorderSpacing());
            style->setVerticalBorderSpacing(parentStyle->verticalBorderSpacing());
        }
        else if (isInitial) {
            style->setHorizontalBorderSpacing(0);
            style->setVerticalBorderSpacing(0);
        }
        break;
    }
    case CSS_PROP__KHTML_BORDER_HORIZONTAL_SPACING: {
        HANDLE_INHERIT_AND_INITIAL(horizontalBorderSpacing, HorizontalBorderSpacing)
        if (!primitiveValue) break;
        short spacing =  primitiveValue->computeLength(style, paintDeviceMetrics);
        style->setHorizontalBorderSpacing(spacing);
        break;
    }
    case CSS_PROP__KHTML_BORDER_VERTICAL_SPACING: {
        HANDLE_INHERIT_AND_INITIAL(verticalBorderSpacing, VerticalBorderSpacing)
        if (!primitiveValue) break;
        short spacing =  primitiveValue->computeLength(style, paintDeviceMetrics);
        style->setVerticalBorderSpacing(spacing);
        break;
    }
    case CSS_PROP_CURSOR:
        HANDLE_INHERIT_AND_INITIAL(cursor, Cursor)
        if (primitiveValue)
            style->setCursor( (ECursor) (primitiveValue->getIdent() - CSS_VAL_AUTO) );
        break;        
// colors || inherit
    case CSS_PROP_BACKGROUND_COLOR:
    case CSS_PROP_BORDER_TOP_COLOR:
    case CSS_PROP_BORDER_RIGHT_COLOR:
    case CSS_PROP_BORDER_BOTTOM_COLOR:
    case CSS_PROP_BORDER_LEFT_COLOR:
    case CSS_PROP_COLOR:
    case CSS_PROP_OUTLINE_COLOR:
        // this property is an extension used to get HTML4 <font> right.
#if !APPLE_CHANGES
    case CSS_PROP_SCROLLBAR_FACE_COLOR:
    case CSS_PROP_SCROLLBAR_SHADOW_COLOR:
    case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:
    case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:
    case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:
    case CSS_PROP_SCROLLBAR_TRACK_COLOR:
    case CSS_PROP_SCROLLBAR_ARROW_COLOR:
#endif
    {
        QColor col;
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_BACKGROUND_COLOR, backgroundColor, BackgroundColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_TOP_COLOR, borderTopColor, BorderTopColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_BOTTOM_COLOR, borderBottomColor, BorderBottomColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_RIGHT_COLOR, borderRightColor, BorderRightColor)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_LEFT_COLOR, borderLeftColor, BorderLeftColor)
            HANDLE_INHERIT_COND(CSS_PROP_COLOR, color, Color)
            HANDLE_INHERIT_COND(CSS_PROP_OUTLINE_COLOR, outlineColor, OutlineColor)
            return;
        }
        else if (isInitial) {
            // The border/outline colors will just map to the invalid color |col| above.  This will have the
            // effect of forcing the use of the currentColor when it comes time to draw the borders (and of
            // not painting the background since the color won't be valid).
            if (id == CSS_PROP_COLOR)
                col = RenderStyle::initialColor();
        }
        else {
            if(!primitiveValue)
                return;
            col = getColorFromPrimitiveValue(primitiveValue);
        }

        //kdDebug( 6080 ) << "applying color " << col.isValid() << endl;
        switch(id)
        {
        case CSS_PROP_BACKGROUND_COLOR:
            style->setBackgroundColor(col); break;
        case CSS_PROP_BORDER_TOP_COLOR:
            style->setBorderTopColor(col); break;
        case CSS_PROP_BORDER_RIGHT_COLOR:
            style->setBorderRightColor(col); break;
        case CSS_PROP_BORDER_BOTTOM_COLOR:
            style->setBorderBottomColor(col); break;
        case CSS_PROP_BORDER_LEFT_COLOR:
            style->setBorderLeftColor(col); break;
        case CSS_PROP_COLOR:
            style->setColor(col); break;
        case CSS_PROP_OUTLINE_COLOR:
            style->setOutlineColor(col); break;
#if !APPLE_CHANGES
        case CSS_PROP_SCROLLBAR_FACE_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Button, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Button, col);
            break;
        case CSS_PROP_SCROLLBAR_SHADOW_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Shadow, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Shadow, col);
            break;
        case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Light, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Light, col);
            break;
        case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:
            break;
        case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Dark, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Dark, col);
            break;
        case CSS_PROP_SCROLLBAR_TRACK_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::Base, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Base, col);
            style->setPaletteColor(QPalette::Active, QColorGroup::Mid, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Mid, col);
            style->setPaletteColor(QPalette::Active, QColorGroup::Background, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::Background, col);
            break;
        case CSS_PROP_SCROLLBAR_ARROW_COLOR:
            style->setPaletteColor(QPalette::Active, QColorGroup::ButtonText, col);
            style->setPaletteColor(QPalette::Inactive, QColorGroup::ButtonText, col);
            break;
#endif
        default:
            return;
        }
        return;
    }
    break;
    
// uri || inherit
    case CSS_PROP_BACKGROUND_IMAGE:
        HANDLE_BACKGROUND_VALUE(backgroundImage, BackgroundImage, value)
        break;
    case CSS_PROP_LIST_STYLE_IMAGE:
    {
        HANDLE_INHERIT_AND_INITIAL(listStyleImage, ListStyleImage)
        if (!primitiveValue) return;
	style->setListStyleImage(static_cast<CSSImageValueImpl *>(primitiveValue)
                                 ->image(element->getDocument()->docLoader()));
        //kdDebug( 6080 ) << "setting image in list to " << image->image() << endl;
        break;
    }

// length
    case CSS_PROP_BORDER_TOP_WIDTH:
    case CSS_PROP_BORDER_RIGHT_WIDTH:
    case CSS_PROP_BORDER_BOTTOM_WIDTH:
    case CSS_PROP_BORDER_LEFT_WIDTH:
    case CSS_PROP_OUTLINE_WIDTH:
    {
	if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_TOP_WIDTH, borderTopWidth, BorderTopWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_RIGHT_WIDTH, borderRightWidth, BorderRightWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_BOTTOM_WIDTH, borderBottomWidth, BorderBottomWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BORDER_LEFT_WIDTH, borderLeftWidth, BorderLeftWidth)
            HANDLE_INHERIT_COND(CSS_PROP_OUTLINE_WIDTH, outlineWidth, OutlineWidth)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_TOP_WIDTH, BorderTopWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_RIGHT_WIDTH, BorderRightWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_BOTTOM_WIDTH, BorderBottomWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BORDER_LEFT_WIDTH, BorderLeftWidth, BorderWidth)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_OUTLINE_WIDTH, OutlineWidth, BorderWidth)
            return;
        }

        if(!primitiveValue) break;
        short width = 3;
        switch(primitiveValue->getIdent())
        {
        case CSS_VAL_THIN:
            width = 1;
            break;
        case CSS_VAL_MEDIUM:
            width = 3;
            break;
        case CSS_VAL_THICK:
            width = 5;
            break;
        case CSS_VAL_INVALID:
            width = primitiveValue->computeLength(style, paintDeviceMetrics);
            break;
        default:
            return;
        }

        if(width < 0) return;
        switch(id)
        {
        case CSS_PROP_BORDER_TOP_WIDTH:
            style->setBorderTopWidth(width);
            break;
        case CSS_PROP_BORDER_RIGHT_WIDTH:
            style->setBorderRightWidth(width);
            break;
        case CSS_PROP_BORDER_BOTTOM_WIDTH:
            style->setBorderBottomWidth(width);
            break;
        case CSS_PROP_BORDER_LEFT_WIDTH:
            style->setBorderLeftWidth(width);
            break;
        case CSS_PROP_OUTLINE_WIDTH:
            style->setOutlineWidth(width);
            break;
        default:
            return;
        }
        return;
    }

    case CSS_PROP_LETTER_SPACING:
    case CSS_PROP_WORD_SPACING:
    {
	
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_LETTER_SPACING, letterSpacing, LetterSpacing)
            HANDLE_INHERIT_COND(CSS_PROP_WORD_SPACING, wordSpacing, WordSpacing)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_LETTER_SPACING, LetterSpacing, LetterWordSpacing)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_WORD_SPACING, WordSpacing, LetterWordSpacing)
            return;
        }
        
        int width = 0;
        if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_NORMAL){
            width = 0;
        } else {
	    if(!primitiveValue) return;
	    width = primitiveValue->computeLength(style, paintDeviceMetrics);
	}
        switch(id)
        {
        case CSS_PROP_LETTER_SPACING:
            style->setLetterSpacing(width);
            break;
        case CSS_PROP_WORD_SPACING:
            style->setWordSpacing(width);
            break;
            // ### needs the definitions in renderstyle
        default: break;
        }
        return;
    }

    case CSS_PROP_WORD_WRAP:
    {
        HANDLE_INHERIT_AND_INITIAL(wordWrap, WordWrap)

        if(!primitiveValue->getIdent()) return;

        EWordWrap s;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_BREAK_WORD:
            s = BREAK_WORD;
            break;
        case CSS_VAL_NORMAL:
        default:
            s = WBNORMAL;
            break;
        }
        style->setWordWrap(s);
        break;
    }

    case CSS_PROP__KHTML_NBSP_MODE:
    {
        HANDLE_INHERIT_AND_INITIAL(nbspMode, NBSPMode)

        if (!primitiveValue->getIdent()) return;

        ENBSPMode m;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_SPACE:
            m = SPACE;
            break;
        case CSS_VAL_NORMAL:
        default:
            m = NBNORMAL;
            break;
        }
        style->setNBSPMode(m);
        break;
    }

    case CSS_PROP__KHTML_LINE_BREAK:
    {
        HANDLE_INHERIT_AND_INITIAL(khtmlLineBreak, KHTMLLineBreak)

        if (!primitiveValue->getIdent()) return;

        EKHTMLLineBreak b;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_AFTER_WHITE_SPACE:
            b = AFTER_WHITE_SPACE;
            break;
        case CSS_VAL_NORMAL:
        default:
            b = LBNORMAL;
            break;
        }
        style->setKHTMLLineBreak(b);
        break;
    }

    case CSS_PROP__KHTML_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR:
    {
        HANDLE_INHERIT_AND_INITIAL(matchNearestMailBlockquoteColor, MatchNearestMailBlockquoteColor)

        if (!primitiveValue->getIdent()) return;

        EMatchNearestMailBlockquoteColor c;
        switch(primitiveValue->getIdent()) {
        case CSS_VAL_NORMAL:
            c = BCNORMAL;
            break;
        case CSS_VAL_MATCH:
        default:
            c = MATCH;
            break;
        }
        style->setMatchNearestMailBlockquoteColor(c);
        break;
    }

        // length, percent
    case CSS_PROP_MAX_WIDTH:
        // +none +inherit
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)
            apply = true;
    case CSS_PROP_TOP:
    case CSS_PROP_LEFT:
    case CSS_PROP_RIGHT:
    case CSS_PROP_BOTTOM:
    case CSS_PROP_WIDTH:
    case CSS_PROP_MIN_WIDTH:
    case CSS_PROP_MARGIN_TOP:
    case CSS_PROP_MARGIN_RIGHT:
    case CSS_PROP_MARGIN_BOTTOM:
    case CSS_PROP_MARGIN_LEFT:
        // +inherit +auto
        if (id == CSS_PROP_WIDTH || id == CSS_PROP_MIN_WIDTH || id == CSS_PROP_MAX_WIDTH) {
            if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_INTRINSIC) {
                l = Length(Intrinsic);
                apply = true;
            }
            else if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_MIN_INTRINSIC) {
                l = Length(MinIntrinsic);
                apply = true;
            }
        }
        if (id != CSS_PROP_MAX_WIDTH && primitiveValue &&
           primitiveValue->getIdent() == CSS_VAL_AUTO)
        {
            //kdDebug( 6080 ) << "found value=auto" << endl;
            apply = true;
        }
    case CSS_PROP_PADDING_TOP:
    case CSS_PROP_PADDING_RIGHT:
    case CSS_PROP_PADDING_BOTTOM:
    case CSS_PROP_PADDING_LEFT:
    case CSS_PROP_TEXT_INDENT:
        // +inherit
    {
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_MAX_WIDTH, maxWidth, MaxWidth)
            HANDLE_INHERIT_COND(CSS_PROP_BOTTOM, bottom, Bottom)
            HANDLE_INHERIT_COND(CSS_PROP_TOP, top, Top)
            HANDLE_INHERIT_COND(CSS_PROP_LEFT, left, Left)
            HANDLE_INHERIT_COND(CSS_PROP_RIGHT, right, Right)
            HANDLE_INHERIT_COND(CSS_PROP_WIDTH, width, Width)
            HANDLE_INHERIT_COND(CSS_PROP_MIN_WIDTH, minWidth, MinWidth)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_TOP, paddingTop, PaddingTop)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_RIGHT, paddingRight, PaddingRight)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_BOTTOM, paddingBottom, PaddingBottom)
            HANDLE_INHERIT_COND(CSS_PROP_PADDING_LEFT, paddingLeft, PaddingLeft)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_TOP, marginTop, MarginTop)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_RIGHT, marginRight, MarginRight)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_BOTTOM, marginBottom, MarginBottom)
            HANDLE_INHERIT_COND(CSS_PROP_MARGIN_LEFT, marginLeft, MarginLeft)
            HANDLE_INHERIT_COND(CSS_PROP_TEXT_INDENT, textIndent, TextIndent)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MAX_WIDTH, MaxWidth, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_BOTTOM, Bottom, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_TOP, Top, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_LEFT, Left, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_RIGHT, Right, Offset)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_WIDTH, Width, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MIN_WIDTH, MinWidth, MinSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_TOP, PaddingTop, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_RIGHT, PaddingRight, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_BOTTOM, PaddingBottom, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_PADDING_LEFT, PaddingLeft, Padding)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_TOP, MarginTop, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_RIGHT, MarginRight, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_BOTTOM, MarginBottom, Margin)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MARGIN_LEFT, MarginLeft, Margin)
            HANDLE_INITIAL_COND(CSS_PROP_TEXT_INDENT, TextIndent)
            return;
        } 

        if (primitiveValue && !apply) {
            int type = primitiveValue->primitiveType();
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                // Handle our quirky margin units if we have them.
                l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed, 
                           primitiveValue->isQuirkValue());
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                l = Length((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
            else if (type == CSSPrimitiveValue::CSS_HTML_RELATIVE)
                l = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_HTML_RELATIVE)), Relative);
            else
                return;
            if (id == CSS_PROP_PADDING_LEFT || id == CSS_PROP_PADDING_RIGHT ||
                id == CSS_PROP_PADDING_TOP || id == CSS_PROP_PADDING_BOTTOM)
                // Padding can't be negative
                apply = !((l.isFixed() || l.isPercent()) && l.width(100) < 0);
            else
                apply = true;
        }
        if(!apply) return;
        switch(id)
            {
            case CSS_PROP_MAX_WIDTH:
                style->setMaxWidth(l); break;
            case CSS_PROP_BOTTOM:
                style->setBottom(l); break;
            case CSS_PROP_TOP:
                style->setTop(l); break;
            case CSS_PROP_LEFT:
                style->setLeft(l); break;
            case CSS_PROP_RIGHT:
                style->setRight(l); break;
            case CSS_PROP_WIDTH:
                style->setWidth(l); break;
            case CSS_PROP_MIN_WIDTH:
                style->setMinWidth(l); break;
            case CSS_PROP_PADDING_TOP:
                style->setPaddingTop(l); break;
            case CSS_PROP_PADDING_RIGHT:
                style->setPaddingRight(l); break;
            case CSS_PROP_PADDING_BOTTOM:
                style->setPaddingBottom(l); break;
            case CSS_PROP_PADDING_LEFT:
                style->setPaddingLeft(l); break;
            case CSS_PROP_MARGIN_TOP:
                style->setMarginTop(l); break;
            case CSS_PROP_MARGIN_RIGHT:
                style->setMarginRight(l); break;
            case CSS_PROP_MARGIN_BOTTOM:
                style->setMarginBottom(l); break;
            case CSS_PROP_MARGIN_LEFT:
                style->setMarginLeft(l); break;
            case CSS_PROP_TEXT_INDENT:
                style->setTextIndent(l); break;
            default: break;
            }
        return;
    }

    case CSS_PROP_MAX_HEIGHT:
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)
            apply = true;
    case CSS_PROP_HEIGHT:
    case CSS_PROP_MIN_HEIGHT:
        if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_INTRINSIC) {
            l = Length(Intrinsic);
            apply = true;
        }
        else if (primitiveValue && primitiveValue->getIdent() == CSS_VAL_MIN_INTRINSIC) {
            l = Length(MinIntrinsic);
            apply = true;
        }
        if(id != CSS_PROP_MAX_HEIGHT && primitiveValue &&
           primitiveValue->getIdent() == CSS_VAL_AUTO)
            apply = true;
        if (isInherit) {
            HANDLE_INHERIT_COND(CSS_PROP_MAX_HEIGHT, maxHeight, MaxHeight)
            HANDLE_INHERIT_COND(CSS_PROP_HEIGHT, height, Height)
            HANDLE_INHERIT_COND(CSS_PROP_MIN_HEIGHT, minHeight, MinHeight)
            return;
        }
        else if (isInitial) {
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MAX_HEIGHT, MaxHeight, MaxSize)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_HEIGHT, Height, Size)
            HANDLE_INITIAL_COND_WITH_VALUE(CSS_PROP_MIN_HEIGHT, MinHeight, MinSize)
            return;
        }

        if (primitiveValue && !apply)
        {
            int type = primitiveValue->primitiveType();
            if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
            {
                // ### compute from parents height!!!
                l = Length((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
            }
            else
                return;
            apply = true;
        }
        if(!apply) return;
        switch(id)
        {
        case CSS_PROP_MAX_HEIGHT:
            style->setMaxHeight(l); break;
        case CSS_PROP_HEIGHT:
            style->setHeight(l); break;
        case CSS_PROP_MIN_HEIGHT:
            style->setMinHeight(l); break;
        default:
            return;
        }
        return;

        break;

    case CSS_PROP_VERTICAL_ALIGN:
        HANDLE_INHERIT_AND_INITIAL(verticalAlign, VerticalAlign)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent()) {
	  khtml::EVerticalAlign align;

	  switch(primitiveValue->getIdent())
	    {
		case CSS_VAL_TOP:
		    align = TOP; break;
		case CSS_VAL_BOTTOM:
		    align = BOTTOM; break;
		case CSS_VAL_MIDDLE:
		    align = MIDDLE; break;
		case CSS_VAL_BASELINE:
		    align = BASELINE; break;
		case CSS_VAL_TEXT_BOTTOM:
		    align = TEXT_BOTTOM; break;
		case CSS_VAL_TEXT_TOP:
		    align = TEXT_TOP; break;
		case CSS_VAL_SUB:
		    align = SUB; break;
		case CSS_VAL_SUPER:
		    align = SUPER; break;
		case CSS_VAL__KHTML_BASELINE_MIDDLE:
		    align = BASELINE_MIDDLE; break;
		default:
		    return;
	    }
	  style->setVerticalAlign(align);
	  return;
        } else {
	  int type = primitiveValue->primitiveType();
	  Length l;
	  if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
	    l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed );
	  else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
	    l = Length( int( primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE) ), Percent );

	  style->setVerticalAlign( LENGTH );
	  style->setVerticalAlignLength( l );
	}
        break;

    case CSS_PROP_FONT_SIZE:
    {
        FontDef fontDef = style->htmlFont().fontDef;
        float oldSize = 0;
        float size = 0;
        
        bool parentIsAbsoluteSize = false;
        if (parentNode) {
            oldSize = parentStyle->htmlFont().fontDef.specifiedSize;
            parentIsAbsoluteSize = parentStyle->htmlFont().fontDef.isAbsoluteSize;
        }

        if (isInherit)
            size = oldSize;
        else if (isInitial)
            size = fontSizeForKeyword(CSS_VAL_MEDIUM, style->htmlHacks());
        else if (primitiveValue->getIdent()) {
            // Keywords are being used.
            switch (primitiveValue->getIdent()) {
            case CSS_VAL_XX_SMALL:
            case CSS_VAL_X_SMALL:
            case CSS_VAL_SMALL:
            case CSS_VAL_MEDIUM:
            case CSS_VAL_LARGE:
            case CSS_VAL_X_LARGE:
            case CSS_VAL_XX_LARGE:
            case CSS_VAL__KHTML_XXX_LARGE:
                size = fontSizeForKeyword(primitiveValue->getIdent(), style->htmlHacks());
                break;
            case CSS_VAL_LARGER:
                size = largerFontSize(oldSize, style->htmlHacks());
                break;
            case CSS_VAL_SMALLER:
                size = smallerFontSize(oldSize, style->htmlHacks());
                break;
            default:
                return;
            }

            fontDef.isAbsoluteSize = parentIsAbsoluteSize && 
                                    (primitiveValue->getIdent() == CSS_VAL_LARGER ||
                                     primitiveValue->getIdent() == CSS_VAL_SMALLER);
        } else {
            int type = primitiveValue->primitiveType();
            fontDef.isAbsoluteSize = parentIsAbsoluteSize ||
                                          (type != CSSPrimitiveValue::CSS_PERCENTAGE &&
                                           type != CSSPrimitiveValue::CSS_EMS && 
                                           type != CSSPrimitiveValue::CSS_EXS);
            if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
                size = primitiveValue->computeLengthFloat(parentStyle, paintDeviceMetrics, false);
            else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
                size = (primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)
                        * oldSize) / 100;
            else
                return;
        }

        if (size <= 0) return;

        //kdDebug( 6080 ) << "computed raw font size: " << size << endl;

        setFontSize(fontDef, size);
        if (style->setFontDef( fontDef ))
	    fontDirty = true;
        return;
    }
        break;
    case CSS_PROP_Z_INDEX:
    {
        HANDLE_INHERIT(zIndex, ZIndex)
        else if (isInitial) {
            style->setHasAutoZIndex();
            return;
        }
        
        if (!primitiveValue)
            return;

        if (primitiveValue->getIdent() == CSS_VAL_AUTO) {
            style->setHasAutoZIndex();
            return;
        }
        
        if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        
        style->setZIndex((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER));
        
        return;
    }
        
    case CSS_PROP_WIDOWS:
    {
        HANDLE_INHERIT_AND_INITIAL(widows, Widows)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        style->setWidows((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER));
        break;
    }
        
    case CSS_PROP_ORPHANS:
    {
        HANDLE_INHERIT_AND_INITIAL(orphans, Orphans)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return;
        style->setOrphans((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER));
        break;
    }        

// length, percent, number
    case CSS_PROP_LINE_HEIGHT:
    {
        HANDLE_INHERIT_AND_INITIAL(lineHeight, LineHeight)
        if(!primitiveValue) return;
        Length lineHeight;
        int type = primitiveValue->primitiveType();
        if (primitiveValue->getIdent() == CSS_VAL_NORMAL)
            lineHeight = Length( -100, Percent );
        else if (type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG) {
            double multiplier = 1.0;
            // Scale for the font zoom factor only for types other than "em" and "ex", since those are
            // already based on the font size.
            if (type != CSSPrimitiveValue::CSS_EMS && type != CSSPrimitiveValue::CSS_EXS && view && view->part()) {
                multiplier = view->part()->zoomFactor() / 100.0;
            }
            lineHeight = Length(primitiveValue->computeLength(style, paintDeviceMetrics, multiplier), Fixed);
        } else if (type == CSSPrimitiveValue::CSS_PERCENTAGE)
            lineHeight = Length( ( style->font().pixelSize() * int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE)) ) / 100, Fixed );
        else if (type == CSSPrimitiveValue::CSS_NUMBER)
            lineHeight = Length(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)*100), Percent);
        else
            return;
        style->setLineHeight(lineHeight);
        return;
    }

// string
    case CSS_PROP_TEXT_ALIGN:
    {
        HANDLE_INHERIT_AND_INITIAL(textAlign, TextAlign)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent())
            style->setTextAlign( (ETextAlign) (primitiveValue->getIdent() - CSS_VAL__KHTML_AUTO) );
	return;
    }

// rect
    case CSS_PROP_CLIP:
    {
	Length top;
	Length right;
	Length bottom;
	Length left;
        bool hasClip = true;
	if (isInherit) {
            if (parentStyle->hasClip()) {
                top = parentStyle->clipTop();
                right = parentStyle->clipRight();
                bottom = parentStyle->clipBottom();
                left = parentStyle->clipLeft();
            }
            else {
                hasClip = false;
                top = right = bottom = left = Length();
            }
        } else if (isInitial) {
            hasClip = false;
            top = right = bottom = left = Length();
        } else if ( !primitiveValue ) {
	    break;
	} else if ( primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RECT ) {
	    RectImpl *rect = primitiveValue->getRectValue();
	    if ( !rect )
		break;
	    top = convertToLength( rect->top(), style, paintDeviceMetrics );
	    right = convertToLength( rect->right(), style, paintDeviceMetrics );
	    bottom = convertToLength( rect->bottom(), style, paintDeviceMetrics );
	    left = convertToLength( rect->left(), style, paintDeviceMetrics );

	} else if ( primitiveValue->getIdent() != CSS_VAL_AUTO ) {
	    break;
	}
// 	qDebug("setting clip top to %d", top.value );
// 	qDebug("setting clip right to %d", right.value );
// 	qDebug("setting clip bottom to %d", bottom.value );
// 	qDebug("setting clip left to %d", left.value );
	style->setClip(top, right, bottom, left);
        style->setHasClip(hasClip);
    
        // rect, ident
        break;
    }

// lists
    case CSS_PROP_CONTENT:
        // list of string, uri, counter, attr, i
    {
        // FIXME: In CSS3, it will be possible to inherit content.  In CSS2 it is not.  This
        // note is a reminder that eventually "inherit" needs to be supported.
        if (!(style->styleType()==RenderStyle::BEFORE ||
                style->styleType()==RenderStyle::AFTER))
            break;

        if (isInitial) {
            if (style->contentData())
                style->contentData()->clearContent();
            return;
        }
        
        if(!value->isValueList()) return;
        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        int len = list->length();

        for(int i = 0; i < len; i++) {
            CSSValueImpl *item = list->item(i);
            if(!item->isPrimitiveValue()) continue;
            CSSPrimitiveValueImpl *val = static_cast<CSSPrimitiveValueImpl *>(item);
            if(val->primitiveType()==CSSPrimitiveValue::CSS_STRING)
            {
                style->setContent(val->getStringValue().implementation(), i != 0);
            }
            else if (val->primitiveType()==CSSPrimitiveValue::CSS_ATTR)
            {
                // FIXME: Should work with generic XML attributes also, and not
                // just the hardcoded HTML set.  Can a namespace be specified for
                // an attr(foo)?
                int attrID = element->getDocument()->attrId(0, val->getStringValue().implementation(), false);
                if (attrID)
                    style->setContent(element->getAttribute(attrID).implementation(), i != 0);
            }
            else if (val->primitiveType()==CSSPrimitiveValue::CSS_URI)
            {
                CSSImageValueImpl *image = static_cast<CSSImageValueImpl *>(val);
                style->setContent(image->image(element->getDocument()->docLoader()), i != 0);
            }

        }
        break;
    }

    case CSS_PROP_COUNTER_INCREMENT:
        // list of CSS2CounterIncrement
    case CSS_PROP_COUNTER_RESET:
        // list of CSS2CounterReset
        break;
    case CSS_PROP_FONT_FAMILY:
        // list of strings and ids
    {
        if (isInherit) {
            FontDef parentFontDef = parentStyle->htmlFont().fontDef;
            FontDef fontDef = style->htmlFont().fontDef;
            fontDef.genericFamily = parentFontDef.genericFamily;
            fontDef.family = parentFontDef.family;
            if (style->setFontDef(fontDef))
                fontDirty = true;
            return;
        }
        else if (isInitial) {
            FontDef fontDef = style->htmlFont().fontDef;
            FontDef initialDef = FontDef();
            fontDef.genericFamily = initialDef.genericFamily;
            fontDef.family = initialDef.firstFamily();
            if (style->setFontDef(fontDef))
                fontDirty = true;
            return;
        }
        
        if (!value->isValueList()) return;
        FontDef fontDef = style->htmlFont().fontDef;
        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        int len = list->length();
        KWQFontFamily &firstFamily = fontDef.firstFamily();
        KWQFontFamily *currFamily = 0;
        
        for(int i = 0; i < len; i++) {
            CSSValueImpl *item = list->item(i);
            if(!item->isPrimitiveValue()) continue;
            CSSPrimitiveValueImpl *val = static_cast<CSSPrimitiveValueImpl *>(item);
            QString face;
            if( val->primitiveType() == CSSPrimitiveValue::CSS_STRING )
                face = static_cast<FontFamilyValueImpl *>(val)->fontName();
            else if (val->primitiveType() == CSSPrimitiveValue::CSS_IDENT) {
                switch (val->getIdent()) {
                    case CSS_VAL__KHTML_BODY:
                        face = settings->stdFontName();
                        break;
                    case CSS_VAL_SERIF:
                        face = settings->serifFontName();
                        fontDef.setGenericFamily(FontDef::eSerif);
                        break;
                    case CSS_VAL_SANS_SERIF:
                        face = settings->sansSerifFontName();
                        fontDef.setGenericFamily(FontDef::eSansSerif);
                        break;
                    case CSS_VAL_CURSIVE:
                        face = settings->cursiveFontName();
                        fontDef.setGenericFamily(FontDef::eCursive);
                        break;
                    case CSS_VAL_FANTASY:
                        face = settings->fantasyFontName();
                        fontDef.setGenericFamily(FontDef::eFantasy);
                        break;
                    case CSS_VAL_MONOSPACE:
                        face = settings->fixedFontName();
                        fontDef.setGenericFamily(FontDef::eMonospace);
                        break;
                }
            }
    
            if ( !face.isEmpty() ) {
                if (!currFamily) {
                    // Filling in the first family.
                    firstFamily.setFamily(face);
                    currFamily = &firstFamily;
                }
                else {
                    KWQFontFamily *newFamily = new KWQFontFamily;
                    newFamily->setFamily(face);
                    currFamily->appendFamily(newFamily);
                    currFamily = newFamily;
                }
                
                if (style->setFontDef( fontDef )) {
                    fontDirty = true;
                }
            }
        }
      break;
    }
    case CSS_PROP_QUOTES:
        // list of strings or i
    case CSS_PROP_SIZE:
        // ### look up
      break;
    case CSS_PROP_TEXT_DECORATION: {
        // list of ident
        HANDLE_INHERIT_AND_INITIAL(textDecoration, TextDecoration)
        int t = RenderStyle::initialTextDecoration();
        if(primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE) {
	    // do nothing
	} else {
	    if(!value->isValueList()) return;
	    CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
	    int len = list->length();
	    for(int i = 0; i < len; i++)
	    {
		CSSValueImpl *item = list->item(i);
		if(!item->isPrimitiveValue()) continue;
		primitiveValue = static_cast<CSSPrimitiveValueImpl *>(item);
		switch(primitiveValue->getIdent())
		{
		    case CSS_VAL_NONE:
			t = TDNONE; break;
		    case CSS_VAL_UNDERLINE:
			t |= UNDERLINE; break;
		    case CSS_VAL_OVERLINE:
			t |= OVERLINE; break;
		    case CSS_VAL_LINE_THROUGH:
			t |= LINE_THROUGH; break;
		    case CSS_VAL_BLINK:
			t |= BLINK; break;
		    default:
			return;
		}
	    }
        }

        style->setTextDecoration(t);
        break;
    }

// shorthand properties
    case CSS_PROP_BACKGROUND:
        if (isInitial) {
            style->clearBackgroundLayers();
            return;
        }
        else if (isInherit) {
            if (parentStyle)
                style->inheritBackgroundLayers(*parentStyle->backgroundLayers());
            else
                style->clearBackgroundLayers();
            return;
        }
        break;
    case CSS_PROP_BORDER:
    case CSS_PROP_BORDER_STYLE:
    case CSS_PROP_BORDER_WIDTH:
    case CSS_PROP_BORDER_COLOR:
        if (id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_COLOR)
        {
            if (isInherit) {
                style->setBorderTopColor(parentStyle->borderTopColor());
                style->setBorderBottomColor(parentStyle->borderBottomColor());
                style->setBorderLeftColor(parentStyle->borderLeftColor());
                style->setBorderRightColor(parentStyle->borderRightColor());
            }
            else if (isInitial) {
                style->setBorderTopColor(QColor()); // Reset to invalid color so currentColor is used instead.
                style->setBorderBottomColor(QColor());
                style->setBorderLeftColor(QColor());
                style->setBorderRightColor(QColor());
            }
        }
        if (id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_STYLE)
        {
            if (isInherit) {
                style->setBorderTopStyle(parentStyle->borderTopStyle());
                style->setBorderBottomStyle(parentStyle->borderBottomStyle());
                style->setBorderLeftStyle(parentStyle->borderLeftStyle());
                style->setBorderRightStyle(parentStyle->borderRightStyle());
            }
            else if (isInitial) {
                style->setBorderTopStyle(RenderStyle::initialBorderStyle());
                style->setBorderBottomStyle(RenderStyle::initialBorderStyle());
                style->setBorderLeftStyle(RenderStyle::initialBorderStyle());
                style->setBorderRightStyle(RenderStyle::initialBorderStyle());
            }
        }
        if (id == CSS_PROP_BORDER || id == CSS_PROP_BORDER_WIDTH)
        {
            if (isInherit) {
                style->setBorderTopWidth(parentStyle->borderTopWidth());
                style->setBorderBottomWidth(parentStyle->borderBottomWidth());
                style->setBorderLeftWidth(parentStyle->borderLeftWidth());
                style->setBorderRightWidth(parentStyle->borderRightWidth());
            }
            else if (isInitial) {
                style->setBorderTopWidth(RenderStyle::initialBorderWidth());
                style->setBorderBottomWidth(RenderStyle::initialBorderWidth());
                style->setBorderLeftWidth(RenderStyle::initialBorderWidth());
                style->setBorderRightWidth(RenderStyle::initialBorderWidth());
            }
        }
        return;
    case CSS_PROP_BORDER_TOP:
        if (isInherit) {
            style->setBorderTopColor(parentStyle->borderTopColor());
            style->setBorderTopStyle(parentStyle->borderTopStyle());
            style->setBorderTopWidth(parentStyle->borderTopWidth());
        }
        else if (isInitial)
            style->resetBorderTop();
        return;
    case CSS_PROP_BORDER_RIGHT:
        if (isInherit) {
            style->setBorderRightColor(parentStyle->borderRightColor());
            style->setBorderRightStyle(parentStyle->borderRightStyle());
            style->setBorderRightWidth(parentStyle->borderRightWidth());
        }
        else if (isInitial)
            style->resetBorderRight();
        return;
    case CSS_PROP_BORDER_BOTTOM:
        if (isInherit) {
            style->setBorderBottomColor(parentStyle->borderBottomColor());
            style->setBorderBottomStyle(parentStyle->borderBottomStyle());
            style->setBorderBottomWidth(parentStyle->borderBottomWidth());
        }
        else if (isInitial)
            style->resetBorderBottom();
        return;
    case CSS_PROP_BORDER_LEFT:
        if (isInherit) {
            style->setBorderLeftColor(parentStyle->borderLeftColor());
            style->setBorderLeftStyle(parentStyle->borderLeftStyle());
            style->setBorderLeftWidth(parentStyle->borderLeftWidth());
        }
        else if (isInitial)
            style->resetBorderLeft();
        return;
    case CSS_PROP_MARGIN:
        if (isInherit) {
            style->setMarginTop(parentStyle->marginTop());
            style->setMarginBottom(parentStyle->marginBottom());
            style->setMarginLeft(parentStyle->marginLeft());
            style->setMarginRight(parentStyle->marginRight());
        }
        else if (isInitial)
            style->resetMargin();
        return;
    case CSS_PROP_PADDING:
        if (isInherit) {
            style->setPaddingTop(parentStyle->paddingTop());
            style->setPaddingBottom(parentStyle->paddingBottom());
            style->setPaddingLeft(parentStyle->paddingLeft());
            style->setPaddingRight(parentStyle->paddingRight());
        }
        else if (isInitial)
            style->resetPadding();
        return;
    case CSS_PROP_FONT:
        if (isInherit) {
            FontDef fontDef = parentStyle->htmlFont().fontDef;
            style->setLineHeight( parentStyle->lineHeight() );
            if (style->setFontDef( fontDef ))
                fontDirty = true;
        } else if (isInitial) {
            FontDef fontDef;
            style->setLineHeight(RenderStyle::initialLineHeight());
            if (style->setFontDef( fontDef ))
                fontDirty = true;
        } else if ( value->isFontValue() ) {
            FontValueImpl *font = static_cast<FontValueImpl *>(value);
            if ( !font->style || !font->variant || !font->weight ||
                 !font->size || !font->lineHeight || !font->family )
                return;
            applyProperty( CSS_PROP_FONT_STYLE, font->style );
            applyProperty( CSS_PROP_FONT_VARIANT, font->variant );
            applyProperty( CSS_PROP_FONT_WEIGHT, font->weight );
            applyProperty( CSS_PROP_FONT_SIZE, font->size );

            // Line-height can depend on font().pixelSize(), so we have to update the font
            // before we evaluate line-height, e.g., font: 1em/1em.  FIXME: Still not
            // good enough: style="font:1em/1em; font-size:36px" should have a line-height of 36px.
            if (fontDirty)
                CSSStyleSelector::style->htmlFont().update( paintDeviceMetrics );
            
            applyProperty( CSS_PROP_LINE_HEIGHT, font->lineHeight );
            applyProperty( CSS_PROP_FONT_FAMILY, font->family );
        }
        return;
        
    case CSS_PROP_LIST_STYLE:
        if (isInherit) {
            style->setListStyleType(parentStyle->listStyleType());
            style->setListStyleImage(parentStyle->listStyleImage());
            style->setListStylePosition(parentStyle->listStylePosition());
        }
        else if (isInitial) {
            style->setListStyleType(RenderStyle::initialListStyleType());
            style->setListStyleImage(RenderStyle::initialListStyleImage());
            style->setListStylePosition(RenderStyle::initialListStylePosition());
        }
        break;
    case CSS_PROP_OUTLINE:
        if (isInherit) {
            style->setOutlineWidth(parentStyle->outlineWidth());
            style->setOutlineColor(parentStyle->outlineColor());
            style->setOutlineStyle(parentStyle->outlineStyle());
        }
        else if (isInitial)
            style->resetOutline();
        break;

    // CSS3 Properties
    case CSS_PROP__KHTML_BINDING: {
#ifndef KHTML_NO_XBL
        if (isInitial || (primitiveValue && primitiveValue->getIdent() == CSS_VAL_NONE)) {
            style->deleteBindingURIs();
            return;
        }
        else if (isInherit) {
            if (parentStyle->bindingURIs())
                style->inheritBindingURIs(parentStyle->bindingURIs());
            else
                style->deleteBindingURIs();
            return;
        }

        if (!value->isValueList()) return;
        CSSValueListImpl* list = static_cast<CSSValueListImpl*>(value);
        bool firstBinding = true;
        for (unsigned int i = 0; i < list->length(); i++) {
            CSSValueImpl *item = list->item(i);
            CSSPrimitiveValueImpl *val = static_cast<CSSPrimitiveValueImpl *>(item);
            if (val->primitiveType() == CSSPrimitiveValue::CSS_URI) {
                if (firstBinding) {
                    firstBinding = false;
                    style->deleteBindingURIs();
                }
                style->addBindingURI(val->getStringValue());
            }
        }
#endif
        break;
    }

    case CSS_PROP_OUTLINE_OFFSET: {
        HANDLE_INHERIT_AND_INITIAL(outlineOffset, OutlineOffset)

        int offset = primitiveValue->computeLength(style, paintDeviceMetrics);
        if (offset < 0) return;
        
        style->setOutlineOffset(offset);
        break;
    }

    case CSS_PROP_TEXT_SHADOW: {
        if (isInherit) {
            style->setTextShadow(parentStyle->textShadow() ? new ShadowData(*parentStyle->textShadow()) : 0);
            return;
        }
        else if (isInitial) {
            style->setTextShadow(0);
            return;
        }

        if (primitiveValue) { // none
            style->setTextShadow(0);
            return;
        }
        
        if (!value->isValueList()) return;
        CSSValueListImpl *list = static_cast<CSSValueListImpl *>(value);
        int len = list->length();
        for (int i = 0; i < len; i++) {
            ShadowValueImpl *item = static_cast<ShadowValueImpl*>(list->item(i));

            int x = item->x->computeLength(style, paintDeviceMetrics);
            int y = item->y->computeLength(style, paintDeviceMetrics);
            int blur = item->blur ? item->blur->computeLength(style, paintDeviceMetrics) : 0;
            QColor col = khtml::transparentColor;
            if (item->color) {
                int ident = item->color->getIdent();
                if (ident)
                    col = colorForCSSValue( ident );
                else if (item->color->primitiveType() == CSSPrimitiveValue::CSS_RGBCOLOR)
                    col.setRgb(item->color->getRGBColorValue());
            }
            ShadowData* shadowData = new ShadowData(x, y, blur, col);
            style->setTextShadow(shadowData, i != 0);
        }

        return;
    }
    case CSS_PROP_OPACITY:
        HANDLE_INHERIT_AND_INITIAL(opacity, Opacity)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        
        // Clamp opacity to the range 0-1
        style->setOpacity(kMin(1.0f, kMax(0, primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER))));
        return;
    case CSS_PROP__KHTML_BOX_ALIGN:
        HANDLE_INHERIT_AND_INITIAL(boxAlign, BoxAlign)
        if (!primitiveValue) return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_STRETCH:
                style->setBoxAlign(BSTRETCH);
                break;
            case CSS_VAL_START:
                style->setBoxAlign(BSTART);
                break;
            case CSS_VAL_END:
                style->setBoxAlign(BEND);
                break;
            case CSS_VAL_CENTER:
                style->setBoxAlign(BCENTER);
                break;
            case CSS_VAL_BASELINE:
                style->setBoxAlign(BBASELINE);
                break;
            default:
                return;
        }
        return;        
    case CSS_PROP__KHTML_BOX_DIRECTION:
        HANDLE_INHERIT_AND_INITIAL(boxDirection, BoxDirection)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_NORMAL)
            style->setBoxDirection(BNORMAL);
        else
            style->setBoxDirection(BREVERSE);
        return;        
    case CSS_PROP__KHTML_BOX_LINES:
        HANDLE_INHERIT_AND_INITIAL(boxLines, BoxLines)
        if(!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_SINGLE)
            style->setBoxLines(SINGLE);
        else
            style->setBoxLines(MULTIPLE);
        return;     
    case CSS_PROP__KHTML_BOX_ORIENT:
        HANDLE_INHERIT_AND_INITIAL(boxOrient, BoxOrient)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_HORIZONTAL ||
            primitiveValue->getIdent() == CSS_VAL_INLINE_AXIS)
            style->setBoxOrient(HORIZONTAL);
        else
            style->setBoxOrient(VERTICAL);
        return;     
    case CSS_PROP__KHTML_BOX_PACK:
        HANDLE_INHERIT_AND_INITIAL(boxPack, BoxPack)
        if (!primitiveValue) return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_START:
                style->setBoxPack(BSTART);
                break;
            case CSS_VAL_END:
                style->setBoxPack(BEND);
                break;
            case CSS_VAL_CENTER:
                style->setBoxPack(BCENTER);
                break;
            case CSS_VAL_JUSTIFY:
                style->setBoxPack(BJUSTIFY);
                break;
            default:
                return;
        }
        return;        
    case CSS_PROP__KHTML_BOX_FLEX:
        HANDLE_INHERIT_AND_INITIAL(boxFlex, BoxFlex)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        style->setBoxFlex(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER));
        return;
    case CSS_PROP__KHTML_BOX_FLEX_GROUP:
        HANDLE_INHERIT_AND_INITIAL(boxFlexGroup, BoxFlexGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        style->setBoxFlexGroup((unsigned int)(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)));
        return;        
    case CSS_PROP__KHTML_BOX_ORDINAL_GROUP:
        HANDLE_INHERIT_AND_INITIAL(boxOrdinalGroup, BoxOrdinalGroup)
        if (!primitiveValue || primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return; // Error case.
        style->setBoxOrdinalGroup((unsigned int)(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)));
        return;
    case CSS_PROP__KHTML_MARQUEE:
        if (value->cssValueType() != CSSValue::CSS_INHERIT || !parentNode) return;
        style->setMarqueeDirection(parentStyle->marqueeDirection());
        style->setMarqueeIncrement(parentStyle->marqueeIncrement());
        style->setMarqueeSpeed(parentStyle->marqueeSpeed());
        style->setMarqueeLoopCount(parentStyle->marqueeLoopCount());
        style->setMarqueeBehavior(parentStyle->marqueeBehavior());
        break;
    case CSS_PROP__KHTML_MARQUEE_REPETITION: {
        HANDLE_INHERIT_AND_INITIAL(marqueeLoopCount, MarqueeLoopCount)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent() == CSS_VAL_INFINITE)
            style->setMarqueeLoopCount(-1); // -1 means repeat forever.
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
            style->setMarqueeLoopCount((int)(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)));
        break;
    }
    case CSS_PROP__KHTML_MARQUEE_SPEED: {
        HANDLE_INHERIT_AND_INITIAL(marqueeSpeed, MarqueeSpeed)      
        if (!primitiveValue) return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent())
            {
                case CSS_VAL_SLOW:
                    style->setMarqueeSpeed(500); // 500 msec.
                    break;
                case CSS_VAL_NORMAL:
                    style->setMarqueeSpeed(85); // 85msec. The WinIE default.
                    break;
                case CSS_VAL_FAST:
                    style->setMarqueeSpeed(10); // 10msec. Super fast.
                    break;
            }
        }
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_S)
            style->setMarqueeSpeed(int(1000*primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_S)));
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_MS)
            style->setMarqueeSpeed(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_MS)));
        else if (primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER) // For scrollamount support.
            style->setMarqueeSpeed(int(primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_NUMBER)));
        break;
    }
    case CSS_PROP__KHTML_MARQUEE_INCREMENT: {
        HANDLE_INHERIT_AND_INITIAL(marqueeIncrement, MarqueeIncrement)
        if (!primitiveValue) return;
        if (primitiveValue->getIdent()) {
            switch (primitiveValue->getIdent())
            {
                case CSS_VAL_SMALL:
                    style->setMarqueeIncrement(Length(1, Fixed)); // 1px.
                    break;
                case CSS_VAL_NORMAL:
                    style->setMarqueeIncrement(Length(6, Fixed)); // 6px. The WinIE default.
                    break;
                case CSS_VAL_LARGE:
                    style->setMarqueeIncrement(Length(36, Fixed)); // 36px.
                    break;
            }
        }
        else {
            bool ok = true;
            Length l = convertToLength(primitiveValue, style, paintDeviceMetrics, &ok);
            if (ok)
                style->setMarqueeIncrement(l);
        }
        break;
    }
    case CSS_PROP__KHTML_MARQUEE_STYLE: {
        HANDLE_INHERIT_AND_INITIAL(marqueeBehavior, MarqueeBehavior)      
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        switch (primitiveValue->getIdent())
        {
            case CSS_VAL_NONE:
                style->setMarqueeBehavior(MNONE);
                break;
            case CSS_VAL_SCROLL:
                style->setMarqueeBehavior(MSCROLL);
                break;
            case CSS_VAL_SLIDE:
                style->setMarqueeBehavior(MSLIDE);
                break;
            case CSS_VAL_ALTERNATE:
                style->setMarqueeBehavior(MALTERNATE);
                break;
            case CSS_VAL_UNFURL:
                style->setMarqueeBehavior(MUNFURL);
                break;
        }
        break;
    }
    case CSS_PROP__KHTML_MARQUEE_DIRECTION: {
        HANDLE_INHERIT_AND_INITIAL(marqueeDirection, MarqueeDirection)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        switch (primitiveValue->getIdent())
        {
            case CSS_VAL_FORWARDS:
                style->setMarqueeDirection(MFORWARD);
                break;
            case CSS_VAL_BACKWARDS:
                style->setMarqueeDirection(MBACKWARD);
                break;
            case CSS_VAL_AUTO:
                style->setMarqueeDirection(MAUTO);
                break;
            case CSS_VAL_AHEAD:
            case CSS_VAL_UP: // We don't support vertical languages, so AHEAD just maps to UP.
                style->setMarqueeDirection(MUP);
                break;
            case CSS_VAL_REVERSE:
            case CSS_VAL_DOWN: // REVERSE just maps to DOWN, since we don't do vertical text.
                style->setMarqueeDirection(MDOWN);
                break;
            case CSS_VAL_LEFT:
                style->setMarqueeDirection(MLEFT);
                break;
            case CSS_VAL_RIGHT:
                style->setMarqueeDirection(MRIGHT);
                break;
        }
        break;
    }
    case CSS_PROP__KHTML_USER_DRAG: {
        HANDLE_INHERIT_AND_INITIAL(userDrag, UserDrag)      
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_AUTO:
                style->setUserDrag(DRAG_AUTO);
                break;
            case CSS_VAL_NONE:
                style->setUserDrag(DRAG_NONE);
                break;
	    case CSS_VAL_ELEMENT:
                style->setUserDrag(DRAG_ELEMENT);
	    default:
		return;
        }
        break;
    }
    case CSS_PROP__KHTML_USER_MODIFY: {
        HANDLE_INHERIT_AND_INITIAL(userModify, UserModify)      
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        EUserModify userModify = EUserModify(primitiveValue->getIdent() - CSS_VAL_READ_ONLY);
        style->setUserModify(userModify);
        break;
    }
    case CSS_PROP__KHTML_USER_SELECT: {
        HANDLE_INHERIT_AND_INITIAL(userSelect, UserSelect)      
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
	switch (primitiveValue->getIdent()) {
	    case CSS_VAL_AUTO:
		style->setUserSelect(SELECT_AUTO);
		break;
	    case CSS_VAL_NONE:
		style->setUserSelect(SELECT_NONE);
		break;
	    case CSS_VAL_TEXT:
		style->setUserSelect(SELECT_TEXT);
		break;
	    default:
		return;
	}
	break;
    }
    case CSS_PROP_TEXT_OVERFLOW: {
        // This property is supported by WinIE, and so we leave off the "-khtml-" in order to
        // work with WinIE-specific pages that use the property.
        HANDLE_INHERIT_AND_INITIAL(textOverflow, TextOverflow)
        if (!primitiveValue || !primitiveValue->getIdent())
            return;
        style->setTextOverflow(primitiveValue->getIdent() == CSS_VAL_ELLIPSIS);
        break;
    }
    case CSS_PROP__KHTML_MARGIN_COLLAPSE: {
        if (isInherit) {
            style->setMarginTopCollapse(parentStyle->marginTopCollapse());
            style->setMarginBottomCollapse(parentStyle->marginBottomCollapse());
        }
        else if (isInitial) {
            style->setMarginTopCollapse(MCOLLAPSE);
            style->setMarginBottomCollapse(MCOLLAPSE);
        }
        break;
    }
    case CSS_PROP__KHTML_MARGIN_TOP_COLLAPSE: {
        HANDLE_INHERIT_AND_INITIAL(marginTopCollapse, MarginTopCollapse)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        EMarginCollapse val;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_SEPARATE:
                val = MSEPARATE;
                break;
            case CSS_VAL_DISCARD:
                val = MDISCARD;
                break;
            default:
                val = MCOLLAPSE;
        }
        style->setMarginTopCollapse(val);
        break;
    }
    case CSS_PROP__KHTML_MARGIN_BOTTOM_COLLAPSE: {
        HANDLE_INHERIT_AND_INITIAL(marginBottomCollapse, MarginBottomCollapse)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        EMarginCollapse val;
        switch (primitiveValue->getIdent()) {
            case CSS_VAL_SEPARATE:
                val = MSEPARATE;
                break;
            case CSS_VAL_DISCARD:
                val = MDISCARD;
                break;
            default:
                val = MCOLLAPSE;
        }
        style->setMarginBottomCollapse(val);
        break;
    }

#if APPLE_CHANGES
    // Apple-specific changes.  Do not merge these properties into KHTML.
    case CSS_PROP__APPLE_LINE_CLAMP: {
        HANDLE_INHERIT_AND_INITIAL(lineClamp, LineClamp)
        if (!primitiveValue) return;
        style->setLineClamp((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE));
        break;
    }
    case CSS_PROP__APPLE_TEXT_SIZE_ADJUST: {
        HANDLE_INHERIT_AND_INITIAL(textSizeAdjust, TextSizeAdjust)
        if (!primitiveValue || !primitiveValue->getIdent()) return;
        style->setTextSizeAdjust(primitiveValue->getIdent() == CSS_VAL_AUTO);
        fontDirty = true;
        break;
    }
    case CSS_PROP__APPLE_DASHBOARD_REGION: {
        HANDLE_INHERIT_AND_INITIAL(dashboardRegions, DashboardRegions)
        if (!primitiveValue)
            return;

        if(primitiveValue->getIdent() == CSS_VAL_NONE) {
            style->setDashboardRegions(RenderStyle::noneDashboardRegions());
            return;
        }

        DashboardRegionImpl *region = primitiveValue->getDashboardRegionValue();
        if (!region)
            return;
            
        DashboardRegionImpl *first = region;
        while (region) {
            Length top = convertToLength (region->top(), style, paintDeviceMetrics );
            Length right = convertToLength (region->right(), style, paintDeviceMetrics );
            Length bottom = convertToLength (region->bottom(), style, paintDeviceMetrics );
            Length left = convertToLength (region->left(), style, paintDeviceMetrics );
            if (region->m_isCircle) {
                style->setDashboardRegion (StyleDashboardRegion::Circle, region->m_label, top, right, bottom, left, region == first ? false : true);
            }
            else if (region->m_isRectangle) {
                style->setDashboardRegion (StyleDashboardRegion::Rectangle, region->m_label, top, right, bottom, left, region == first ? false : true);
            }
            region = region->m_next;
        }
        
        element->getDocument()->setHasDashboardRegions (true);
        
        break;
    }   
#endif

    default:
        return;
    }
}

void CSSStyleSelector::mapBackgroundAttachment(BackgroundLayer* layer, DOM::CSSValueImpl* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundAttachment(RenderStyle::initialBackgroundAttachment());
        return;
    }

    if (!value->isPrimitiveValue()) return;
    CSSPrimitiveValueImpl* primitiveValue = static_cast<CSSPrimitiveValueImpl*>(value);
    switch (primitiveValue->getIdent()) {
        case CSS_VAL_FIXED:
            layer->setBackgroundAttachment(false);
            break;
        case CSS_VAL_SCROLL:
            layer->setBackgroundAttachment(true);
            break;
        default:
            return;
    }
}

void CSSStyleSelector::mapBackgroundImage(BackgroundLayer* layer, DOM::CSSValueImpl* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundImage(RenderStyle::initialBackgroundImage());
        return;
    }
    
    if (!value->isPrimitiveValue()) return;
    CSSPrimitiveValueImpl* primitiveValue = static_cast<CSSPrimitiveValueImpl*>(value);
    layer->setBackgroundImage(static_cast<CSSImageValueImpl *>(primitiveValue)->image(element->getDocument()->docLoader()));
}

void CSSStyleSelector::mapBackgroundRepeat(BackgroundLayer* layer, DOM::CSSValueImpl* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundRepeat(RenderStyle::initialBackgroundRepeat());
        return;
    }
    
    if (!value->isPrimitiveValue()) return;
    CSSPrimitiveValueImpl* primitiveValue = static_cast<CSSPrimitiveValueImpl*>(value);
    switch(primitiveValue->getIdent()) {
	case CSS_VAL_REPEAT:
	    layer->setBackgroundRepeat(REPEAT);
	    break;
	case CSS_VAL_REPEAT_X:
	    layer->setBackgroundRepeat(REPEAT_X);
	    break;
	case CSS_VAL_REPEAT_Y:
	    layer->setBackgroundRepeat(REPEAT_Y);
	    break;
	case CSS_VAL_NO_REPEAT:
	    layer->setBackgroundRepeat(NO_REPEAT);
	    break;
	default:
	    return;
    }
}

void CSSStyleSelector::mapBackgroundXPosition(BackgroundLayer* layer, DOM::CSSValueImpl* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundXPosition(RenderStyle::initialBackgroundXPosition());
        return;
    }
    
    if (!value->isPrimitiveValue()) return;
    CSSPrimitiveValueImpl* primitiveValue = static_cast<CSSPrimitiveValueImpl*>(value);
    Length l;
    int type = primitiveValue->primitiveType();
    if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
        l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
    else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
    else
        return;
    layer->setBackgroundXPosition(l);
}

void CSSStyleSelector::mapBackgroundYPosition(BackgroundLayer* layer, DOM::CSSValueImpl* value)
{
    if (value->cssValueType() == CSSValue::CSS_INITIAL) {
        layer->setBackgroundYPosition(RenderStyle::initialBackgroundYPosition());
        return;
    }
    
    if (!value->isPrimitiveValue()) return;
    CSSPrimitiveValueImpl* primitiveValue = static_cast<CSSPrimitiveValueImpl*>(value);
    Length l;
    int type = primitiveValue->primitiveType();
    if(type > CSSPrimitiveValue::CSS_PERCENTAGE && type < CSSPrimitiveValue::CSS_DEG)
        l = Length(primitiveValue->computeLength(style, paintDeviceMetrics), Fixed);
    else if(type == CSSPrimitiveValue::CSS_PERCENTAGE)
        l = Length((int)primitiveValue->getFloatValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
    else
        return;
    layer->setBackgroundYPosition(l);
}

#if APPLE_CHANGES
void CSSStyleSelector::checkForTextSizeAdjust()
{
    if (style->textSizeAdjust())
        return;
 
    FontDef newFontDef(style->htmlFont().fontDef);
    newFontDef.computedSize = newFontDef.specifiedSize;
    style->setFontDef(newFontDef);
}
#endif

void CSSStyleSelector::checkForGenericFamilyChange(RenderStyle* aStyle, RenderStyle* aParentStyle)
{
  const FontDef& childFont = aStyle->htmlFont().fontDef;
  
  if (childFont.isAbsoluteSize || !aParentStyle)
    return;

  const FontDef& parentFont = aParentStyle->htmlFont().fontDef;

  if (childFont.genericFamily == parentFont.genericFamily)
    return;

  // For now, lump all families but monospace together.
  if (childFont.genericFamily != FontDef::eMonospace &&
      parentFont.genericFamily != FontDef::eMonospace)
    return;

  // We know the parent is monospace or the child is monospace, and that font
  // size was unspecified.  We want to scale our font size as appropriate.
  float fixedScaleFactor = ((float)settings->mediumFixedFontSize())/settings->mediumFontSize();
  float size = (parentFont.genericFamily == FontDef::eMonospace) ? 
      childFont.specifiedSize/fixedScaleFactor :
      childFont.specifiedSize*fixedScaleFactor;
  
  FontDef newFontDef(childFont);
  setFontSize(newFontDef, size);
  aStyle->setFontDef(newFontDef);
}

void CSSStyleSelector::setFontSize(FontDef& fontDef, float size)
{
    fontDef.specifiedSize = size;
    fontDef.computedSize = getComputedSizeFromSpecifiedSize(fontDef.isAbsoluteSize, size);
}

float CSSStyleSelector::getComputedSizeFromSpecifiedSize(bool isAbsoluteSize, float specifiedSize)
{
    // We support two types of minimum font size.  The first is a hard override that applies to
    // all fonts.  This is "minSize."  The second type of minimum font size is a "smart minimum"
    // that is applied only when the Web page can't know what size it really asked for, e.g.,
    // when it uses logical sizes like "small" or expresses the font-size as a percentage of
    // the user's default font setting.

    // With the smart minimum, we never want to get smaller than the minimum font size to keep fonts readable.
    // However we always allow the page to set an explicit pixel size that is smaller,
    // since sites will mis-render otherwise (e.g., http://www.gamespot.com with a 9px minimum).
    int minSize = settings->minFontSize();
    int minLogicalSize = settings->minLogicalFontSize();

    float zoomPercent = (!khtml::printpainter && view) ? view->part()->zoomFactor()/100.0f : 1.0f;
    float zoomedSize = specifiedSize * zoomPercent;

    // Apply the hard minimum first.  We only apply the hard minimum if after zooming we're still too small.
    if (zoomedSize < minSize)
        zoomedSize = minSize;

    // Now apply the "smart minimum."  This minimum is also only applied if we're still too small
    // after zooming.  The font size must either be relative to the user default or the original size
    // must have been acceptable.  In other words, we only apply the smart minimum whenever we're positive
    // doing so won't disrupt the layout.
    if (zoomedSize < minLogicalSize && (specifiedSize >= minLogicalSize || !isAbsoluteSize))
        zoomedSize = minLogicalSize;
    
    return KMAX(zoomedSize, 1.0f);
}

const int fontSizeTableMax = 16;
const int fontSizeTableMin = 9;
const int totalKeywords = 8;

// WinIE/Nav4 table for font sizes.  Designed to match the legacy font mapping system of HTML.
static const int quirksFontSizeTable[fontSizeTableMax - fontSizeTableMin + 1][totalKeywords] =
{
      { 9,    9,     9,     9,    11,    14,    18,    28 },
      { 9,    9,     9,    10,    12,    15,    20,    31 },
      { 9,    9,     9,    11,    13,    17,    22,    34 },
      { 9,    9,    10,    12,    14,    18,    24,    37 },
      { 9,    9,    10,    13,    16,    20,    26,    40 }, // fixed font default (13)
      { 9,    9,    11,    14,    17,    21,    28,    42 },
      { 9,   10,    12,    15,    17,    23,    30,    45 },
      { 9,   10,    13,    16,    18,    24,    32,    48 }  // proportional font default (16)
};
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref

// Strict mode table matches MacIE and Mozilla's settings exactly.
static const int strictFontSizeTable[fontSizeTableMax - fontSizeTableMin + 1][totalKeywords] =
{
      { 9,    9,     9,     9,    11,    14,    18,    27 },
      { 9,    9,     9,    10,    12,    15,    20,    30 },
      { 9,    9,    10,    11,    13,    17,    22,    33 },
      { 9,    9,    10,    12,    14,    18,    24,    36 },
      { 9,   10,    12,    13,    16,    20,    26,    39 }, // fixed font default (13)
      { 9,   10,    12,    14,    17,    21,    28,    42 },
      { 9,   10,    13,    15,    18,    23,    30,    45 },
      { 9,   10,    13,    16,    18,    24,    32,    48 }  // proportional font default (16)
};
// HTML       1      2      3      4      5      6      7
// CSS  xxs   xs     s      m      l     xl     xxl
//                          |
//                      user pref

// For values outside the range of the table, we use Todd Fahrner's suggested scale
// factors for each keyword value.
static const float fontSizeFactors[totalKeywords] = { 0.60, 0.75, 0.89, 1.0, 1.2, 1.5, 2.0, 3.0 };

float CSSStyleSelector::fontSizeForKeyword(int keyword, bool quirksMode) const
{
    // FIXME: One issue here is that we set up the fixed font size as a scale factor applied
    // to the proportional pref.  This means that we won't get pixel-perfect size matching for
    // the 13px default, since we actually use the 16px row in the table and apply the fixed size
    // scale later.
    int mediumSize = settings->mediumFontSize();
    if (mediumSize >= fontSizeTableMin && mediumSize <= fontSizeTableMax) {
        // Look up the entry in the table.
        int row = mediumSize - fontSizeTableMin;
        int col = (keyword - CSS_VAL_XX_SMALL);
        return quirksMode ? quirksFontSizeTable[row][col] : strictFontSizeTable[row][col];
    }
    
    // Value is outside the range of the table. Apply the scale factor instead.
    float minLogicalSize = kMax(settings->minLogicalFontSize(), 1.0f);
    return kMax(fontSizeFactors[keyword - CSS_VAL_XX_SMALL]*mediumSize, minLogicalSize);
}

float CSSStyleSelector::largerFontSize(float size, bool quirksMode) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale up to
    // the next size level.  
    return size*1.2;
}

float CSSStyleSelector::smallerFontSize(float size, bool quirksMode) const
{
    // FIXME: Figure out where we fall in the size ranges (xx-small to xxx-large) and scale down to
    // the next size level. 
    return size/1.2;
}

QColor CSSStyleSelector::getColorFromPrimitiveValue(CSSPrimitiveValueImpl* primitiveValue)
{
    QColor col;
    int ident = primitiveValue->getIdent();
    if (ident) {
        if (ident == CSS_VAL__KHTML_TEXT)
            col = element->getDocument()->textColor();
        else if (ident == CSS_VAL__KHTML_LINK) {
            QColor linkColor = element->getDocument()->linkColor();
            QColor visitedColor = element->getDocument()->visitedLinkColor();
            if (linkColor == visitedColor)
                col = linkColor;
            else {
                if (pseudoState == PseudoUnknown || pseudoState == PseudoAnyLink)
                    checkPseudoState(element);
                col = (pseudoState == PseudoLink) ? linkColor : visitedColor;
            }
        }
        else if (ident == CSS_VAL__KHTML_ACTIVELINK)
            col = element->getDocument()->activeLinkColor();
        else
            col = colorForCSSValue( ident );
    } else if ( primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_RGBCOLOR )
        col.setRgb(primitiveValue->getRGBColorValue());
    return col;    
}

} // namespace khtml
