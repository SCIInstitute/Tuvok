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

#include <iostream>
#include <list>

#include "LargeFile.h"

// experimentally found to be the largest value we can mmap.
const uint64_t UINT64_PAGE_MAX = 35184372088832ULL;

/** A large raw file backed by an 'mmap' implementation. */
class LargeFileMMap : public LargeFile {
  public:
    LargeFileMMap(const std::string fn,
                  std::ios_base::openmode mode = std::ios_base::in,
                  boost::uint64_t header_size=0,
                  boost::uint64_t length = UINT64_PAGE_MAX);
    virtual ~LargeFileMMap() {}

    /// reads a block of data, returns a pointer to it.  User must cast it to
    /// the type that makes sense for them.
    std::tr1::weak_ptr<const void> read(boost::uint64_t offset, size_t len);
    std::tr1::weak_ptr<const void> read(size_t len);

    /// writes a block of data.
    void write(const std::tr1::shared_ptr<const void>& data,
               boost::uint64_t offset, size_t len);

    virtual bool is_open() const;
    virtual void close();

  protected:
    void open(const char* filename=NULL,
              std::ios_base::openmode mode = std::ios_base::in);

  private:
    int fd;
    void* map;
    boost::uint64_t length;
    std::list<std::tr1::shared_ptr<const void> > memory;
};

#endif /* BASICS_LARGEFILE_MMAP_H */
