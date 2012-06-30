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
#pragma once

#ifndef TUVOK_BASICS_CTTI_H
#define TUVOK_BASICS_CTTI_H

/** \brief CTTI: compile time type information
 * A lot of this we can glean from C++11, but code needing this kind of
 * functionality existed before C++11. So:
 * @todo see what can be removed in favor of C++11 definitions.
 */
#include "StdDefines.h"
#ifdef DETECTED_OS_WINDOWS
# include <type_traits>
#else
# include <tr1/type_traits>
#endif

struct signed_tag {};
struct unsigned_tag {};

namespace {
/// "Compile time type info"
/// Metaprogramming type traits that we need but aren't in tr1.
/// is_signed: tr1's is_signed only returns true for *integral* signed types..
///            ridiculous.  so we use this as a replacement.
/// size_type: unsigned variant of 'T'.
/// signed_type: signed variant of 'T'.
///@{
template <typename T> struct ctti_base {
  enum { is_signed = std::tr1::is_signed<T>::value ||
                     std::tr1::is_floating_point<T>::value };
};
template <typename T> struct ctti : ctti_base<T> { };

template<> struct ctti<bool> : ctti_base<bool> {
  typedef bool size_type;
  typedef signed_tag sign_tag;
};
template<> struct ctti<char> : ctti_base<char> {
  typedef unsigned char size_type;
  typedef signed char signed_type;
  typedef signed_tag sign_tag ;
};
template<> struct ctti<signed char> : ctti_base<signed char> {
  typedef unsigned char size_type;
  typedef signed char signed_type;
  static const bool is_signed = true;
  typedef signed_tag sign_tag;
};
template<> struct ctti<unsigned char> : ctti_base<unsigned char> {
  typedef unsigned char size_type;
  typedef signed char signed_type;
  typedef unsigned_tag sign_tag;
};
template<> struct ctti<short> : ctti_base<short> {
  typedef unsigned short size_type;
  typedef short signed_type;
  typedef signed_tag sign_tag;
};
template<> struct ctti<unsigned short> : ctti_base<unsigned short> {
  typedef unsigned short size_type;
  typedef short signed_type;
  typedef unsigned_tag sign_tag;
};
#if defined(DETECTED_OS_WINDOWS) && _MSC_VER < 1600
template<> struct ctti<int> : ctti_base<int> {
  typedef unsigned int size_type;
  typedef int  signed_type;
  typedef signed_tag sign_tag;
};

template<> struct ctti<unsigned int> : ctti_base<uint32_t> {
  typedef uint32_t size_type;
  typedef int32_t signed_type;
  typedef unsigned_tag sign_tag;
};
#endif
template<> struct ctti<int32_t> : ctti_base<int32_t> {
  typedef uint32_t size_type;
  typedef int32_t signed_type;
  typedef signed_tag sign_tag;
};
template<> struct ctti<uint32_t> : ctti_base<uint32_t> {
  typedef uint32_t size_type;
  typedef int32_t signed_type;
  typedef unsigned_tag sign_tag;
};
template<> struct ctti<int64_t> : ctti_base<int64_t> {
  typedef uint64_t size_type;
  typedef int64_t signed_type;
  typedef signed_tag sign_tag;
};
template<> struct ctti<uint64_t> : ctti_base<uint64_t> {
  typedef uint64_t size_type;
  typedef int64_t signed_type;
  typedef unsigned_tag sign_tag;
};
template<> struct ctti<float> : ctti_base<float> {
  typedef float size_type;
  typedef float signed_type;
  typedef signed_tag sign_tag;
};
template<> struct ctti<double> : ctti_base<double> {
  typedef double size_type;
  typedef double signed_type;
  typedef signed_tag sign_tag;
};
///@}
};

/// Type tagging.
namespace {
  struct signed_type {};
  struct unsigned_type {};
  signed_type   type_category(signed char)    { return signed_type(); }
  unsigned_type type_category(unsigned char)  { return unsigned_type(); }
  signed_type   type_category(short)          { return signed_type(); }
  unsigned_type type_category(unsigned short) { return unsigned_type(); }
#if defined(DETECTED_OS_WINDOWS) && _MSC_VER < 1600
  signed_type   type_category(int)            { return signed_type(); }
  unsigned_type type_category(unsigned int)         { return unsigned_type(); }
#endif
  signed_type   type_category(int32_t)        { return signed_type(); }
  unsigned_type type_category(uint32_t)       { return unsigned_type(); }
  signed_type   type_category(int64_t)        { return signed_type(); }
  unsigned_type type_category(uint64_t)       { return unsigned_type(); }
  signed_type   type_category(float)          { return signed_type(); }
  signed_type   type_category(double)         { return signed_type(); }
}
#endif // TUVOK_BASICS_CTTI_H
