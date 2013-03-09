#include <array>
#include <cstdint>
#include <fstream>
#include <cxxtest/TestSuite.h>
#include "Basics/SysTools.h"
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
  return ds;
}

std::array<size_t,3> idx3d(size_t idx1d,
                           std::array<size_t,3> dim) {
  return {{
    idx1d % dim[0],
    (idx1d / dim[0]) % dim[1],
    idx1d / (dim[1]*dim[2])
  }};
}

// number of ghost cells per dimension...
static unsigned ghost() { return 4; }

/// tries to find the engine, so that we can use it for some tests.
/// @returns false if we can't find it, so you can abort the test if so.
bool check_for_engine() {
  if(!SysTools::FileExists("engine.uvf") &&
     !SysTools::FileExists("engine.raw.gz")) {
    return false;
  }
  if(SysTools::FileExists("engine.uvf")) { return true; }

  // if we have the raw data, we can just convert it.
  if(SysTools::FileExists("engine.raw.gz")) {
    TS_TRACE("Found raw engine data; converting it for tests.");
    IOManager& iom = *Controller::Instance().IOMan();
    std::ofstream nhdr("engine.nhdr");
    if(!nhdr) { return false; }
    nhdr << "NRRD0001\n"
            "encoding: gzip\n"
            "type: uint8\n"
            "sizes: 256 256 128\n"
            "dimension: 3\n"
            "data file: engine.raw.gz\n";
    nhdr.close();
    return iom.ConvertDataset("engine.nhdr", "engine.uvf", ".", true, 256, 2,
                              false);
  }
  return true;
}

// just creates and destroys the object.
void tsimple() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{8,8,8}});
}

// splits a 1-brick 8x8x1 volume into two bricks, of size 4x8x1 each.
void tmake_two() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{4,8,1}});
  // it should be 3 bricks, not 2, because this will create a new LoD
  TS_ASSERT_EQUALS(dynamic.GetTotalBrickCount(),
                   static_cast<BrickTable::size_type>(3));
}

// does not divide the volume evenly.
void tuneven() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  TS_ASSERT_THROWS(DynamicBrickingDS dynamic(ds, {{3,8,1}}),
                   std::runtime_error);
}

// all previous test split on X, make sure Y works too!
void ty() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{8,4,1}});
  TS_ASSERT_EQUALS(dynamic.GetTotalBrickCount(),
                   static_cast<BrickTable::size_type>(3));
}

void tuneven_multiple_dims() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  TS_ASSERT_THROWS(DynamicBrickingDS dynamic(ds, {{3,8,1}}),
                   std::runtime_error);
}

// we gave an 8x8x1 buffer of values in [0,31]; even though the data are
// uint64_t, we should recognize that we actually have 8bit data, etc.
void tdata_type() {
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

void tno_dynamic() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  BrickKey bk(0,0,0); std::vector<uint8_t> d;
  if(ds->GetBrick(bk, d) == false) {
    TS_FAIL("could not read data");
  }
  TS_ASSERT_EQUALS(d.size(),
    (data.size()+ghost()) * (data[0].size()+ghost()) * (1+ghost())
  );
  const UINT64VECTOR3 bs(
    ds->GetBrickMetadata(bk).n_voxels[0],
    ds->GetBrickMetadata(bk).n_voxels[1],
    ds->GetBrickMetadata(bk).n_voxels[2]
  );

  // run through each element and check for equality.  however we have ghost
  // data, make sure to skip over that (since our source array doesn't have
  // it!)
  const size_t offset = ghost()/2;
  const size_t slice_sz = (data.size() + ghost()) * (data[0].size() + ghost());
  for(size_t y=offset; y < bs[1]-offset; ++y) {
    for(size_t x=offset; x < bs[0]-offset; ++x) {
      const size_t idx = slice_sz*2 + y*bs[0] + x;
      // yes, the x/y indices are reversed in 'data'.
      TS_ASSERT_EQUALS(d[idx], data[y-offset][x-offset]);
    }
  }
}

void tdomain_size() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  {
    DynamicBrickingDS dynamic(ds, {{8,8,8}});
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[0], dynamic.GetDomainSize(0,0)[0]);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[1], dynamic.GetDomainSize(0,0)[1]);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[2], dynamic.GetDomainSize(0,0)[2]);
  }
  {
    DynamicBrickingDS dynamic(ds, {{4,4,4}});
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[0], dynamic.GetDomainSize(0,0)[0]);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[1], dynamic.GetDomainSize(0,0)[1]);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[2], dynamic.GetDomainSize(0,0)[2]);
  }
}

// very simple case: "rebrick" a dataset into the same number of bricks.
void tdata_simple () {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{8,8,8}});
  BrickKey bk(0,0,0); std::vector<uint8_t> d;
  if(dynamic.GetBrick(bk, d) == false) {
    TS_FAIL("getting brick data failed.");
  }
  TS_ASSERT_EQUALS(d.size(),
    (data.size()+ghost()) * (data[0].size()+ghost()) * (1+ghost())
  );

  const UINT64VECTOR3 bs(
    ds->GetBrickMetadata(bk).n_voxels[0],
    ds->GetBrickMetadata(bk).n_voxels[1],
    ds->GetBrickMetadata(bk).n_voxels[2]
  );
  TS_ASSERT_EQUALS(bs[0], 12ULL);
  TS_ASSERT_EQUALS(bs[1], 12ULL);
  TS_ASSERT_EQUALS(bs[2], 5ULL);
  const UINTVECTOR3 dynamic_bs = dynamic.GetBrickMetadata(bk).n_voxels;
  TS_ASSERT_EQUALS(bs[0], dynamic_bs[0]);
  TS_ASSERT_EQUALS(bs[1], dynamic_bs[1]);
  TS_ASSERT_EQUALS(bs[2], dynamic_bs[2]);

  // run through each element and check for equality.  however we have ghost
  // data, make sure to skip over that (since our source array doesn't have
  // it!)
  const size_t offset = ghost()/2;
  const size_t slice_sz = (data.size() + ghost()) * (data[0].size() + ghost());
  for(size_t y=offset; y < bs[1]-offset; ++y) {
    for(size_t x=offset; x < bs[0]-offset; ++x) {
      const size_t idx = slice_sz*2 + y*bs[0] + x;
      // yes, the x/y indexes are reversed in 'data'.
      TS_ASSERT_EQUALS(d[idx], data[y-offset][x-offset]);
    }
  }
}

// create bricks that are half the size of the source.
void tdata_half_split() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{4,8,1}});

  BrickKey bk(0,0,0);
  const UINTVECTOR3 bs = dynamic.GetBrickMetadata(bk).n_voxels;
  TS_ASSERT_EQUALS(bs[0], 8ULL);
  TS_ASSERT_EQUALS(bs[1], 12ULL);
  TS_ASSERT_EQUALS(bs[2], 5ULL);

  std::vector<uint8_t> d;
  if(dynamic.GetBrick(bk, d) == false) {
    TS_FAIL("reading brick data failed");
  }
  TS_ASSERT_EQUALS(d.size(), bs.volume());

  const size_t slice_sz = bs[0] * bs[1];
  const size_t offset = ghost()/2;
  for(size_t y=offset; y < bs[1]-offset; ++y) {
    for(size_t x=offset; x < bs[0]-offset; ++x) {
      const size_t idx = slice_sz*2 + y*bs[0] + x;
      TS_ASSERT_EQUALS(d[idx], data[y-offset][x-offset]);
    }
  }
}

// tests GetBrickVoxelCount API.
void tvoxel_count() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  {
    DynamicBrickingDS dynamic(ds, {{8,8,1}});
    const BrickKey bk(0,0,0);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[0], 12U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[1], 12U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[2],  5U);
  }
  {
    DynamicBrickingDS dynamic(ds, {{4,8,1}});
    BrickKey bk(0,0,0);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[0],  8U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[1], 12U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[2],  5U);
    bk = BrickKey(0,0,1);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[0],  8U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[1], 12U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[2],  5U);
  }
}

// rebricking should not change the world space layouts
void tmetadata() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  const BrickKey bk(0,0,0);
  const BrickMD& src_md = ds->GetBrickMetadata(bk);
  { // bricks are bigger than DS -> DS unchanged -> metadata unchanged
    DynamicBrickingDS dynamic(ds, {{8,8,1}});
    const BrickMD& tgt_md = dynamic.GetBrickMetadata(bk);

    TS_ASSERT_EQUALS(src_md.center[0], tgt_md.center[0]);
    TS_ASSERT_EQUALS(src_md.center[1], tgt_md.center[1]);
    TS_ASSERT_EQUALS(src_md.center[2], tgt_md.center[2]);
    TS_ASSERT_EQUALS(src_md.extents[2], tgt_md.extents[2]);
  }
}

void trealdata() {
  if(!check_for_engine()) { TS_FAIL("need engine for this test"); }
  std::shared_ptr<UVFDataset> ds(new UVFDataset("engine.uvf", 256, false,
                                                false));
  DynamicBrickingDS dynamic(ds, {{256,256,128}});
  BrickKey k(0,0, 3);
  std::vector<uint8_t> data;
  dynamic.GetBrick(k, data);
}

void trealdata_2() {
  if(!check_for_engine()) { TS_FAIL("need engine for this test"); }
  std::shared_ptr<UVFDataset> ds(new UVFDataset("engine.uvf", 256, false,
                                                false));
  DynamicBrickingDS dynamic(ds, {{126,256,128}});
}

void trealdata_make_two_lod2() {
  if(!check_for_engine()) { TS_FAIL("need engine for this test"); }
  std::shared_ptr<UVFDataset> ds(new UVFDataset("engine.uvf", 256, false,
                                                false));
  DynamicBrickingDS dynamic(ds, {{126,256,128}});
  BrickKey k(0,2,0);
  std::vector<uint8_t> data;
  dynamic.GetBrick(k, data);
}

class RebrickerTests : public CxxTest::TestSuite {
public:
  void test_simple() { tsimple(); }
  void test_make_two() { tmake_two(); }
  void test_uneven() { tuneven(); }
  void test_y() { ty(); }
  void test_uneven_multiple_dims() { tuneven_multiple_dims(); }
  void test_data_type() { tdata_type(); }
  void test_no_dynamic() { tno_dynamic(); }
  void test_domain_size() { tdomain_size(); }
  void test_data_simple() { tdata_simple(); }
  void test_data_half_split() { tdata_half_split(); }
  void test_voxel_count() { tvoxel_count(); }
  void test_metadata() { tmetadata(); }
  void test_real() { trealdata(); }
  void test_real_2() { trealdata_2(); }
  void test_real_make_two_lod2() { trealdata_make_two_lod2(); }
};
