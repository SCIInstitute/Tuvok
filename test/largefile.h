#include <cstdlib>
#include <algorithm>
#include <functional>
#include <vector>
#include <boost/cstdint.hpp>

#include <cxxtest/TestSuite.h>

#include "LargeFileAIO.h"
#include "LargeFileC.h"
#include "LargeFileFD.h"
#include "LargeFileMMap.h"

#include "util-test.h"

// create a temporary file with a constant value.
namespace {
  template <typename T>
  std::string tmp_constant(T constant, size_t len) {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);

    gen_constant<T>(ofs, len, constant);
    ofs.close();
    return tmpf;
  }

  // verifies 'open' works without testing anything else... if this is broken,
  // there's no use looking at other tests until it's fixed.
  template<class T> void lf_generic_open() {
    const std::string tmpf = tmp_constant<uint64_t>(86, 42);

    T lf(tmpf, std::ios::in, 0);
    if(!lf.is_open()) {
      TS_FAIL("Could not open file at all.");
    }
    remove(tmpf.c_str());
  }
}

namespace {
  // uses standard ostreams to write a file with a constant value in it, then
  // reads the file with the class under test.  Verifies that reads work
  // without relying on any write functionality in the class.
  template<class T> void lf_generic_read() {
    const uint64_t cval = 86;
    const size_t len = 42;
    const std::string tmpf = tmp_constant<uint64_t>(cval, len);

    T lf(tmpf, std::ios::in);
    if(!lf.is_open()) {
      TS_FAIL("Could not open file at all.");
      return;
    }
    std::tr1::shared_ptr<const void> mem = lf.rd(0, len * sizeof(uint64_t));
    const uint64_t* data = static_cast<const uint64_t*>(mem.get());
    assert(data != NULL);
    for(size_t i=0; i < len; ++i) {
      TS_ASSERT_EQUALS(data[i], cval);
    }

    remove(tmpf.c_str());
  }
}

static int64_t generate_constant(int64_t val) { return val; }
static void null_deleter(void*) {}

namespace {
  // basic write test.  Write 'N' elements, read them back and make sure
  // they're what we just wrote.
  template<class T> void lf_generic_write() {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    ofs.close();

    const size_t N = 64;
    const int64_t VALUE = -42;
    { /* write */
      T lf(tmpf, std::ios::out, 0, sizeof(int64_t)*N);
      int64_t data[N];
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE));
      lf.wr(std::tr1::shared_ptr<const void>(data, null_deleter), 0,
                                             sizeof(int64_t)*N);
      lf.close();
    }
    MESSAGE("Write complete, closed.  Starting read.");
    {
      T lf(tmpf, std::ios::in);
      std::tr1::shared_ptr<const void> mem = lf.rd(0, N*sizeof(int64_t));
      const int64_t* data = static_cast<const int64_t*>(mem.get());
      for(size_t i=0; i < N; ++i) {
        TS_ASSERT_EQUALS(data[i], VALUE);
      }
    }
    remove(tmpf.c_str());
  }
}

namespace {
  // tests write, but uses std streams to read it back; so doesn't rely on the
  // 'read' implementation.
  template<class T> void lf_generic_write_only() {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    ofs.close();

    const size_t N = 64;
    const int64_t VALUE = -42;
    { /* write */
      T lf(tmpf, std::ios::out, 0, sizeof(int64_t)*N);
      int64_t data[N];
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE));
      lf.wr(std::tr1::shared_ptr<const void>(data, null_deleter), 0,
                                             sizeof(int64_t)*N);
      lf.close();
    }
    {
      std::ifstream ifs(tmpf.c_str(), std::ios::in | std::ios::binary);
      ifs.exceptions(std::fstream::failbit | std::fstream::badbit);
      if(!ifs.is_open()) {
        TS_FAIL("Could not open the file we just wrote!");
        goto end;
      }
      int64_t elem;
      for(size_t i=0; i < N; ++i) {
        elem = 0; // reset elem so we'll know if the read failed.
        ifs.read(reinterpret_cast<char*>(&elem), sizeof(int64_t));
        TS_ASSERT_EQUALS(elem, VALUE);
      }
    }
    end: remove(tmpf.c_str());
  }
}

// make sure we respect header offsets.
namespace {
  template<class T> void lf_generic_header() {
    EnableDebugMessages dbg;
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    ofs.close();

    // we'll write N elements of VALUE[0], then N of VALUE[1].  Then we'll use
    // a header offset on read to jump into the middle of that.
    const size_t N = 64;
    const int64_t VALUE[2] = { -42, 42 };
    { /* write */
      T lf(tmpf, std::ios::out, 0, sizeof(int64_t)*N*2);

      int64_t data[N];
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[0]));
      lf.wr(std::tr1::shared_ptr<const void>(data, null_deleter), 0,
                                             sizeof(int64_t)*N);
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[1]));
      lf.wr(std::tr1::shared_ptr<const void>(data, null_deleter),
                                             sizeof(int64_t)*N,
                                             sizeof(int64_t)*N);
      lf.close();
    }
    { /* read.  offset so we see 1 VALUE[0] and N VALUE[1]'s. */
      T lf(tmpf, std::ios::in, sizeof(int64_t)*(N-1));
      std::tr1::shared_ptr<const void> mem = lf.rd(0, (N+1)*sizeof(int64_t));
      const int64_t* data = static_cast<const int64_t*>(mem.get());
      TS_ASSERT_EQUALS(data[0], VALUE[0]);
      for(size_t i=1; i < N+1; ++i) {
        TS_ASSERT_EQUALS(data[i], VALUE[1]);
      }
    }
    remove(tmpf.c_str());
  }
}

namespace {
  // tests with a large header.  Leaves a bunch of bytes at the head of the
  // file undefined, by using offsets to control where the writes happen.
  // Then read everything back using an offsetted file, ensure we got what we
  // wrote.
  template<class T> void lf_generic_large_header() {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    ofs.close();

    const size_t N = 64;
    const int64_t VALUE[2] = { -42, 42 };
    const size_t offset = 32768;
    { /* write. */
      T lf(tmpf, std::ios::out, 0, sizeof(int64_t)*N*2+offset);

      int64_t data[N];
      lf.seek(offset);
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[0]));
      lf.wr(std::tr1::shared_ptr<const void>(data, null_deleter), offset,
                                             sizeof(int64_t)*N);
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[1]));
      lf.wr(std::tr1::shared_ptr<const void>(data, null_deleter),
                                             sizeof(int64_t)*N + offset,
                                             sizeof(int64_t)*N);
      lf.close();
    }
    { /* now read.  We'll use a header offset so that we expect to see one
       * VALUE[0] and then N VALUE[1]s.*/
      T lf(tmpf, std::ios::in, offset);
      std::tr1::shared_ptr<const void> mem = lf.rd(0, 2*N*sizeof(int64_t));
      const int64_t* data = static_cast<const int64_t*>(mem.get());
      for(size_t i=0; i < N; ++i) {
        TS_ASSERT_EQUALS(data[i], VALUE[0]);
      }
      for(size_t i=N; i < 2*N; ++i) {
        TS_ASSERT_EQUALS(data[i], VALUE[1]);
      }
    }
    remove(tmpf.c_str());
  }
}

// tests AIO with the nocopy flag doing multiple writes.
void lf_aio_nocopy() {
  std::ofstream ofs;
  const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
  ofs.close();

  const size_t N = 64;
  const int64_t VALUE[3] = { -42, 42, 19 };
  const size_t offset = 32768;
  int64_t data[3][N];
  {
    LargeFileAIO lf(tmpf, std::ios::out, 0, sizeof(int64_t)*N*3+offset);
    lf.copy_writes(false);

    // write 3 arrays of data, each array is filled with VALUE[i]
    for(size_t i=0; i < 3; ++i) {
      std::generate(data[i], data[i]+N,
                    std::tr1::bind(generate_constant, VALUE[i]));
      lf.wr(std::tr1::shared_ptr<const void>(data[i], null_deleter),
                                             offset + i*N*sizeof(int64_t),
                                             sizeof(int64_t)*N);
    }
  }
  {
    std::ifstream ifs(tmpf.c_str(), std::ios::in | std::ios::binary);
    ifs.exceptions(std::fstream::failbit | std::fstream::badbit);
    if(!ifs.is_open()) {
      TS_FAIL("Could not open the file we just wrote!");
      goto end;
    }
    ifs.seekg(offset); // jump past the offset we should have skipped above
    int64_t elem;
    for(size_t j=0; j < 3; ++j) {
      for(size_t i=0; i < N; ++i) {
        elem = 0; // reset elem so we'll know if the read failed.
        ifs.read(reinterpret_cast<char*>(&elem), sizeof(int64_t));
        TS_ASSERT_EQUALS(elem, VALUE[j]);
      }
    }
  }
  end: remove(tmpf.c_str());
}

// does a bunch of writes followed by a read.  Hints that we'll need the read
// data soonish in between all those writes.
namespace {
  template<typename T> void lf_generic_enqueue() {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    ofs.close();

    const size_t N = 64;
    const int64_t VALUE[8] = { -42,42, 19,-6, 4,12, 24,9  };
    const size_t offset = 32768;
    T lf(tmpf, std::ios::in | std::ios::out, 0, sizeof(int64_t)*N*8+offset);

    for(size_t i=0; i < 8; ++i) {
      int64_t data[N];
      const boost::uint64_t this_offset = offset + i*(sizeof(int64_t)*N);
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[i]));
      lf.wr(std::tr1::shared_ptr<const void>(data, null_deleter),
                                             this_offset,
                                             sizeof(int64_t)*N);
      if(i == 5) {
        lf.enqueue(offset, sizeof(int64_t)*N);
      }
    }
    std::tr1::shared_ptr<const void> mem = lf.rd(offset, sizeof(int64_t)*N);
    const int64_t* data = static_cast<const int64_t*>(mem.get());
    for(size_t i=0; i < N; ++i) {
      TS_ASSERT_EQUALS(data[i], VALUE[0]);
    }
    remove(tmpf.c_str());
  }
}

// tests reopening a file r/w after opening in RO.
namespace {
  template<class T> void lf_generic_reopen() {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    ofs.close();

    const size_t N = 64;
    const int64_t VALUE = -42;
    T lf(tmpf, std::ios::in, 0, sizeof(int64_t)*N);

    {
      int64_t data[N];
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE));
      lf.open(std::ios::out);
      lf.wr(std::tr1::shared_ptr<const void>(data, null_deleter), 0,
                                             sizeof(int64_t)*N);
    }
    lf.close();

    T lfread(tmpf, std::ios::in, 0, sizeof(int64_t)*N);
    std::tr1::shared_ptr<const void> mem = lfread.rd(0, sizeof(int64_t)*N);
    const int64_t* data = static_cast<const int64_t*>(mem.get());
    for(size_t i=0; i < N; ++i) { TS_ASSERT_EQUALS(data[i], VALUE); }

    remove(tmpf.c_str());
  }
}

// tests to make sure 'truncate' works
static void lf_truncate() {
  std::ofstream ofs;
  const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);

  const size_t N = 64;
  const int64_t VALUE = -42;
  {
    int64_t data[N];
    std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE));
    ofs.write(reinterpret_cast<const char*>(data), sizeof(int64_t)*N);
  }

  ofs.close();
  LargeFile::truncate(tmpf.c_str(), 0);

  std::ifstream ifs(tmpf.c_str(), std::ios::in | std::ios::binary);
  {
    int64_t test;
    TS_ASSERT(!ifs.fail()); // stream should be okay.
    ifs.read(reinterpret_cast<char*>(&test), sizeof(int64_t));
    TS_ASSERT(ifs.fail()); // ... but that read should have broken it.
  }
}

// tests convenience read/write of a single value calls
namespace {
  template<class T> void lf_generic_rw_single() {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    ofs.close();

    boost::int8_t s8 = -7;
    boost::uint8_t u8 = 19;
    boost::int16_t s16 = 6;
    boost::uint16_t u16 = 74;
    boost::int32_t s32 = -15;
    boost::uint32_t u32 = 2048;
    boost::int64_t s64 = -21438907;
    boost::uint64_t u64 = 234987;
    float f = 9.81;
    double d = 4.242;

    {
      // just overestimate the size, it doesn't matter much anyway.
      T lf(tmpf, std::ios::out, 0, sizeof(int64_t)*128);
      lf.write(s8);
      lf.write(u8);
      lf.write(s16);
      lf.write(u16);
      lf.write(s32);
      lf.write(u32);
      lf.write(s64);
      lf.write(u64);
      lf.write(f);
      lf.write(d);
    }
    {
      // just overestimate the size, it doesn't matter much anyway.
      T lf(tmpf, std::ios::out, 0, sizeof(int64_t)*128);
      s8 = u8 = s16 = u16 = s32 = u32 = s64 = u64 = 0;
      d = f = 0.0f;
      lf.read(&s8); lf.read(&u8);
      lf.read(&s16); lf.read(&u16);
      lf.read(&s32); lf.read(&u32);
      lf.read(&s64); lf.read(&u64);
      lf.read(&f); lf.read(&d);
      TS_ASSERT_EQUALS(s8, -7);
      TS_ASSERT_EQUALS(u8, 19);
      TS_ASSERT_EQUALS(s16,  6);
      TS_ASSERT_EQUALS(u16, 74);
      TS_ASSERT_EQUALS(s32, -15);
      TS_ASSERT_EQUALS(u32, 2048UL);
      TS_ASSERT_EQUALS(s64, -21438907);
      TS_ASSERT_EQUALS(u64, 234987ULL);
      TS_ASSERT_DELTA(f, 9.81, 0.0001);
      TS_ASSERT_DELTA(d, 4.242, 0.0001);
    }
    remove(tmpf.c_str());
  }
}

// test truncate actually cutting off some values
namespace {
  template<class T> void lf_generic_truncate() {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    ofs.close();

    const size_t N = 64;
    const int64_t VALUE[] = { -42, 96, 67 };
    { // write N VALUE[0]'s, N VALUE[1]'s, then truncate down to two values.
      T lf(tmpf, std::ios::out, 0, sizeof(int64_t)*128);
      int64_t data[N];
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[0]));
      lf.write(data, N);
      {
        int64_t d[N];
        std::generate(d, d+N, std::tr1::bind(generate_constant, VALUE[1]));
        lf.write(d, N);
      }
      lf.close();
      lf.truncate(sizeof(int64_t)*2);
    }
    { // ensure the filesize is correct (truncate did something)
      std::ifstream ifs(tmpf.c_str(), std::ios::in | std::ios::binary);
      ifs.seekg(0, std::ios::end);
      TS_ASSERT_EQUALS(static_cast<uint64_t>(ifs.tellg()), sizeof(int64_t)*2);
    }
    { // append N VALUE[2]'s
      T lf(tmpf, std::ios::out, 0, sizeof(int64_t)*128);
      int64_t data[N];
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[2]));
      lf.seek(sizeof(int64_t)*2);
      lf.write(data, N);
    }
    { // now make sure we have 2 VALUE[0]'s and N VALUE[2]'s, no VALUE[1]'s!
      const boost::uint64_t len = sizeof(int64_t)*(N+2);
      T lf(tmpf, std::ios::in, 0, len);
      std::tr1::shared_ptr<const void> mem = lf.rd(0, len);
      const int64_t* data = static_cast<const int64_t*>(mem.get());
      TS_ASSERT_EQUALS(data[0], VALUE[0]);
      TS_ASSERT_EQUALS(data[1], VALUE[0]);
      for(size_t i=2; i < N+2; ++i) {
        // should be no VALUE[1]'s; we truncated that.
        TS_ASSERT_EQUALS(data[i], VALUE[2]);
      }
    }
    remove(tmpf.c_str());
  }
}

// ensures writing increases the offset as it should.
namespace {
  template<class T> void lf_generic_wroffset() {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    ofs.close();
    const size_t N = 64;
    const int64_t VALUE[] = { -42, 96, 67 };
    { // write N VALUE[0]'s, N VALUE[1]'s, then truncate down to two values.
      T lf(tmpf, std::ios::out, 0, sizeof(int64_t)*128);
      TS_ASSERT_EQUALS(lf.offset(), static_cast<boost::uint64_t>(0));
      int64_t data[N];
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[0]));
      lf.write(data, N);
      TS_ASSERT_EQUALS(lf.offset(), sizeof(int64_t)*N);
      lf.write(data[0]);
      TS_ASSERT_EQUALS(lf.offset(), sizeof(int64_t)*(N+1));
      lf.seek(lf.offset()-sizeof(int64_t));
      TS_ASSERT_EQUALS(lf.offset(), sizeof(int64_t)*N);
      lf.seek(0);
      TS_ASSERT_EQUALS(lf.offset(), static_cast<boost::uint64_t>(0));
    }
    remove(tmpf.c_str());
  }
}

// ensures reading increases the offset as it should.
namespace {
  template<class T> void lf_generic_rdoffset() {
    std::ofstream ofs;
    const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
    const size_t N = 64;
    const int64_t VALUE[] = { -42 };
    {
      int64_t data[N];
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[0]));
      ofs.write(reinterpret_cast<const char*>(data), sizeof(int64_t)*N);
    }
    ofs.close();
    T lf(tmpf, std::ios::in, 0, sizeof(int64_t)*128);
    TS_ASSERT_EQUALS(lf.offset(), static_cast<boost::uint64_t>(0));
    const size_t len = sizeof(int64_t)*(N/2);
    lf.rd(len);
    TS_ASSERT_EQUALS(lf.offset(), sizeof(int64_t)*(N/2));
    {
      int64_t data[N/2];
      lf.seek(sizeof(int64_t));
      lf.read(data, N/4);
      TS_ASSERT_EQUALS(lf.offset(), sizeof(int64_t)*((N/4)+1));
      TS_ASSERT_EQUALS(data[0], VALUE[0]);
    }

    remove(tmpf.c_str());
  }
}

class LargeFileTests : public CxxTest::TestSuite {
public:
  void test_truncate() { lf_truncate(); }

  void test_mmap_open() { lf_generic_open<LargeFileMMap>(); }
  void test_mmap_read() { lf_generic_read<LargeFileMMap>(); }
  void test_mmap_write() { lf_generic_write<LargeFileMMap>(); }
  void test_mmap_write_only() { lf_generic_write_only<LargeFileMMap>(); }
  void test_mmap_header() { lf_generic_header<LargeFileMMap>(); }
  void test_mmap_large_header() { lf_generic_large_header<LargeFileMMap>(); }
  void test_mmap_enqueue() { lf_generic_enqueue<LargeFileMMap>(); }
  void test_mmap_reopen() { lf_generic_reopen<LargeFileMMap>(); }
  void test_mmap_rw_single() { lf_generic_rw_single<LargeFileMMap>(); }
  void test_mmap_truncate() { lf_generic_truncate<LargeFileMMap>(); }
  void test_mmap_wroffset() { lf_generic_wroffset<LargeFileMMap>(); }
  void test_mmap_rdoffset() { lf_generic_rdoffset<LargeFileMMap>(); }

  void test_fd_open() { lf_generic_open<LargeFileFD>(); }
  void test_fd_read() { lf_generic_read<LargeFileFD>(); }
  void test_fd_write() { lf_generic_write<LargeFileFD>(); }
  void test_fd_write_only() { lf_generic_write_only<LargeFileFD>(); }
  void test_fd_header() { lf_generic_header<LargeFileFD>(); }
  void test_fd_large_header() { lf_generic_large_header<LargeFileFD>(); }
  void test_fd_enqueue() { lf_generic_enqueue<LargeFileFD>(); }
  void test_fd_reopen() { lf_generic_reopen<LargeFileFD>(); }
  void test_fd_rw_single() { lf_generic_rw_single<LargeFileFD>(); }
  void test_fd_truncate() { lf_generic_truncate<LargeFileFD>(); }
  void test_fd_wroffset() { lf_generic_wroffset<LargeFileFD>(); }
  void test_fd_rdoffset() { lf_generic_rdoffset<LargeFileFD>(); }

  void test_aio_open() { lf_generic_open<LargeFileAIO>(); }
  void test_aio_read() { lf_generic_read<LargeFileAIO>(); }
  void test_aio_write() { lf_generic_write<LargeFileAIO>(); }
  void test_aio_write_only() { lf_generic_write_only<LargeFileAIO>(); }
  void test_aio_header() { lf_generic_header<LargeFileAIO>(); }
  void test_aio_large_header() { lf_generic_large_header<LargeFileAIO>(); }
  void test_aio_nocopy() { lf_aio_nocopy(); }
  void test_aio_enqueue() { lf_generic_enqueue<LargeFileAIO>(); }
  void test_aio_reopen() { lf_generic_reopen<LargeFileAIO>(); }
  void test_aio_rw_single() { lf_generic_rw_single<LargeFileAIO>(); }
  void test_aio_truncate() { lf_generic_truncate<LargeFileAIO>(); }
  void test_aio_wroffset() { lf_generic_wroffset<LargeFileAIO>(); }
  void test_aio_rdoffset() { lf_generic_rdoffset<LargeFileAIO>(); }

  void test_c_open() { lf_generic_open<LargeFileC>(); }
  void test_c_read() { lf_generic_read<LargeFileC>(); }
  void test_c_write() { lf_generic_write<LargeFileC>(); }
  void test_c_write_only() { lf_generic_write_only<LargeFileC>(); }
  void test_c_header() { lf_generic_header<LargeFileC>(); }
  void test_c_large_header() { lf_generic_large_header<LargeFileC>(); }
  void test_c_enqueue() { lf_generic_enqueue<LargeFileC>(); }
  void test_c_reopen() { lf_generic_reopen<LargeFileC>(); }
  void test_c_rw_single() { lf_generic_rw_single<LargeFileC>(); }
  void test_c_truncate() { lf_generic_truncate<LargeFileC>(); }
  void test_c_wroffset() { lf_generic_wroffset<LargeFileC>(); }
  void test_c_rdoffset() { lf_generic_rdoffset<LargeFileC>(); }
};
