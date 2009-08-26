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
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/

#include <cassert>
#include "VariantArray.h"

namespace tuvok {

VariantArray::VariantArray(): length(0) { }
VariantArray::~VariantArray() {}

void VariantArray::set(const std::tr1::shared_ptr<float> data, size_t len)
{
  this->length = len;
  this->scalar_f = data;
  this->scalar_ub.reset();
  this->data_type = DT_FLOAT;
}
void VariantArray::set(const std::tr1::shared_ptr<unsigned char> data,
                       size_t len)
{
  this->length = len;
  this->scalar_f.reset();
  this->scalar_ub = data;
  this->data_type = DT_UBYTE;
}

const float* VariantArray::getf() const
{
  assert(this->data_type == DT_FLOAT);
  return this->scalar_f.get();
}

const unsigned char* VariantArray::getub() const
{
  assert(this->data_type == DT_UBYTE);
  return this->scalar_ub.get();
}

VariantArray& VariantArray::operator=(const VariantArray &va)
{
  this->scalar_f = va.scalar_f;
  this->scalar_ub = va.scalar_ub;
  this->length = va.length;
  this->data_type = va.data_type;
  return *this;
}

}; // namespace tuvok
