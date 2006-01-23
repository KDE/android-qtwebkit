// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Apple Computer, Inc.
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

#ifndef KJS_PROTECTED_REFERENCE_H
#define KJS_PROTECTED_REFERENCE_H

#include "protect.h"
#include "reference.h"
#include "interpreter.h" 

namespace KJS {

    class ProtectedReference : public Reference {
    public:
        ProtectedReference(const Reference& r) 
            : Reference(r) 
        {
            JSLock lock;
            gcProtectNullTolerant(base); 
        }

        ~ProtectedReference()
        { 
            JSLock lock;
            gcUnprotectNullTolerant(base);
        }

        ProtectedReference& operator=(const Reference &r)
        {
            JSLock lock;
            JSValue *old = base;
            Reference::operator=(r); 
            gcProtectNullTolerant(base);
            gcUnprotectNullTolerant(old); 
            return *this;
        }

    private:
        ProtectedReference();
        ProtectedReference(JSObject *b, const Identifier& p);
        ProtectedReference(JSObject *b, unsigned p);
        ProtectedReference(const Identifier& p);
        ProtectedReference(unsigned p);
    };

} // namespace

#endif
