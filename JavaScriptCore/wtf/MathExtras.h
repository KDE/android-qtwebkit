/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef WTF_MathExtras_h
#define WTF_MathExtras_h

#include <math.h>

#if COMPILER(MSVC)

#include "kjs/operations.h"
#include "kjs/value.h"
#include <xmath.h>
#include <limits>

#if HAVE(FLOAT_H)
#include <float.h>
#endif

inline bool isinf(double num) { return !_finite(num) && !_isnan(num); }
inline bool isnan(double num) { return !!_isnan(num); }
inline long lround(double num) { return static_cast<long>(num > 0 ? num + 0.5 : ceil(num - 0.5)); }
inline long lroundf(float num) { return static_cast<long>(num > 0 ? num + 0.5f : ceilf(num - 0.5f)); }
inline double round(double num) { return num > 0 ? floor(num + 0.5) : ceil(num - 0.5); }
inline float roundf(float num) { return num > 0 ? floorf(num + 0.5f) : ceilf(num - 0.5f); }
inline bool signbit(double num) { return _copysign(1.0, num) < 0; }

inline double nextafter(double x, double y) { return _nextafter(x, y); }
inline float nextafterf(float x, float y) { return x > y ? x - FLT_EPSILON : x + FLT_EPSILON; }

inline double copysign(double x, double y) { return _copysign(x, y); }
inline int isfinite(double x) { return _finite(x); }

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif  //  M_PI

#ifndef M_PI_4
#define M_PI_4 0.785398163397448309616
#endif  //  M_PI_4

// Work around a bug in Win, where atan2(+-infinity, +-infinity) yields NaN instead of specific values.
inline double wtf_atan2(double x, double y)
{
    static double posInf = std::numeric_limits<double>::infinity();
    static double negInf = -std::numeric_limits<double>::infinity();

    double result = KJS::NaN;

    if (x == posInf && y == posInf)
        result = M_PI_4;
    else if (x == posInf && y == negInf)
        result = 3 * M_PI_4;
    else if (x == negInf && y == posInf)
        result = -M_PI_4;
    else if (x == negInf && y == negInf)
        result = -3 * M_PI_4;
    else
        result = ::atan2(x, y);

    return result;
}

// Work around a bug in the Microsoft CRT, where fmod(x, +-infinity) yields NaN instead of x.
inline double wtf_fmod(double x, double y) { return (!isinf(x) && isinf(y)) ? x : fmod(x, y); }

#define fmod(x, y) wtf_fmod(x, y)

#define atan2(x, y) wtf_atan2(x, y)

#endif // #if COMPILER(MSVC)

#endif // #ifndef WTF_MathExtras_h
