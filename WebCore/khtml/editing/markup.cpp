/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "markup.h"

#include "htmlediting.h"

#include "css/css_computedstyle.h"
#include "css/css_valueimpl.h"
#include "editing/visible_position.h"
#include "editing/visible_units.h"
#include "html/html_elementimpl.h"
#include "xml/dom_position.h"
#include "xml/dom2_rangeimpl.h"
#include "rendering/render_text.h"
#include "misc/htmlattrs.h"
#include "misc/htmltags.h"
#include "html/dtd.h"

using DOM::AttributeImpl;
using DOM::CommentImpl;
using DOM::CSSComputedStyleDeclarationImpl;
using DOM::CSSMutableStyleDeclarationImpl;
using DOM::DocumentFragmentImpl;
using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::FORBIDDEN;
using DOM::endTagRequirement;
using DOM::HTMLElementImpl;
using DOM::NamedAttrMapImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::Position;
using DOM::RangeImpl;
using DOM::TextImpl;

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#else
#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define LOG(channel, formatAndArgs...) ((void)0)
#define ERROR(formatAndArgs...) ((void)0)
#define ASSERT(assertion) assert(assertion)
#endif

namespace khtml {

static QString escapeHTML(const QString &in)
{
    QString s = "";

    unsigned len = in.length();
    for (unsigned i = 0; i < len; ++i) {
        switch (in[i].latin1()) {
            case '&':
                s += "&amp;";
                break;
            case '<':
                s += "&lt;";
                break;
            case '>':
                s += "&gt;";
                break;
            default:
                s += in[i];
        }
    }

    return s;
}

static QString stringValueForRange(const NodeImpl *node, const RangeImpl *range)
{
    DOMString str = node->nodeValue().copy();
    if (range) {
        int exceptionCode;
        if (node == range->endContainer(exceptionCode)) {
            str.truncate(range->endOffset(exceptionCode));
        }
        if (node == range->startContainer(exceptionCode)) {
            str.remove(0, range->startOffset(exceptionCode));
        }
    }
    return str.string();
}

static QString renderedText(const NodeImpl *node, const RangeImpl *range)
{
    RenderObject *r = node->renderer();
    if (!r)
        return QString();
    
    if (!node->isTextNode())
        return QString();

    QString result = "";

    int exceptionCode;
    const TextImpl *textNode = static_cast<const TextImpl *>(node);
    unsigned startOffset = 0;
    unsigned endOffset = textNode->length();

    if (range && node == range->startContainer(exceptionCode))
        startOffset = range->startOffset(exceptionCode);
    if (range && node == range->endContainer(exceptionCode))
        endOffset = range->endOffset(exceptionCode);
    
    RenderText *textRenderer = static_cast<RenderText *>(r);
    QString str = node->nodeValue().string();
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        unsigned start = box->m_start;
        unsigned end = box->m_start + box->m_len;
        if (endOffset < start)
            break;
        if (startOffset <= end) {
            unsigned s = kMax(start, startOffset);
            unsigned e = kMin(end, endOffset);
            result.append(str.mid(s, e-s));
            if (e == end) {
                // now add in collapsed-away spaces if at the end of the line
                InlineTextBox *nextBox = box->nextTextBox();
                if (nextBox && box->root() != nextBox->root()) {
                    const char nonBreakingSpace = '\xa0';
                    // count the number of characters between the end of the
                    // current box and the start of the next box.
                    int collapsedStart = e;
                    int collapsedPastEnd = kMin((unsigned)nextBox->m_start, endOffset + 1);
                    bool addNextNonNBSP = true;
                    for (int i = collapsedStart; i < collapsedPastEnd; i++) {
                        if (str[i] == nonBreakingSpace) {
                            result.append(str[i]);
                            addNextNonNBSP = true;
                        }
                        else if (addNextNonNBSP) {
                            result.append(str[i]);
                            addNextNonNBSP = false;
                        }
                    }
                }
            }
        }

    }
    
    return result;
}

static QString startMarkup(const NodeImpl *node, const RangeImpl *range, EAnnotateForInterchange annotate, CSSMutableStyleDeclarationImpl *defaultStyle)
{
    unsigned short type = node->nodeType();
    switch (type) {
        case Node::TEXT_NODE: {
            if (node->parentNode()) {
                switch (node->parentNode()->id()) {
                    case ID_PRE:
                    case ID_SCRIPT:
                    case ID_STYLE:
                    case ID_TEXTAREA:
                        return stringValueForRange(node, range);
                }
            }
            QString markup = annotate ? escapeHTML(renderedText(node, range)) : escapeHTML(stringValueForRange(node, range));            
            if (defaultStyle) {
                NodeImpl *element = node->parentNode();
                if (element) {
                    CSSMutableStyleDeclarationImpl *style = Position(element, 0).computedStyle()->copyInheritableProperties();
                    style->ref();
                    defaultStyle->diff(style);
                    if (style->length() > 0) {
                        // FIXME: Handle case where style->cssText() has illegal characters in it, like "
                        QString openTag = QString("<span class=\"") + AppleStyleSpanClass + "\" style=\"" + style->cssText().string() + "\">";
                        markup = openTag + markup + "</span>";
                    }
                    style->deref();
                }            
            }
            return annotate ? convertHTMLTextToInterchangeFormat(markup) : markup;
        }
        case Node::COMMENT_NODE:
            return static_cast<const CommentImpl *>(node)->toString().string();
        case Node::DOCUMENT_NODE:
            return "";
        default: {
            QString markup = QChar('<') + node->nodeName().string();
            if (type == Node::ELEMENT_NODE) {
                const ElementImpl *el = static_cast<const ElementImpl *>(node);
                DOMString additionalStyle;
                if (defaultStyle && el->isHTMLElement()) {
                    CSSMutableStyleDeclarationImpl *style = Position(const_cast<ElementImpl *>(el), 0).computedStyle()->copyInheritableProperties();
                    style->ref();
                    defaultStyle->diff(style);
                    if (style->length() > 0) {
                        CSSMutableStyleDeclarationImpl *inlineStyleDecl = static_cast<const HTMLElementImpl *>(el)->inlineStyleDecl();
                        if (inlineStyleDecl)
                            inlineStyleDecl->diff(style);
                        additionalStyle = style->cssText();
                    }
                    style->deref();
                }
                NamedAttrMapImpl *attrs = el->attributes();
                unsigned long length = attrs->length();
                if (length == 0 && additionalStyle.length() > 0) {
                    // FIXME: Handle case where additionalStyle has illegal characters in it, like "
                    markup += " " + node->getDocument()->attrName(ATTR_STYLE).string() + "=\"" + additionalStyle.string() + "\"";
                }
                else {
                    for (unsigned int i=0; i<length; i++) {
                        AttributeImpl *attr = attrs->attributeItem(i);
                        DOMString value = attr->value();
                        if (attr->id() == ATTR_STYLE && additionalStyle.length() > 0)
                            value += "; " + additionalStyle;
                        // FIXME: Handle case where value has illegal characters in it, like "
                        markup += " " + node->getDocument()->attrName(attr->id()).string() + "=\"" + value.string() + "\"";
                    }
                }
            }
            markup += node->isHTMLElement() ? ">" : "/>";
            return markup;
        }
    }
}

static QString endMarkup(const NodeImpl *node)
{
    if ((!node->isHTMLElement() || endTagRequirement(node->id()) != FORBIDDEN) && node->nodeType() != Node::TEXT_NODE && node->nodeType() != Node::DOCUMENT_NODE) {
        return "</" + node->nodeName().string() + ">";
    }
    return "";
}

static QString markup(const NodeImpl *startNode, bool onlyIncludeChildren, bool includeSiblings, QPtrList<NodeImpl> *nodes)
{
    // Doesn't make sense to only include children and include siblings.
    ASSERT(!(onlyIncludeChildren && includeSiblings));
    QString me = "";
    for (const NodeImpl *current = startNode; current != NULL; current = includeSiblings ? current->nextSibling() : NULL) {
        if (!onlyIncludeChildren) {
            if (nodes) {
                nodes->append(current);
            }
            me += startMarkup(current, 0, DoNotAnnotateForInterchange, 0);
        }        
        if (!current->isHTMLElement() || endTagRequirement(current->id()) != FORBIDDEN) {
            // print children
            if (NodeImpl *n = current->firstChild()) {
                me += markup(n, false, true, nodes);
            }
            // Print my ending tag
            if (!onlyIncludeChildren) {
                me += endMarkup(current);
            }
        }
    }
    return me;
}

static void completeURLs(NodeImpl *node, const QString &baseURL)
{
    NodeImpl *end = node->traverseNextSibling();
    for (NodeImpl *n = node; n != end; n = n->traverseNextNode()) {
        if (n->nodeType() == Node::ELEMENT_NODE) {
            ElementImpl *e = static_cast<ElementImpl *>(n);
            NamedAttrMapImpl *attrs = e->attributes();
            unsigned long length = attrs->length();
            for (unsigned long i = 0; i < length; i++) {
                AttributeImpl *attr = attrs->attributeItem(i);
                if (e->isURLAttribute(attr)) {
                    e->setAttribute(attr->id(), KURL(baseURL, attr->value().string()).url());
                }
            }
        }
    }
}

QString createMarkup(const RangeImpl *range, QPtrList<NodeImpl> *nodes, EAnnotateForInterchange annotate)
{
    if (!range || range->isDetached())
        return QString();

    static const QString interchangeNewlineString = QString("<br class=\"") + AppleInterchangeNewline + "\">";

    int exceptionCode = 0;
    NodeImpl *commonAncestor = range->commonAncestorContainer(exceptionCode);
    ASSERT(exceptionCode == 0);

    DocumentImpl *doc = commonAncestor->getDocument();
    doc->updateLayout();

    NodeImpl *commonAncestorBlock = 0;
    if (commonAncestor != 0) {
        commonAncestorBlock = commonAncestor->enclosingBlockFlowElement();
    }
    if (commonAncestorBlock == 0) {
        return "";    
    }

    QStringList markups;
    NodeImpl *pastEnd = range->pastEndNode();
    NodeImpl *lastClosed = 0;
    QPtrList<NodeImpl> ancestorsToClose;

    // calculate the "default style" for this markup
    Position pos(doc->documentElement(), 0);
    CSSMutableStyleDeclarationImpl *defaultStyle = pos.computedStyle()->copyInheritableProperties();
    defaultStyle->ref();
    
    NodeImpl *startNode = range->startNode();
    VisiblePosition visibleStart(range->startPosition(), VP_DEFAULT_AFFINITY);
    VisiblePosition visibleEnd(range->endPosition(), VP_DEFAULT_AFFINITY);
    if (isEndOfBlock(visibleStart)) {
        if (visibleStart == visibleEnd.previous())
            return interchangeNewlineString;
        markups.append(interchangeNewlineString);
        startNode = startNode->traverseNextNode();
    }
    
    // Iterate through the nodes of the range.
    NodeImpl *next;
    for (NodeImpl *n = startNode; n != pastEnd; n = next) {
        next = n->traverseNextNode();

        if (n->isBlockFlow() && next == pastEnd) {
            // Don't write out an empty block.
            continue;
        }
        
        // Add the node to the markup.
        if (n->renderer()) {
            markups.append(startMarkup(n, range, annotate, defaultStyle));
            if (nodes) {
                nodes->append(n);
            }
        }
        
        if (n->firstChild() == 0) {
            // Node has no children, add its close tag now.
            if (n->renderer()) {
                markups.append(endMarkup(n));
                lastClosed = n;
            }
            
            // Check if the node is the last leaf of a tree.
            if (n->nextSibling() == 0 || next == pastEnd) {
                if (!ancestorsToClose.isEmpty()) {
                    // Close up the ancestors.
                    while (NodeImpl *ancestor = ancestorsToClose.last()) {
                        if (next != pastEnd && ancestor == next->parentNode()) {
                            break;
                        }
                        // Not at the end of the range, close ancestors up to sibling of next node.
                        markups.append(endMarkup(ancestor));
                        lastClosed = ancestor;
                        ancestorsToClose.removeLast();
                    }
                } else {
                    // No ancestors to close, but need to add ancestors not in range since next node is in another tree. 
                    if (next != pastEnd) {
                        NodeImpl *nextParent = next->parentNode();
                        if (n != nextParent) {
                            for (NodeImpl *parent = n->parent(); parent != 0 && parent != nextParent; parent = parent->parentNode()) {
                                markups.prepend(startMarkup(parent, range, annotate, defaultStyle));
                                markups.append(endMarkup(parent));
                                if (nodes) {
                                    nodes->append(parent);
                                }                            
                                lastClosed = parent;
                            }
                        }
                    }
                }
            }
        } else if (n->renderer()) {
            // Node is an ancestor, set it to close eventually.
            ancestorsToClose.append(n);
        }
    }
    
    NodeImpl *rangeStartNode = range->startNode();
    int rangeStartOffset = range->startOffset(exceptionCode);
    ASSERT(exceptionCode == 0);
    
    // Add ancestors up to the common ancestor block so inline ancestors such as FONT and B are part of the markup.
    if (lastClosed) {
        for (NodeImpl *ancestor = lastClosed->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
            if (RangeImpl::compareBoundaryPoints(ancestor, 0, rangeStartNode, rangeStartOffset) >= 0) {
                // we have already added markup for this node
                continue;
            }
            bool breakAtEnd = false;
            if (commonAncestorBlock == ancestor) {
                NodeImpl::Id id = ancestor->id();
                // Include ancestors that are required to retain the appearance of the copied markup.
                if (id == ID_PRE || id == ID_TABLE || id == ID_OL || id == ID_UL) {
                    breakAtEnd = true;
                } else {
                    break;
                }
            }
            markups.prepend(startMarkup(ancestor, range, annotate, defaultStyle));
            markups.append(endMarkup(ancestor));
            if (nodes) {
                nodes->append(ancestor);
            }        
            if (breakAtEnd) {
                break;
            }
        }
    }

    if (annotate) {
        Position pos(endPosition(range));
        NodeImpl *block = pos.node()->enclosingBlockFlowElement();
        NodeImpl *upstreamBlock = pos.upstream().node()->enclosingBlockFlowElement();
        if (block != upstreamBlock) {
            markups.append(interchangeNewlineString);
        }
    }

    // Retain the Mail quote level by including all ancestor mail block quotes.
    for (NodeImpl *ancestor = commonAncestorBlock; ancestor; ancestor = ancestor->parentNode()) {
        if (isMailBlockquote(ancestor)) {
            markups.prepend(startMarkup(ancestor, range, annotate, defaultStyle));
            markups.append(endMarkup(ancestor));
        }
    }
    
    // add in the "default style" for this markup
    // FIXME: Handle case where value has illegal characters in it, like "
    QString openTag = QString("<span class=\"") + AppleStyleSpanClass + "\" style=\"" + defaultStyle->cssText().string() + "\">";
    markups.prepend(openTag);
    markups.append("</span>");
    defaultStyle->deref();

    return markups.join("");
}

DocumentFragmentImpl *createFragmentFromMarkup(DocumentImpl *document, const QString &markup, const QString &baseURL)
{
    // FIXME: What if the document element is not an HTML element?
    HTMLElementImpl *element = static_cast<HTMLElementImpl *>(document->documentElement());

    DocumentFragmentImpl *fragment = element->createContextualFragment(markup);
    ASSERT(fragment);

    if (!baseURL.isEmpty() && baseURL != document->baseURL())
        completeURLs(fragment, baseURL);

    return fragment;
}

QString createMarkup(const DOM::NodeImpl *node, EChildrenOnly includeChildren,
    QPtrList<DOM::NodeImpl> *nodes, EAnnotateForInterchange annotate)
{
    ASSERT(annotate == DoNotAnnotateForInterchange); // annotation not yet implemented for this code path

    // FIXME: We could take out this if statement if we had more time to test.
    // I'm concerned that making this crash when the document is nil might be too risky a change at the moment.
    DocumentImpl *doc = node->getDocument();
    assert(doc);
    if (doc)
        doc->updateLayout();

    return markup(node, includeChildren, false, nodes);
}

DOM::DocumentFragmentImpl *createFragmentFromText(DOM::DocumentImpl *document, const QString &text)
{
    if (!document)
        return 0;

    DocumentFragmentImpl *fragment = document->createDocumentFragment();
    fragment->ref();
    
    if (!text.isEmpty()) {
        QString string = text;

        // Replace tabs with four plain spaces.
        // These spaces will get converted along with the other existing spaces below.
        string.replace('\t', "    ");

        // FIXME: Wrap the NBSP's in a span that says "converted space".
        int offset = 0;
        int stringLength = string.length();
        while (1) {
            // FIXME: This only converts plain old spaces, and does not
            // deal with more exotic whitespace. Note that we want to 
            // leave newlines and returns alone at this point anyway, 
            // since those are handled specially later.
            int spacesPos = string.find("  ", offset);
            if (spacesPos < 0)
                break;

            // Found two adjoining spaces.
            // Now, lookahead to see if these two spaces are followed by:
            //   1. another space and then a non-space
            //   2. another space and then the end of the string
            // If either 1 or 2 is true, replace the three spaces found with nbsp+nbsp+space, 
            // otherwise, replace the first two spaces with nbsp+space.
            if ((spacesPos + 3 < stringLength && string[spacesPos + 2] == ' ' && string[spacesPos + 3] != ' ')
                    || (spacesPos + 3 == stringLength && string[spacesPos + 2] == ' ')) {
                string.replace(spacesPos, 3, "\xA0\xA0 ");
            } else {
                string.replace(spacesPos, 2, "\xA0 ");
            }
            offset = spacesPos + 2;
        }

        // Break string into paragraphs. Extra line breaks turn into empty paragraphs.
        string.replace(QString("\r\n"), "\n");
        string.replace('\r', '\n');
        QStringList list = QStringList::split('\n', string, true); // true gets us empty strings in the list
        while (!list.isEmpty()) {
            QString s = list.first();
            list.pop_front();

            int exceptionCode = 0;
            ElementImpl *element;
            if (s.isEmpty() && list.isEmpty()) {
                // For last line, use the "magic BR" rather than a P.
                element = document->createHTMLElement("br", exceptionCode);
                ASSERT(exceptionCode == 0);
                element->ref();
                element->setAttribute(ATTR_CLASS, AppleInterchangeNewline);            
            } else {
                element = createDefaultParagraphElement(document);
                NodeImpl *paragraphContents;
                if (s.isEmpty()) {
                    paragraphContents = createBlockPlaceholderElement(document);
                } else {
                    paragraphContents = document->createTextNode(s);
                    ASSERT(exceptionCode == 0);
                }
                element->ref();
                element->appendChild(paragraphContents, exceptionCode);
                ASSERT(exceptionCode == 0);
            }
            fragment->appendChild(element, exceptionCode);
            ASSERT(exceptionCode == 0);
            element->deref();
        }
    }
    
    // Trick to get the fragment back to the floating state, with 0
    // refs but not destroyed.
    fragment->setParent(document);
    fragment->deref();
    fragment->setParent(0);
    
    return fragment;
}

}
