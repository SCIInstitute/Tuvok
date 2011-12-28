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
#include <errno.h>
#include <stdexcept>

#ifndef NDEBUG
# include <iostream>
# define DEBUG(...) do { std::cerr << __VA_ARGS__ << "\n"; } while(0)
#else
# define DEBUG(...) do { /* nothing, debug msg removed. */ } while(0)
#endif
#include "LargeFile.h"

LargeFile::LargeFile(const std::string fn,
                     std::ios_base::openmode,
                     boost::uint64_t hsz,
                     boost::uint64_t /* length */) :
  m_filename(fn),
  header_size(hsz),
  byte_offset(0),
  bytes_read(0)
{
}
LargeFile::LargeFile(const std::wstring fn,
                     std::ios_base::openmode,
                     boost::uint64_t hsz,
                     boost::uint64_t /* length */) :
  m_filename(fn.begin(), fn.end()),
  header_size(hsz),
  byte_offset(0),
  bytes_read(0)
{
}

std::tr1::shared_ptr<const void> LargeFile::rd(size_t length)
{
  // The other 'read' will take account of the header_size.
  std::tr1::shared_ptr<const void> rv(this->rd(this->byte_offset, length));
  this->byte_offset += length;
  return rv;
}

boost::uint64_t LargeFile::gcount() const { return this->bytes_read; }

void LargeFile::wr(const std::tr1::shared_ptr<const void>& data,
                   size_t length)
{
  this->wr(data, this->byte_offset, length);
  this->byte_offset += length;
}

void LargeFile::seek(boost::uint64_t to) { this->byte_offset = to; }
boost::uint64_t LargeFile::offset() const { return this->byte_offset; }

void LargeFile::truncate(const char* path, boost::uint64_t length)
{
  int rv = 0;
  DEBUG("path=" << path);
  do {
    rv = ::truncate(path, length);
  } while(rv == -1 && errno == EINTR);
  if(rv == -1) {
    switch(errno) {
      case EFBIG: /* fall through */
      case EINVAL: throw std::length_error("broken length"); break;
      case EIO: throw std::ios::failure("io error"); break;
      case EACCES: throw std::runtime_error("permission error"); break;
      case EISDIR: throw std::domain_error("path given is directory"); break;
      case ELOOP: throw std::runtime_error("too many levels of symlinks");
        break;
      case ENAMETOOLONG: throw std::runtime_error("path too long"); break;
      case ENOENT: throw std::runtime_error("bad path"); break;
      case ENOTDIR: throw std::domain_error("path is not valid"); break;
      case EROFS: throw std::runtime_error("path is on RO filesystem"); break;
    }
  }
}

void LargeFile::truncate(boost::uint64_t length)
{
  LargeFile::truncate(this->m_filename.c_str(), length);
  // move offset down if it's beyond EOF.
  this->byte_offset = std::min(this->byte_offset, length);
}
