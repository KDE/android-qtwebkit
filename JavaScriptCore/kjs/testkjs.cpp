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

#include "HashTraits.h"
#include "JSLock.h"
#include "interpreter.h"
#include "object.h"
#include "types.h"
#include "value.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

using namespace KJS;
using namespace KXMLCore;

static void testIsInteger();
static char* createStringWithContentsOfFile(const char* fileName);

class StopWatch
{
public:
    void start();
    void stop();
    long getElapsedMS(); // call stop() first
    
private:
#if !WIN32
    // Windows does not have timeval, disabling this class for now (bug 7399)
    timeval m_startTime;
    timeval m_stopTime;
#endif
};

void StopWatch::start()
{
#if !WIN32
    gettimeofday(&m_startTime, 0);
#endif
}

void StopWatch::stop()
{
#if !WIN32
    gettimeofday(&m_stopTime, 0);
#endif
}

long StopWatch::getElapsedMS()
{
#if !WIN32
    timeval elapsedTime;
    timersub(&m_stopTime, &m_startTime, &elapsedTime);
    
    return elapsedTime.tv_sec * 1000 + lroundf(elapsedTime.tv_usec / 1000.0);
#else
    return 0;
#endif
}

class GlobalImp : public JSObject {
public:
  virtual UString className() const { return "global"; }
};

class TestFunctionImp : public JSObject {
public:
  TestFunctionImp(int i, int length);
  virtual bool implementsCall() const { return true; }
  virtual JSValue* callAsFunction(ExecState* exec, JSObject* thisObj, const List &args);

  enum { Print, Debug, Quit, GC, Version, Run };

private:
  int id;
};

TestFunctionImp::TestFunctionImp(int i, int length) : JSObject(), id(i)
{
  putDirect(lengthPropertyName,length,DontDelete|ReadOnly|DontEnum);
}

JSValue* TestFunctionImp::callAsFunction(ExecState* exec, JSObject*, const List &args)
{
  switch (id) {
    case Print:
      printf("--> %s\n", args[0]->toString(exec).UTF8String().c_str());
      return jsUndefined();
    case Debug:
      fprintf(stderr, "--> %s\n", args[0]->toString(exec).UTF8String().c_str());
      return jsUndefined();
    case GC:
    {
      JSLock lock;
      Interpreter::collect();
      return jsUndefined();
    }
    case Version:
      // We need this function for compatibility with the Mozilla JS tests but for now
      // we don't actually do any version-specific handling
      return jsUndefined();
    case Run:
    {
      StopWatch stopWatch;
      char* fileName = strdup(args[0]->toString(exec).UTF8String().c_str());
      char* script = createStringWithContentsOfFile(fileName);
      if (!script)
        return throwError(exec, GeneralError, "Could not open file.");

      stopWatch.start();
      exec->dynamicInterpreter()->evaluate(fileName, 0, script);
      stopWatch.stop();

      free(script);
      free(fileName);
      
      return jsNumber(stopWatch.getElapsedMS());
    }
    case Quit:
      exit(0);
    default:
      abort();
  }
}

int main(int argc, char** argv)
{
  if (argc < 2) {
    fprintf(stderr, "Usage: testkjs file1 [file2...]\n");
    return -1;
  }

  testIsInteger();

  bool success = true;
  {
    JSLock lock;
    
    GlobalImp* global = new GlobalImp();

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
    global->put(interp.globalExec(), "version", new TestFunctionImp(TestFunctionImp::Version, 1));
    global->put(interp.globalExec(), "run", new TestFunctionImp(TestFunctionImp::Run, 1));

    Interpreter::setShouldPrintExceptions(true);
    
    for (int i = 1; i < argc; i++) {
      const char* fileName = argv[i];
      if (strcmp(fileName, "-f") == 0) // mozilla test driver script uses "-f" prefix for files
        continue;
      
      char* script = createStringWithContentsOfFile(fileName);
      if (!script) {
        success = false;
        break; // fail early so we can catch missing files
      }

      Completion completion = interp.evaluate(fileName, 0, script);
      success = success && completion.complType() != Throw;
      free(script);
    }
    
    delete global;
  } // end block, so that interpreter gets deleted

  if (success)
    fprintf(stderr, "OK.\n");
  
#ifdef KJS_DEBUG_MEM
  Interpreter::finalCheck();
#endif
  return success ? 0 : 3;
}

static void testIsInteger()
{
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

  assert(!IsInteger<char*>::value);
  assert(!IsInteger<const char* >::value);
  assert(!IsInteger<volatile char* >::value);
  assert(!IsInteger<double>::value);
  assert(!IsInteger<float>::value);
  assert(!IsInteger<GlobalImp>::value);
}

static char* createStringWithContentsOfFile(const char* fileName)
{
  char* buffer;
  
  int buffer_size = 0;
  int buffer_capacity = 1024;
  buffer = (char*)malloc(buffer_capacity);
  
  FILE* f = fopen(fileName, "r");
  if (!f) {
    fprintf(stderr, "Could not open file: %s\n", fileName);
    return 0;
  }
  
  while (!feof(f) && !ferror(f)) {
    buffer_size += fread(buffer + buffer_size, 1, buffer_capacity - buffer_size, f);
    if (buffer_size == buffer_capacity) { // guarantees space for trailing '\0'
      buffer_capacity *= 2;
      buffer = (char*)realloc(buffer, buffer_capacity);
      assert(buffer);
    }
    
    assert(buffer_size < buffer_capacity);
  }
  fclose(f);
  buffer[buffer_size] = '\0';
  
  return buffer;
}
