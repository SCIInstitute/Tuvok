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
  \file    context.cpp
  \author  Tom Fogal
           SCI Institute
           University of Utah
  \brief   Simple wrapper for establishing an OpenGL context.
*/
#ifndef TUVOK_TEST_CONTEXT_H
#define TUVOK_TEST_CONTEXT_H

#include "StdTuvokDefines.h"
#include <cstdint>
#include <exception>

namespace tuvok {

class TvkContext {
  public:
    virtual ~TvkContext();
    // Virtual coonstructor for appropriate kind of context.
    static TvkContext* Create(uint32_t width, uint32_t height,
                              uint8_t color_bits=32, uint8_t depth_bits=24,
                              uint8_t stencil_bits=8, bool double_buffer=true,
                              bool visible=false);

    virtual bool isValid() const=0;
    virtual bool makeCurrent()=0;
    virtual bool swapBuffers()=0;
};

class NoAvailableContext : public std::exception {
  public:
    virtual ~NoAvailableContext() throw() {}
    virtual const char* what() const throw() {
      return "No context was available to utilize.";
    }
};

}

#endif // TUVOK_TEST_CONTEXT_H
