// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#include "config.h"
#include "regexp_object.h"

#include "regexp_object.lut.h"

#include <stdio.h>
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "internal.h"
#include "regexp.h"
#include "error_object.h"
#include "lookup.h"

using namespace KJS;

// ------------------------------ RegExpPrototypeImp ---------------------------

// ECMA 15.9.4

const ClassInfo RegExpPrototypeImp::info = {"RegExpPrototype", 0, 0, 0};

RegExpPrototypeImp::RegExpPrototypeImp(ExecState *exec,
                                       ObjectPrototypeImp *objProto,
                                       FunctionPrototypeImp *funcProto)
  : ObjectImp(objProto)
{
  setInternalValue(String(""));

  // The constructor will be added later in RegExpObject's constructor (?)

  static const Identifier execPropertyName("exec");
  putDirect(execPropertyName,     new RegExpProtoFuncImp(exec,funcProto,RegExpProtoFuncImp::Exec,     0), DontEnum);
  static const Identifier testPropertyName("test");
  putDirect(testPropertyName,     new RegExpProtoFuncImp(exec,funcProto,RegExpProtoFuncImp::Test,     0), DontEnum);
  putDirect(toStringPropertyName, new RegExpProtoFuncImp(exec,funcProto,RegExpProtoFuncImp::ToString, 0), DontEnum);
}

// ------------------------------ RegExpProtoFuncImp ---------------------------

RegExpProtoFuncImp::RegExpProtoFuncImp(ExecState *exec,
                                       FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

bool RegExpProtoFuncImp::implementsCall() const
{
  return true;
}

ValueImp *RegExpProtoFuncImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&RegExpImp::info)) {
    if (thisObj->inherits(&RegExpPrototypeImp::info)) {
      switch (id) {
        case ToString: return String("//");
      }
    }
    
    return throwError(exec, TypeError);
  }

  switch (id) {
  case Test:      // 15.10.6.2
  case Exec:
  {
    RegExp *regExp = static_cast<RegExpImp*>(thisObj)->regExp();
    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->lexicalInterpreter()->builtinRegExp());

    UString input;
    if (args.isEmpty())
      input = regExpObj->get(exec, "input")->toString(exec);
    else
      input = args[0]->toString(exec);

    double lastIndex = thisObj->get(exec, "lastIndex")->toInteger(exec);

    bool globalFlag = thisObj->get(exec, "global")->toBoolean(exec);
    if (!globalFlag)
      lastIndex = 0;
    if (lastIndex < 0 || lastIndex > input.size()) {
      thisObj->put(exec, "lastIndex", jsZero(), DontDelete | DontEnum);
      return Null();
    }

    int foundIndex;
    UString match = regExpObj->performMatch(regExp, input, static_cast<int>(lastIndex), &foundIndex);
    bool didMatch = !match.isNull();

    // Test
    if (id == Test)
      return Boolean(didMatch);

    // Exec
    if (didMatch) {
      if (globalFlag)
        thisObj->put(exec, "lastIndex", Number(foundIndex + match.size()), DontDelete | DontEnum);
      return regExpObj->arrayOfMatches(exec, match);
    } else {
      if (globalFlag)
        thisObj->put(exec, "lastIndex", jsZero(), DontDelete | DontEnum);
      return Null();
    }
  }
  break;
  case ToString:
    UString result = "/" + thisObj->get(exec, "source")->toString(exec) + "/";
    if (thisObj->get(exec, "global")->toBoolean(exec)) {
      result += "g";
    }
    if (thisObj->get(exec, "ignoreCase")->toBoolean(exec)) {
      result += "i";
    }
    if (thisObj->get(exec, "multiline")->toBoolean(exec)) {
      result += "m";
    }
    return String(result);
  }

  return Undefined();
}

// ------------------------------ RegExpImp ------------------------------------

const ClassInfo RegExpImp::info = {"RegExp", 0, 0, 0};

RegExpImp::RegExpImp(RegExpPrototypeImp *regexpProto)
  : ObjectImp(regexpProto), reg(0L)
{
}

RegExpImp::~RegExpImp()
{
  delete reg;
}

// ------------------------------ RegExpObjectImp ------------------------------

const ClassInfo RegExpObjectImp::info = {"RegExp", &InternalFunctionImp::info, &RegExpTable, 0};

/* Source for regexp_object.lut.h
@begin RegExpTable 20
  input           RegExpObjectImp::Input          None
  $_              RegExpObjectImp::Input          DontEnum
  multiline       RegExpObjectImp::Multiline      None
  $*              RegExpObjectImp::Multiline      DontEnum
  lastMatch       RegExpObjectImp::LastMatch      DontDelete|ReadOnly
  $&              RegExpObjectImp::LastMatch      DontDelete|ReadOnly|DontEnum
  lastParen       RegExpObjectImp::LastParen      DontDelete|ReadOnly
  $+              RegExpObjectImp::LastParen      DontDelete|ReadOnly|DontEnum
  leftContext     RegExpObjectImp::LeftContext    DontDelete|ReadOnly
  $`              RegExpObjectImp::LeftContext    DontDelete|ReadOnly|DontEnum
  rightContext    RegExpObjectImp::RightContext   DontDelete|ReadOnly
  $'              RegExpObjectImp::RightContext   DontDelete|ReadOnly|DontEnum
  $1              RegExpObjectImp::Dollar1        DontDelete|ReadOnly
  $2              RegExpObjectImp::Dollar2        DontDelete|ReadOnly
  $3              RegExpObjectImp::Dollar3        DontDelete|ReadOnly
  $4              RegExpObjectImp::Dollar4        DontDelete|ReadOnly
  $5              RegExpObjectImp::Dollar5        DontDelete|ReadOnly
  $6              RegExpObjectImp::Dollar6        DontDelete|ReadOnly
  $7              RegExpObjectImp::Dollar7        DontDelete|ReadOnly
  $8              RegExpObjectImp::Dollar8        DontDelete|ReadOnly
  $9              RegExpObjectImp::Dollar9        DontDelete|ReadOnly
@end
*/

RegExpObjectImp::RegExpObjectImp(ExecState *exec,
                                 FunctionPrototypeImp *funcProto,
                                 RegExpPrototypeImp *regProto)

  : InternalFunctionImp(funcProto), multiline(false), lastInput(""), lastOvector(0), lastNumSubPatterns(0)
{
  // ECMA 15.10.5.1 RegExp.prototype
  putDirect(prototypePropertyName, regProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsTwo(), ReadOnly|DontDelete|DontEnum);
}

RegExpObjectImp::~RegExpObjectImp()
{
  delete [] lastOvector;
}

/* 
  To facilitate result caching, exec(), test(), match(), search(), and replace() dipatch regular
  expression matching through the performMatch function. We use cached results to calculate, 
  e.g., RegExp.lastMatch and RegExp.leftParen.
*/
UString RegExpObjectImp::performMatch(RegExp* r, const UString& s, int startOffset, int *endOffset, int **ovector)
{
  int tmpOffset;
  int *tmpOvector;
  UString match = r->match(s, startOffset, &tmpOffset, &tmpOvector);

  if (endOffset)
    *endOffset = tmpOffset;
  if (ovector)
    *ovector = tmpOvector;
  
  if (!match.isNull()) {
    assert(tmpOvector);
    
    lastInput = s;
    delete [] lastOvector;
    lastOvector = tmpOvector;
    lastNumSubPatterns = r->subPatterns();
  }
  
  return match;
}

ObjectImp *RegExpObjectImp::arrayOfMatches(ExecState *exec, const UString &result) const
{
  List list;
  // The returned array contains 'result' as first item, followed by the list of matches
  list.append(String(result));
  if ( lastOvector )
    for ( unsigned i = 1 ; i < lastNumSubPatterns + 1 ; ++i )
    {
      int start = lastOvector[2*i];
      if (start == -1)
        list.append(jsUndefined());
      else {
        UString substring = lastInput.substr( start, lastOvector[2*i+1] - start );
        list.append(String(substring));
      }
    }
  ObjectImp *arr = exec->lexicalInterpreter()->builtinArray()->construct(exec, list);
  arr->put(exec, "index", Number(lastOvector[0]));
  arr->put(exec, "input", String(lastInput));
  return arr;
}

ValueImp *RegExpObjectImp::getBackref(unsigned i) const
{
  if (lastOvector && i < lastNumSubPatterns + 1) {
    UString substring = lastInput.substr(lastOvector[2*i], lastOvector[2*i+1] - lastOvector[2*i] );
    return String(substring);
  } 

  return String("");
}

ValueImp *RegExpObjectImp::getLastMatch() const
{
  if (lastOvector) {
    UString substring = lastInput.substr(lastOvector[0], lastOvector[1] - lastOvector[0]);
    return String(substring);
  }
  
  return String("");
}

ValueImp *RegExpObjectImp::getLastParen() const
{
  int i = lastNumSubPatterns;
  if (i > 0) {
    assert(lastOvector);
    UString substring = lastInput.substr(lastOvector[2*i], lastOvector[2*i+1] - lastOvector[2*i]);
    return String(substring);
  }
    
  return String("");
}

ValueImp *RegExpObjectImp::getLeftContext() const
{
  if (lastOvector) {
    UString substring = lastInput.substr(0, lastOvector[0]);
    return String(substring);
  }
  
  return String("");
}

ValueImp *RegExpObjectImp::getRightContext() const
{
  if (lastOvector) {
    UString s = lastInput;
    UString substring = s.substr(lastOvector[1], s.size() - lastOvector[1]);
    return String(substring);
  }
  
  return String("");
}

bool RegExpObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<RegExpObjectImp, InternalFunctionImp>(exec, &RegExpTable, this, propertyName, slot);
}

ValueImp *RegExpObjectImp::getValueProperty(ExecState *exec, int token) const
{
  switch (token) {
    case Dollar1:
      return getBackref(1);
    case Dollar2:
      return getBackref(2);
    case Dollar3:
      return getBackref(3);
    case Dollar4:
      return getBackref(4);
    case Dollar5:
      return getBackref(5);
    case Dollar6:
      return getBackref(6);
    case Dollar7:
      return getBackref(7);
    case Dollar8:
      return getBackref(8);
    case Dollar9:
      return getBackref(9);
    case Input:
      return jsString(lastInput);
    case Multiline:
      return jsBoolean(multiline);
    case LastMatch:
      return getLastMatch();
    case LastParen:
      return getLastParen();
    case LeftContext:
      return getLeftContext();
    case RightContext:
      return getRightContext();
    default:
      assert(0);
  }

  return String("");
}

void RegExpObjectImp::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
  lookupPut<RegExpObjectImp, InternalFunctionImp>(exec, propertyName, value, attr, &RegExpTable, this);
}

void RegExpObjectImp::putValueProperty(ExecState *exec, int token, ValueImp *value, int attr)
{
  switch (token) {
    case Input:
      lastInput = value->toString(exec);
      break;
    case Multiline:
      multiline = value->toBoolean(exec);
      break;
    default:
      assert(0);
  }
}
  
bool RegExpObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.10.4
ObjectImp *RegExpObjectImp::construct(ExecState *exec, const List &args)
{
  ObjectImp *o = args[0]->getObject();
  if (o && o->inherits(&RegExpImp::info)) {
    if (!args[1]->isUndefined())
      return throwError(exec, TypeError);
    return o;
  }
  
  UString p = args[0]->isUndefined() ? UString("") : args[0]->toString(exec);
  UString flags = args[1]->isUndefined() ? UString("") : args[1]->toString(exec);

  RegExpPrototypeImp *proto = static_cast<RegExpPrototypeImp*>(exec->lexicalInterpreter()->builtinRegExpPrototype());
  RegExpImp *dat = new RegExpImp(proto);

  bool global = (flags.find("g") >= 0);
  bool ignoreCase = (flags.find("i") >= 0);
  bool multiline = (flags.find("m") >= 0);
  // TODO: throw a syntax error on invalid flags

  dat->putDirect("global", jsBoolean(global), DontDelete | ReadOnly | DontEnum);
  dat->putDirect("ignoreCase", jsBoolean(ignoreCase), DontDelete | ReadOnly | DontEnum);
  dat->putDirect("multiline", jsBoolean(multiline), DontDelete | ReadOnly | DontEnum);

  dat->putDirect("source", jsString(p), DontDelete | ReadOnly | DontEnum);
  dat->putDirect("lastIndex", jsZero(), DontDelete | DontEnum);

  int reflags = RegExp::None;
  if (global)
      reflags |= RegExp::Global;
  if (ignoreCase)
      reflags |= RegExp::IgnoreCase;
  if (multiline)
      reflags |= RegExp::Multiline;
  dat->setRegExp(new RegExp(p, reflags));

  return dat;
}

bool RegExpObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.10.3
ValueImp *RegExpObjectImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/,
			    const List &args)
{
  // TODO: handle RegExp argument case (15.10.3.1)

  return construct(exec, args);
}
