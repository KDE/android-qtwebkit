/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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

#ifndef InternalFunction_h
#define InternalFunction_h

#include "JSObject.h"
#include "identifier.h"

namespace KJS {

    class FunctionPrototype;

    class InternalFunction : public JSObject {
    public:
        virtual const ClassInfo* classInfo() const { return &info; }
        static const ClassInfo info;

        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual void put(ExecState*, const Identifier& propertyName, JSValue*);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

        const Identifier& functionName() const { return m_name; }

    protected:
        InternalFunction();
        InternalFunction(FunctionPrototype*, const Identifier&);

    private:
        static JSValue* nameGetter(ExecState*, const Identifier&, const PropertySlot&);
        virtual CallType getCallData(CallData&) = 0;
        virtual bool implementsHasInstance() const;

        Identifier m_name;
    };

} // namespace KJS

#endif // InternalFunction_h
