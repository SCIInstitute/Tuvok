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
#include "StdDefines.h"

#include <cassert>
#include <cstdio>
#include <errno.h>
#include <stdexcept>
#if _POSIX_C_SOURCE >= 200112L
# include <sys/types.h>
#endif
#ifdef _MSC_VER
# include <io.h>
#endif
#ifndef NDEBUG
# include <iostream>
# define DEBUG(...) do { std::cerr << __VA_ARGS__ << "\n"; } while(0)
#else
# define DEBUG(...) do { /* nothing, debug msg removed. */ } while(0)
#endif
#if defined(DETECTED_OS_APPLE) || defined(DETECTED_OS_LINUX)
# include <unistd.h>
#endif
#include "LargeFileC.h"

LargeFileC::LargeFileC(const std::string fn,
                       std::ios_base::openmode mode,
                       uint64_t header_size,
                       uint64_t /* length */) :
  LargeFile(fn, mode, header_size), fp(NULL)
{
  this->open(mode);
}
LargeFileC::LargeFileC(const std::wstring fn,
                       std::ios_base::openmode mode,
                       uint64_t header_size,
                       uint64_t /* length */) :
  LargeFile(fn, mode, header_size), fp(NULL)
{
  this->open(mode);
}

LargeFileC::~LargeFileC()
{
  if(this->is_open()) {
    this->close();
  }
}

int seeko(FILE* strm, uint64_t off, int whence) {
#if _POSIX_C_SOURCE >= 200112L
  if(fseeko(strm, off, whence) < 0) {
#else
  if(fseek(strm, static_cast<long>(off), whence) < 0) {
#endif
    DEBUG("seek failed, errno=" << errno);
    return -1;
  }
  return 0;
}

std::shared_ptr<const void> LargeFileC::rd(uint64_t offset,
                                                size_t len)
{
  if(!this->is_open()) {
    throw std::ios_base::failure("file is not open!");
  }

  if(seeko(this->fp, offset+this->header_size, SEEK_SET) < 0) {
    throw std::ios_base::failure("could not seek to correct file position");
  }

  std::shared_ptr<char> data(new char[len], nonstd::DeleteArray<char>());

  size_t nitems = fread(data.get(), 1, len, this->fp);
  this->bytes_read = nitems;

  // we need to cast it to a 'const void' to return it...
  std::shared_ptr<const void> mem =
    std::static_pointer_cast<const void>(data);
  return mem;
}

void LargeFileC::wr(const std::shared_ptr<const void>& data,
                    uint64_t offset,
                    size_t len)
{
  if(!this->is_open()) {
    throw std::ios_base::failure("file is not open!");
  }

  if(seeko(this->fp, offset+this->header_size, SEEK_SET) < 0) {
    throw std::ios_base::failure("seek failed!");
  }

  if(fwrite(data.get(), len, 1, this->fp) <= 0) {
    std::ios_base::failure("write failed.");
  }
}

void LargeFileC::enqueue(uint64_t, size_t)
{
  /* unimplemented... */
}

static uint64_t offs(FILE* fp) {
#if _POSIX_C_SOURCE >= 200112L
  return static_cast<uint64_t>(ftello(fp));
#else
  return static_cast<uint64_t>(ftell(fp));
#endif
}

uint64_t LargeFileC::filesize() const
{
  if(!this->is_open()) {
    throw std::ios_base::failure("file is not open");
  }

  const uint64_t current = offs(this->fp); // save so we can reset
  seeko(this->fp, 0, SEEK_END);
  uint64_t end = offs(this->fp);
  seeko(this->fp, 0, SEEK_SET);
  uint64_t begin = offs(this->fp);
  seeko(this->fp, current, SEEK_SET); // reset file pointer
  return end - begin;
}

bool LargeFileC::is_open() const { return this->fp != NULL; }

void LargeFileC::close()
{
  if(!LargeFileC::is_open()) { return; }

  if(fclose(this->fp) != 0) {
    assert(errno != EBADF);
    if(errno == EBADF) {
      throw std::invalid_argument("invalid file descriptor");
    }
    throw std::runtime_error("I/O error, writes might not have flushed!");
  }
  this->fp = NULL;
}

void LargeFileC::open(std::ios_base::openmode mode)
{
  if(this->is_open()) {
    this->close();
  }
  const char* omode = NULL;
  if((mode & std::ios::out) && (mode & std::ios::trunc)) {
    omode = "w+b";
  } else if(mode & std::ios::out) {
    omode = "r+b";
  } else {
    omode = "rb";
  }

  this->fp = fopen(this->m_filename.c_str(), omode);
  if(this->fp == NULL) {
    throw std::ios_base::failure("Could not open file.");
  }
}

int filenumber(FILE* strm) {
#ifdef _MSC_VER
  return _fileno(strm);
#else
  return fileno(strm);
#endif
}

int lftruncate(int fd, uint64_t length) {
#ifdef _MSC_VER
  return _chsize(fd, length);
#else
  return ftruncate(fd, length);
#endif
}

void LargeFileC::truncate(uint64_t len) {
  if(!this->is_open()) {
    this->open(std::ios::out);
  }
  int fno = filenumber(this->fp);

  if(lftruncate(fno, len) != 0) {
    DEBUG("truncate to len=" << len << " failed, errno=" << errno);
    throw std::ios::failure("truncate failed");
  }

  // move the offset down if it's beyond EOF.
  this->byte_offset = std::min(this->byte_offset, len);
}
