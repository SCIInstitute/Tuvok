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
/// \brief Errors that can happen during syntax analysis
#ifndef TUVOK_EXPRESSION_SYNTAX_H
#define TUVOK_EXPRESSION_SYNTAX_H

#include "TuvokException.h"

namespace tuvok { namespace expression {

class SyntaxError: virtual public tuvok::Exception {
  public:
    explicit SyntaxError(const char* e,
                         size_t col_start, size_t col_end,
                         const char* where, size_t ln=0) :
      tuvok::Exception(e, where, ln), column_start(col_start),
      column_end(col_end) {}

    size_t ColumnStart() const { return this->column_start; }
    size_t ColumnEnd() const { return this->column_end; }

  private:
    const size_t column_start;
    const size_t column_end;
};

}}

#endif // TUVOK_EXPRESSION_SYNTAX_H
