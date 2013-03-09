#include <array>
#include <cstdint>
#include <cxxtest/TestSuite.h>
#include "Basics/MathTools.h"
#include "const-brick-iterator.h"

void simple_center() {
  std::array<uint64_t,3> voxels = {{12,6,24}};
  std::array<size_t,3> bsize = {{12,6,24}};
  std::array<std::array<float,3>,2> extents = {{
    {{ 0.0f, 0.0f, 0.0f }},
    {{ 10.0f, 35.0f, 19.0f }}
  }};
  tuvok::const_brick_iterator cbi = tuvok::begin(voxels, bsize, extents);
  TS_ASSERT_DELTA((*cbi).second.center[0],  5.0f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.center[1], 17.5f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.center[2],  9.5f, 0.0001f);
}

void simple_extents() {
  std::array<uint64_t,3> voxels = {{12,6,24}};
  std::array<size_t,3> bsize = {{12,6,24}};
  std::array<std::array<float,3>,2> extents = {{
    {{ 0.0f, 0.0f, 0.0f }},
    {{ 12.0f, 6.0f, 24.0f }}
  }};
  tuvok::const_brick_iterator cbi = tuvok::begin(voxels, bsize, extents);
  TS_ASSERT_DELTA((*cbi).second.extents[0], 12.0f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.extents[1],  6.0f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.extents[2], 24.0f, 0.0001f);
}

// split the domain in two && make sure centers are correct
void two_center() {
  std::array<uint64_t,3> voxels = {{12,6,24}};
  std::array<size_t,3> bsize = {{6,6,24}};
  std::array<std::array<float,3>,2> extents = {{
    {{ 0.0f, 0.0f, 0.0f }},
    {{ 10.0f, 35.0f, 19.0f }}
  }};
  tuvok::const_brick_iterator cbi = tuvok::begin(voxels, bsize, extents);
  TS_ASSERT_DELTA((*cbi).second.center[0],  2.5f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.center[1], 17.5f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.center[2],  9.5f, 0.0001f);

  ++cbi;
  TS_ASSERT_DELTA((*cbi).second.center[0],  7.5f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.center[1], 17.5f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.center[2],  9.5f, 0.0001f);
}

// split the domain in two && make sure extents are correct
void two_extents() {
  std::array<uint64_t,3> voxels = {{12,6,24}};
  std::array<size_t,3> bsize = {{6,6,24}};
  std::array<std::array<float,3>,2> extents = {{
    {{ 0.0f, 0.0f, 0.0f }},
    {{ 10.0f, 35.0f, 19.0f }}
  }};
  tuvok::const_brick_iterator cbi = tuvok::begin(voxels, bsize, extents);
  TS_ASSERT_DELTA((*cbi).second.extents[0],  5.0f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.extents[1], 35.0f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.extents[2], 19.0f, 0.0001f);

  ++cbi;
  TS_ASSERT_DELTA((*cbi).second.extents[0],  5.0f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.extents[1], 35.0f, 0.0001f);
  TS_ASSERT_DELTA((*cbi).second.extents[2], 19.0f, 0.0001f);
}

class BHIteratorTests : public CxxTest::TestSuite {
public:
  void test_simple_center() { simple_center(); }
  void test_simple_extents() { simple_extents(); }
  void test_two_center() { two_center(); }
  void test_two_extents() { two_extents(); }
};
