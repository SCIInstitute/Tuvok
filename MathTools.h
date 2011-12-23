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
  uint32_t Log(uint32_t value, uint32_t base);
  float Log(float value, float base);
  uint32_t Pow(uint32_t base, uint32_t exponent);
  uint64_t Pow(uint64_t base, uint64_t exponent);

  uint32_t Log2(uint32_t n);
  uint32_t Pow2(uint32_t e);
  uint64_t Log2(uint64_t n);
  uint64_t Pow2(uint64_t e);
  uint32_t GaussianSum(uint32_t n);
  bool IsPow2(uint32_t n);
  uint32_t NextPow2(uint32_t n, bool bReturn_ID_on_Pow2=true);

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

  // Attempts to detect NaNs.  Imprecise; only useful for asserts and
  // debugging, do not rely on it!
  bool NaN(float f);

  float Clamp(float val, float a, float b);
  uint32_t Clamp(uint32_t val, uint32_t a, uint32_t b);
  uint64_t Clamp(uint64_t val, uint64_t a, uint64_t b);
  int Clamp(int val, int a, int b);
};

#endif // MATHTOOLS_H
