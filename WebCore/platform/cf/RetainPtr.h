// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005, 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef RETAINPTR_H_
#define RETAINPTR_H_

#include <algorithm>
#include <CoreFoundation/CoreFoundation.h>

namespace WTF {

    template <typename T> struct RemovePointer {
        typedef T type;
    };

    template <typename T> struct RemovePointer<T*> {
        typedef T type;
    };

    // Unlike most most of our smart pointers, RetainPtr can take either the pointer type or the pointed-to type,
    // so both RetainPtr<NSDictionary> and RetainPtr<CFDictionaryRef> will work.

    template <typename T> class RetainPtr
    {
    public:
        typedef typename RemovePointer<T>::type ValueType;
        typedef ValueType* PtrType;
        typedef ValueType& RefType;

        RetainPtr() : m_ptr(0) {}
        RetainPtr(PtrType ptr) : m_ptr(ptr) { if (ptr) CFRetain(ptr); }
        RetainPtr(const RetainPtr& o) : m_ptr(o.m_ptr) { if (PtrType ptr = m_ptr) CFRetain(ptr); }

        ~RetainPtr() { if (PtrType ptr = m_ptr) CFRelease(ptr); }
        
        template <typename U> RetainPtr(const RetainPtr<U>& o) : m_ptr(o.get()) { if (PtrType ptr = m_ptr) CFRetain(ptr); }
        
        PtrType get() const { return m_ptr; }
        
        RefType operator*() const { return *m_ptr; }
        PtrType operator->() const { return m_ptr; }
        
        bool operator!() const { return !m_ptr; }
    
        // This conversion operator allows implicit conversion to bool but not to other integer types.
        typedef PtrType (RetainPtr::*UnspecifiedBoolType)() const;
        operator UnspecifiedBoolType() const { return m_ptr ? &RetainPtr::get : 0; }
        
        RetainPtr& operator=(const RetainPtr&);
        template <typename U> RetainPtr& operator=(const RetainPtr<U>&);
        RetainPtr& operator=(PtrType);
        template <typename U> RetainPtr& operator=(U*);

        void swap(RetainPtr&);

    private:
        PtrType m_ptr;
    };
    
    template <typename T> inline RetainPtr<T>& RetainPtr<T>::operator=(const RetainPtr<T>& o)
    {
        PtrType optr = o.get();
        if (optr)
            CFRetain(optr);
        PtrType ptr = m_ptr;
        m_ptr = optr;
        if (ptr)
            CFRelease(ptr);
        return *this;
    }
    
    template <typename T> template <typename U> inline RetainPtr<T>& RetainPtr<T>::operator=(const RetainPtr<U>& o)
    {
        PtrType optr = o.get();
        if (optr)
            CFRetain(optr);
        PtrType ptr = m_ptr;
        m_ptr = optr;
        if (ptr)
            CFRelease(ptr);
        return *this;
    }
    
    template <typename T> inline RetainPtr<T>& RetainPtr<T>::operator=(PtrType optr)
    {
        if (optr)
            CFRetain(optr);
        PtrType ptr = m_ptr;
        m_ptr = optr;
        if (ptr)
            CFRelease(ptr);
        return *this;
    }

    template <typename T> template <typename U> inline RetainPtr<T>& RetainPtr<T>::operator=(U* optr)
    {
        if (optr)
            CFRetain(optr);
        PtrType ptr = m_ptr;
        m_ptr = optr;
        if (ptr)
            CFRelease(ptr);
        return *this;
    }

    template <class T> inline void RetainPtr<T>::swap(RetainPtr<T>& o)
    {
        std::swap(m_ptr, o.m_ptr);
    }

    template <class T> inline void swap(RetainPtr<T>& a, RetainPtr<T>& b)
    {
        a.swap(b);
    }

    template <typename T, typename U> inline bool operator==(const RetainPtr<T>& a, const RetainPtr<U>& b)
    { 
        return a.get() == b.get(); 
    }

    template <typename T, typename U> inline bool operator==(const RetainPtr<T>& a, U* b)
    { 
        return a.get() == b; 
    }
    
    template <typename T, typename U> inline bool operator==(T* a, const RetainPtr<U>& b) 
    {
        return a == b.get(); 
    }
    
    template <typename T, typename U> inline bool operator!=(const RetainPtr<T>& a, const RetainPtr<U>& b)
    { 
        return a.get() != b.get(); 
    }

    template <typename T, typename U> inline bool operator!=(const RetainPtr<T>& a, U* b)
    {
        return a.get() != b; 
    }

    template <typename T, typename U> inline bool operator!=(T* a, const RetainPtr<U>& b)
    { 
        return a != b.get(); 
    }
    
    template <typename T, typename U> inline RetainPtr<T> static_pointer_cast(const RetainPtr<U>& p)
    { 
        return RetainPtr<T>(static_cast<typename RetainPtr<T>::PtrType>(p.get())); 
    }

    template <typename T, typename U> inline RetainPtr<T> const_pointer_cast(const RetainPtr<U>& p)
    { 
        return RetainPtr<T>(const_cast<typename RetainPtr<T>::PtrType>(p.get())); 
    }

    template <typename T> inline typename RemovePointer<T>::type getPtr(const RetainPtr<T>& p)
    {
        return p.get();
    }

} // namespace WTF

using WTF::RetainPtr;
using WTF::static_pointer_cast;
using WTF::const_pointer_cast;

#endif // RETAINPTR_H_
