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
LargeFileFD::LargeFileFD(const std::wstring fn,
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

std::tr1::shared_ptr<const void> LargeFileFD::rd(boost::uint64_t offset,
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
    std::tr1::shared_ptr<char>(new char[len], nonstd::DeleteArray<char>());
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

  this->bytes_read = completed;
  // we need to cast it to a 'const void' to return it...
  std::tr1::shared_ptr<const void> mem =
    std::tr1::static_pointer_cast<const void>(data);
  return mem;
}

void LargeFileFD::wr(const std::tr1::shared_ptr<const void>& data,
                     boost::uint64_t offset,
                     size_t len)
{
  if(!this->is_open()) {
    throw std::ios_base::failure("file is not open!!");
  }

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
}

void LargeFileFD::enqueue(boost::uint64_t offset, size_t len)
{
  if(len == 0) { return; }
#if _POSIX_C_SOURCE >= 200112L
  int adv = posix_fadvise(this->fd, offset, len, POSIX_FADV_WILLNEED);
  // this should basically always succeed.  the only way it can fail is if we
  // gave it a bogus FD or something.  if that's the case, that points to
  // either a programming error or some sort of nasty memory corruption.
  switch(adv) {
    case 0: /* nothing, good. */ break;
    case EBADF: throw std::out_of_range("bad file descriptor."); break;
    case EINVAL: throw std::invalid_argument("bad argument"); break;
    case ESPIPE: throw std::domain_error("fd refers to a pipe?!"); break;
  }
#else
  (void) offset;
  (void) len;
#endif
}

boost::uint64_t LargeFileFD::filesize() const
{
  struct stat st;
  if(stat(m_filename.c_str(), &st) == -1) {
    switch(errno) {
      case EACCES: throw std::ios::failure("no search permission."); break;
      case EIO: throw std::ios::failure("I/O error."); break;
      case ELOOP: throw std::ios::failure("loop in symbolic links."); break;
      case ENAMETOOLONG: throw std::ios::failure("name too long"); break;
      case ENOENT: throw std::ios::failure("file does not exist."); break;
      case EOVERFLOW: throw std::overflow_error("need more bits."); break;
    }
    return 0;
  }

  return static_cast<boost::uint64_t>(st.st_size);
}

bool LargeFileFD::is_open() const
{
  return this->fd != -1;
}

void LargeFileFD::close()
{
  if(!LargeFileFD::is_open()) { return; }

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
  if(LargeFileFD::is_open()) {
    this->close();
  }
  int access = O_RDONLY;
  if(mode & std::ios_base::out) {
    access = O_RDWR | O_CREAT;
  } else {
    access = O_RDONLY;
  }

  this->fd = ::open(this->m_filename.c_str(), access,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if(this->fd == -1) {
    throw std::ios_base::failure("Could not open file.");
  }
}

void LargeFileFD::truncate(boost::uint64_t len) {
  if(this->fd == -1) {
    LargeFile::truncate(this->m_filename.c_str(), len);
    return;
  }
  if(ftruncate(this->fd, len) != 0) {
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
      case EBADF: throw std::ios::failure("invalid file descriptor"); break;
    }
  }
  // move the offset down if it's beyond EOF.
  this->byte_offset = std::min(this->byte_offset, len);
}
