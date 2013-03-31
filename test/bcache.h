#include <array>
#include <algorithm>
#include <cxxtest/TestSuite.h>
#include "BrickCache.h"
#include "Controller/Controller.h"
#include "util-test.h"

using namespace tuvok;
// just make the object!
void simple() { tuvok::BrickCache c; }
void add() {
  BrickCache c;
  BrickKey k(0,0,0);
  std::array<uint8_t, 4> elems = {{9, 12, 42, 19}};
  {
    std::vector<uint8_t> data(4);
    std::copy(elems.begin(), elems.end(), data.begin());
    c.add(k, data);
  }
  // what we put in should be the same coming out!
  const uint8_t* rv = static_cast<const uint8_t*>(c.lookup(k));
  TS_ASSERT(std::equal(elems.begin(), elems.end(), rv));
}

// once had a bug that lookup would re-insert every entry!
void lookup_bug() {
  BrickCache c;
  BrickKey k(0,0,0);
  std::array<uint8_t, 4> elems = {{9, 12, 42, 19}};
  {
    std::vector<uint8_t> data(4);
    std::copy(elems.begin(), elems.end(), data.begin());
    c.add(k, data);
  }
  TS_ASSERT_EQUALS(c.size(), sizeof(uint8_t)*4);
  c.lookup(k);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint8_t)*4);
  c.lookup(k);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint8_t)*4);
  c.lookup(k);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint8_t)*4);
  c.remove();
  TS_ASSERT_EQUALS(c.size(), 0U);
}

void lookup_bug16() {
  BrickCache c;
  BrickKey k(0,0,0);
  std::array<uint16_t, 4> elems = {{9, 12, 42, 19}};
  {
    std::vector<uint16_t> data(4);
    std::copy(elems.begin(), elems.end(), data.begin());
    c.add(k, data);
  }
  TS_ASSERT_EQUALS(c.size(), sizeof(uint16_t)*4);
  c.lookup(k);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint16_t)*4);
  c.lookup(k);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint16_t)*4);
  c.lookup(k);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint16_t)*4);
  c.remove();
  TS_ASSERT_EQUALS(c.size(), 0U);
}

void remove() {
  BrickCache c;
  BrickKey k(0,0,0);
  {
    std::vector<uint8_t> data(1);
    data[0] = 42;
    c.add(k, data);
  }
  TS_ASSERT_EQUALS(c.size(), sizeof(uint8_t)*1);
  c.remove();
  TS_ASSERT_EQUALS(c.size(), 0U);
}

void sizes() {
  BrickCache c;
  BrickKey k(0,0,0);
  {
    std::vector<uint32_t> data(1); data[0] = 42U;
    c.add(k, data);
  }
  TS_ASSERT_EQUALS(c.size(), sizeof(uint32_t) * 1);
  c.remove();
  TS_ASSERT_EQUALS(c.size(), 0U);
  c.remove();
  TS_ASSERT_EQUALS(c.size(), 0U);
}

namespace {
  template<typename T>
  void normal(std::vector<T>& data, const T& mean, const T& stddev) {
    // double: RNGs are only defined for FP types.  We'll generate double
    // and just cast to T.
    std::random_device genSeed;
    std::mt19937 mtwister(genSeed());
    std::normal_distribution<double> normalDist(mean, stddev);
    std::function<double()> getRandNormal(std::bind(normalDist, mtwister));

    for(size_t i=0; i < data.size(); ++i) {
      T v = static_cast<T>(getRandNormal());
      data[i] = v;
    }
  }
}

// this is really a benchmark, not a test per se...
void add_many() {
  BrickCache c;
  std::vector<uint8_t> src(68*68*68);
  normal<uint8_t>(src, uint8_t(64), uint8_t(12));

  EnableDebugMessages edm;
  BrickKey key(0,0,0);
  for(size_t i=0; i < 2048; ++i) {
    std::vector<uint8_t> buf(68*68*68);
    std::copy(src.begin(), src.end(), buf.begin());
    key = std::make_tuple(0U, 0U, i);
    c.add(key, buf);
  }
  double cache_add = Controller::Instance().PerfQuery(PERF_CACHE_ADD);
  double cache_lookup = Controller::Instance().PerfQuery(PERF_CACHE_LOOKUP);
  double something = Controller::Instance().PerfQuery(PERF_SOMETHING);
  double bcopy = Controller::Instance().PerfQuery(PERF_BRICK_COPY);
  fprintf(stderr, "\ncache add: %g\ncache lookup: %g\bcopy: %g\n"
          "something: %g\n",
          cache_add, cache_lookup, bcopy, something);
}

class BCacheTests : public CxxTest::TestSuite {
public:
  void test_simple() { simple(); }
  void test_add() { add(); }
  void test_remove() { remove(); }
  void test_sizes() { sizes(); }
  void test_lookup_bug() { lookup_bug(); }
  void test_lookup_bug16() { lookup_bug16(); }
//  void test_add_many() { add_many(); }
};
