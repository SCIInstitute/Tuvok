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
#ifndef BASICS_LARGEFILE_H
#define BASICS_LARGEFILE_H

#ifndef _LARGEFILE64_SOURCE
# define _LARGEFILE64_SOURCE 1
#endif

#include <iostream>
#include <string>
#ifdef _MSC_VER
# include <memory>
#else
# include <tr1/memory>
#endif

#include <boost/cstdint.hpp>

/** Generic class for accessing large quantities of binary data. */
class LargeFile {
  public:
    /// @argument header_size is maintained as a "base" offset.  Seeking to
    /// byte 0 actually seeks to 'header_size'.
    /// @argument length some implementations are better if you can guarantee
    /// up front how much data you'll access.
    LargeFile(const std::string fn,
              std::ios_base::openmode mode = std::ios_base::in,
              boost::uint64_t header_size=0,
              boost::uint64_t length=0);
    /// @argument header_size is maintained as a "base" offset.  Seeking to
    /// byte 0 actually seeks to 'header_size'.
    /// @argument length some implementations are better if you can guarantee
    /// up front how much data you'll access.
    LargeFile(const std::wstring fn,
              std::ios_base::openmode mode = std::ios_base::in,
              boost::uint64_t header_size=0,
              boost::uint64_t length=0);
    virtual ~LargeFile() {}

    /// The file is automatically opened upon construction, but you can use
    /// this to reopen in a different mode, for example.
    /// LargeFiles are always 'raw': std::ios::binary is implied.
    virtual void open(std::ios_base::openmode mode = std::ios_base::in) = 0;

    /// Uses the current byte offset to read data from the file.
    virtual std::tr1::shared_ptr<const void> rd(size_t len);
    /// reads a block of data, returns a pointer to it.  User must cast it to
    /// the type that makes sense for them.
    /// The file's current byte offset is undefined after this operation.
    virtual std::tr1::shared_ptr<const void> rd(boost::uint64_t offset,
                                                size_t len) = 0;
    /// returns the number of bytes read during the last read.
    virtual boost::uint64_t gcount() const;

    /// writes a block of data.
    /// The file's current byte offset is undefined after this operation.
    virtual void wr(const std::tr1::shared_ptr<const void>& data,
                       boost::uint64_t offset,
                       size_t len) = 0;
    /// writes data at the current byte offset
    virtual void wr(const std::tr1::shared_ptr<const void>& data,
                       size_t len);

    /// read/write calls of a single element.  Only usable with implicit
    /// offsets.
    ///@{
    // a 'delete' functor that just does nothing.
    struct null_deleter { void operator()(const void*) const {} };
    template<typename T> void read(T* v, size_t N=1) {
      std::tr1::shared_ptr<const void> m = this->rd(this->byte_offset,
                                                    sizeof(T)*N);
      const T* mem = static_cast<const T*>(m.get());
      std::copy(mem, mem+N, v);
      this->byte_offset += sizeof(T)*N;
    }
    template<typename T> void write(const T& v) {
      this->wr(std::tr1::shared_ptr<const void>(&v, null_deleter()),
               this->byte_offset, sizeof(T));
      this->byte_offset += sizeof(T);
    }
    template<typename T> void write(const T* v, size_t N=1) {
      this->wr(std::tr1::shared_ptr<const void>(v, null_deleter()),
               sizeof(T)*N);
      // don't increase byte_offset -- this version of wr() does it for us!
    }
    ///@}

    /// notifies the object that we're going to need the following data soon.
    /// Many implementations will prefetch this data when it knows this.
    virtual void enqueue(boost::uint64_t offset, size_t len) = 0;

    std::string filename() const { return this->m_filename; }
    virtual boost::uint64_t filesize() const = 0;

    virtual void seek(boost::uint64_t);
    virtual boost::uint64_t offset() const;

    virtual bool is_open() const = 0;
    virtual void close() = 0;
    virtual void truncate(boost::uint64_t length=0);

    static void truncate(const char* path, boost::uint64_t length);

  protected:
    std::string     m_filename;
    boost::uint64_t header_size;
    boost::uint64_t byte_offset;
    boost::uint64_t bytes_read; // number of bytes read during last read

  private:
    LargeFile(const LargeFile&);
    LargeFile& operator=(const LargeFile&);
};

#endif /* BASICS_LARGEFILE_H */
