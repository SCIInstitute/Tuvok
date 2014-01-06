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

LargeFileMMap::LargeFileMMap(const std::string fn,
                             std::ios_base::openmode mode,
                             uint64_t header_size,
                             uint64_t length) :
  LargeFileFD(fn, mode, header_size), map(NULL), length(length)
{
  this->open(mode);
}
LargeFileMMap::LargeFileMMap(const std::wstring fn,
                             std::ios_base::openmode mode,
                             uint64_t header_size,
                             uint64_t length) :
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

std::shared_ptr<const void> LargeFileMMap::rd(uint64_t offset,
                                                   size_t len)
{
  if(len == 0 || (!this->is_open())) {
    return std::shared_ptr<const void>();
  }

  const char* bytes = static_cast<const char*>(this->map);
  const char* begin = bytes + this->header_size + offset;
  const char* end = bytes+offset+this->header_size+len;
  const char* end_buffer = bytes + this->length;
  // what if they try to read beyond the end of the buffer?
  if(end > end_buffer) { end = end_buffer; }

  /* Our shared_ptr here is a bit sketch.  We give them a pointer, but if
   * 'this' dies we'll kill the mmap, thereby invalidating the pointer.  So,
   * umm.. don't do that. */
#ifdef __clang__
  // Clang has an issue in its libc++ where an allocator is created using the
  // type of the first parameter (in this case, begin), and if the type is
  // 'const', then duplicate member functions will be created in std::allocator.
  // In this case, the address function:
  // const char * address ...
  // const const char * address ... (this gets reduced to the above, presumably)
  std::shared_ptr<const void> mem(const_cast<char*>(begin), 
                                  nonstd::null_deleter());
#else
  std::shared_ptr<const void> mem(begin, nonstd::null_deleter());
#endif

  this->bytes_read = end - begin;
  /* Note that we don't actually use the 'length' parameter.  We really can't.
   * It's up to the user to make sure they don't go beyond the length they gave
   * us... */
  return mem;
}

void LargeFileMMap::wr(const std::shared_ptr<const void>& data,
                       uint64_t offset, size_t len)
{
  if(len == 0) { return; }
  if(!this->is_open()) {
    throw std::invalid_argument("file is not open.");
  }

  if(this->filesize() < offset+this->header_size+len) {
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
  if(LargeFileFD::is_open()) {
    LargeFileFD::close();
  }
  LargeFileFD::open(mode);

#if _POSIX_C_SOURCE >= 200112L
  /* if they're going to write... make sure the disk space exists.  This should
   * help keep the file contiguous on disk. */
  if(mode & std::ios_base::out) {
    posix_fallocate(this->fd, 0, this->length+this->header_size);
  }
#endif

  int mmap_prot = PROT_READ, mmap_flags = MAP_SHARED;
  if(mode & std::ios_base::out) {
    mmap_prot = PROT_WRITE;
    mmap_flags = MAP_SHARED;
  } else if(mode & std::ios_base::in) {
    mmap_prot = PROT_READ;
    mmap_flags = MAP_SHARED; /* MAP_PRIVATE? */
  }

  // if the file is empty... we can't map it anyway.  Just close it and bail.
  if(this->filesize() == 0) {
    LargeFileFD::close();
    return;
  }

  /* are we opening the file read only?  Truncate the length down to the file
   * size, then -- mapping less memory is easier on the kernel. */
  if(mode & std::ios_base::in && !(mode & std::ios_base::out)) {
    this->length = std::min(this->length, this->filesize());
  }

  /* length must be a multiple of the page size.  Round it up. */
  const long page_size = sysconf(_SC_PAGESIZE);
  const uint64_t u_page_size = static_cast<uint64_t>(page_size);
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
