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

// number of ghost cells per dimension...
static unsigned ghost() { return 4; }

/// tries to find the given dataset.
///@returns false if the data are not available.
bool check_for(std::string file) {
  if(SysTools::FileExists(file + ".uvf")) { return true; }
  // can convert engine..
  if(SysTools::FileExists(file + ".raw.gz") && file == "engine") {
    TS_TRACE("Found raw engine data; converting it for tests.");
    const IOManager& iom = Controller::Const().IOMan();
    std::ofstream nhdr("engine.nhdr");
    if(!nhdr) { return false; }
    nhdr << "NRRD0001\n"
            "encoding: gzip\n"
            "type: uint8\n"
            "sizes: 256 256 128\n"
            "dimension: 3\n"
            "data file: engine.raw.gz\n";
    nhdr.close();
    return iom.ConvertDataset("engine.nhdr", file+".uvf", ".", true, 256, 2,
                              false);
  }
  // otherwise just try to convert it.
  if(SysTools::FileExists(file + ".dat")) {
    TS_TRACE("Attempting to convert data...");
    const IOManager& iom = Controller::Const().IOMan();
    return iom.ConvertDataset(file+".dat", file+".uvf", ".", true, 256, 2,
                              false);
  }
  return false;
}

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
    const IOManager& iom = Controller::Const().IOMan();
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

static const size_t cacheBytes = 1024U*1024U*2048U;

// just creates and destroys the object.
void tsimple() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  MESSAGE("8x8x1 size: %llux%llux%llu",
          ds->GetDomainSize(0,0)[0], ds->GetDomainSize(0,0)[1],
          ds->GetDomainSize(0,0)[2]);
  DynamicBrickingDS dynamic(ds, {{16,16,16}}, cacheBytes);
}

// splits a 1-brick 8x8x1 volume into two bricks, of size 4x8x1 each.
void tmake_two() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{8,16,16}}, cacheBytes);
  TS_ASSERT_EQUALS(dynamic.GetTotalBrickCount(),
                   static_cast<BrickTable::size_type>(5));
}

// does not divide the volume evenly.
void tuneven() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  TS_ASSERT_THROWS(DynamicBrickingDS dynamic(ds, {{9,16,16}}, cacheBytes),
                   std::runtime_error);
}

// all previous test split on X, make sure Y works too!
void ty() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{16,8,16}}, cacheBytes);
  TS_ASSERT_EQUALS(dynamic.GetTotalBrickCount(),
                   static_cast<BrickTable::size_type>(5));
}

void tuneven_multiple_dims() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  TS_ASSERT_THROWS(DynamicBrickingDS dynamic(ds, {{9,9,16}}, cacheBytes),
                   std::runtime_error);
}

// we gave an 8x8x1 buffer of values in [0,31]; even though the data are
// uint64_t, we should recognize that we actually have 8bit data, etc.
void tdata_type() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{16,16,16}}, cacheBytes);
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
    DynamicBrickingDS dynamic(ds, {{16,16,16}}, cacheBytes);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[0], dynamic.GetDomainSize(0,0)[0]);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[1], dynamic.GetDomainSize(0,0)[1]);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[2], dynamic.GetDomainSize(0,0)[2]);
  }
  {
    DynamicBrickingDS dynamic(ds, {{8,8,16}}, cacheBytes);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[0], dynamic.GetDomainSize(0,0)[0]);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[1], dynamic.GetDomainSize(0,0)[1]);
    TS_ASSERT_EQUALS(ds->GetDomainSize(0,0)[2], dynamic.GetDomainSize(0,0)[2]);
  }
}

// very simple case: "rebrick" a dataset into the same number of bricks.
void tdata_simple () {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{16,16,16}}, cacheBytes);
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
  TS_ASSERT_EQUALS(bs[2],  5ULL);
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
static void verify_half_split(DynamicBrickingDS& dynamic) {
  BrickKey bk(0,0,0);
  const UINTVECTOR3 bs = dynamic.GetBrickMetadata(bk).n_voxels;
  TS_ASSERT_EQUALS(bs[0],  6ULL);
  TS_ASSERT_EQUALS(bs[1], 12ULL);
  TS_ASSERT_EQUALS(bs[2],  5ULL);

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

void tdata_half_split() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{6,16,16}}, cacheBytes);

  verify_half_split(dynamic);
}

// tests GetBrickVoxelCount API.
void tvoxel_count() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  {
    DynamicBrickingDS dynamic(ds, {{16,16,16}}, cacheBytes);
    const BrickKey bk(0,0,0);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[0], 12U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[1], 12U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[2],  5U);
  }
  {
    DynamicBrickingDS dynamic(ds, {{6,16,16}}, cacheBytes);
    BrickKey bk(0,0,0);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[0],  6U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[1], 12U);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[2],  5U);
    bk = BrickKey(0,0,1);
    TS_ASSERT_EQUALS(dynamic.GetBrickVoxelCounts(bk)[0],  6U);
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
    DynamicBrickingDS dynamic(ds, {{16,16,16}}, cacheBytes);
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
  DynamicBrickingDS dynamic(ds, {{256,256,256}}, cacheBytes);
  BrickKey k(0,0, 0);
  std::vector<uint8_t> data;
  dynamic.GetBrick(k, data);
}

void trealdata_2() {
  if(!check_for_engine()) { TS_FAIL("need engine for this test"); }
  std::shared_ptr<UVFDataset> ds(new UVFDataset("engine.uvf", 256, false,
                                                false));
  DynamicBrickingDS dynamic(ds, {{130,256,256}}, cacheBytes);
}

void trealdata_make_two_lod2() {
  if(!check_for_engine()) { TS_FAIL("need engine for this test"); }
  std::shared_ptr<UVFDataset> ds(new UVFDataset("engine.uvf", 256, false,
                                                false));
  DynamicBrickingDS dynamic(ds, {{130,256,256}}, cacheBytes);
  BrickKey k(0,1,0);
  std::vector<uint8_t> data;
  dynamic.GetBrick(k, data);
  TS_ASSERT_EQUALS(data.size(), 130U*132U*68U);

  // root bricks are both 126x256x128... + ghost voxels.
  TS_ASSERT_EQUALS(dynamic.GetBrickMetadata(BrickKey(0,0,0)).n_voxels,
                   UINTVECTOR3(130U, 256U, 132U));
  TS_ASSERT_EQUALS(dynamic.GetBrickMetadata(BrickKey(0,0,1)).n_voxels,
                   UINTVECTOR3(130U, 256U, 132U));
  // and there's a tiny little brick left over on the side.
  TS_ASSERT_EQUALS(dynamic.GetBrickMetadata(BrickKey(0,0,2)).n_voxels,
                   UINTVECTOR3(8U, 256U, 132U));

  // as we get coarser, the LODs end up the same again.
  const BrickMD dy_md = dynamic.GetBrickMetadata(BrickKey(0,2,0));
  const BrickMD uvf_md = ds->GetBrickMetadata(BrickKey(0,2,0));
  TS_ASSERT_EQUALS(dy_md.n_voxels, uvf_md.n_voxels);
}

void tbsizes() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{16,16,16}}, cacheBytes);
  TS_ASSERT_EQUALS(dynamic.GetMaxBrickSize()[0], 16U);
  TS_ASSERT_EQUALS(dynamic.GetMaxBrickSize()[1], 16U);
  TS_ASSERT_EQUALS(dynamic.GetMaxBrickSize()[2], 16U);

  TS_ASSERT_EQUALS(dynamic.GetMaxUsedBrickSizes()[0], 12U);
  TS_ASSERT_EQUALS(dynamic.GetMaxUsedBrickSizes()[1], 12U);
  TS_ASSERT_EQUALS(dynamic.GetMaxUsedBrickSizes()[2],  5U);
}

void tprecompute() {
  if(!check_for_engine()) { TS_FAIL("need engine for this test"); }
  std::shared_ptr<UVFDataset> ds(new UVFDataset("engine.uvf", 512, false,
                                                false));
  DynamicBrickingDS dynamic(ds, {{130,256,256}}, cacheBytes,
                            DynamicBrickingDS::MM_PRECOMPUTE);
}

void output_slice(const std::vector<uint8_t>& data,
                  const std::array<size_t,3> size,
                  const size_t z) {
  fputs("\n", stderr);
  for(size_t y=0; y < size[1]; ++y) {
    for(size_t x=0; x < size[0]; ++x) {
      const size_t idx = z*size[1]*size[0] + y*size[0] + x;
      fprintf(stderr, "%02u ", static_cast<uint16_t>(data[idx]));
    }
    fputs("\n", stderr);
  }
  fputs("\n", stderr);
}

// compute the min/max info when requested, and it should be exactly right
void tminmax_dynamic() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{16,8,16}}, cacheBytes,
                            DynamicBrickingDS::MM_DYNAMIC);
  BrickKey bk(0,0,0); std::vector<uint8_t> d;
  if(dynamic.GetBrick(bk, d) == false) {
    TS_FAIL("getting brick data failed.");
  }
  TS_ASSERT_EQUALS(d.size(), 12U*8U*5U);

#if 0
  if(dynamic.GetBrick(BrickKey(0,0,1), d) == false) {
    TS_FAIL("getting brick data failed.");
  }
  TS_ASSERT_EQUALS(d.size(), 12U*8U*5U);
  output_slice(d, {{12,8,5}}, 2);
#endif

  MinMaxBlock mm = dynamic.MaxMinForKey(BrickKey(0,0,0));
  TS_ASSERT_DELTA(mm.minScalar, 0.0, 0.001);
  TS_ASSERT_DELTA(mm.maxScalar, 47.0, 0.001); // 47: includes the ghost!
  mm = dynamic.MaxMinForKey(BrickKey(0,0,1));
  TS_ASSERT_DELTA(mm.minScalar, 0.0, 0.001); // likewise w/ 0: ghost.
  TS_ASSERT_DELTA(mm.maxScalar, 63.0, 0.001);
}

void tcache_disable() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{6,16,16}}, cacheBytes);
  verify_half_split(dynamic);
  dynamic.SetCacheSize(0);
  verify_half_split(dynamic);
}

void tengine_four() {
  if(!check_for_engine()) { TS_FAIL("need engine for this test"); }
  std::shared_ptr<UVFDataset> ds(new UVFDataset("engine.uvf", 256, false,
                                                false));
  DynamicBrickingDS dynamic(ds, {{67,256,256}}, cacheBytes,
                            DynamicBrickingDS::MM_SOURCE);
}

void rmi_bench() {
  if(!check_for("rmi")) { TS_FAIL("need RMI for this test."); return; }
  std::shared_ptr<UVFDataset> ds(new UVFDataset("rmi.uvf", 1024, false, false));
  DynamicBrickingDS dynamic(ds, {{68,68,68}}, cacheBytes,
                            DynamicBrickingDS::MM_SOURCE);

  std::vector<uint8_t> data;

  for(size_t rep=0; rep < 4; ++rep) {
    size_t i=0;
    for(auto b=dynamic.BricksBegin(); i < 32 && b != dynamic.BricksEnd(); ++b) {
      dynamic.GetBrick(b->first, data);
      ++i;
    }
  }
#if 0
  double cache_add = Controller::Instance().PerfQuery(PERF_CACHE_ADD);
  double cache_lookup = Controller::Instance().PerfQuery(PERF_CACHE_LOOKUP);
  double something = Controller::Instance().PerfQuery(PERF_SOMETHING);
  fprintf(stderr, "\ncache add: %g\ncache lookup: %g\nsomething: %g\n",
          cache_add, cache_lookup, something);
#endif
}

void trescale() {
  std::shared_ptr<UVFDataset> ds = mk8x8testdata();
  DynamicBrickingDS dynamic(ds, {{6,16,16}}, cacheBytes);

  // should forward to the underlying object
  TS_ASSERT_EQUALS(dynamic.GetRescaleFactors(), ds->GetRescaleFactors());
  // should forward; they are one in the same, so changing either should be
  // visible from the other.
  dynamic.SetRescaleFactors(DOUBLEVECTOR3(2.0, 1.0, 1.0));
  TS_ASSERT_EQUALS(ds->GetRescaleFactors(), DOUBLEVECTOR3(2.0, 1.0, 1.0));

  ds->SetRescaleFactors(DOUBLEVECTOR3(1.0, 2.0, 1.0));
  TS_ASSERT_EQUALS(dynamic.GetRescaleFactors(), DOUBLEVECTOR3(1.0, 2.0, 1.0));

  TS_ASSERT_EQUALS(dynamic.GetScale(), ds->GetScale());

  const size_t lod_dynamic = dynamic.GetLargestSingleBrickLOD(0);
  const size_t lod_root = ds->GetLargestSingleBrickLOD(0);
  const BrickKey k_dynamic(0, lod_dynamic, 0);
  const BrickKey k_root(0, lod_root, 0);
  TS_ASSERT_EQUALS(dynamic.GetBrickExtents(k_dynamic),
                   ds->GetBrickExtents(k_root));
}

void tmulti_component() {
  if(!check_for("vhuman")) { TS_FAIL("need vishuman for this test."); return; }
  std::shared_ptr<UVFDataset> ds(new UVFDataset("vhuman.uvf", 260, false,
                                                false));
  DynamicBrickingDS dynamic(ds, {{260, 260, 260}}, cacheBytes);

  TS_ASSERT_EQUALS(dynamic.GetComponentCount(), ds->GetComponentCount());

  for(size_t i=0; i < 32; ++i) {
    BrickKey bk(0,0,i);
    std::vector<uint8_t> dyndata, srcdata;
    if(!dynamic.GetBrick(bk, dyndata)) { TS_FAIL("failed read dynamic data"); }
    if(!ds->GetBrick(bk, srcdata)) { TS_FAIL("failed read source data"); }

    TS_ASSERT_EQUALS(dyndata.size(), srcdata.size());
    TS_ASSERT(std::equal(dyndata.begin(), dyndata.end(), srcdata.begin()));

    double iso = 42.42;
    TS_ASSERT_EQUALS(dynamic.ContainsData(bk, iso), ds->ContainsData(bk, iso));
  }
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
  void test_brick_sizes() { tbsizes(); }
  void test_precompute() { tprecompute(); }
  void test_minmax_dynamic() { tminmax_dynamic(); }
  void test_cache_disable() { tcache_disable(); }
  void test_engine_four() { tengine_four(); }
  void test_rmi_bench() { rmi_bench(); }
  void test_rescale() { trescale(); }
};
