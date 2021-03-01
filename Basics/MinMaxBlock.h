#ifndef TUVOK_MAX_MIN_BLOCK_H
#define TUVOK_MAX_MIN_BLOCK_H

#include <algorithm>
#include <cfloat>

namespace tuvok {

/// Stores minimum and maximum data for a block of data.
class MinMaxBlock {
public:
  MinMaxBlock() :
   minScalar(DBL_MAX),
   maxScalar(-FLT_MAX),
   minGradient(DBL_MAX),
   maxGradient(-FLT_MAX)
  {}

  MinMaxBlock(double _minScalar, double _maxScalar,
              double _minGradient, double _maxGradient) :
   minScalar(_minScalar),
   maxScalar(_maxScalar),
   minGradient(_minGradient),
   maxGradient(_maxGradient)
  {}

  void Merge(const MinMaxBlock& other) {
    minScalar = std::min(minScalar, other.minScalar);
    maxScalar = std::max(maxScalar, other.maxScalar);
    minGradient = std::min(minGradient, other.minGradient);
    maxGradient = std::max(maxGradient, other.maxGradient);
  }

  double minScalar;
  double maxScalar;
  double minGradient;
  double maxGradient;
};
}
#endif
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2013 Interactive Visualization and Data Analysis Group

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
