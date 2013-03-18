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
  std::vector<uint8_t> rv = c.lookup(k, uint8_t());
  TS_ASSERT_EQUALS(elems.size(), rv.size());
  TS_ASSERT(std::equal(elems.begin(), elems.end(), rv.begin()));
}

void remove() {
  BrickCache c;
  BrickKey k(0,0,0);
  {
    std::vector<uint8_t> data(1);
    data[0] = 42;
    c.add(k, data);
  }
  c.remove();
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
};
