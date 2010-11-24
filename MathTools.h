/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/**
  \file    MathTools.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    October 2008
*/
#pragma once

#ifndef MATHTOOLS_H
#define MATHTOOLS_H

#include "StdDefines.h"

// Old gcc's have some tr1 missing features/bugs.  Apple didn't fix
// these even with their compiler upgrade, however.
#if defined(_MSC_VER) ||                                             \
    defined(DETECTED_OS_APPLE) ||                                    \
    (defined(__GNUC__) && ((__GNUC__ == 4 && (__GNUC_MINOR__ == 0 || \
                                              __GNUC_MINOR__ == 1))))
# include <cmath>
#else
# define USABLE_TR1
# include <tr1/cmath>
#endif
#ifdef _DEBUG
# include <stdexcept>
#endif

#define ROOT3 1.732050f

namespace MathTools {
  UINT32 Log(UINT32 value, UINT32 base);
  float Log(float value, float base);
  UINT32 Pow(UINT32 base, UINT32 exponent);
  UINT64 Pow(UINT64 base, UINT64 exponent);

  UINT32 Log2(UINT32 n);
  UINT32 Pow2(UINT32 e);
  UINT64 Log2(UINT64 n);
  UINT64 Pow2(UINT64 e);
  UINT32 GaussianSum(UINT32 n);
  bool IsPow2(UINT32 n);
  UINT32 NextPow2(UINT32 n, bool bReturn_ID_on_Pow2=true);

  template<class T> inline T sign(T v) { return T((v > T(0)) - (v < T(0))); }

  template<class T> inline T MakeMultiple(T v, T m) {
    return v + (((v%m) == T(0)) ? T(0) : m-(v%m));
  }

  template<typename in, typename out>
  static inline out
  lerp(in value, in imin, in imax, out omin, out omax)
  {
    out ret = out(omin + (value-imin) * (static_cast<double>(omax-omin) /
                                                            (imax-imin)));
#if defined(_DEBUG) && defined(USABLE_TR1)
    // Very useful while debugging, but too expensive for general use.
    if(std::tr1::isnan(ret) || std::tr1::isinf(ret)) {
      throw std::range_error("NaN or infinity!");
    }
#endif
    return ret;
  }

  float Clamp(float val, float a, float b);
  UINT32 Clamp(UINT32 val, UINT32 a, UINT32 b);
  UINT64 Clamp(UINT64 val, UINT64 a, UINT64 b);
  int Clamp(int val, int a, int b);
};

#endif // MATHTOOLS_H
