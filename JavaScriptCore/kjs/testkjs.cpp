/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Bjoern Graf (bjoern.graf@gmail.com)
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

#include "JSGlobalObject.h"
#include "JSLock.h"
#include "Parser.h"
#include "array_object.h"
#include "collector.h"
#include "function.h"
#include "interpreter.h"
#include "nodes.h"
#include "object.h"
#include "protect.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/HashTraits.h>

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if PLATFORM(WIN_OS)
#include <crtdbg.h>
#include <windows.h>
#endif

#if PLATFORM(QT)
#include <QDateTime>
#endif

using namespace KJS;
using namespace WTF;

static bool fillBufferWithContentsOfFile(const UString& fileName, Vector<char>& buffer);

static JSValue* functionPrint(ExecState*, JSObject*, const List&);
static JSValue* functionDebug(ExecState*, JSObject*, const List&);
static JSValue* functionGC(ExecState*, JSObject*, const List&);
static JSValue* functionVersion(ExecState*, JSObject*, const List&);
static JSValue* functionRun(ExecState*, JSObject*, const List&);
static JSValue* functionLoad(ExecState*, JSObject*, const List&);
static JSValue* functionReadline(ExecState*, JSObject*, const List&);
static JSValue* functionQuit(ExecState*, JSObject*, const List&);

class StopWatch {
public:
    void start();
    void stop();
    long getElapsedMS(); // call stop() first

private:
#if PLATFORM(QT)
    uint m_startTime;
    uint m_stopTime;
#elif PLATFORM(WIN_OS)
    DWORD m_startTime;
    DWORD m_stopTime;
#else
    // Windows does not have timeval, disabling this class for now (bug 7399)
    timeval m_startTime;
    timeval m_stopTime;
#endif
};

void StopWatch::start()
{
#if PLATFORM(QT)
    QDateTime t = QDateTime::currentDateTime();
    m_startTime = t.toTime_t() * 1000 + t.time().msec();
#elif PLATFORM(WIN_OS)
    m_startTime = timeGetTime();
#else
    gettimeofday(&m_startTime, 0);
#endif
}

void StopWatch::stop()
{
#if PLATFORM(QT)
    QDateTime t = QDateTime::currentDateTime();
    m_stopTime = t.toTime_t() * 1000 + t.time().msec();
#elif PLATFORM(WIN_OS)
    m_stopTime = timeGetTime();
#else
    gettimeofday(&m_stopTime, 0);
#endif
}

long StopWatch::getElapsedMS()
{
#if PLATFORM(WIN_OS) || PLATFORM(QT)
    return m_stopTime - m_startTime;
#else
    timeval elapsedTime;
    timersub(&m_stopTime, &m_startTime, &elapsedTime);

    return elapsedTime.tv_sec * 1000 + lroundf(elapsedTime.tv_usec / 1000.0f);
#endif
}

class GlobalObject : public JSGlobalObject {
public:
    GlobalObject(Vector<UString>& arguments);
    virtual UString className() const { return "global"; }
};
COMPILE_ASSERT(!IsInteger<GlobalObject>::value, WTF_IsInteger_GlobalObject_false);

GlobalObject::GlobalObject(Vector<UString>& arguments)
{
    putDirectFunction(new PrototypeFunction(globalExec(), functionPrototype(), 1, "debug", functionDebug));
    putDirectFunction(new PrototypeFunction(globalExec(), functionPrototype(), 1, "print", functionPrint));
    putDirectFunction(new PrototypeFunction(globalExec(), functionPrototype(), 0, "quit", functionQuit));
    putDirectFunction(new PrototypeFunction(globalExec(), functionPrototype(), 0, "gc", functionGC));
    putDirectFunction(new PrototypeFunction(globalExec(), functionPrototype(), 1, "version", functionVersion));
    putDirectFunction(new PrototypeFunction(globalExec(), functionPrototype(), 1, "run", functionRun));
    putDirectFunction(new PrototypeFunction(globalExec(), functionPrototype(), 1, "load", functionLoad));
    putDirectFunction(new PrototypeFunction(globalExec(), functionPrototype(), 0, "readline", functionReadline));

    JSObject* array = arrayConstructor()->construct(globalExec(), globalExec()->emptyList());
    for (size_t i = 0; i < arguments.size(); ++i)
        array->put(globalExec(), i, jsString(arguments[i]));
    putDirect("arguments", array);

    Interpreter::setShouldPrintExceptions(true);
}

JSValue* functionPrint(ExecState* exec, JSObject*, const List& args)
{
    printf("%s\n", args[0]->toString(exec).UTF8String().c_str());
    return jsUndefined();
}

JSValue* functionDebug(ExecState* exec, JSObject*, const List& args)
{
    fprintf(stderr, "--> %s\n", args[0]->toString(exec).UTF8String().c_str());
    return jsUndefined();
}

JSValue* functionGC(ExecState*, JSObject*, const List&)
{
    JSLock lock;
    Collector::collect();
    return jsUndefined();
}

JSValue* functionVersion(ExecState*, JSObject*, const List&)
{
    // We need this function for compatibility with the Mozilla JS tests but for now
    // we don't actually do any version-specific handling
    return jsUndefined();
}

JSValue* functionRun(ExecState* exec, JSObject*, const List& args)
{
    StopWatch stopWatch;
    UString fileName = args[0]->toString(exec);
    Vector<char> script;
    if (!fillBufferWithContentsOfFile(fileName, script))
        return throwError(exec, GeneralError, "Could not open file.");

    stopWatch.start();
    Interpreter::evaluate(exec->dynamicGlobalObject()->globalExec(), fileName, 0, script.data());
    stopWatch.stop();

    return jsNumber(stopWatch.getElapsedMS());
}

JSValue* functionLoad(ExecState* exec, JSObject*, const List& args)
{
    UString fileName = args[0]->toString(exec);
    Vector<char> script;
    if (!fillBufferWithContentsOfFile(fileName, script))
        return throwError(exec, GeneralError, "Could not open file.");

    Interpreter::evaluate(exec->dynamicGlobalObject()->globalExec(), fileName, 0, script.data());

    return jsUndefined();
}

JSValue* functionReadline(ExecState*, JSObject*, const List&)
{
    Vector<char, 256> line;
    int c;
    while ((c = getchar()) != EOF) {
        // FIXME: Should we also break on \r? 
        if (c == '\n')
            break;
        line.append(c);
    }
    line.append('\0');
    return jsString(line.data());
}

JSValue* functionQuit(ExecState*, JSObject*, const List&)
{
    exit(0);
#if !COMPILER(MSVC)
    // MSVC knows that exit(0) never returns, so it flags this return statement as unreachable.
    return jsUndefined();
#endif
}

// Use SEH for Release builds only to get rid of the crash report dialog
// (luckily the same tests fail in Release and Debug builds so far). Need to
// be in a separate main function because the kjsmain function requires object
// unwinding.

#if PLATFORM(WIN_OS) && !defined(_DEBUG)
#define TRY       __try {
#define EXCEPT(x) } __except (EXCEPTION_EXECUTE_HANDLER) { x; }
#else
#define TRY
#define EXCEPT(x)
#endif

int kjsmain(int argc, char** argv);

int main(int argc, char** argv)
{
#if defined(_DEBUG) && PLATFORM(WIN_OS)
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
#endif

    int res = 0;
    TRY
        res = kjsmain(argc, argv);
    EXCEPT(res = 3)
    return res;
}

static bool prettyPrintScript(const UString& fileName, const Vector<char>& script)
{
    int errLine = 0;
    UString errMsg;
    UString scriptUString(script.data());
    RefPtr<ProgramNode> programNode = parser().parse<ProgramNode>(fileName, 0, scriptUString.data(), scriptUString.size(), 0, &errLine, &errMsg);
    if (!programNode) {
        fprintf(stderr, "%s:%d: %s.\n", fileName.UTF8String().c_str(), errLine, errMsg.UTF8String().c_str());
        return false;
    }

    printf("%s\n", programNode->toString().UTF8String().c_str());
    return true;
}

static bool runWithScripts(const Vector<UString>& fileNames, Vector<UString>& arguments, bool prettyPrint)
{
    GlobalObject* globalObject = new GlobalObject(arguments);
    Vector<char> script;

    bool success = true;

    for (size_t i = 0; i < fileNames.size(); i++) {
        UString fileName = fileNames[i];

        if (!fillBufferWithContentsOfFile(fileName, script))
            return false; // fail early so we can catch missing files

        if (prettyPrint)
            prettyPrintScript(fileName, script);
        else {
            Completion completion = Interpreter::evaluate(globalObject->globalExec(), fileName, 0, script.data());
            success = success && completion.complType() != Throw;
        }
    }
    return success;
}

static void printUsageStatement()
{
    fprintf(stderr, "Usage: testkjs -f file1 [-f file2...][-p][-- arguments...]\n");
    exit(-1);
}

static void parseArguments(int argc, char** argv, Vector<UString>& fileNames, Vector<UString>& arguments, bool& prettyPrint)
{
    if (argc < 3)
        printUsageStatement();

    int i = 1;
    for (; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "-f") == 0) {
            if (++i == argc)
                printUsageStatement();
            fileNames.append(argv[i]);
            continue;
        }
        if (strcmp(arg, "-p") == 0) {
            prettyPrint = true;
            continue;
        }
        if (strcmp(arg, "--") == 0) {
            ++i;
            break;
        }
        break;
    }

    for (; i < argc; ++i)
        arguments.append(argv[i]);
}

int kjsmain(int argc, char** argv)
{
    JSLock lock;

    bool prettyPrint = false;
    Vector<UString> fileNames;
    Vector<UString> arguments;
    parseArguments(argc, argv, fileNames, arguments, prettyPrint);

    bool success = runWithScripts(fileNames, arguments, prettyPrint);

#ifndef NDEBUG
    Collector::collect();
#endif

    return success ? 0 : 3;
}

static bool fillBufferWithContentsOfFile(const UString& fileName, Vector<char>& buffer)
{
    FILE* f = fopen(fileName.UTF8String().c_str(), "r");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName.UTF8String().c_str());
        return false;
    }

    size_t buffer_size = 0;
    size_t buffer_capacity = 1024;

    buffer.resize(buffer_capacity);

    while (!feof(f) && !ferror(f)) {
        buffer_size += fread(buffer.data() + buffer_size, 1, buffer_capacity - buffer_size, f);
        if (buffer_size == buffer_capacity) { // guarantees space for trailing '\0'
            buffer_capacity *= 2;
            buffer.resize(buffer_capacity);
        }
    }
    fclose(f);
    buffer[buffer_size] = '\0';

    return true;
}
