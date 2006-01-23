// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004 Apple Computer, Inc.
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kxmlcore/HashTraits.h>
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "JSLock.h"

using namespace KJS;
using namespace KXMLCore;

class TestFunctionImp : public JSObject {
public:
  TestFunctionImp(int i, int length);
  virtual bool implementsCall() const { return true; }
  virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);

  enum { Print, Debug, Quit, GC };

private:
  int id;
};

TestFunctionImp::TestFunctionImp(int i, int length) : JSObject(), id(i)
{
  putDirect(lengthPropertyName,length,DontDelete|ReadOnly|DontEnum);
}

JSValue *TestFunctionImp::callAsFunction(ExecState *exec, JSObject * /*thisObj*/, const List &args)
{
  switch (id) {
  case Print:
  case Debug:
    fprintf(stderr,"--> %s\n",args[0]->toString(exec).ascii());
    return jsUndefined();
  case Quit:
    exit(0);
    return jsUndefined();
  case GC:
  {
    JSLock lock;
    Interpreter::collect();
  }
    break;
  default:
    break;
  }

  return jsUndefined();
}

class VersionFunctionImp : public JSObject {
public:
  VersionFunctionImp() : JSObject() {}
  virtual bool implementsCall() const { return true; }
  virtual JSValue *callAsFunction(ExecState *exec, JSObject *thisObj, const List &args);
};

JSValue *VersionFunctionImp::callAsFunction(ExecState * /*exec*/, JSObject * /*thisObj*/, const List &/*args*/)
{
  // We need this function for compatibility with the Mozilla JS tests but for now
  // we don't actually do any version-specific handling
  return jsUndefined();
}

class GlobalImp : public JSObject {
public:
  virtual UString className() const { return "global"; }
};

int main(int argc, char **argv)
{
  // expecting a filename
  if (argc < 2) {
    fprintf(stderr, "You have to specify at least one filename\n");
    return -1;
  }

  bool ret = true;
  {
    JSLock lock;
    
    // Unit tests for KXMLCore::IsInteger. Don't have a better place for them now.
    // FIXME: move these once we create a unit test directory for KXMLCore.
    assert(IsInteger<bool>::value);
    assert(IsInteger<char>::value);
    assert(IsInteger<signed char>::value);
    assert(IsInteger<unsigned char>::value);
    assert(IsInteger<short>::value);
    assert(IsInteger<unsigned short>::value);
    assert(IsInteger<int>::value);
    assert(IsInteger<unsigned int>::value);
    assert(IsInteger<long>::value);
    assert(IsInteger<unsigned long>::value);
    assert(IsInteger<long long>::value);
    assert(IsInteger<unsigned long long>::value);

    assert(!IsInteger<char *>::value);
    assert(!IsInteger<const char *>::value);
    assert(!IsInteger<volatile char *>::value);
    assert(!IsInteger<double>::value);
    assert(!IsInteger<float>::value);
    assert(!IsInteger<GlobalImp>::value);
    
    JSObject *global(new GlobalImp());

    // create interpreter
    Interpreter interp(global);
    // add debug() function
    global->put(interp.globalExec(), "debug", new TestFunctionImp(TestFunctionImp::Debug, 1));
    // add "print" for compatibility with the mozilla js shell
    global->put(interp.globalExec(), "print", new TestFunctionImp(TestFunctionImp::Print, 1));
    // add "quit" for compatibility with the mozilla js shell
    global->put(interp.globalExec(), "quit", new TestFunctionImp(TestFunctionImp::Quit, 0));
    // add "gc" for compatibility with the mozilla js shell
    global->put(interp.globalExec(), "gc", new TestFunctionImp(TestFunctionImp::GC, 0));
    // add "version" for compatibility with the mozilla js shell 
    global->put(interp.globalExec(), "version", new VersionFunctionImp());

    for (int i = 1; i < argc; i++) {
      int code_len = 0;
      int code_alloc = 1024;
      char *code = (char*)malloc(code_alloc);

      const char *file = argv[i];
      if (strcmp(file, "-f") == 0)
        continue;
      FILE *f = fopen(file, "r");
      if (!f) {
        fprintf(stderr, "Error opening %s.\n", file);
        return 2;
      }

      while (!feof(f) && !ferror(f)) {
        size_t len = fread(code+code_len,1,code_alloc-code_len,f);
        code_len += len;
        if (code_len >= code_alloc) {
          code_alloc *= 2;
          code = (char*)realloc(code,code_alloc);
        }
      }
      code = (char*)realloc(code,code_len+1);
      code[code_len] = '\0';

      // run
      Completion comp(interp.evaluate(file, 1, code));

      fclose(f);

      if (comp.complType() == Throw) {
        ExecState *exec = interp.globalExec();
        JSValue *exVal = comp.value();
        char *msg = exVal->toString(exec).ascii();
        int lineno = -1;
        if (exVal->isObject()) {
          JSValue *lineVal = static_cast<JSObject *>(exVal)->get(exec,"line");
          if (lineVal->isNumber())
            lineno = int(lineVal->toNumber(exec));
        }
        if (lineno != -1)
          fprintf(stderr,"Exception, line %d: %s\n",lineno,msg);
        else
          fprintf(stderr,"Exception: %s\n",msg);
        ret = false;
      }
      else if (comp.complType() == ReturnValue) {
        char *msg = comp.value()->toString(interp.globalExec()).ascii();
        fprintf(stderr,"Return value: %s\n",msg);
      }

      free(code);
    }
  } // end block, so that interpreter gets deleted

  if (ret)
    fprintf(stderr, "OK.\n");

#ifdef KJS_DEBUG_MEM
  Interpreter::finalCheck();
#endif
  return ret ? 0 : 3;
}
