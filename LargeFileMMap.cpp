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
#define _POSIX_C_SOURCE 200112L
#include "LargeFileMMap.h"

#include <algorithm>
#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

// a 'delete' functor that just does nothing.
struct null_deleter { void operator()(const void*) const {} };

LargeFileMMap::LargeFileMMap(const std::string fn,
                             std::ios_base::openmode mode,
                             boost::uint64_t header_size,
                             boost::uint64_t length) :
  LargeFile(fn, header_size), fd(-1), map(NULL), length(length)
{
  this->open(fn.c_str(), mode);
}

std::tr1::weak_ptr<const void> LargeFileMMap::read(boost::uint64_t offset,
                                                   size_t len)
{
  if(len == 0 || (!this->is_open())) {
    return std::tr1::weak_ptr<void>();
  }

  /* We'll create a shared_ptr to the data they want, and then give them a
   * weak_ptr.  When we die, we'll kill all our shared_ptr's.  So the pointer
   * they get is only valid while this object is alive.
   * This way, if this object goes out of scope, instead of all the pointers
   * instantly becoming invalid... they just can't be lock()ed. */
  const char* bytes = static_cast<const char*>(this->map);
  std::tr1::shared_ptr<const void> mem(bytes+this->header_size+offset,
                                       null_deleter());
  this->memory.push_front(mem);

  /* Note that we don't actually use the 'length' parameter.  We really can't.
   * It's up to the user to make sure they don't go beyond the length they gave
   * us... */
  return std::tr1::weak_ptr<const void>(mem);
}

std::tr1::weak_ptr<const void> LargeFileMMap::read(size_t length)
{
  return this->read(this->offset, length);
}

static uint64_t filesize(int fd) {
  off_t original = lseek(fd, 0, SEEK_CUR);
  if(original == -1) {
    throw std::runtime_error("could not get current file position");
  }
  off_t sz = lseek(fd, 0, SEEK_END);
  if(sz == -1) {
    throw std::runtime_error("could not get file size");
  }
  if(lseek(fd, original, SEEK_SET) == -1) {
    throw std::runtime_error("could not reset file position.");
  }
  return static_cast<uint64_t>(sz);
}

void LargeFileMMap::write(const std::tr1::shared_ptr<const void>& data,
                          boost::uint64_t offset, size_t len)
{
  if(len == 0) { return; }
  if(!this->is_open()) {
    throw std::invalid_argument("file is not open.");
  }

  if(filesize(fd) < offset+this->header_size+len) {
    /* extend the file because it is too small -- mmap cannot make files
     * larger. */
    if(ftruncate(fd, offset+this->header_size+len) != 0) {
      assert(errno == 0 && "ftruncate failed");
      throw std::runtime_error("could not extend file.");
    }
  }

  const char* bytes = static_cast<const char*>(data.get());
  char* dest_bytes = static_cast<char*>(this->map);
  std::copy(bytes, bytes + len,
            dest_bytes + this->header_size + offset);
}

void LargeFileMMap::open(const char *file, std::ios_base::openmode mode)
{
  if(file) {
    this->m_filename = std::string(file);
  }
  if(this->is_open()) {
    this->close();
  }

  int mmap_prot = PROT_READ, mmap_flags = MAP_SHARED;
  int access = O_RDONLY;
  using namespace std;
  if(mode & std::ios_base::in) {
    access = O_RDONLY;
    mmap_prot = PROT_READ;
    mmap_flags = MAP_SHARED; /* MAP_PRIVATE? */
  } else if(mode & std::ios_base::out) {
    /* even if we will only write and not read, mmap requires we open with
     * O_RDWR when using PROT_WRITE. */
    access = O_RDWR | O_CREAT;
    mmap_prot = PROT_WRITE;
    mmap_flags = MAP_SHARED;
  }

  this->fd = ::open(this->m_filename.c_str(), access,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if(this->fd == -1) {
    return;
  }

  /* length must be a multiple of the page size.  Round it up. */
  const long page_size = sysconf(_SC_PAGESIZE);
  const boost::uint64_t u_page_size = static_cast<boost::uint64_t>(page_size);
  if(page_size != -1 && (this->length % u_page_size) != 0) {
    /* i.e. sysconf was successful and length isn't a multiple of page size. */
    this->length += (length % u_page_size);
  }
  assert((this->length % u_page_size) == 0);

  /* Hack.  This should actually be enabled anytime a system supports posix
   * 2001... which Macs don't of course. */
#ifdef DETECTED_OS_LINUX
  /* if they're going to write... make sure the disk space exists.  This should
   * help keep the file contiguous on disk. */
  if(mode | std::ios_base::out) {
    posix_fallocate(this->fd, 0, this->length+this->header_size);
  }
#endif

  off_t begin = static_cast<off_t>(this->header_size);
  this->map = mmap(NULL, static_cast<size_t>(this->length), mmap_prot,
                   mmap_flags, this->fd, begin);
  if(MAP_FAILED == this->map) {
    assert(this->map != MAP_FAILED);
    this->close();
    return;
  }
}

bool LargeFileMMap::is_open() const
{
  return this->fd != -1 && this->map != NULL;
}

void LargeFileMMap::close()
{
  if(this->map) {
    int mu = munmap(this->map, static_cast<size_t>(this->length));
    /* the only real errors that can occur here are programming errors; us not
     * properly maintaining "length", for example. */
    assert(mu == 0 && "munmap can only fail due to a programming error.");
    if(mu != 0) {
      throw std::invalid_argument("could not unmap file; writes probably did "
                                  "not propagate.");
    }
  }
  this->map = NULL;

  if(this->fd != -1) {
    int cl;
    do {
      cl = ::close(this->fd);
    } while (cl == -1 && errno == EINTR);
    assert(errno != EBADF);
    if(errno == EBADF) {
      throw std::invalid_argument("invalid file descriptor");
    } else if(errno == EIO) {
      throw std::runtime_error("I/O error.");
    }
  }
  this->fd = -1;
}
