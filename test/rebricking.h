#include <array>
#include <cstdint>
#include <fstream>
#include <cxxtest/TestSuite.h>
#include "Controller/Controller.h"
#include "DynamicBrickingDS.h"
#include "RAWConverter.h"
#include "uvfDataset.h"
#include "util-test.h"

static const std::array<std::array<uint16_t, 8>, 8> data = {{
  {{  0, 1, 2, 3, 4, 5, 6, 7 }},
  {{  8, 9,10,11,12,13,14,15 }},
  {{ 16,17,18,19,20,21,22,23 }},
  {{ 24,25,26,27,28,29,30,31 }},
  {{ 32,33,34,35,36,37,38,39 }},
  {{ 40,41,42,43,44,45,46,47 }},
  {{ 48,49,50,51,52,53,54,55 }},
  {{ 56,57,58,59,60,61,62,63 }}
}};

static void mk8x8(const char* filename) {
  std::ofstream ofs(filename, std::ios::trunc | std::ios::binary);
  ofs.write(reinterpret_cast<const char*>(data[0].data()),
            sizeof(uint16_t) * 8*8);
  ofs.close();
}
static void mk_uvf(const char* filename, const char* uvf) {
  RAWConverter::ConvertRAWDataset(filename, uvf, ".", 0, sizeof(uint16_t)*8, 1,
                                  1, false, false, false,
                                  UINT64VECTOR3(8,8,1), FLOATVECTOR3(1,1,1),
                                  "desc", "iotest", 16, 2, true, false, 0,0,
                                  0, NULL, false);
}

// creates an 8x8x1 uvf test data set and returns it.
std::shared_ptr<UVFDataset> mk8x8testdata() {
  const char* outfn = "out.uvf"; ///< @todo fixme use a real temp filename
  mk8x8("abc"); ///< @todo fixme use a real temporary file
  mk_uvf("abc", outfn);
  std::shared_ptr<UVFDataset> ds(new UVFDataset(outfn, 128, false));
  TS_ASSERT(ds->IsOpen());
  return ds;
}

std::pair<size_t, size_t> idx2d(size_t idx1d,
                                       std::array<size_t,2> dims) {
  return std::make_pair(idx1d / dims[1], idx1d % dims[0]);
}

// number of ghost cells per dimension...
static unsigned ghost() { return 4; }

class RebrickerTests : public CxxTest::TestSuite {
public:
  // just creates and destroys the object.
  void test_simple() {
    std::shared_ptr<UVFDataset> ds = mk8x8testdata();
    DynamicBrickingDS dynamic(ds, {{8,8,8}});
  }

  // splits a 1-brick 8x8x1 volume into two bricks, of size 4x8x1 each.
  void test_make_two() {
    std::shared_ptr<UVFDataset> ds = mk8x8testdata();
    DynamicBrickingDS dynamic(ds, {{4,8,1}});
    // it should be 3 bricks, not 2, because this will create a new LoD
    TS_ASSERT_EQUALS(dynamic.GetTotalBrickCount(),
                     static_cast<BrickTable::size_type>(3));
  }

  // does not divide the volume evenly.
  void test_uneven() {
    std::shared_ptr<UVFDataset> ds = mk8x8testdata();
    TS_ASSERT_THROWS(DynamicBrickingDS dynamic(ds, {{3,8,1}}),
                     std::runtime_error);
  }

  // all previous test split on X, make sure Y works too!
  void test_y() {
    std::shared_ptr<UVFDataset> ds = mk8x8testdata();
    DynamicBrickingDS dynamic(ds, {{8,4,1}});
    TS_ASSERT_EQUALS(dynamic.GetTotalBrickCount(),
                     static_cast<BrickTable::size_type>(3));
  }

  void test_uneven_multiple_dims() {
    std::shared_ptr<UVFDataset> ds = mk8x8testdata();
    TS_ASSERT_THROWS(DynamicBrickingDS dynamic(ds, {{3,8,1}}),
                     std::runtime_error);
  }

  // we gave an 8x8x1 buffer of values in [0,31]; even though the data are
  // uint64_t, we should recognize that we actually have 8bit data, etc.
  void test_data_type() {
    std::shared_ptr<UVFDataset> ds = mk8x8testdata();
    DynamicBrickingDS dynamic(ds, {{8,8,8}});
    TS_ASSERT_EQUALS(dynamic.GetBitWidth(), 8U);
    TS_ASSERT_EQUALS(dynamic.GetComponentCount(), 1ULL);
    TS_ASSERT_EQUALS(dynamic.GetIsSigned(), false);
    TS_ASSERT_EQUALS(dynamic.GetIsFloat(), false);
    TS_ASSERT_EQUALS(dynamic.IsSameEndianness(), true);
    TS_ASSERT_DELTA(dynamic.GetRange().first, 0.0, 0.001);
    TS_ASSERT_DELTA(dynamic.GetRange().second, 63.0, 0.001);
  }

  void test_no_dynamic() {
    std::shared_ptr<UVFDataset> ds = mk8x8testdata();
    BrickKey bk(0,0,0); std::vector<uint8_t> d;
    if(ds->GetBrick(bk, d) == false) {
      TS_FAIL("could not read data");
    }
    TS_ASSERT_EQUALS(d.size(),
      (data.size()+ghost()) * (data[0].size()+ghost()) * (1+ghost())
    );
#if 1
    // run through each element and check for equality
    for(size_t i=0; i < 4 && i < data.size()*data[0].size(); ++i) {
      std::pair<size_t,size_t> coord = idx2d(i, {{8,8}});
      TS_ASSERT_EQUALS(d[i], data[coord.first][coord.second]);
    }
#endif
  }

  void test_data_simple() {
    std::shared_ptr<UVFDataset> ds = mk8x8testdata();
    DynamicBrickingDS dynamic(ds, {{8,8,8}});
    BrickKey bk(0,0,0); std::vector<uint8_t> d;
    if(dynamic.GetBrick(bk, d) == false) {
      TS_FAIL("getting brick data failed.");
    }
    TS_ASSERT_EQUALS(d.size(),
      (data.size()+ghost()) * (data[0].size()+ghost()) * (1+ghost())
    );
    // run through each element and check for equality
    for(size_t i=0; i < data.size()*data[0].size(); ++i) {
      std::pair<size_t,size_t> coord = idx2d(i, {{8,8}});
      TS_ASSERT_EQUALS(d[i], data[coord.first][coord.second]);
    }
  }
};
