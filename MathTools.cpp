/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
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
  \file    MathTools.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \version  1.11
  \date    October 2008
*/

#include "MathTools.h"
#include <algorithm>

uint32_t MathTools::Log(uint32_t value, uint32_t base) {
  return uint32_t(log(float(value)) / log(float(base)));
}

float MathTools::Log(float value, float base) {
  return log(value) / log(base);
}

uint32_t MathTools::Pow(uint32_t base, uint32_t exponent) {
  return uint32_t(0.5f+pow(float(base), float(exponent)));
}

uint64_t MathTools::Pow(uint64_t base, uint64_t exponent) {
  return uint64_t(0.5+pow(double(base), double(exponent)));
}

uint32_t MathTools::Log2(uint32_t n) {
  int iLog=0;
  while( n>>=1 ) iLog++;
  return iLog;
}

uint32_t MathTools::Pow2(uint32_t e) {
  return 1<<e;
}


uint64_t MathTools::Log2(uint64_t n) {
  int iLog=0;
  while( n>>=1 ) iLog++;
  return iLog;
}

uint64_t MathTools::Pow2(uint64_t e) {
  return static_cast<uint64_t>(1) << e;
}

uint32_t MathTools::GaussianSum(uint32_t n) {
  return n*(n+1)/2;
}

bool MathTools::IsPow2(uint32_t n) {
    return ((n&(n-1))==0);
};

uint32_t MathTools::NextPow2(uint32_t n, bool bReturn_ID_on_Pow2) {
    if (bReturn_ID_on_Pow2 && IsPow2(n)) return n;
    return Pow2(Log2(n)+1);
}

bool MathTools::NaN(float f)
{
#ifdef USABLE_TR1
  return std::tr1::isnan(f);
#elif defined(_MSC_VER)
  return _finite(f) == 0;
#else
  // Hack, relies on 'f' being IEEE-754!
  return f != f;
#endif
}

float MathTools::Clamp(float val, float a, float b) {
  return std::max(a, std::min(b, val));
}

uint32_t MathTools::Clamp(uint32_t val, uint32_t a, uint32_t b) {
  return std::max(a, std::min(b, val));
}

uint64_t MathTools::Clamp(uint64_t val, uint64_t a, uint64_t b) {
  return std::max(a, std::min(b, val));
}

int MathTools::Clamp(int val, int a, int b) {
  return std::max(a, std::min(b, val));
}
