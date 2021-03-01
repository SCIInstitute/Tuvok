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
/// \brief Base class for all Tuvok exceptions.
#ifndef TUVOK_EXCEPTION_H
#define TUVOK_EXCEPTION_H

#include "StdTuvokDefines.h"
#include <exception>

namespace tuvok {

class Exception : virtual public std::exception {
  public:
    Exception() : location(nullptr), line(0) {}
    Exception(std::string e, const char* where = nullptr, size_t ln=0)
      : std::exception(), error(e), location(where), line(ln) { }
    virtual ~Exception() throw() { }

    virtual const char* what() const throw() { return this->error.c_str(); }
    const char* where() const { return this->location; }
    size_t lineno() const { return this->line; }

  protected:
    std::string error;
    const char* location;
    size_t line;
};

}

#endif // TUVOK_EXCEPTION_H
