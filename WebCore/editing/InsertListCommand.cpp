/*
 * Copyright (C) 2006, 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Element.h"
#include "InsertListCommand.h"
#include "DocumentFragment.h"
#include "htmlediting.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "TextIterator.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<HTMLElement> InsertListCommand::insertList(Document* document, Type type)
{
    RefPtr<InsertListCommand> insertCommand = create(document, type);
    insertCommand->apply();
    return insertCommand->m_listElement;
}

HTMLElement* InsertListCommand::fixOrphanedListChild(Node* node)
{
    RefPtr<HTMLElement> listElement = createUnorderedListElement(document());
    insertNodeBefore(listElement, node);
    removeNode(node);
    appendNode(node, listElement);
    m_listElement = listElement;
    return listElement.get();
}

InsertListCommand::InsertListCommand(Document* document, Type type) 
    : CompositeEditCommand(document), m_type(type)
{
}

void InsertListCommand::doApply()
{
    if (endingSelection().isNone())
        return;
    
    if (!endingSelection().rootEditableElement())
        return;
    
    VisiblePosition visibleEnd = endingSelection().visibleEnd();
    VisiblePosition visibleStart = endingSelection().visibleStart();
    // When a selection ends at the start of a paragraph, we rarely paint 
    // the selection gap before that paragraph, because there often is no gap.  
    // In a case like this, it's not obvious to the user that the selection 
    // ends "inside" that paragraph, so it would be confusing if InsertUn{Ordered}List 
    // operated on that paragraph.
    // FIXME: We paint the gap before some paragraphs that are indented with left 
    // margin/padding, but not others.  We should make the gap painting more consistent and 
    // then use a left margin/padding rule here.
    if (visibleEnd != visibleStart && isStartOfParagraph(visibleEnd))
        setEndingSelection(VisibleSelection(visibleStart, visibleEnd.previous(true)));

    if (endingSelection().isRange()) {
        VisibleSelection selection = selectionForParagraphIteration(endingSelection());
        ASSERT(selection.isRange());
        VisiblePosition startOfSelection = selection.visibleStart();
        VisiblePosition endOfSelection = selection.visibleEnd();
        VisiblePosition startOfLastParagraph = startOfParagraph(endOfSelection);

        if (startOfParagraph(startOfSelection) != startOfLastParagraph) {
            Node* startList = enclosingList(startOfSelection.deepEquivalent().node());
            Node* endList = enclosingList(endOfSelection.deepEquivalent().node());
            bool forceCreateList = !startList || startList != endList;

            VisiblePosition startOfCurrentParagraph = startOfSelection;
            while (startOfCurrentParagraph != startOfLastParagraph) {
                // doApply() may operate on and remove the last paragraph of the selection from the document 
                // if it's in the same list item as startOfCurrentParagraph.  Return early to avoid an 
                // infinite loop and because there is no more work to be done.
                // FIXME(<rdar://problem/5983974>): The endingSelection() may be incorrect here.  Compute 
                // the new location of endOfSelection and use it as the end of the new selection.
                if (!startOfLastParagraph.deepEquivalent().node()->inDocument())
                    return;
                setEndingSelection(startOfCurrentParagraph);
                doApplyForSingleParagraph(forceCreateList);

                // Fetch the start of the selection after moving the first paragraph,
                // because moving the paragraph will invalidate the original start.  
                // We'll use the new start to restore the original selection after 
                // we modified all selected paragraphs.
                if (startOfCurrentParagraph == startOfSelection)
                    startOfSelection = endingSelection().visibleStart();

                startOfCurrentParagraph = startOfNextParagraph(endingSelection().visibleStart());
            }
            setEndingSelection(endOfSelection);
            doApplyForSingleParagraph(forceCreateList);
            // Fetch the end of the selection, for the reason mentioned above.
            endOfSelection = endingSelection().visibleEnd();
            setEndingSelection(VisibleSelection(startOfSelection, endOfSelection));
            return;
        }
    }

    doApplyForSingleParagraph(false);
}

void InsertListCommand::doApplyForSingleParagraph(bool forceCreateList)
{
    // FIXME: This will produce unexpected results for a selection that starts just before a
    // table and ends inside the first cell, selectionForParagraphIteration should probably
    // be renamed and deployed inside setEndingSelection().
    Node* selectionNode = endingSelection().start().node();
    const QualifiedName listTag = (m_type == OrderedList) ? olTag : ulTag;
    Node* listChildNode = enclosingListChild(selectionNode);
    bool switchListType = false;
    if (listChildNode) {
        // Remove the list chlild.
        HTMLElement* listNode = enclosingList(listChildNode);
        if (!listNode)
            listNode = fixOrphanedListChild(listChildNode);
        if (!listNode->hasTagName(listTag))
            // listChildNode will be removed from the list and a list of type m_type will be created.
            switchListType = true;

        unlistifyParagraph(endingSelection().visibleStart(), listNode, listChildNode);
    }

    if (!listChildNode || switchListType || forceCreateList)
        m_listElement = listifyParagraph(endingSelection().visibleStart(), listTag);
}

void InsertListCommand::unlistifyParagraph(const VisiblePosition& originalStart, HTMLElement* listNode, Node* listChildNode)
{
    Node* nextListChild;
    Node* previousListChild;
    VisiblePosition start;
    VisiblePosition end;
    if (listChildNode->hasTagName(liTag)) {
        start = firstDeepEditingPositionForNode(listChildNode);
        end = lastDeepEditingPositionForNode(listChildNode);
        nextListChild = listChildNode->nextSibling();
        previousListChild = listChildNode->previousSibling();
    } else {
        // A paragraph is visually a list item minus a list marker.  The paragraph will be moved.
        start = startOfParagraph(originalStart);
        end = endOfParagraph(start);
        nextListChild = enclosingListChild(end.next().deepEquivalent().node());
        ASSERT(nextListChild != listChildNode);
        if (enclosingList(nextListChild) != listNode)
            nextListChild = 0;
        previousListChild = enclosingListChild(start.previous().deepEquivalent().node());
        ASSERT(previousListChild != listChildNode);
        if (enclosingList(previousListChild) != listNode)
            previousListChild = 0;
    }
    // When removing a list, we must always create a placeholder to act as a point of insertion
    // for the list content being removed.
    RefPtr<Element> placeholder = createBreakElement(document());
    RefPtr<Element> nodeToInsert = placeholder;
    // If the content of the list item will be moved into another list, put it in a list item
    // so that we don't create an orphaned list child.
    if (enclosingList(listNode)) {
        nodeToInsert = createListItemElement(document());
        appendNode(placeholder, nodeToInsert);
    }

    if (nextListChild && previousListChild) {
        // We want to pull listChildNode out of listNode, and place it before nextListChild 
        // and after previousListChild, so we split listNode and insert it between the two lists.  
        // But to split listNode, we must first split ancestors of listChildNode between it and listNode,
        // if any exist.
        // FIXME: We appear to split at nextListChild as opposed to listChildNode so that when we remove
        // listChildNode below in moveParagraphs, previousListChild will be removed along with it if it is 
        // unrendered. But we ought to remove nextListChild too, if it is unrendered.
        splitElement(listNode, splitTreeToNode(nextListChild, listNode));
        insertNodeBefore(nodeToInsert, listNode);
    } else if (nextListChild || listChildNode->parentNode() != listNode) {
        // Just because listChildNode has no previousListChild doesn't mean there isn't any content
        // in listNode that comes before listChildNode, as listChildNode could have ancestors
        // between it and listNode. So, we split up to listNode before inserting the placeholder
        // where we're about to move listChildNode to.
        if (listChildNode->parentNode() != listNode)
            splitElement(listNode, splitTreeToNode(listChildNode, listNode).get());
        insertNodeBefore(nodeToInsert, listNode);
    } else
        insertNodeAfter(nodeToInsert, listNode);

    VisiblePosition insertionPoint = VisiblePosition(Position(placeholder.get(), 0));
    moveParagraphs(start, end, insertionPoint, true);
}

PassRefPtr<HTMLElement> InsertListCommand::listifyParagraph(const VisiblePosition& originalStart, const QualifiedName& listTag)
{
    VisiblePosition start = startOfParagraph(originalStart);
    VisiblePosition end = endOfParagraph(start);

    // Check for adjoining lists.
    VisiblePosition previousPosition = start.previous(true);
    VisiblePosition nextPosition = end.next(true);
    RefPtr<HTMLElement> listItemElement = createListItemElement(document());
    RefPtr<HTMLElement> placeholder = createBreakElement(document());
    appendNode(placeholder, listItemElement);
    Element* previousList = outermostEnclosingList(previousPosition.deepEquivalent().node());
    Element* nextList = outermostEnclosingList(nextPosition.deepEquivalent().node());
    Node* startNode = start.deepEquivalent().node();
    Node* previousCell = enclosingTableCell(previousPosition.deepEquivalent());
    Node* nextCell = enclosingTableCell(nextPosition.deepEquivalent());
    Node* currentCell = enclosingTableCell(start.deepEquivalent());
    if (previousList && (!previousList->hasTagName(listTag) || startNode->isDescendantOf(previousList) || previousCell != currentCell))
        previousList = 0;
    if (nextList && (!nextList->hasTagName(listTag) || startNode->isDescendantOf(nextList) || nextCell != currentCell))
        nextList = 0;
    // Place list item into adjoining lists.
    RefPtr<HTMLElement> listElement;
    if (previousList)
        appendNode(listItemElement, previousList);
    else if (nextList)
        insertNodeAt(listItemElement, Position(nextList, 0));
    else {
        // Create the list.
        listElement = createHTMLElement(document(), listTag);
        appendNode(listItemElement, listElement);

        if (start == end && isBlock(start.deepEquivalent().node())) {
            // Inserting the list into an empty paragraph that isn't held open 
            // by a br or a '\n', will invalidate start and end.  Insert 
            // a placeholder and then recompute start and end.
            RefPtr<Node> placeholder = insertBlockPlaceholder(start.deepEquivalent());
            start = VisiblePosition(Position(placeholder.get(), 0));
            end = start;
        }

        // Insert the list at a position visually equivalent to start of the
        // paragraph that is being moved into the list. 
        // Try to avoid inserting it somewhere where it will be surrounded by 
        // inline ancestors of start, since it is easier for editing to produce 
        // clean markup when inline elements are pushed down as far as possible.
        Position insertionPos(start.deepEquivalent().upstream());
        // Also avoid the containing list item.
        Node* listChild = enclosingListChild(insertionPos.node());
        if (listChild && listChild->hasTagName(liTag))
            insertionPos = positionInParentBeforeNode(listChild);

        insertNodeAt(listElement, insertionPos);

        // We inserted the list at the start of the content we're about to move
        // Update the start of content, so we don't try to move the list into itself.  bug 19066
        if (insertionPos == start.deepEquivalent())
            start = startOfParagraph(originalStart);
        previousList = outermostEnclosingList(previousPosition.deepEquivalent().node(), enclosingList(listElement.get()));
        nextList = outermostEnclosingList(nextPosition.deepEquivalent().node(), enclosingList(listElement.get()));
    }

    moveParagraph(start, end, VisiblePosition(Position(placeholder.get(), 0)), true);

    // FIXME: listifyParagraph should not depend on a member variable.
    // Since fixOrphanedListChild is the only other method that updates m_listElement,
    // we should fix unlistifyParagraph to support orphaned list child to get rid of this assignment.
    if (!listElement && m_listElement)
        listElement = m_listElement;

    if (listElement) {
        if (canMergeLists(previousList, listElement.get()))
            mergeIdenticalElements(previousList, listElement.get());
        if (canMergeLists(listElement.get(), nextList))
            mergeIdenticalElements(listElement.get(), nextList);
    } else if (canMergeLists(nextList, previousList))
        mergeIdenticalElements(previousList, nextList);

    return listElement;
}

}
