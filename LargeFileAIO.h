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
#ifndef BASICS_LARGEFILE_AIO_H
#define BASICS_LARGEFILE_AIO_H

#include <unordered_map>
#include "LargeFileFD.h"

struct aiocb;

/** Uses AIO to read data.  'read' is still synchronous, but the additional
 * 'buffer' method can be used to enqueue a read that is not yet needed. */
class LargeFileAIO : public LargeFileFD {
  public:
    /// @argument header_size is maintained as a "base" offset.  Seeking to
    /// byte 0 actually seeks to 'header_size'.
    LargeFileAIO(const std::string fn,
                 std::ios_base::openmode mode = std::ios_base::in,
                 uint64_t header_size=0,
                 uint64_t length=0);
    /// @argument header_size is maintained as a "base" offset.  Seeking to
    /// byte 0 actually seeks to 'header_size'.
    LargeFileAIO(const std::wstring fn,
                 std::ios_base::openmode mode = std::ios_base::in,
                 uint64_t header_size=0,
                 uint64_t length=0);
    virtual ~LargeFileAIO();

    /// reads a block of data, returns a pointer to it.  User must cast it to
    /// the type that makes sense for them.
    virtual std::shared_ptr<const void> rd(uint64_t offset,
                                                size_t len);
    using LargeFile::read;
    using LargeFile::rd;

    /// notifies the object that we're going to need the following data soon.
    /// Many implementations will prefetch this data when it knows this.
    virtual void enqueue(uint64_t offset, size_t len);

    /// writes a block of data.
    virtual void wr(const std::shared_ptr<const void>& data,
                    uint64_t offset,
                    size_t len);
    using LargeFile::write;
    /// Allows the user to unset a copy of the data on write.  When false, this
    /// means that once you've given a pointer to 'write', the client must not
    /// touch the data it points to... ever again.
    /// When true (the default), the system has to copy the data to a new chunk
    /// of memory before it can return.
    void copy_writes(bool);

    virtual void close();

    typedef std::unordered_map<struct aiocb*,
                               std::shared_ptr<const void> > Reqs;
  private:
    LargeFileAIO(const LargeFileAIO&);
    LargeFileAIO& operator=(const LargeFileAIO&);

    struct aiocb* submit_new_request(uint64_t offset, size_t len);
    void flush_writes();

  private:
    Reqs control;
    bool writes_copied;
};

#endif /* BASICS_LARGEFILE_AIO_H */
