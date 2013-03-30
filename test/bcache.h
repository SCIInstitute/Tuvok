#include <array>
#include <algorithm>
#include <cxxtest/TestSuite.h>
#include "BrickCache.h"

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
  std::vector<uint8_t> rv;
  c.lookup(k, rv);
  TS_ASSERT_EQUALS(elems.size(), rv.size());
  TS_ASSERT(std::equal(elems.begin(), elems.end(), rv.begin()));
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
  std::vector<uint8_t> dummy;
  TS_ASSERT_EQUALS(c.size(), sizeof(uint8_t)*4);
  c.lookup(k, dummy);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint8_t)*4);
  c.lookup(k, dummy);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint8_t)*4);
  c.lookup(k, dummy);
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
  std::vector<uint16_t> dummy;
  TS_ASSERT_EQUALS(c.size(), sizeof(uint16_t)*4);
  c.lookup(k, dummy);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint16_t)*4);
  c.lookup(k, dummy);
  TS_ASSERT_EQUALS(c.size(), sizeof(uint16_t)*4);
  c.lookup(k, dummy);
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

class BCacheTests : public CxxTest::TestSuite {
public:
  void test_simple() { simple(); }
  void test_add() { add(); }
  void test_remove() { remove(); }
  void test_sizes() { sizes(); }
  void test_lookup_bug() { lookup_bug(); }
  void test_lookup_bug16() { lookup_bug16(); }
};
