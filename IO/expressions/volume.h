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
/// \brief Volume access expression, i.e. "v[0]".
#ifndef TUVOK_VOLUME_H
#define TUVOK_VOLUME_H

#include "expression.h"

namespace tuvok { namespace expression {

class Volume : public Expression {
  public:
    /// Sets the index into the array of volumes.
    void SetIndex(double i);
    /// Returns the index.  We know it'll be usable as a size_t, because we
    /// would fail semantic analysis otherwise.
    size_t Index() const;

    void Analyze() const throw(semantic::Error);
    void Print(std::ostream&) const;
    void SetVolumes(const std::vector<VariantArray>&);

    double Evaluate(size_t idx) const;

  private:
    // Yes, it makes more sense for this to be some kind of unsigned
    // integer.  However we just want to store what we get from the
    // lexer at first, so that we can do error detection during
    // semantic analysis, later.
    double index;

    std::vector<VariantArray> v; ///< input volumes.
};

}}

#endif // TUVOK_VOLUME_H
