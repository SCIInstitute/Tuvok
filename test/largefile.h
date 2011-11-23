#include <cstdlib>
#include <algorithm>
#include <functional>
#include <vector>
#include <boost/cstdint.hpp>

#include <cxxtest/TestSuite.h>

#include "LargeFileMMap.h"

#include "util-test.h"

void lf_mmap_open() {
  std::ofstream ofs;
  std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);

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

void lf_mmap_read() {
  std::ofstream ofs;
  std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);

  const uint64_t cval = 86;
  gen_constant<uint64_t>(ofs, 42, cval);
  ofs.close();

  LargeFileMMap lf(tmpf, std::ios::in, 0);
  if(!lf.is_open()) {
    TS_FAIL("Could not open file at all.");
    return;
  }
  std::tr1::weak_ptr<const void> mem = lf.read(0, 42 * sizeof(uint64_t));
  const uint64_t* data = static_cast<const uint64_t*>(mem.lock().get());
  for(size_t i=0; i < 42; ++i) {
    TS_ASSERT_EQUALS(data[i], cval);
  }

  remove(tmpf.c_str());
}

static int64_t generate_constant(int64_t val) { return val; }

static void lf_mmap_write() {
  EnableDebugMessages dbg;
  std::ofstream ofs;
  std::string tmpf = mk_tmpfile(ofs, std::ios::out | std::ios::binary);
  ofs.close();

  const size_t N = 64;
  const int64_t VALUE = -42;
  { /* write. */
    LargeFileMMap lf(tmpf, std::ios::out);

    int64_t data[N];
    std::generate(data, data+N, std::tr1::bind(generate_constant, VALUE));
    lf.write(std::tr1::shared_ptr<const void>(data), 0, sizeof(int64_t)*N);
    lf.close();
  }
  MESSAGE("Closed.");
  { /* now read. */
    LargeFileMMap lf(tmpf, std::ios::in);
    std::tr1::weak_ptr<const void> mem = lf.read(0, N*sizeof(int64_t));
    const int64_t* data = static_cast<const int64_t*>(mem.lock().get());
    for(size_t i=0; i < N; ++i) {
      TS_ASSERT_EQUALS(data[i], VALUE);
    }
  }
}

class LargeFileTests : public CxxTest::TestSuite {
  public:
//    void test_mmap_open() { lf_mmap_open(); }
//    void test_mmap_read() { lf_mmap_read(); }
    void test_mmap_write() { lf_mmap_write(); }
};
