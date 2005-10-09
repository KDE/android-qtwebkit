/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef DATE_OBJECT_H
#define DATE_OBJECT_H

#include "internal.h"

namespace KJS {

    class FunctionPrototypeImp;
    class ObjectPrototypeImp;

    class DateInstanceImp : public ObjectImp {
    public:
        DateInstanceImp(ObjectImp *proto);

        virtual const ClassInfo *classInfo() const { return &info; }
        static const ClassInfo info;
    };

    /**
     * @internal
     *
     * The initial value of Date.prototype (and thus all objects created
     * with the Date constructor
     */
    class DatePrototypeImp : public DateInstanceImp {
    public:
        DatePrototypeImp(ExecState *, ObjectPrototypeImp *);
        virtual bool getOwnPropertySlot(ExecState *, const Identifier &, PropertySlot&);
        virtual const ClassInfo *classInfo() const { return &info; }
        static const ClassInfo info;
    };

    /**
     * @internal
     *
     * The initial value of the the global variable's "Date" property
     */
    class DateObjectImp : public InternalFunctionImp {
    public:
        DateObjectImp(ExecState *, FunctionPrototypeImp *, DatePrototypeImp *);

        virtual bool implementsConstruct() const;
        virtual ObjectImp *construct(ExecState *, const List &args);
        virtual bool implementsCall() const;
        virtual ValueImp *callAsFunction(ExecState *, ObjectImp *thisObj, const List &args);

        Completion execute(const List &);
        ObjectImp *construct(const List &);
    };

} // namespace

#endif
