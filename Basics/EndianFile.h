/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2011 Scientific Computing and Imaging Institute,
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
#ifndef BASICS_ENDIAN_FILE_H
#define BASICS_ENDIAN_FILE_H

#include <algorithm>
#include "EndianConvert.h"
#include "LargeFile.h"

/// An 'EndianFile' wraps a LargeFile class so that it transparently
/// converts endianness on reads.  You must tell the class whether
/// you're on a big endian or little endian system when you instantiate
/// it, and if necessary it will convert every read.
class EndianFile {
  public:
    EndianFile(LargeFile& lf, bool isBigEndian);
    virtual ~EndianFile() {}

    /// read/write calls of a single element.  Only usable with implicit
    /// offsets.
    template<typename T> void read(T* v, size_t N=1) {
      this->lf.read(v, N);
      if(this->big_endian != EndianConvert::IsBigEndian()) {
        std::transform(v, v+N, v, EndianConvert::Swap<T>);
      }
    }
    template<typename T> void write(const T& v) {
      T u = v;
      if(this->big_endian != EndianConvert::IsBigEndian()) {
        u = EndianConvert::Swap<T>(u);
      }
      lf.write(u);
    }
    template<typename T> void write(const T* v, size_t N=1) {
      if(this->big_endian != EndianConvert::IsBigEndian()) {
        std::shared_ptr<void> mem =
          std::shared_ptr<void>(new T[N]);
        std::transform(v, v+N, static_cast<T*>(mem.get()),
                       EndianConvert::Swap<T>);
        lf.wr(mem, N*sizeof(T));
      } else {
        lf.write(v, N);
      }
    }

  private:
    LargeFile& lf;
    bool       big_endian;
};

#endif /* BASICS_ENDIAN_FILE_H */
