/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.


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
#pragma once

#ifndef TUVOK_GL_HASH_TABLE_H
#define TUVOK_GL_HASH_TABLE_H

#include "StdTuvokDefines.h"
#if defined(_MSC_VER)
# include <memory>
#else
# include <tr1/memory>
#endif
#include "Basics/Vectors.h"
#include "GLObject.h"

namespace tuvok {

struct hinfo;

class GLHashTable : public GLObject {
  public:
    GLHashTable(const UINTVECTOR2& texSize,
                std::tr1::shared_ptr<AbstrRenderer>& ren);
    virtual ~GLHashTable() { }

    void Activate(uint32_t imgUnit, uint32_t texUnit);
    std::vector<UINTVECTOR4> GetList();

    virtual uint64_t GetCPUSize() const;
    virtual uint64_t GetGPUSize() const;

  private:
    std::tr1::shared_ptr<struct hinfo> hi;
};

}
#endif /* TUVOK_GL_HASH_TABLE_H */
