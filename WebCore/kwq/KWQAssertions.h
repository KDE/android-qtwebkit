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

// Note, this file uses many GCC extensions, but it should be compatible with
// C, Objective C, C++, and Objective C++.

// For non-debug builds, everything is disabled by default.
// Defining any of the symbols explicitly prevents this from having any effect.

#ifdef NDEBUG
#define KWQ_ASSERTIONS_DISABLED_DEFAULT 1
#else
#define KWQ_ASSERTIONS_DISABLED_DEFAULT 0
#endif

#ifndef ASSERT_DISABLED
#define ASSERT_DISABLED KWQ_ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef ASSERT_ARG_DISABLED
#define ASSERT_ARG_DISABLED KWQ_ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef FATAL_DISABLED
#define FATAL_DISABLED KWQ_ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef ERROR_DISABLED
#define ERROR_DISABLED KWQ_ASSERTIONS_DISABLED_DEFAULT
#endif

#ifndef LOG_DISABLED
#define LOG_DISABLED KWQ_ASSERTIONS_DISABLED_DEFAULT
#endif

// These helper functions are always declared, but not necessarily always defined if the corresponding function is disabled.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned mask;
    const char *defaultName;
    enum { KWQLogChannelUninitialized, KWQLogChannelOff, KWQLogChannelOn } state;
} KWQLogChannel;

void KWQReportAssertionFailure(const char *file, int line, const char *function, const char *assertion);
void KWQReportAssertionFailureWithMessage(const char *file, int line, const char *function, const char *assertion, const char *format, ...);
void KWQReportArgumentAssertionFailure(const char *file, int line, const char *function, const char *argName, const char *assertion);
void KWQReportFatalError(const char *file, int line, const char *function, const char *format, ...) ;
void KWQReportError(const char *file, int line, const char *function, const char *format, ...);
void KWQLog(const char *file, int line, const char *function, KWQLogChannel *channel, const char *format, ...);

#ifdef __cplusplus
}
#endif

// CRASH -- gets us into the debugger or the crash reporter -- signals are ignored by the crash reporter so we must do better

#define CRASH() *(int *)0xbbadbeef = 0

// ASSERT, ASSERT_WITH_MESSAGE, ASSERT_NOT_REACHED

#if ASSERT_DISABLED

#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)

#else

#define ASSERT(assertion) do \
    if (!(assertion)) { \
        KWQReportAssertionFailure(__FILE__, __LINE__, __PRETTY_FUNCTION__, #assertion); \
        CRASH(); \
    } \
while (0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) do \
    if (!(assertion)) { \
        KWQReportAssertionFailureWithMessage(__FILE__, __LINE__, __PRETTY_FUNCTION__, #assertion, formatAndArgs); \
        CRASH(); \
    } \
while (0)
#define ASSERT_NOT_REACHED() do { \
    KWQReportAssertionFailure(__FILE__, __LINE__, __PRETTY_FUNCTION__, 0); \
    CRASH(); \
} while (0)

#endif

// ASSERT_ARG

#if ASSERT_ARG_DISABLED

#define ASSERT_ARG(argName, assertion) ((void)0)

#else

#define ASSERT_ARG(argName, assertion) do \
    if (!(assertion)) { \
        KWQReportArgumentAssertionFailure(__FILE__, __LINE__, __PRETTY_FUNCTION__, #argName, #assertion); \
        CRASH(); \
    } \
while (0)

#endif

// FATAL

#if FATAL_DISABLED
#define FATAL(formatAndArgs...) ((void)0)
#else
#define FATAL(formatAndArgs...) do { \
    KWQReportFatalError(__FILE__, __LINE__, __PRETTY_FUNCTION__, formatAndArgs); \
    CRASH(); \
} while (0)
#endif

// ERROR

#if ERROR_DISABLED
#define ERROR(formatAndArgs...) ((void)0)
#else
#define ERROR(formatAndArgs...) KWQReportError(__FILE__, __LINE__, __PRETTY_FUNCTION__, formatAndArgs)
#endif

// LOG

#if LOG_DISABLED
#define LOG(channel, formatAndArgs...) ((void)0)
#else
#define LOG(channel, formatAndArgs...) KWQLog(__FILE__, __LINE__, __PRETTY_FUNCTION__, &JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, channel), formatAndArgs)
#define JOIN_LOG_CHANNEL_WITH_PREFIX(prefix, channel) JOIN_LOG_CHANNEL_WITH_PREFIX_LEVEL_2(prefix, channel)
#define JOIN_LOG_CHANNEL_WITH_PREFIX_LEVEL_2(prefix, channel) prefix ## channel
#endif
