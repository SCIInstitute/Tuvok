/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2009 Scientific Computing and Imaging Institute,
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
  \file    DSFactory.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
  \brief   Exceptions for use in the IO subsystem
*/
#pragma once
#ifndef TUVOK_IO_ERROR_H
#define TUVOK_IO_ERROR_H

#include "StdTuvokDefines.h"
#include <stdexcept>
#include "IOException.h"

namespace tuvok {
namespace io {

/// Generic base for a failed open
class DSOpenFailed : virtual public IOException {
  public:
    DSOpenFailed(std::string s, const char* where=nullptr,
                 size_t ln=0) : IOException(s, where, ln) {}
    DSOpenFailed(std::string filename, std::string s,
                 const char* where=nullptr, size_t ln=0)
                 : IOException(s, where, ln),
                   file(filename) {}
    virtual ~DSOpenFailed() throw() {}

    const char* File() const { return file.c_str(); }

  private:
    std::string file;
};


/// something went wrong in the parse process of a file
class DSParseFailed : virtual public DSOpenFailed {
  public:
    DSParseFailed(std::string err, const char* where=nullptr,
                  size_t ln=0) : DSOpenFailed(err, where, ln) {}
    DSParseFailed(std::string f, std::string s,
                  const char* where=nullptr, size_t ln=0)
                  : DSOpenFailed(f, s, where, ln) {}
    virtual ~DSParseFailed() throw() {}
};


/// any kind of dataset verification failed; e.g. a checksum for the
/// file was invalid.
class DSVerificationFailed : virtual public DSOpenFailed {
  public:
    DSVerificationFailed(std::string s, const char* where=nullptr,
                         size_t ln=0) : DSOpenFailed(s, where, ln) {}
    virtual ~DSVerificationFailed() throw() {}
};

/// Oversized bricks; needs re-bricking
class DSBricksOversized : virtual public DSOpenFailed {
  public:
    DSBricksOversized(std::string s,
                      size_t bsz,
                      const char* where=nullptr,
                      size_t ln=0)
                      : DSOpenFailed(s, where, ln),
                        brick_size(bsz) {}
    size_t BrickSize() const { return this->brick_size; }

  private:
    size_t brick_size;
};

} // io
} // tuvok

#endif // TUVOK_IO_ERROR_H
