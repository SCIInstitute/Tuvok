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
#pragma once

#ifndef TUVOK_VARIANT_ARRAY_H
#define TUVOK_VARIANT_ARRAY_H

#include "StdTuvokDefines.h"
#include <memory>

namespace tuvok {

/** A variant array essentially tries to unionize shared_ptrs.  Since they are
 * non-POD types, it is not technically possible to store them in a union.  The
 * natural second choice is to simply utilize the templated nature of
 * shared_ptr and templatize your container, passing the type down into the
 * shared_ptr.  The downside is that you must templatize and therefore "inline"
 * a potentially large amount of code.
 * The implementation here provides a tradeoff: a variant array has additional
 * constant overhead in both storage and access time, but allows one to avoid
 * templatizing large bodies of code.  Thus it is a good fit when you:
 *    . Want to store a large amount of data
 *    . Grab the pointer once and access large amounts of the data
 * and a poor choice when your data are small or you frequently only need to
 * access a small subset of the data.
 * \note A VariantArray only holds one data type at a time; telling it to hold
 *       a different type will invalidate access to other data.  It is
 *       runtime-typed and will give errors if you try to access data in a
 *       manner which is not type safe. */
class VariantArray {
public:
  enum DataType {
    DT_UBYTE=0, DT_BYTE, DT_USHORT, DT_SHORT, DT_FLOAT, DT_DOUBLE
  };

public:
  VariantArray();
  VariantArray(const VariantArray &);
  ~VariantArray();

  void set(const std::shared_ptr<uint8_t>, size_t len);
  void set(const std::shared_ptr<int8_t>, size_t len);
  void set(const std::shared_ptr<uint16_t>, size_t len);
  void set(const std::shared_ptr<int16_t>, size_t len);
  void set(const std::shared_ptr<float>, size_t len);
  void set(const std::shared_ptr<double>, size_t len);

  size_t size() const { return length; }

  const uint8_t* getub() const;
  const int8_t* getb() const;
  const uint16_t* getus() const;
  const int16_t* gets() const;
  const float* getf() const;
  const double* getd() const;
  DataType type() const;

  VariantArray& operator=(const VariantArray &);

private:
  void reset(); ///< set all internal pointers to null.

private:
  std::shared_ptr<uint8_t>  scalar_ub;
  std::shared_ptr<int8_t>   scalar_b;
  std::shared_ptr<int16_t>  scalar_s;
  std::shared_ptr<uint16_t> scalar_us;
  std::shared_ptr<float>    scalar_f;
  std::shared_ptr<double>   scalar_d;
  size_t                         length;
  enum DataType                  data_type;
};

} // namespace tuvok

#endif // TUVOK_VARIANT_ARRAY_H
