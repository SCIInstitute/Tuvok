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
  \file    FileNotFound.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
  \brief   Base class for all IO exceptions
*/
#pragma once

#ifndef SCIO_FILE_NOT_FOUND_H
#define SCIO_FILE_NOT_FOUND_H

#include "IOException.h"

/// Interface at the exception site:
///    throw FILE_NOT_FOUND("the_filename");
/// It automatically sets the location information.
#define FILE_NOT_FOUND(msg) tuvok::io::FileNotFound(msg, __FILE__, __LINE__)

namespace tuvok { namespace io {

class FileNotFound : virtual public IOException {
  public:
    FileNotFound(const char* e, const char* where=NULL,
                 size_t ln=0)
      : IOException(e, where, ln) {}
    virtual ~FileNotFound() throw() {}
};

}}

#endif /* SCIO_FILE_NOT_FOUND_H */
