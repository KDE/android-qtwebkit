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

#ifndef QLIST_H_
#define QLIST_H_

#include "KWQDef.h"
#include "KWQCollection.h"
#include "KWQListImpl.h"

#ifdef _KWQ_IOSTREAM_
#include <ostream>
#endif

template <class T> class QPtrListIterator;

template <class T> class QPtrList : public QPtrCollection {
public:
    QPtrList() : impl(deleteFunc) { }
    ~QPtrList() { impl.clear(del_item); }
    
    QPtrList& operator=(const QPtrList &l) { impl.assign(l.impl, del_item); return *this; }

    bool isEmpty() const { return impl.isEmpty(); }
    uint count() const { return impl.count(); }
    void clear() { impl.clear(del_item); }
    void sort() { impl.sort(compareFunc, this); }

    T *at(uint n) { return (T *)impl.at(n); }

    bool insert(uint n, const T *item) { return impl.insert(n, item); }
    bool remove() { return impl.remove(del_item); }
    bool remove(uint n) { return impl.remove(n, del_item); }
    bool remove(const T *item) { return impl.remove(item, del_item, compareFunc, this); }
    bool removeFirst() { return impl.removeFirst(del_item); }
    bool removeLast() { return impl.removeLast(del_item); }
    bool removeRef(const T *item) { return impl.removeRef(item, del_item); }

    T *getFirst() const { return (T *)impl.getFirst(); }
    T *getLast() const { return (T *)impl.getLast(); }
    T *current() const { return (T *)impl.current(); }
    T *first() { return (T *)impl.first(); }
    T *last() { return (T *)impl.last(); }
    T *next() { return (T *)impl.next(); }
    T *prev() { return (T *)impl.prev(); }
    T *take(uint n) { return (T *)impl.take(n); }
    T *take() { return (T *)impl.take(); }

    void append(const T *item) { impl.append(item); }
    void prepend(const T *item) { impl.prepend(item); }

    uint containsRef(const T *item) const { return impl.containsRef(item); }

    virtual int compareItems(void *a, void *b) { return a != b; }

 private:
    static void deleteFunc(void *item) { delete (T *)item; }
    static int compareFunc(void *a, void *b, void *data) { return ((QPtrList *)data)->compareItems(a, b);  }

    friend class QPtrListIterator<T>;

    KWQListImpl impl;
};

template <class T> class QPtrListIterator {
public:
    QPtrListIterator() { }
    QPtrListIterator(const QPtrList<T> &l) : impl(l.impl) { }

    uint count() const { return impl.count(); }
    T *toFirst() { return (T *)impl.toFirst(); }
    T *toLast() { return (T *)impl.toLast(); }
    T *current() const { return (T *)impl.current(); }

    operator T *() const { return (T *)impl.current(); }
    T *operator--() { return (T *)--impl; }
    T *operator++()  { return (T *)++impl; }

private:
    KWQListIteratorImpl impl;
};

#ifdef _KWQ_IOSTREAM_

template<class T>
inline std::ostream &operator<<(std::ostream &stream, const QPtrList<T> &l)
{
    QPtrListIterator<T> iter(l);
    unsigned count = l.count();

    stream << "QPtrList: [size: " << l.count() << "; items: ";

    // print first
    if (count != 0) {
	stream << *iter.current();
	++iter;
	--count;
    }
    
    // print rest
    while (count != 0) {
	stream << ", " << *iter.current();
	++iter;
	--count;
    }

    return stream << "]";
}

#endif

#endif
