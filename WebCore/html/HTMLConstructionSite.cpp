/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLTreeBuilder.h"

#include "Comment.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLHtmlElement.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "HTMLToken.h"
#include "HTMLTokenizer.h"
#include "LegacyHTMLDocumentParser.h"
#include "LegacyHTMLTreeBuilder.h"
#include "LocalizedStrings.h"
#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif
#include "NotImplemented.h"
#if ENABLE(SVG)
#include "SVGNames.h"
#endif
#include "ScriptController.h"
#include "Settings.h"
#include "Text.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

using namespace HTMLNames;

namespace {

bool hasImpliedEndTag(Element* element)
{
    return element->hasTagName(ddTag)
        || element->hasTagName(dtTag)
        || element->hasTagName(liTag)
        || element->hasTagName(optionTag)
        || element->hasTagName(optgroupTag)
        || element->hasTagName(pTag)
        || element->hasTagName(rpTag)
        || element->hasTagName(rtTag);
}

bool causesFosterParenting(const QualifiedName& tagName)
{
    return tagName == tableTag
        || tagName == tbodyTag
        || tagName == tfootTag
        || tagName == theadTag
        || tagName == trTag;
}

} // namespace

template<typename ChildType>
PassRefPtr<ChildType> HTMLConstructionSite::attach(Node* parent, PassRefPtr<ChildType> prpChild)
{
    RefPtr<ChildType> child = prpChild;

    // FIXME: It's confusing that HTMLConstructionSite::attach does the magic
    // redirection to the foster parent but HTMLConstructionSite::attachAtSite
    // doesn't.  It feels like we're missing a concept somehow.
    if (shouldFosterParent()) {
        fosterParent(child.get());
        ASSERT(child->attached());
        return child.release();
    }

    parent->parserAddChild(child);
    // It's slightly unfortunate that we need to hold a reference to child
    // here to call attach().  We should investigate whether we can rely on
    // |parent| to hold a ref at this point.  In the common case (at least
    // for elements), however, we'll get to use this ref in the stack of
    // open elements.
    if (parent->attached()) {
        ASSERT(!child->attached());
        child->attach();
    }
    return child.release();
}

void HTMLConstructionSite::attachAtSite(const AttachmentSite& site, PassRefPtr<Node> prpChild)
{
    RefPtr<Node> child = prpChild;

    if (site.nextChild) {
        // FIXME: We need an insertElement which does not send mutation events.
        ExceptionCode ec = 0;
        site.parent->insertBefore(child, site.nextChild, ec);
        ASSERT(!ec);
        if (site.parent->attached() && !child->attached())
            child->attach();
        return;
    }
    site.parent->parserAddChild(child);
    // It's slightly unfortunate that we need to hold a reference to child
    // here to call attach().  We should investigate whether we can rely on
    // |site.parent| to hold a ref at this point.
    if (site.parent->attached() && !child->attached())
        child->attach();
}

HTMLConstructionSite::HTMLConstructionSite(Document* document, FragmentScriptingPermission scriptingPermission)
    : m_document(document)
    , m_fragmentScriptingPermission(scriptingPermission)
    , m_redirectAttachToFosterParent(false)
{
}

HTMLConstructionSite::~HTMLConstructionSite()
{
}

void HTMLConstructionSite::insertHTMLHtmlStartTagBeforeHTML(AtomicHTMLToken& token)
{
    RefPtr<Element> element = HTMLHtmlElement::create(m_document);
    element->setAttributeMap(token.takeAtributes(), m_fragmentScriptingPermission);
    m_openElements.pushHTMLHtmlElement(attach(m_document, element.release()));
}

void HTMLConstructionSite::mergeAttributesFromTokenIntoElement(AtomicHTMLToken& token, Element* element)
{
    if (!token.attributes())
        return;

    NamedNodeMap* attributes = element->attributes(false);
    for (unsigned i = 0; i < token.attributes()->length(); ++i) {
        Attribute* attribute = token.attributes()->attributeItem(i);
        if (!attributes->getAttributeItem(attribute->name()))
            element->setAttribute(attribute->name(), attribute->value());
    }
}

void HTMLConstructionSite::insertHTMLHtmlStartTagInBody(AtomicHTMLToken& token)
{
    // FIXME: parse error
    mergeAttributesFromTokenIntoElement(token, m_openElements.htmlElement());
}

void HTMLConstructionSite::insertHTMLBodyStartTagInBody(AtomicHTMLToken& token)
{
    // FIXME: parse error
    notImplemented(); // fragment case
    mergeAttributesFromTokenIntoElement(token, m_openElements.bodyElement());
}

void HTMLConstructionSite::insertDoctype(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::DOCTYPE);
    attach(m_document, DocumentType::create(m_document, token.name(), String::adopt(token.publicIdentifier()), String::adopt(token.systemIdentifier())));
    // FIXME: Move quirks mode detection from DocumentType element to here.
    notImplemented();
    if (token.forceQuirks())
        m_document->setParseMode(Document::Compat);
}

void HTMLConstructionSite::insertComment(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attach(currentElement(), Comment::create(m_document, token.comment()));
}

void HTMLConstructionSite::insertCommentOnDocument(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attach(m_document, Comment::create(m_document, token.comment()));
}

void HTMLConstructionSite::insertCommentOnHTMLHtmlElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attach(m_openElements.htmlElement(), Comment::create(m_document, token.comment()));
}

PassRefPtr<Element> HTMLConstructionSite::attachToCurrent(PassRefPtr<Element> child)
{
    return attach(currentElement(), child);
}

void HTMLConstructionSite::insertHTMLHtmlElement(AtomicHTMLToken& token)
{
    ASSERT(!shouldFosterParent());
    m_openElements.pushHTMLHtmlElement(attachToCurrent(createHTMLElement(token)));
}

void HTMLConstructionSite::insertHTMLHeadElement(AtomicHTMLToken& token)
{
    ASSERT(!shouldFosterParent());
    m_head = attachToCurrent(createHTMLElement(token));
    m_openElements.pushHTMLHeadElement(m_head);
}

void HTMLConstructionSite::insertHTMLBodyElement(AtomicHTMLToken& token)
{
    ASSERT(!shouldFosterParent());
    m_openElements.pushHTMLBodyElement(attachToCurrent(createHTMLElement(token)));
}

void HTMLConstructionSite::insertHTMLElement(AtomicHTMLToken& token)
{
    m_openElements.push(attachToCurrent(createHTMLElement(token)));
}

void HTMLConstructionSite::insertSelfClosingHTMLElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    attachToCurrent(createHTMLElement(token));
    // FIXME: Do we want to acknowledge the token's self-closing flag?
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#acknowledge-self-closing-flag
}

void HTMLConstructionSite::insertFormattingElement(AtomicHTMLToken& token)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#the-stack-of-open-elements
    // Possible active formatting elements include:
    // a, b, big, code, em, font, i, nobr, s, small, strike, strong, tt, and u.
    insertHTMLElement(token);
    m_activeFormattingElements.append(currentElement());
}

void HTMLConstructionSite::insertScriptElement(AtomicHTMLToken& token)
{
    RefPtr<HTMLScriptElement> element = HTMLScriptElement::create(scriptTag, m_document, true);
    element->setAttributeMap(token.takeAtributes(), m_fragmentScriptingPermission);
    m_openElements.push(attachToCurrent(element.release()));
}

void HTMLConstructionSite::insertForeignElement(AtomicHTMLToken& token, const AtomicString& namespaceURI)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    notImplemented(); // parseError when xmlns or xmlns:xlink are wrong.

    RefPtr<Element> element = attachToCurrent(createElement(token, namespaceURI));
    if (!token.selfClosing())
        m_openElements.push(element);
}

void HTMLConstructionSite::insertTextNode(const String& characters)
{
    AttachmentSite site;
    site.parent = currentElement();
    site.nextChild = 0;
    if (shouldFosterParent())
        findFosterSite(site);

    Node* previousChild = site.nextChild ? site.nextChild->previousSibling() : site.parent->lastChild();
    if (previousChild && previousChild->isTextNode()) {
        // FIXME: We're only supposed to append to this text node if it
        // was the last text node inserted by the parser.
        CharacterData* textNode = static_cast<CharacterData*>(previousChild);
        textNode->parserAppendData(characters);
        return;
    }

    attachAtSite(site, Text::create(m_document, characters));
}

PassRefPtr<Element> HTMLConstructionSite::createElement(AtomicHTMLToken& token, const AtomicString& namespaceURI)
{
    QualifiedName tagName(nullAtom, token.name(), namespaceURI);
    RefPtr<Element> element = m_document->createElement(tagName, true);
    element->setAttributeMap(token.takeAtributes(), m_fragmentScriptingPermission);
    return element.release();
}

PassRefPtr<Element> HTMLConstructionSite::createHTMLElement(AtomicHTMLToken& token)
{
    RefPtr<Element> element = createElement(token, xhtmlNamespaceURI);
    ASSERT(element->isHTMLElement());
    return element.release();
}

PassRefPtr<Element> HTMLConstructionSite::createHTMLElementFromElementRecord(HTMLElementStack::ElementRecord* record)
{
    // FIXME: This will change to use
    // return createHTMLElementFromSavedElement(record->element());
    // in a later patch once tested.
    AtomicHTMLToken fakeToken(HTMLToken::StartTag, record->element()->localName());
    return createHTMLElement(fakeToken);
}

namespace {

PassRefPtr<NamedNodeMap> cloneAttributes(Element* element)
{
    NamedNodeMap* attributes = element->attributes(true);
    if (!attributes)
        return 0;

    RefPtr<NamedNodeMap> newAttributes = NamedNodeMap::create();
    for (size_t i = 0; i < attributes->length(); ++i) {
        Attribute* attribute = attributes->attributeItem(i);
        RefPtr<Attribute> clone = Attribute::createMapped(attribute->name(), attribute->value());
        newAttributes->addAttribute(clone);
    }
    return newAttributes.release();
}

}

PassRefPtr<Element> HTMLConstructionSite::createHTMLElementFromSavedElement(Element* element)
{
    // FIXME: This method is wrong.  We should be using the original token.
    // Using an Element* causes us to fail examples like this:
    // <b id="1"><p><script>document.getElementById("1").id = "2"</script></p>TEXT</b>
    // When reconstructActiveFormattingElements calls this method to open
    // a second <b> tag to wrap TEXT, it will have id "2", even though the HTML5
    // spec implies it should be "1".  Minefield matches the HTML5 spec here.

    ASSERT(element->isHTMLElement()); // otherwise localName() might be wrong.
    AtomicHTMLToken fakeToken(HTMLToken::StartTag, element->localName(), cloneAttributes(element));
    return createHTMLElement(fakeToken);
}

bool HTMLConstructionSite::indexOfFirstUnopenFormattingElement(unsigned& firstUnopenElementIndex) const
{
    if (m_activeFormattingElements.isEmpty())
        return false;
    unsigned index = m_activeFormattingElements.size();
    do {
        --index;
        const HTMLFormattingElementList::Entry& entry = m_activeFormattingElements.at(index);
        if (entry.isMarker() || m_openElements.contains(entry.element())) {
            firstUnopenElementIndex = index + 1;
            return firstUnopenElementIndex < m_activeFormattingElements.size();
        }
    } while (index);
    firstUnopenElementIndex = index;
    return true;
}

void HTMLConstructionSite::reconstructTheActiveFormattingElements()
{
    unsigned firstUnopenElementIndex;
    if (!indexOfFirstUnopenFormattingElement(firstUnopenElementIndex))
        return;

    unsigned unopenEntryIndex = firstUnopenElementIndex;
    ASSERT(unopenEntryIndex < m_activeFormattingElements.size());
    for (; unopenEntryIndex < m_activeFormattingElements.size(); ++unopenEntryIndex) {
        HTMLFormattingElementList::Entry& unopenedEntry = m_activeFormattingElements.at(unopenEntryIndex);
        RefPtr<Element> reconstructed = createHTMLElementFromSavedElement(unopenedEntry.element());
        m_openElements.push(attachToCurrent(reconstructed.release()));
        unopenedEntry.replaceElement(currentElement());
    }
}

void HTMLConstructionSite::generateImpliedEndTagsWithExclusion(const AtomicString& tagName)
{
    while (hasImpliedEndTag(currentElement()) && !currentElement()->hasLocalName(tagName))
        m_openElements.pop();
}

void HTMLConstructionSite::generateImpliedEndTags()
{
    while (hasImpliedEndTag(currentElement()))
        m_openElements.pop();
}

void HTMLConstructionSite::findFosterSite(AttachmentSite& site)
{
    HTMLElementStack::ElementRecord* lastTableElementRecord = m_openElements.topmost(tableTag.localName());
    if (lastTableElementRecord) {
        Element* lastTableElement = lastTableElementRecord->element();
        if (Node* parent = lastTableElement->parent()) {
            site.parent = parent;
            site.nextChild = lastTableElement;
            return;
        }
        site.parent = lastTableElementRecord->next()->element();
        site.nextChild = 0;
        return;
    }
    // Fragment case
    site.parent = m_openElements.bottom(); // <html> element
    site.nextChild = 0;
}

bool HTMLConstructionSite::shouldFosterParent() const
{
    return m_redirectAttachToFosterParent
        && causesFosterParenting(currentElement()->tagQName());
}

void HTMLConstructionSite::fosterParent(Node* node)
{
    AttachmentSite site;
    findFosterSite(site);
    attachAtSite(site, node);
}

}
