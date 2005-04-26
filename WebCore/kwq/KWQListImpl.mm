/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQListImpl.h"

#import <cstddef>
#import <algorithm>
#import <CoreFoundation/CFArray.h>
#import "KWQAssertions.h"
#import "misc/main_thread_malloc.h"

class KWQListNode
{
public:
    KWQListNode(void *d) : data(d), next(NULL), prev(NULL) { }

    MAIN_THREAD_ALLOCATED;

    void *data;
    KWQListNode *next;
    KWQListNode *prev;
};


static KWQListNode *copyList(KWQListNode *l, KWQListNode *&tail)
{
    KWQListNode *node = l;
    KWQListNode *copyHead = NULL;
    KWQListNode *last = NULL;

    while (node != NULL) {
	KWQListNode *copy = new KWQListNode(node->data);
	if (last != NULL) {
	    last->next = copy;
	} else {
	    copyHead = copy;
	}

	copy->prev = last;
	
	last = copy;
	node = node->next;
    }

    tail = last;
    return copyHead;
}


KWQListImpl::KWQListImpl(void (*deleteFunc)(void *)) :
    head(NULL),
    tail(NULL),
    cur(NULL),
    nodeCount(0),
    deleteItem(deleteFunc),
    iterators(NULL)
{
}

KWQListImpl::KWQListImpl(const KWQListImpl &impl) :
    cur(NULL),
    nodeCount(impl.nodeCount),
    deleteItem(impl.deleteItem),
    iterators(NULL)
{
    head = copyList(impl.head, tail);
}

KWQListImpl::~KWQListImpl()
{
    clear(false);
    
    KWQListIteratorImpl *next;
    for (KWQListIteratorImpl *it = iterators; it != NULL; it = next) {
        next = it->next;
        it->list = NULL;
        ASSERT(it->node == NULL);
        it->next = NULL;
        it->prev = NULL;
    }
}
     
void KWQListImpl::clear(bool deleteItems)
{
    KWQListNode *next;
    
    for (KWQListNode *node = head; node != NULL; node = next) {
        next = node->next;
        if (deleteItems) {
            deleteItem(node->data);
        }
        delete node;
    }

    head = NULL;
    tail = NULL;
    cur = NULL;
    nodeCount = 0;

    for (KWQListIteratorImpl *it = iterators; it != NULL; it = it->next) {
	it->node = NULL;
    }
}

void KWQListImpl::sort(int (*compareFunc)(void *a, void *b, void *data), void *data)
{
    // no sorting for 0 or 1-element lists
    if (nodeCount <= 1) {
        return;
    }

    // special case for 2-element lists
    if (nodeCount == 2) {
        void *a = head->data;
        void *b = head->next->data;
        if (compareFunc(a, b, data) <= 0) {
            return;
        }
        head->next->data = a;
        head->data = b;
        return;
    }

    // insertion sort for most common sizes
    const uint cutoff = 32;
    if (nodeCount <= cutoff) {
        // Straight out of Sedgewick's Algorithms in C++.

        // put all the elements into an array
        void *a[cutoff];
        uint i = 0;
        for (KWQListNode *node = head; node != NULL; node = node->next) {
            a[i++] = node->data;
        }

        // move the smallest element to the start to serve as a sentinel
        for (i = nodeCount - 1; i > 0; i--) {
            if (compareFunc(a[i-1], a[i], data) > 0) {
                void *t = a[i-1];
                a[i-1] = a[i];
                a[i] = t;
            }
        }

        // move other items to the right and put a[i] into position
        for (i = 2; i < nodeCount; i++) {
            void *v = a[i];
            uint j = i;
            while (compareFunc(v, a[j-1], data) < 0) {
                a[j] = a[j-1];
                j--;
            }
            a[j] = v;
        }

        // finally, put everything back into the list
        i = 0;
        for (KWQListNode *node = head; node != NULL; node = node->next) {
            node->data = a[i++];
        }
        return;
    }

    // CFArray sort for larger lists
    
    CFMutableArrayRef array = CFArrayCreateMutable(NULL, nodeCount, NULL);

    for (KWQListNode *node = head; node != NULL; node = node->next) {
	CFArrayAppendValue(array, node->data);
    }

    CFArraySortValues(array, CFRangeMake(0, nodeCount), (CFComparatorFunction) compareFunc, data);

    int i = 0;
    for (KWQListNode *node = head; node != NULL; node = node->next) {
	node->data = const_cast<void *>(CFArrayGetValueAtIndex(array, i++));
    }

    CFRelease(array);
}

void *KWQListImpl::at(uint n)
{
    KWQListNode *node;
    if (n >= nodeCount - 1) {
        node = tail;
    } else {
        node = head;
        for (uint i = 0; i < n && node != NULL; i++) {
            node = node->next;
        }
    }

    cur = node;
    return node ? node->data : 0;
}

bool KWQListImpl::insert(uint n, const void *item)
{
    if (n > nodeCount) {
	return false;
    }

    KWQListNode *node = new KWQListNode((void *)item);

    if (n == 0) {
	// inserting at head
	node->next = head;
	if (head != NULL) {
	    head->prev = node;
	}
	head = node;
        if (tail == NULL) {
            tail = node;
        }
    } else if (n == nodeCount) {
        // inserting at tail
        node->prev = tail;
        if (tail != NULL) {
            tail->next = node;
        }
        tail = node;
    } else {
	// general insertion
	
	// iterate to one node before the insertion point, can't be null
	// since we know n > 0 and n < nodeCount
	KWQListNode *prevNode = head;

	for (uint i = 0; i < n - 1; i++) {
	    prevNode = prevNode->next;
	}
	node->prev = prevNode;
	node->next = prevNode->next;
	if (node->next != NULL) {
	    node->next->prev = node;
	}
	prevNode->next = node;
    }

    nodeCount++;
    cur = node;
    return true;
}

bool KWQListImpl::remove(bool shouldDeleteItem)
{
    KWQListNode *node = cur;
    if (node == NULL) {
	return false;
    }

    if (node->prev == NULL) {
	head = node->next;
    } else {
	node->prev->next = node->next;
    }

    if (node->next == NULL) {
        tail = node->prev;
    } else {
	node->next->prev = node->prev;
    }

    if (node->next != NULL) {
	cur = node->next;
    } else {
	cur = node->prev;
    }

    for (KWQListIteratorImpl *it = iterators; it != NULL; it = it->next) {
	if (it->node == node) {
	    it->node = cur;
	}
    }

    if (shouldDeleteItem) {
	deleteItem(node->data);
    }
    delete node;

    nodeCount--;

    return true;
}

bool KWQListImpl::remove(uint n, bool deleteItem)
{
    if (n >= nodeCount) {
	return false;
    }

    at(n);
    return remove(deleteItem);
}

bool KWQListImpl::removeFirst(bool deleteItem)
{
    return remove(0, deleteItem);
}

bool KWQListImpl::removeLast(bool deleteItem)
{
    return remove(nodeCount - 1, deleteItem);
}

bool KWQListImpl::remove(const void *item, bool deleteItem, int (*compareFunc)(void *a, void *b, void *data), void *data)
{
    KWQListNode *node;

    node = head;

    while (node != NULL && compareFunc((void *)item, node->data, data) != 0) {
	node = node->next;
    }
    
    if (node == NULL) {
	return false;
    }

    cur = node;

    return remove(deleteItem);
}

bool KWQListImpl::removeRef(const void *item, bool deleteItem)
{
    KWQListNode *node;

    node = head;

    while (node != NULL && item != node->data) {
	node = node->next;
    }
    
    if (node == NULL) {
	return false;
    }

    cur = node;

    return remove(deleteItem);
}

void *KWQListImpl::getFirst() const
{
    return head ? head->data : 0;
}

void *KWQListImpl::getLast() const
{
    return tail ? tail->data : 0;
}

void *KWQListImpl::current() const
{
    if (cur != NULL) {
	return cur->data;
    } else {
	return NULL;
    }
}

void *KWQListImpl::first()
{
    cur = head;
    return current();
}

void *KWQListImpl::last()
{
    cur = tail;
    return current();
}

void *KWQListImpl::next()
{
    if (cur != NULL) {
	cur = cur->next;
    }
    return current();
}

void *KWQListImpl::prev()
{
    if (cur != NULL) {
	cur = cur->prev;
    }
    return current();
}

void *KWQListImpl::take(uint n)
{
    void *retval = at(n);
    remove(false);
    return retval;
}

void *KWQListImpl::take()
{
    void *retval = current();
    remove(false);
    return retval;
}

void KWQListImpl::append(const void *item)
{
    insert(nodeCount, item);
}

void KWQListImpl::prepend(const void *item)
{
    insert(0, item);
}

uint KWQListImpl::containsRef(const void *item) const
{
    uint count = 0;
    
    for (KWQListNode *node = head; node != NULL; node = node->next) {
        if (item == node->data) {
            ++count;
        }
    }
    
    return count;
}

KWQListImpl &KWQListImpl::assign(const KWQListImpl &impl, bool deleteItems)
{
    clear(deleteItems);
    KWQListImpl(impl).swap(*this);
    return *this;
}

void KWQListImpl::addIterator(KWQListIteratorImpl *iter) const
{
    iter->next = iterators;
    iter->prev = NULL;
    
    if (iterators != NULL) {
        iterators->prev = iter;
    }
    iterators = iter;
}

void KWQListImpl::removeIterator(KWQListIteratorImpl *iter) const
{
    if (iter->prev == NULL) {
        iterators = iter->next;
    } else {
        iter->prev->next = iter->next;
    }

    if (iter->next != NULL) {
        iter->next->prev = iter->prev;
    }
}

void KWQListImpl::swap(KWQListImpl &other)
{
    using std::swap;
    
    ASSERT(iterators == NULL);
    ASSERT(other.iterators == NULL);
    
    swap(head, other.head);
    swap(tail, other.tail);
    swap(cur, other.cur);
    swap(nodeCount, other.nodeCount);
    swap(deleteItem, other.deleteItem);
}


KWQListIteratorImpl::KWQListIteratorImpl() :
    list(NULL),
    node(NULL)
{
}

KWQListIteratorImpl::KWQListIteratorImpl(const KWQListImpl &impl)  :
    list(&impl),
    node(impl.head)
{
    impl.addIterator(this);
}

KWQListIteratorImpl::~KWQListIteratorImpl()
{
    if (list != NULL) {
	list->removeIterator(this);
    }
}

KWQListIteratorImpl::KWQListIteratorImpl(const KWQListIteratorImpl &impl) :
    list(impl.list),
    node(impl.node)
{
    if (list != NULL) {
        list->addIterator(this);
    }
}

uint KWQListIteratorImpl::count() const
{
    return list == NULL ? 0 : list->count();
}

void *KWQListIteratorImpl::toFirst()
{
    if (list != NULL) {
        node = list->head;
    }
    return current();
}

void *KWQListIteratorImpl::toLast()
{
    if (list != NULL) {
        node = list->tail;
    }
    return current();
}

void *KWQListIteratorImpl::current() const
{
    return node == NULL ? NULL : node->data;
}

void *KWQListIteratorImpl::operator--()
{
    if (node != NULL) {
	node = node->prev;
    }
    return current();
}

void *KWQListIteratorImpl::operator++()
{
    if (node != NULL) {
	node = node->next;
    }
    return current();
}

KWQListIteratorImpl &KWQListIteratorImpl::operator=(const KWQListIteratorImpl &impl)
{
    if (list != NULL) {
	list->removeIterator(this);
    }
    
    list = impl.list;
    node = impl.node;
    
    if (list != NULL) {
	list->addIterator(this);
    }

    return *this;
}
