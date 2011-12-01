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
#include <aio.h>
#include <assert.h>
#include <cstring>
#include <errno.h>
#ifndef NDEBUG
# include <iostream>
# define DEBUG(s) do { std::cerr << s << "\n"; } while(0)
#else
# define DEBUG(s) do { /* nothing, debug msg removed. */ } while(0)
#endif
#include <vector>
#include "LargeFileAIO.h"

/// searches for a request among a list of outstanding AIO ops.  The given
/// offset is intended to be exact (i.e. this won't apply header offsets).
static struct aiocb* find_request(const LargeFileAIO::Reqs& reqs,
                                  boost::uint64_t offset, size_t len);

LargeFileAIO::LargeFileAIO(const std::string fn,
                           std::ios_base::openmode mode,
                           boost::uint64_t header_size,
                           boost::uint64_t /* length */) :
  LargeFileFD(fn, mode, header_size), writes_copied(true)
{
  this->open(mode);
}

LargeFileAIO::~LargeFileAIO()
{
  this->close();
}

namespace {
  template<typename T> struct DeleteArray {
    void operator()(const T* t) const { delete[] t; }
  };
  // hack.  This only works here because we know we are always allocating
  // bytes, even though our interface requires us to shove them into a const
  // void*.
  template<> struct DeleteArray<const void> {
    void operator()(const void* t) const {
      delete[] static_cast<const char*>(t);
    }
  };
}

std::tr1::shared_ptr<const void> LargeFileAIO::read(boost::uint64_t offset,
                                                    size_t len)
{
  struct aiocb *cb;

  // first, try to find this request among our list of outstanding requests.
  const boost::uint64_t real_offset = offset + this->header_size;
  cb = find_request(this->control, real_offset, len);
  if(NULL == cb) {
    if((cb = submit_new_request(real_offset, len)) == NULL) {
      DEBUG("could not submit new request."); // .. so just bail.
      return std::tr1::shared_ptr<const void>();
    }
  }
  const struct aiocb* const cblist[1] = { cb };
  aio_suspend(cblist, 1, NULL);

  assert(aio_error(cb) == 0);
#ifndef NDEBUG
  ssize_t bytes = aio_return(cb);
  assert(bytes == static_cast<ssize_t>(len));
#else
  aio_return(cb);
#endif
  // both our control block and our data buffer are dynamically allocated.
  // Make sure they'll both get freed up; control block now, and the data will
  // be the return shared_ptr, so it's that ptr's responsibility to clean it
  // up.
  std::tr1::shared_ptr<const void> mem = this->control.find(cb)->second;
  delete cb;
  this->control.erase(cb);
  return mem;
}

void LargeFileAIO::write(const std::tr1::shared_ptr<const void>& data,
                         boost::uint64_t offset,
                         size_t len)
{
  if(len == 0) { return; }

  flush_writes(); // only allow one pending write..

  struct aiocb* cb = new struct aiocb;
  ::memset(cb, 0, sizeof(struct aiocb));

  cb->aio_fildes = this->fd;
  cb->aio_offset = offset + this->header_size;
  cb->aio_nbytes = len;

  std::tr1::shared_ptr<const void> saved_data;
  if(writes_copied) {
    // allocate a new buffer and hold on to that.
    int8_t* buf = new int8_t[len];
    ::memcpy(buf, data.get(), len);
    saved_data = std::tr1::shared_ptr<const void>(buf);
  } else {
    saved_data = std::tr1::shared_ptr<const void>(data);
  }
  // cast away the const.  We know the AIO op won't modify the data anyway --
  // it's a write operation.
  cb->aio_buf = const_cast<void*>(saved_data.get());
  {
    struct sigevent sigev;
    ::memset(&sigev, 0, sizeof(struct sigevent));
    sigev.sigev_notify = SIGEV_NONE;
    cb->aio_sigevent = sigev;
  }
  cb->aio_lio_opcode = LIO_WRITE;

  if(aio_write(cb) != 0) {
    DEBUG("aio_write failed, errno=" << errno);
    switch(errno) {
      case EAGAIN:
        throw std::ios_base::failure("temporary lack of resources");
        break;
      case ENOSYS:
        throw std::ios_base::failure("AIO not implemented on this platform.");
        break;
      case EBADF:
        throw std::ios_base::failure("invalid file descriptor.");
        break;
      case EINVAL:
        throw std::ios_base::failure("offset or reqprio was invalid.");
        break;
    }
    return;
  }

  // make sure we hold on to the memory.
  this->control.insert(std::make_pair(cb, saved_data));
}

static void wait_on(struct aiocb* cb) {
  const struct aiocb* const cblist[1] = { cb };
  int rv = 0;
  do {
    rv = aio_suspend(cblist, 1, NULL);
  } while(rv == -1 && errno == EINTR);
  assert(rv == 0);
  rv = aio_error(cb);
  assert(rv != EINPROGRESS);
  assert(rv != ECANCELED);
  rv = aio_return(cb);
  assert(rv != EINVAL);
}

void LargeFileAIO::copy_writes(bool copy) { this->writes_copied = copy; }

void LargeFileAIO::close()
{
  if(!this->is_open()) { return; }

  // if there were pending reads... who cares.  But pending writes?  We need to
  // wait on those and make sure they finish.
  this->flush_writes();

  // okay.  anything left in there isn't important.  just cancel all of 'em.
  //aio_cancel(this->fd, NULL);

  // all of those aiocb* were dynamically allocated.  free them up.
  for(Reqs::const_iterator i = this->control.begin();
      i != this->control.end(); ++i) {
    delete i->first;
  }
  // we don't worry about the shared_ptr's -- they'll clean themselves up
  // automagically.
  this->control.clear();
  LargeFileFD::close();
}

struct aiocb* LargeFileAIO::submit_new_request(boost::uint64_t offset,
                                               size_t len)
{
  struct aiocb* cb = new struct aiocb;
  ::memset(cb, 0, sizeof(struct aiocb));

  cb->aio_fildes = this->fd;
  cb->aio_offset = offset;
  std::tr1::shared_ptr<void> data(new int8_t[len]);
  cb->aio_buf = data.get();
  cb->aio_nbytes = len;
  {
    struct sigevent sigev;
    ::memset(&sigev, 0, sizeof(struct sigevent));
    sigev.sigev_notify = SIGEV_NONE;
    cb->aio_sigevent = sigev;
  }
  cb->aio_lio_opcode = LIO_READ;

  if(aio_read(cb) == -1) {
    delete cb;
    switch(errno) {
      case EAGAIN:
        throw std::ios_base::failure("temporary lack of resources.");
        break;
      case ENOSYS:
        throw std::ios_base::failure("AIO not implemented on this platform");
        break;
      case EBADF:
        throw std::ios_base::failure("invalid file descriptor.");
        break;
      case EINVAL:
        throw std::ios_base::failure("offset or reqprio is invalid.");
        break;
    }
    return NULL;
  }
  this->control.insert(std::make_pair(cb, data));
  return cb;
}

/// searches for a request among a list of outstanding AIO ops.  The given
/// offset is intended to be exact (i.e. this won't apply header offsets).
static struct aiocb*
find_request(
  const LargeFileAIO::Reqs& reqs,
  boost::uint64_t offset,
  size_t len)
{
  for(LargeFileAIO::Reqs::const_iterator i = reqs.begin(); i != reqs.end(); ++i)
  {
    if(static_cast<boost::uint64_t>(i->first->aio_offset) == offset &&
       i->first->aio_nbytes == len) {
      return i->first;
    }
  }
  return NULL;
}

// flushes any pending writes by waiting on them.
void LargeFileAIO::flush_writes()
{
  Reqs::const_iterator i = this->control.begin();
  while(i != this->control.end()) {
    if(i->first->aio_lio_opcode == LIO_WRITE) {
      wait_on(i->first);
      delete i->first;
      i = this->control.erase(i);
    } else {
      ++i;
    }
  }
}
