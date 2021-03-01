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
VariantArray::VariantArray(const VariantArray &va) :
  length(va.length),
  data_type(va.data_type)
{
  this->scalar_ub = va.scalar_ub;
  this->scalar_b = va.scalar_b;
  this->scalar_us = va.scalar_us;
  this->scalar_s = va.scalar_s;
  this->scalar_f = va.scalar_f;
  this->scalar_d = va.scalar_d;
}

VariantArray::~VariantArray() {}

void VariantArray::set(const std::shared_ptr<uint8_t> data,
                       size_t len)
{
  this->reset();
  this->length = len;
  this->scalar_ub = data;
  this->data_type = DT_UBYTE;
}
void VariantArray::set(const std::shared_ptr<int8_t> data, size_t len)
{
  this->reset();
  this->length = len;
  this->scalar_b = data;
  this->data_type = DT_BYTE;
}

void VariantArray::set(const std::shared_ptr<uint16_t> data,
                       size_t len)
{
  this->reset();
  this->length = len;
  this->scalar_us = data;
  this->data_type = DT_USHORT;
}
void VariantArray::set(const std::shared_ptr<int16_t> data,
                       size_t len)
{
  this->reset();
  this->length = len;
  this->scalar_s = data;
  this->data_type = DT_SHORT;
}

void VariantArray::set(const std::shared_ptr<float> data, size_t len)
{
  this->reset();
  this->length = len;
  this->scalar_f = data;
  this->data_type = DT_FLOAT;
}

void VariantArray::set(const std::shared_ptr<double> data, size_t len)
{
  this->reset();
  this->length = len;
  this->scalar_d = data;
  this->data_type = DT_DOUBLE;
}

const uint8_t* VariantArray::getub() const
{
  assert(this->data_type == DT_UBYTE);
  return this->scalar_ub.get();
}
const int8_t* VariantArray::getb() const
{
  assert(this->data_type == DT_BYTE);
  return this->scalar_b.get();
}

const uint16_t* VariantArray::getus() const
{
  assert(this->data_type == DT_USHORT);
  return this->scalar_us.get();
}
const int16_t* VariantArray::gets() const
{
  assert(this->data_type == DT_SHORT);
  return this->scalar_s.get();
}

const float* VariantArray::getf() const
{
  assert(this->data_type == DT_FLOAT);
  return this->scalar_f.get();
}
const double* VariantArray::getd() const
{
  assert(this->data_type == DT_DOUBLE);
  return this->scalar_d.get();
}

VariantArray& VariantArray::operator=(const VariantArray &va)
{
  this->scalar_ub = va.scalar_ub;
  this->scalar_b = va.scalar_b;
  this->scalar_us = va.scalar_us;
  this->scalar_s = va.scalar_s;
  this->scalar_f = va.scalar_f;
  this->scalar_d = va.scalar_d;
  this->length = va.length;
  this->data_type = va.data_type;
  return *this;
}

VariantArray::DataType VariantArray::type() const { return this->data_type; }

void VariantArray::reset()
{
  this->scalar_ub.reset();
  this->scalar_b.reset();
  this->scalar_us.reset();
  this->scalar_s.reset();
  this->scalar_f.reset();
  this->scalar_d.reset();
}

}; // namespace tuvok
