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


LargeFileMMap::LargeFileMMap(const std::string fn,
                             std::ios_base::openmode mode,
                             boost::uint64_t header_size,
                             boost::uint64_t length) :
  LargeFileFD(fn, mode, header_size), map(NULL), length(length)
{
  this->open(mode);
}

LargeFileMMap::~LargeFileMMap()
{
  /* change the memory protection, if we can.  We might have given away
   * pointers (on say a read() call) which won't be valid anymore, since we're
   * destroying the map.
   * With this, at least the user will get a segfault, instead of silently
   * reading garbage data. */
  mprotect(this->map, this->length, PROT_NONE);
  this->close();
}

std::tr1::shared_ptr<const void> LargeFileMMap::read(boost::uint64_t offset,
                                                     size_t len)
{
  if(len == 0 || (!this->is_open())) {
    return std::tr1::shared_ptr<const void>();
  }

  /* Our shared_ptr here is a bit sketch.  We give them a pointer, but if
   * 'this' dies we'll kill the mmap, thereby invalidating the pointer.  So,
   * umm.. don't do that. */
  const char* bytes = static_cast<const char*>(this->map);
  std::tr1::shared_ptr<const void> mem(bytes+this->header_size+offset,
                                       null_deleter());

  /* Note that we don't actually use the 'length' parameter.  We really can't.
   * It's up to the user to make sure they don't go beyond the length they gave
   * us... */
  return mem;
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

void LargeFileMMap::open(std::ios_base::openmode mode)
{
  LargeFileFD::open(mode);

#if _POSIX_C_SOURCE >= 200112L
  /* if they're going to write... make sure the disk space exists.  This should
   * help keep the file contiguous on disk. */
  if(mode & std::ios_base::out) {
    posix_fallocate(this->fd, 0, this->length+this->header_size);
  }
#endif

  int mmap_prot = PROT_READ, mmap_flags = MAP_SHARED;
  if(mode & std::ios_base::in) {
    mmap_prot = PROT_READ;
    mmap_flags = MAP_SHARED; /* MAP_PRIVATE? */
  } else if(mode & std::ios_base::out) {
    mmap_prot = PROT_WRITE;
    mmap_flags = MAP_SHARED;
  }

  /* are we opening the file read only?  Truncate the length down to the file
   * size, then -- mapping less memory is easier on the kernel. */
  if(mode & std::ios_base::in) {
    this->length = std::min(this->length, filesize(this->fd));
  }

  /* length must be a multiple of the page size.  Round it up. */
  const long page_size = sysconf(_SC_PAGESIZE);
  const boost::uint64_t u_page_size = static_cast<boost::uint64_t>(page_size);
  if(page_size != -1 && (this->length % u_page_size) != 0) {
    /* i.e. sysconf was successful and length isn't a multiple of page size. */
    this->length += (u_page_size * ((this->length-1) / u_page_size)) +
                    (u_page_size - this->length);
    assert((this->length % u_page_size) == 0);
  }
  assert(this->length > 0);

  // what header size did they request?  Maybe we can just offset our
  // mmap a bit.  This is generally good because some mmap's don't like
  // the length of the map to get too long.
  off_t begin = 0;
  if(this->byte_offset > u_page_size) {
    begin = this->byte_offset / u_page_size;
    this->byte_offset = this->byte_offset % u_page_size;
  }

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
  return LargeFileFD::is_open() && this->map != NULL;
}

void LargeFileMMap::close()
{
  if(!this->is_open()) { return; }
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

  LargeFileFD::close();
}
