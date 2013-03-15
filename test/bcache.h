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

class BCacheTests : public CxxTest::TestSuite {
public:
  void test_simple() { simple(); }
  void test_add() { add(); }
};
