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
#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef NDEBUG
# include <iostream>
# define DEBUG(s) do { std::cerr << s << "\n"; } while(0)
#else
# define DEBUG(s) do { /* nothing, debug msg removed. */ } while(0)
#endif
#include "LargeFileFD.h"


LargeFileFD::LargeFileFD(const std::string fn,
                         std::ios_base::openmode mode,
                         boost::uint64_t header_size,
                         boost::uint64_t /* length */) :
  LargeFile(fn, mode, header_size), fd(-1)
{
  this->open(mode);
}

LargeFileFD::~LargeFileFD()
{
  if(this->is_open()) {
    this->close();
  }
}

namespace {
  template<typename T> struct DeleteArray {
    void operator()(const T* t) const { delete[] t; }
  };
}

std::tr1::shared_ptr<const void> LargeFileFD::read(boost::uint64_t offset,
                                                   size_t len)
{
  if(!this->is_open()) {
    throw std::ios_base::failure("file is not open!!");
  }

  if(lseek(this->fd, offset+this->header_size, SEEK_SET) < 0) {
    throw std::ios_base::failure("could not seek to correct file position.");
  }
#if _POSIX_C_SOURCE >= 200112L
  posix_fadvise(this->fd, offset+this->header_size, len, POSIX_FADV_WILLNEED);
#endif

  std::tr1::shared_ptr<char> data =
    std::tr1::shared_ptr<char>(new char[len], DeleteArray<char>());
  ssize_t bytes;
  size_t completed = 0;
  do {
    bytes = ::read(this->fd, static_cast<char*>(data.get())+completed,
                   len-completed);
    if(bytes < 0 && errno != EINTR) {
      throw std::ios_base::failure("read failure.");
    }
    completed += bytes;
  } while(completed < len && errno == EINTR);

  // we need to cast it to a 'const void' to return it...
  std::tr1::shared_ptr<const void> mem =
    std::tr1::static_pointer_cast<const void>(data);
  return mem;
}

void LargeFileFD::write(const std::tr1::shared_ptr<const void>& data,
                        boost::uint64_t offset,
                        size_t len)
{
  if(!this->is_open()) {
    throw std::ios_base::failure("file is not open!!");
  }
  boost::uint64_t cur_offset = this->offset();

  if(lseek(this->fd, offset+this->header_size, SEEK_SET) < 0) {
    throw std::ios_base::failure("could not seek to correct file position.");
  }

  const char* bytes = static_cast<const char*>(data.get());
  ssize_t wr;
  size_t written=0;
  do {
    wr = ::write(this->fd, bytes+written, len-written);
    if(wr < 0 && errno != EINTR) {
      throw std::ios_base::failure("write failure.");
    }
    written += wr;
  } while(written < len && errno == EINTR);

  /* again, don't bother checking for failure; we'll detect on the next IOp. */
  lseek(this->fd, cur_offset, SEEK_SET);
}

bool LargeFileFD::is_open() const
{
  return this->fd != -1;
}

void LargeFileFD::close()
{
  if(!this->is_open()) { return; }

  int cl;
  do {
    cl = ::close(this->fd);
  } while(cl == -1 && errno == EINTR);
  assert(errno != EBADF);
  if(cl == -1 && errno == EBADF) {
    throw std::invalid_argument("invalid file descriptor");
  } else if(cl == -1 && errno == EIO) {
    throw std::runtime_error("I/O error.");
  }
  this->fd = -1;
}

void LargeFileFD::open(std::ios_base::openmode mode)
{
  if(this->is_open()) {
    this->close();
  }
  int access = O_RDONLY;
  if(mode & std::ios_base::in) {
    access = O_RDONLY;
  } else {
    access = O_RDWR | O_CREAT;
  }

  this->fd = ::open(this->m_filename.c_str(), access,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if(this->fd == -1) {
    throw std::ios_base::failure("Could not open file.");
  }
}
