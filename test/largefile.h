#include <cstdlib>
#include <algorithm>
#include <functional>
#include <vector>
#include <boost/cstdint.hpp>

#include <cxxtest/TestSuite.h>

#include "LargeFileMMap.h"

#include "util-test.h"

static void lf_mmap_open() {
  std::ofstream ofs;
  const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);

  const uint64_t cval = 86;
  gen_constant<uint64_t>(ofs, 42, cval);
  ofs.close();

  LargeFileMMap lf(tmpf, std::ios::in, 0);
  if(!lf.is_open()) {
    TS_FAIL("Could not open file at all.");
    return;
  }

  remove(tmpf.c_str());
}

static void lf_mmap_read() {
  std::ofstream ofs;
  const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);

  const uint64_t cval = 86;
  gen_constant<uint64_t>(ofs, 42, cval);
  ofs.close();

  LargeFileMMap lf(tmpf, std::ios::in, 0);
  if(!lf.is_open()) {
    TS_FAIL("Could not open file at all.");
    return;
  }
  std::tr1::shared_ptr<const void> mem = lf.read(0, 42 * sizeof(uint64_t));
  const uint64_t* data = static_cast<const uint64_t*>(mem.get());
  assert(data != NULL);
  for(size_t i=0; i < 42; ++i) {
    TS_ASSERT_EQUALS(data[i], cval);
  }

  remove(tmpf.c_str());
}

static int64_t generate_constant(int64_t val) { return val; }
static void null_deleter(void*) {}

static void lf_mmap_write() {
  std::ofstream ofs;
  const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
  ofs.close();

  const size_t N = 64;
  const int64_t VALUE = -42;
  { /* write. */
    LargeFileMMap lf(tmpf, std::ios::out, 0, sizeof(int64_t)*N);

    int64_t data[N];
    std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE));
    lf.write(std::tr1::shared_ptr<const void>(data, null_deleter), 0,
                                              sizeof(int64_t)*N);
    lf.close();
  }
  MESSAGE("Closed.");
  { /* now read. */
    LargeFileMMap lf(tmpf, std::ios::in);
    std::tr1::shared_ptr<const void> mem = lf.read(0, N*sizeof(int64_t));
    const int64_t* data = static_cast<const int64_t*>(mem.get());
    for(size_t i=0; i < N; ++i) {
      TS_ASSERT_EQUALS(data[i], VALUE);
    }
  }
  remove(tmpf.c_str());
}

static void lf_mmap_header() {
  std::ofstream ofs;
  const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
  ofs.close();

  const size_t N = 64;
  const int64_t VALUE[2] = { -42, 42 };
  { /* write. */
    LargeFileMMap lf(tmpf, std::ios::out, 0, sizeof(int64_t)*N*2);

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
  { /* now read.  We'll use a header offset so that we expect to see one
     * VALUE[0] and then N VALUE[1]s.*/
    LargeFileMMap lf(tmpf, std::ios::in, sizeof(int64_t)*(N-1));
    std::tr1::shared_ptr<const void> mem = lf.read(0, (N+1)*sizeof(int64_t));
    const int64_t* data = static_cast<const int64_t*>(mem.get());
    TS_ASSERT_EQUALS(data[0], VALUE[0]);
    for(size_t i=1; i < N+1; ++i) {
      TS_ASSERT_EQUALS(data[i], VALUE[1]);
    }
  }
}

static void lf_mmap_large_header() {
  std::ofstream ofs;
  const std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
  ofs.close();

  const size_t N = 64;
  const int64_t VALUE[2] = { -42, 42 };
  const size_t offset = 32768;
  { /* write. */
    LargeFileMMap lf(tmpf, std::ios::out, 0, sizeof(int64_t)*N*2+offset);

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
    LargeFileMMap lf(tmpf, std::ios::in, offset);
    std::tr1::shared_ptr<const void> mem = lf.read(0, 2*N*sizeof(int64_t));
    const int64_t* data = static_cast<const int64_t*>(mem.get());
    for(size_t i=0; i < N; ++i) {
      TS_ASSERT_EQUALS(data[i], VALUE[0]);
    }
    for(size_t i=N; i < 2*N; ++i) {
      TS_ASSERT_EQUALS(data[i], VALUE[1]);
    }
  }
}


class LargeFileTests : public CxxTest::TestSuite {
  public:
    void test_mmap_open() { lf_mmap_open(); }
    void test_mmap_read() { lf_mmap_read(); }
    void test_mmap_write() { lf_mmap_write(); }
    void test_mmap_header() { lf_mmap_header(); }
    void test_mmap_large_header() { lf_mmap_large_header(); }
};
