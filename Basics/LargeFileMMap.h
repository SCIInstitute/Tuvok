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
#ifndef BASICS_LARGEFILE_MMAP_H
#define BASICS_LARGEFILE_MMAP_H

#include "LargeFileFD.h"

// experimentally found to be the largest value we can mmap.
const uint64_t UINT64_PAGE_MAX = 35184372088832ULL;

/** A large raw file backed by an 'mmap' implementation. */
class LargeFileMMap : public LargeFileFD {
  public:
    LargeFileMMap(const std::string fn,
                  std::ios_base::openmode mode = std::ios_base::in,
                  uint64_t header_size=0,
                  uint64_t length = UINT64_PAGE_MAX);
    LargeFileMMap(const std::wstring fn,
                  std::ios_base::openmode mode = std::ios_base::in,
                  uint64_t header_size=0,
                  uint64_t length = UINT64_PAGE_MAX);
    virtual ~LargeFileMMap();

    void open(std::ios_base::openmode mode = std::ios_base::in);

    /// reads a block of data, returns a pointer to it.  User must cast it to
    /// the type that makes sense for them.
    std::shared_ptr<const void> rd(uint64_t offset, size_t len);
    using LargeFile::read;
    using LargeFile::rd;

    /// writes a block of data.
    void wr(const std::shared_ptr<const void>& data,
            uint64_t offset, size_t len);
    using LargeFile::wr;

    virtual bool is_open() const;
    virtual void close();

  private:
    void* map;
    uint64_t length;

  private:
    LargeFileMMap(const LargeFileMMap&);
    LargeFileMMap& operator=(const LargeFileMMap&);
};

#endif /* BASICS_LARGEFILE_MMAP_H */
