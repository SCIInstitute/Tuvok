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
#ifndef BASICS_LARGEFILE_FD_H
#define BASICS_LARGEFILE_FD_H

#include "LargeFile.h"

/** Data access using standard file descriptors. */
class LargeFileFD : public LargeFile {
  public:
    /// @argument header_size is maintained as a "base" offset.  Seeking to
    /// byte 0 actually seeks to 'header_size'.
    LargeFileFD(const std::string fn,
                std::ios_base::openmode mode = std::ios_base::in,
                uint64_t header_size=0,
                uint64_t length=0);
    /// @argument header_size is maintained as a "base" offset.  Seeking to
    /// byte 0 actually seeks to 'header_size'.
    LargeFileFD(const std::wstring fn,
                std::ios_base::openmode mode = std::ios_base::in,
                uint64_t header_size=0,
                uint64_t length=0);
    virtual ~LargeFileFD();

    virtual void open(std::ios_base::openmode mode = std::ios_base::in);

    /// reads a block of data, returns a pointer to it.  User must cast it to
    /// the type that makes sense for them.
    virtual std::shared_ptr<const void> rd(uint64_t offset,
                                                size_t len);
    using LargeFile::read;
    using LargeFile::rd;

    /// writes a block of data.
    virtual void wr(const std::shared_ptr<const void>& data,
                    uint64_t offset,
                    size_t len);

    /// notifies the object that we're going to need the following data soon.
    /// Many implementations will prefetch this data when it knows this.
    virtual void enqueue(uint64_t offset, size_t len);

    virtual uint64_t filesize() const;
    virtual bool is_open() const;
    virtual void close();
    virtual void truncate(uint64_t length=0);

  protected:
    int fd;
};

#endif /* BASICS_LARGEFILE_FD_H */
