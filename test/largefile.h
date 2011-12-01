#include <cstdlib>
#include <algorithm>
#include <functional>
#include <vector>
#include <boost/cstdint.hpp>

#include <cxxtest/TestSuite.h>

#include "LargeFileMMap.h"
#include "LargeFileFD.h"

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
    std::tr1::shared_ptr<const void> mem = lf.read(0, len * sizeof(uint64_t));
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
      lf.write(std::tr1::shared_ptr<const void>(data, null_deleter), 0,
                                                sizeof(int64_t)*N);
      lf.close();
    }
    MESSAGE("Write complete, closed.  Starting read.");
    {
      T lf(tmpf, std::ios::in);
      std::tr1::shared_ptr<const void> mem = lf.read(0, N*sizeof(int64_t));
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
      lf.write(std::tr1::shared_ptr<const void>(data, null_deleter), 0,
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
    end:
      remove(tmpf.c_str());
  }
}

// make sure we respect header offsets.
namespace {
  template<class T> void lf_generic_header() {
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
      lf.write(std::tr1::shared_ptr<const void>(data, null_deleter), 0,
                                                sizeof(int64_t)*N);
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[1]));
      lf.write(std::tr1::shared_ptr<const void>(data, null_deleter),
                                                sizeof(int64_t)*N,
                                                sizeof(int64_t)*N);
      lf.close();
    }
    { /* read.  offset so we see 1 VALUE[0] and N VALUE[1]'s. */
      T lf(tmpf, std::ios::in, sizeof(int64_t)*(N-1));
      std::tr1::shared_ptr<const void> mem = lf.read(0, (N+1)*sizeof(int64_t));
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
      lf.write(std::tr1::shared_ptr<const void>(data, null_deleter), offset,
                                                sizeof(int64_t)*N);
      std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE[1]));
      lf.write(std::tr1::shared_ptr<const void>(data, null_deleter),
                                                sizeof(int64_t)*N + offset,
                                                sizeof(int64_t)*N);
      lf.close();
    }
    { /* now read.  We'll use a header offset so that we expect to see one
       * VALUE[0] and then N VALUE[1]s.*/
      T lf(tmpf, std::ios::in, offset);
      std::tr1::shared_ptr<const void> mem = lf.read(0, 2*N*sizeof(int64_t));
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

class LargeFileTests : public CxxTest::TestSuite {
public:
  void test_mmap_open() { lf_generic_open<LargeFileMMap>(); }
  void test_mmap_read() { lf_generic_read<LargeFileMMap>(); }
  void test_mmap_write() { lf_generic_write<LargeFileMMap>(); }
  void test_mmap_header() { lf_generic_header<LargeFileMMap>(); }
  void test_mmap_large_header() { lf_generic_large_header<LargeFileMMap>(); }
  void test_fd_open() { lf_generic_open<LargeFileFD>(); }
  void test_fd_read() { lf_generic_read<LargeFileFD>(); }
  void test_fd_write() { lf_generic_write<LargeFileFD>(); }
  void test_fd_header() { lf_generic_header<LargeFileFD>(); }
  void test_fd_large_header() { lf_generic_large_header<LargeFileFD>(); }
  void test_mmap_write_only() { lf_generic_write_only<LargeFileMMap>(); }
  void test_fd_write_only() { lf_generic_write_only<LargeFileFD>(); }
};
