// * can we chain these recursively?  e.g.:
//      DynBrickingDS a(ds, {{128, 128, 128}});
//      DynBrickingDS b(a, {{16, 16, 16}});
// * ContainsData: deal with new metadata appropriately
#include <algorithm>
#include <array>
#include <cassert>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "Basics/SysTools.h"
#include "Controller/Controller.h"
#include "Controller/StackTimer.h"
#include "BrickCache.h"
#include "DynamicBrickingDS.h"
#include "FileBackedDataset.h"
#include "IOManager.h"
#include "const-brick-iterator.h"
#include "uvfDataset.h"

// This file deals with some tricky indexing.  The convention here is that a
// std::array<unsigned,3> refers to a BRICK index, whereas a
// std::array<uint64_t,3> refers to a VOXEL index.  We also try to use "source"
// in variable names which refer to indices from the data set which actually
// exists, and "target" to refer to indices in the faux/rebricked data set.
typedef std::array<unsigned,3> BrickLayout;
typedef std::array<unsigned,3> BrickIndex;
typedef std::array<uint64_t,3> VoxelIndex;
typedef std::array<uint64_t,3> VoxelLayout;
typedef std::array<size_t,3> BrickSize;

namespace tuvok {

#ifndef NDEBUG
static bool test();
#endif

struct GBPrelim {
  const BrickKey skey;
  const BrickSize tgt_bs;
  const BrickSize src_bs;
  VoxelIndex src_offset;
};

struct DynamicBrickingDS::dbinfo {
  std::shared_ptr<LinearIndexDataset> ds;
  const BrickSize brickSize;
  BrickCache cache;
  size_t cacheBytes;
  std::unordered_map<BrickKey, MinMaxBlock, BKeyHash> minmax;
  enum MinMaxMode mmMode;

  dbinfo(std::shared_ptr<LinearIndexDataset> d,
         BrickSize bs, size_t bytes, enum MinMaxMode mm) :
    ds(d), brickSize(bs), cacheBytes(bytes), mmMode(mm) {}

  // early, non-type-specific parts of GetBrick.
  GBPrelim BrickSetup(const BrickKey&, const DynamicBrickingDS& tgt) const;

  // reads the brick + handles caching
  template<typename T> bool Brick(const DynamicBrickingDS& ds,
                                  const BrickKey& key,
                                  std::vector<T>& data);

  // given the brick key in the dynamic DS, return the corresponding BrickKey
  // in the source data.
  BrickKey SourceBrickKey(const BrickKey&) const;

  BrickLayout TargetBrickLayout(size_t lod, size_t ts) const;

  /// since ComputeMinMaxes is soooo absurdly slow, we try to cache the
  /// results.  These load/save all our min/maxes to a stream.
  ///@{
  void LoadMinMax(std::istream&);
  void SaveMinMax(std::ostream&) const;
  ///@}

  /// run through all of the bricks and compute min/max info.
  void ComputeMinMaxes(BrickedDataset&);

  // sets the cache size (bytes)
  void SetCacheSize(size_t bytes);
  // get the cache size (bytes)
  size_t GetCacheSize() const;

  /// @returns true if 'bytes' bytes will fit into the current cache
  bool FitsInCache(size_t bytes) const;

  void VerifyBrick(const std::pair<BrickKey, BrickMD>& brk) const;

  /// @returns the size of the brick, minus any ghost voxels.
  BrickSize BrickSansGhost() const;

  template<typename T, typename Iter>
  bool CopyBrick(std::vector<T>& dest, Iter src, size_t components,
                 const BrickSize tgt_bs, const BrickSize src_bs,
                 VoxelIndex src_offset);
};

BrickSize SourceMaxBrickSize(const BrickedDataset&);

BrickSize ComputedTargetBrickSize(BrickIndex idx, VoxelLayout voxels,
                                  BrickSize bsize);

// gives the number of ghost voxels (per dimension) in a brick.  must be the
// same for both source and target.
static unsigned ghost(const Dataset& ds) {
  assert(ds.GetBrickOverlapSize()[0] == ds.GetBrickOverlapSize()[1] &&
         ds.GetBrickOverlapSize()[1] == ds.GetBrickOverlapSize()[2]);
  return ds.GetBrickOverlapSize()[0]*2;
}

/// gives the brick layout for a given decomposition. i.e. the number of bricks
/// in each dimension
static std::array<uint64_t,3> layout(const std::array<uint64_t,3> voxels,
                                     const BrickSize bsize) {
  std::array<uint64_t,3> tmp = {{
    static_cast<uint64_t>(ceil(static_cast<float>(voxels[0]) / bsize[0])),
    static_cast<uint64_t>(ceil(static_cast<float>(voxels[1]) / bsize[1])),
    static_cast<uint64_t>(ceil(static_cast<float>(voxels[2]) / bsize[2])),
  }};
  return tmp;
}

// ditto, but the location is unsigned instead of uint64_t.
// remember unsigned is for brick indices, uint64_t is for voxel indices
static uint64_t to1d(const std::array<unsigned,3>& loc,
                     const VoxelLayout& size) {
  assert(loc[2] < size[2]);
  assert(loc[1] < size[1]);
  assert(loc[0] < size[0]);
  return loc[2]*size[1]*size[0] + loc[1]*size[0] + loc[0];
}

DynamicBrickingDS::DynamicBrickingDS(std::shared_ptr<LinearIndexDataset> ds,
                                     BrickSize maxBrickSize, size_t bytes,
                                     enum MinMaxMode mm) :
  di(new DynamicBrickingDS::dbinfo(ds, maxBrickSize, bytes, mm))
{
  this->Rebrick();
}
DynamicBrickingDS::~DynamicBrickingDS() {}

// Many of the methods here just forward to the provided 'Dataset'
// implementation.  This just saves us a bunch of typing.
// 'C' is short for 'const' and 'RET' means it returns something.
#define CFORWARDRET(type, methodName) \
type DynamicBrickingDS::methodName() const { \
  return di->ds->methodName(); \
}
#define FORWARDRET(type, methodName) \
type DynamicBrickingDS::methodName() { \
  return di->ds->methodName(); \
}
CFORWARDRET(float, MaxGradientMagnitude)
CFORWARDRET(std::shared_ptr<const Histogram1D>, Get1DHistogram)
CFORWARDRET(std::shared_ptr<const Histogram2D>, Get2DHistogram)

void DynamicBrickingDS::SetCacheSize(size_t megabytes) {
  /// they give us megabytes, but our internal class deals with bytes!
  const size_t bytes = megabytes * 1024 * 1024;
  this->di->SetCacheSize(bytes);
}

size_t DynamicBrickingDS::GetCacheSize() const {
  /// they give us bytes, but we want megabytes!
  const size_t megabyte = 1024 * 1024;
  return this->di->GetCacheSize() / megabyte;
}

// Removes all the cache information we've made so far.
void DynamicBrickingDS::Clear() {
  di->ds->Clear();
  while(this->di->cache.size() > 0) { this->di->cache.remove(); }
  BrickedDataset::Clear();
  this->Rebrick();
}

// with the layout and 1D index, convert into the 3D index.
// we use it to convert 1D brick indices into 3D brick indices
static std::array<unsigned,3>
to3d(const std::array<uint64_t,3> dim, uint64_t idx) {
  assert(dim[0] > 0); assert(dim[1] > 0); assert(dim[2] > 0);
  assert(idx < (dim[0]*dim[1]*dim[2]));

  std::array<unsigned,3> tmp = {{
    static_cast<unsigned>(idx % dim[0]),
    static_cast<unsigned>((idx / dim[0]) % dim[1]),
    static_cast<unsigned>(idx / (dim[0] * dim[1]))
  }};
  assert(tmp[0] < dim[0]);
  assert(tmp[1] < dim[1]);
  assert(tmp[2] < dim[2]);
  return tmp;
}

// what is our brick layout (how many bricks in each dimension) in the
// given source dataset?
BrickLayout SourceBrickLayout(const std::shared_ptr<LinearIndexDataset> ds,
                              size_t lod, size_t timestep) {
  const UINTVECTOR3 layout = ds->GetBrickLayout(lod, timestep);
  const BrickLayout tmp = {{
    static_cast<unsigned>(layout[0]),
    static_cast<unsigned>(layout[1]),
    static_cast<unsigned>(layout[2])
  }};
  return tmp;
}

BrickSize BrickSansGhost(BrickSize bsize) {
#if 0
  bsize[0] = bsize[0] > ghost() ? bsize[0]-ghost() : bsize[0];
  bsize[1] = bsize[1] > ghost() ? bsize[1]-ghost() : bsize[1];
  bsize[2] = bsize[2] > ghost() ? bsize[2]-ghost() : bsize[2];
#else
  bsize[0] = bsize[0] > 4 ? bsize[0]-4 : bsize[0];
  bsize[1] = bsize[1] > 4 ? bsize[1]-4 : bsize[1];
  bsize[2] = bsize[2] > 4 ? bsize[2]-4 : bsize[2];
#endif
  return bsize;
}
BrickSize DynamicBrickingDS::dbinfo::BrickSansGhost() const {
  const size_t gh = ghost(*this->ds);
  BrickSize bsize = {{ this->brickSize[0] - gh, this->brickSize[1] - gh,
                       this->brickSize[2] - gh }};
  return bsize;
}

// gives the number of bricks in each dimension.
// @param bsize the brick size to use WITHOUT ghost voxels!
static const BrickLayout GenericBrickLayout(
  const VoxelLayout voxels,
  const BrickSize bsize
) {
  const BrickLayout blayout = {{
    static_cast<unsigned>(layout(voxels, bsize)[0]),
    static_cast<unsigned>(layout(voxels, bsize)[1]),
    static_cast<unsigned>(layout(voxels, bsize)[2])
  }};
  assert(blayout[0] > 0);
  assert(blayout[1] > 0);
  assert(blayout[2] > 0);
  return blayout;
}

// identifies the number of bricks we have in the target dataset for
// each brick in the source dataset. that is, how many bricks we're
// stuffing into one brick.
// this fits in integer numbers because we enforce that rebricking subdivides
// the original volume/bricks nicely.
std::array<unsigned,3> TargetBricksPerSource(BrickSize src, BrickSize tgt) {
  std::array<unsigned,3> rv = {{
    static_cast<unsigned>(src[0] / tgt[0]),
    static_cast<unsigned>(src[1] / tgt[1]),
    static_cast<unsigned>(src[2] / tgt[2])
  }};
  assert(rv[0] > 0);
  assert(rv[1] > 0);
  assert(rv[2] > 0);
  return rv;
}

// with a brick identifier from the target dataset, find the 3D brick index in
// the source dataset.
// basic idea:
//   1. how many voxels do we have in this LOD?  how big are our bricks?
//   2. values in (1) should divide evenly; this lets us convert VOXEL indices
//      to BRICK indices.
//   3. we know how our 3D bricks are layed out via (2); use that to convert
//      our 1D-brick-index into a 3D-brick-index
//   4. identify how many bricks we have in the target data set for each brick
//      in the source dataset.
//   5. divide the computed index (3) by our ratio (4).  lop off any remainder.
BrickIndex SourceBrickIndex(const BrickKey& k,
                            const BrickedDataset& ds,
                            const BrickSize bsize)
{
  // See the comment Rebrick: we shouldn't have more LODs than the source data.
  assert(std::get<1>(k) < ds.GetLODLevelCount());
  const size_t lod = std::min(std::get<1>(k),
                              static_cast<size_t>(ds.GetLODLevelCount()));
  const size_t timestep = std::get<0>(k);
  // identify how many voxels we've got
  const VoxelLayout voxels = {{
    ds.GetDomainSize(lod, timestep)[0],
    ds.GetDomainSize(lod, timestep)[1],
    ds.GetDomainSize(lod, timestep)[2]
  }};
  // now we know how many voxels we've got.  we can use that to convert the 1D
  // index we have back into the 3D index.
  const size_t idx1d = std::get<2>(k);
  BrickIndex idx = to3d(layout(voxels, BrickSansGhost(bsize)), idx1d);

  const BrickSize tgt_bs = BrickSansGhost(bsize);
  const BrickSize src_bs = SourceMaxBrickSize(ds);
  const std::array<unsigned,3> bricks_per_src =
    TargetBricksPerSource(src_bs, tgt_bs);
  BrickIndex rv = {{
    idx[0] / bricks_per_src[0],
    idx[1] / bricks_per_src[1],
    idx[2] / bricks_per_src[2]
  }};
  return rv;
}

VoxelIndex OffsetIntoSource(const BrickedDataset& src,
                            const BrickKey& tgtkey,
                            const BrickSize bsize /* target */)
{
  // See the comment Rebrick: we shouldn't have more LODs than the source data.
  assert(std::get<1>(tgtkey) < src.GetLODLevelCount());
  const size_t lod = std::min(std::get<1>(tgtkey),
                              static_cast<size_t>(src.GetLODLevelCount()));
  const size_t timestep = std::get<0>(tgtkey);
  // identify how many voxels we've got
  const VoxelLayout voxels = {{
    src.GetDomainSize(lod, timestep)[0],
    src.GetDomainSize(lod, timestep)[1],
    src.GetDomainSize(lod, timestep)[2]
  }};
  // now we know how many voxels we've got.  we can use that to convert the 1D
  // index we have back into the 3D index.
  const size_t idx1d = std::get<2>(tgtkey);
  BrickIndex idx = to3d(layout(voxels, BrickSansGhost(bsize)), idx1d);

  const BrickSize tgt_bs = BrickSansGhost(bsize);
  const BrickSize src_bs = SourceMaxBrickSize(src);
  const std::array<unsigned,3> bricks_per_src =
    TargetBricksPerSource(src_bs, tgt_bs);
  VoxelIndex rv = {{
    (idx[0] % bricks_per_src[0]) * tgt_bs[0],
    (idx[1] % bricks_per_src[1]) * tgt_bs[1],
    (idx[2] % bricks_per_src[2]) * tgt_bs[2]
  }};
  return rv;
}

// @returns the number of voxels in the given level of detail.
VoxelLayout VoxelsInLOD(const Dataset& ds, size_t lod) {
  const size_t timestep = 0; /// @todo properly implement.
  UINT64VECTOR3 domain = ds.GetDomainSize(lod, timestep);
  VoxelLayout tmp = {{ domain[0], domain[1], domain[2] }};
  return tmp;
}

// @return the brick size which the given dataset *tries* to use.  Of course,
// if the bricks don't fit evenly, there will be some bricks on the edge which
// are smaller.
BrickSize SourceMaxBrickSize(const BrickedDataset& ds) {
  BrickSize src_bs = {{
    static_cast<size_t>(ds.GetMaxBrickSize()[0])-ghost(ds),
    static_cast<size_t>(ds.GetMaxBrickSize()[1])-ghost(ds),
    static_cast<size_t>(ds.GetMaxBrickSize()[2])-ghost(ds),
  }};
  assert(src_bs[0] > 0 && src_bs[0] < 65535); // must make sense.
  assert(src_bs[1] > 0 && src_bs[1] < 65535);
  assert(src_bs[2] > 0 && src_bs[2] < 65535);
  return src_bs;
}

// with the source brick index, give a brick key for the source dataset.
BrickKey SourceKey(const BrickIndex brick_idx, size_t lod,
                   const BrickedDataset& ds) {
  const VoxelLayout src_voxels = VoxelsInLOD(ds, lod);
  const BrickSize src_bricksize = SourceMaxBrickSize(ds);
  const size_t timestep = 0; /// @todo properly implement
  return BrickKey(timestep, lod,
                  to1d(brick_idx, layout(src_voxels, src_bricksize)));
}

// figure out the voxel index of the upper left corner of a brick
static VoxelIndex Index(
  const Dataset& ds, size_t lod, uint64_t idx1d,
  const BrickSize bricksize
) {
  const VoxelLayout voxels = VoxelsInLOD(ds, lod);
  const BrickIndex idx3d = to3d(
    layout(voxels, BrickSansGhost(bricksize)), idx1d
  );
  VoxelIndex idx = {{ 0, 0, 0 }};

  for(size_t x=0; x < idx3d[0]; ++x) {
    BrickIndex i = {{ unsigned(x), 0, 0 }};
    idx[0] += ComputedTargetBrickSize(i, voxels, bricksize)[0] - ghost(ds);
  }
  for(size_t y=0; y < idx3d[1]; ++y) {
    BrickIndex i = {{ 0, unsigned(y), 0 }};
    idx[1] += ComputedTargetBrickSize(i, voxels, bricksize)[1] - ghost(ds);
  }
  for(size_t z=0; z < idx3d[2]; ++z) {
    BrickIndex i = {{ 0, 0, unsigned(z) }};
    idx[2] += ComputedTargetBrickSize(i, voxels, bricksize)[2] - ghost(ds);
  }
  return idx;
}

// index of the first voxel of the given brick, among the whole level.
VoxelIndex TargetIndex(const BrickKey& k, const Dataset& ds,
                       const BrickSize bricksize) {
  const size_t idx1d = std::get<2>(k);
  const size_t lod = std::get<1>(k);

  return Index(ds, lod, idx1d, bricksize);
}

// index of the first voxel in the current brick, among the whole level
VoxelIndex SourceIndex(const BrickKey& k, const BrickedDataset& ds) {
  const size_t lod = std::get<1>(k);
  const size_t idx1d = std::get<2>(k);

  const BrickSize src_bs = SourceMaxBrickSize(ds);
  return Index(ds, lod, idx1d, src_bs);
}

BrickSize ComputedTargetBrickSize(BrickIndex idx, VoxelLayout voxels,
                                  BrickSize bsize) {
  const VoxelIndex bl = layout(voxels, BrickSansGhost(bsize));
  std::array<bool,3> last = {{
    idx[0] == bl[0]-1,
    idx[1] == bl[1]-1,
    idx[2] == bl[2]-1,
  }};
  const BrickSize no_ghost = BrickSansGhost(bsize);
  const BrickSize extra = {{
    static_cast<size_t>(voxels[0] % no_ghost[0]),
    static_cast<size_t>(voxels[1] % no_ghost[1]),
    static_cast<size_t>(voxels[2] % no_ghost[2])
  }};
  const BrickSize rv = {{
    last[0] && extra[0] ? 4 + extra[0] : bsize[0],
    last[1] && extra[1] ? 4 + extra[1] : bsize[1],
    last[2] && extra[2] ? 4 + extra[2] : bsize[2]
  }};
  return rv;
}

// gives the size of the given brick from the target DS
BrickSize TargetBrickSize(const Dataset& tgt, const BrickKey& k) {
  const BrickedDataset& b = dynamic_cast<const BrickedDataset&>(tgt);
  UINTVECTOR3 sz = b.GetBrickMetadata(k).n_voxels;
  BrickSize tmp = {{
    static_cast<size_t>(sz[0]),
    static_cast<size_t>(sz[1]),
    static_cast<size_t>(sz[2]),
  }};
  return tmp;
}
// gives the size of the given brick from the source DS
BrickSize SourceBrickSize(const BrickedDataset& src, const BrickKey& k) {
  const BrickedDataset& b = dynamic_cast<const BrickedDataset&>(src);
  UINTVECTOR3 sz = b.GetBrickMetadata(k).n_voxels;
  BrickSize tmp = {{
    static_cast<size_t>(sz[0]),
    static_cast<size_t>(sz[1]),
    static_cast<size_t>(sz[2])
  }};
  return tmp;
}

// given the brick key in the dynamic DS, return the corresponding BrickKey
// in the source data.
BrickKey DynamicBrickingDS::dbinfo::SourceBrickKey(const BrickKey& k) const {
  const size_t lod = std::get<1>(k);
  const BrickIndex src_bidx = SourceBrickIndex(k, *this->ds, this->brickSize);
  BrickKey skey = SourceKey(src_bidx, lod, *(this->ds));
#ifndef NDEBUG
  // brick key should make sense.
  std::shared_ptr<BrickedDataset> bds =
    std::dynamic_pointer_cast<BrickedDataset>(this->ds);
  assert(std::get<0>(skey) < bds->GetNumberOfTimesteps());
  assert(std::get<2>(skey) < bds->GetTotalBrickCount());
#endif
  MESSAGE("keymap query: <%u,%u,%u> -> <%u,%u,%u>",
          static_cast<unsigned>(std::get<0>(k)),
          static_cast<unsigned>(std::get<1>(k)),
          static_cast<unsigned>(std::get<2>(k)),
          static_cast<unsigned>(std::get<0>(skey)),
          static_cast<unsigned>(std::get<1>(skey)),
          static_cast<unsigned>(std::get<2>(skey)));
  return skey;
}

BrickLayout
DynamicBrickingDS::dbinfo::TargetBrickLayout(size_t lod, size_t ts) const {
  const VoxelLayout voxels = {{
    this->ds->GetDomainSize(lod, ts)[0],
    this->ds->GetDomainSize(lod, ts)[1],
    this->ds->GetDomainSize(lod, ts)[2]
  }};
  BrickLayout tgt_blayout = GenericBrickLayout(voxels, this->BrickSansGhost());
  return tgt_blayout;
}

// This is the type-dependent part of ::GetBrick.  Basically, the copying of
// the source data into the target brick.
template<typename T, typename Iter>
bool DynamicBrickingDS::dbinfo::CopyBrick(
  std::vector<T>& dest, const Iter srcdata, size_t components,
  const BrickSize tgt_bs, const BrickSize src_bs,
  VoxelIndex src_offset
) {
  assert(tgt_bs[0] <= src_bs[0] && "target can't be larger than source");
  assert(tgt_bs[1] <= src_bs[1] && "target can't be larger than source");
  assert(tgt_bs[2] <= src_bs[2] && "target can't be larger than source");

  // make sure the vector is big enough.
  //assert(dest.size() >= (tgt_bs[0]*tgt_bs[1]*tgt_bs[2])); // be sure!
  dest.resize(tgt_bs[0]*tgt_bs[1]*tgt_bs[2]*components);

  const VoxelIndex orig_offset = src_offset;

  // our copy size/scanline size is the width of our target brick.
  const size_t scanline = tgt_bs[0] * components;

  for(uint64_t z=0; z < tgt_bs[2]; ++z) {
    for(uint64_t y=0; y < tgt_bs[1]; ++y) {
      const uint64_t x = 0;
      const uint64_t tgt_offset = (z*tgt_bs[0]*tgt_bs[1] + y*tgt_bs[0] + x) *
                                   components;

      const uint64_t src_o = (src_offset[2]*src_bs[0]*src_bs[1] +
                              src_offset[1]*src_bs[0] + src_offset[0]) *
                              components;
#if 0
      // memcpy-based: works fast even in debug.
      std::copy(srcdata.data()+src_o, srcdata.data()+src_o+scanline,
                dest.data()+tgt_offset);
#else
      // iterators: gives nice error messages in debug.
      std::copy(srcdata+src_o, srcdata+src_o+scanline,
                dest.begin()+tgt_offset);
#endif
      src_offset[1]++; // should follow 'y' increment.
    }
    src_offset[1] = orig_offset[1];
    src_offset[2]++; // .. and increment z
  }
  return true;
}

// early, non-type-specific parts of GetBrick.
// Note that because of how we do the re-bricking, we know that all the target
// bricks will fit nicely inside a (single) source brick.  This is important,
// becuase otherwise we'd have to read a bunch of bricks from the source, and
// copy pieces from all of them.
GBPrelim
DynamicBrickingDS::dbinfo::BrickSetup(const BrickKey& k,
                                      const DynamicBrickingDS& tgt) const {
  assert(tgt.bricks.find(k) != tgt.bricks.end());

  const BrickKey skey = this->SourceBrickKey(k);
  const BrickSize tgt_bs = TargetBrickSize(tgt, k);
  const BrickSize src_bs = SourceBrickSize(*this->ds, skey);

  assert(tgt_bs[0] <= src_bs[0] && "target can't be larger than source");
  assert(tgt_bs[1] <= src_bs[1] && "target can't be larger than source");
  assert(tgt_bs[2] <= src_bs[2] && "target can't be larger than source");

  // Unless this (target) brick sits at the bottom corner of the source brick,
  // we'll need to start reading from the source brick at an offset.  What is
  // that offset?
  VoxelIndex src_offset = OffsetIntoSource(*this->ds, k, this->brickSize);
  GBPrelim rv = { skey, tgt_bs, src_bs, src_offset };
  return rv;
}

// Looks for the brick in the cache; if so, uses it.  Otherwise, grab the brick
template<typename T>
bool DynamicBrickingDS::dbinfo::Brick(const DynamicBrickingDS& ds,
                                      const BrickKey& key,
                                      std::vector<T>& data) {
  StackTimer gbrick(PERF_DY_GET_BRICK);
  GBPrelim pre = this->BrickSetup(key, ds);

  const void* lookup;
  {
    tuvok::Controller::Instance().IncrementPerfCounter(PERF_DY_CACHE_LOOKUPS, 1.0);
    StackTimer cc(PERF_DY_CACHE_LOOKUP);
    lookup = this->cache.lookup(pre.skey, T(42));
  }
  // first: check the cache and see if we can get the data easy.
  if(NULL != lookup) {
    MESSAGE("found <%u,%u,%u> in the cache!",
            static_cast<unsigned>(std::get<0>(pre.skey)),
            static_cast<unsigned>(std::get<1>(pre.skey)),
            static_cast<unsigned>(std::get<2>(pre.skey)));
    tuvok::Controller::Instance().IncrementPerfCounter(PERF_DY_BRICK_COPIED, 1.0);
    StackTimer copies(PERF_DY_BRICK_COPY);
    const T* srcdata = static_cast<const T*>(lookup);
    const size_t components = this->ds->GetComponentCount();
    return this->CopyBrick<T>(data, srcdata, components, pre.tgt_bs, pre.src_bs,
                              pre.src_offset);
  }
  // nope?  oh well.  read it.
  std::vector<T> srcdata;
  {
    StackTimer loadBrick(PERF_DY_RESERVE_BRICK);
    srcdata.resize(this->ds->GetMaxBrickSize().volume());
  }
  {
    StackTimer loadBrick(PERF_DY_LOAD_BRICK);
    if(!this->ds->GetBrick(pre.skey, srcdata)) { return false; }
  }

  // add it to the cache.
  const T* sdata = static_cast<const T*>(srcdata.data());
  if(this->cacheBytes > 0) {
    tuvok::Controller::Instance().IncrementPerfCounter(PERF_DY_CACHE_ADDS, 1.0);
    StackTimer cc(PERF_DY_CACHE_ADD);
    // is the cache full?  find room.
    while(!this->FitsInCache(srcdata.size() * sizeof(T))) {
      this->cache.remove();
    }
    sdata = static_cast<const T*>(this->cache.add(pre.skey, srcdata));
  }
  const size_t components = this->ds->GetComponentCount();
  tuvok::Controller::Instance().IncrementPerfCounter(PERF_DY_BRICK_COPIED, 1.0);
  StackTimer copies(PERF_DY_BRICK_COPY);
  return this->CopyBrick<T>(data, sdata, components, pre.tgt_bs, pre.src_bs,
                            pre.src_offset);
}


namespace {
  template<typename T> MinMaxBlock mm(const BrickKey& bk,
                                      const BrickedDataset& ds) {
    std::vector<T> data(ds.GetMaxBrickSize().volume());
    ds.GetBrick(bk, data);
    auto mmax = std::minmax_element(data.begin(), data.end());
    return MinMaxBlock(*mmax.first, *mmax.second, DBL_MAX, -FLT_MAX);
  }
}

MinMaxBlock minmax_brick(const BrickKey& bk, const BrickedDataset& ds) {
  // identify type (float, etc)
  const unsigned size = ds.GetBitWidth() / 8;
  assert(ds.GetComponentCount() == 1);
  const bool sign = ds.GetIsSigned();
  const bool fp = ds.GetIsFloat();

  // giant if-block to simply call the right compile-time function w/ knowledge
  // we only know at run-time.
  if(!sign && !fp && size == 1) {
    return mm<uint8_t>(bk, ds);
  } else if(!sign && !fp && size == 2) {
    return mm<uint16_t>(bk, ds);
  } else if(!sign && !fp && size == 4) {
    return mm<uint32_t>(bk, ds);
  } else {
    T_ERROR("unsupported type.");
    assert(false);
  }
  return MinMaxBlock();
}

/// we can cache the precomputed brick min/maxes in a file, and then
/// just read those. this can be a big win, since the calculation is
/// veeeery slow.
/// @return the file we would save for this case.
static std::string precomputed_filename(const BrickedDataset& ds,
                                        const BrickSize bsize)
{
  try {
    std::ostringstream fname;
    const FileBackedDataset& fbds = dynamic_cast<const FileBackedDataset&>(ds);
    fname << "." << bsize[0] << "x" << bsize[1] << "x" << bsize[2] << "-"
          << SysTools::GetFilename(fbds.Filename()) << ".cached";
    return fname.str();
  } catch(const std::bad_cast&) {
    WARNING("Data doesn't come from a file.  We can't save minmaxes.");
  }
  return "";
}

void DynamicBrickingDS::dbinfo::LoadMinMax(std::istream& is) {
  if(!is) { T_ERROR("could not open min/max cache!"); return; }

  uint64_t n_elems;
  is.read(reinterpret_cast<char*>(&n_elems), sizeof(uint64_t));

  for(uint64_t i=0; i < n_elems; ++i) {
    uint64_t timestep, lod, brick;
    is.read(reinterpret_cast<char*>(&timestep), sizeof(uint64_t));
    is.read(reinterpret_cast<char*>(&lod), sizeof(uint64_t));
    is.read(reinterpret_cast<char*>(&brick), sizeof(uint64_t));
    const BrickKey key(timestep, lod, brick);
    MinMaxBlock mm;
    is.read(reinterpret_cast<char*>(&mm.minScalar), sizeof(double));
    is.read(reinterpret_cast<char*>(&mm.maxScalar), sizeof(double));
    if(!is) {
      T_ERROR("read failed?  cache is broken.  ignoring it.");
      this->minmax.clear();
      return;
    }
    this->minmax.insert(std::make_pair(key, mm));
  }
}

void DynamicBrickingDS::dbinfo::SaveMinMax(std::ostream& os) const {
  if(!os) { T_ERROR("could not create min/max cache."); return; }

  const uint64_t n_elems = this->minmax.size();
  MESSAGE("Saving %llu brick min/maxes", n_elems);
  os.write(reinterpret_cast<const char*>(&n_elems), sizeof(uint64_t));

  for(auto b=this->minmax.cbegin(); b != this->minmax.cend(); ++b) {
    uint64_t timestep, lod, brick;
    timestep = std::get<0>(b->first);
    lod = std::get<1>(b->first);
    brick = std::get<2>(b->first);
    os.write(reinterpret_cast<const char*>(&timestep), sizeof(uint64_t));
    os.write(reinterpret_cast<const char*>(&lod), sizeof(uint64_t));
    os.write(reinterpret_cast<const char*>(&brick), sizeof(uint64_t));
    MinMaxBlock mm = b->second;
    os.write(reinterpret_cast<const char*>(&mm.minScalar), sizeof(double));
    os.write(reinterpret_cast<const char*>(&mm.maxScalar), sizeof(double));
  }
}

/// run through all of the bricks and compute min/max info.
void DynamicBrickingDS::dbinfo::ComputeMinMaxes(BrickedDataset& ds) {
  // first, check if we have this cached.
  const std::string fname = precomputed_filename(ds, this->brickSize);
  if(SysTools::FileExists(fname)) {
    MESSAGE("Brick min/maxes are precomputed.  Reloading from file...");
    std::ifstream mmfile(fname, std::ios::binary);
    this->LoadMinMax(mmfile);
    mmfile.close();
    return;
  }

  {
    StackTimer precompute(PERF_MM_PRECOMPUTE);
    unsigned i=0;
    const unsigned len = static_cast<unsigned>(
      std::distance(ds.BricksBegin(), ds.BricksEnd())
    );
    for(auto b=ds.BricksBegin(); b != ds.BricksEnd(); ++b, ++i) {
      MESSAGE("precomputing brick %u of %u", i, len);
      MinMaxBlock mm = minmax_brick(b->first, ds);
      this->minmax.insert(std::make_pair(b->first, mm));
      while(this->cache.size() > this->cacheBytes) { this->cache.remove(); }
    }
  }
  // remove all cached bricks
  while(this->cache.size() > 0) { this->cache.remove(); }

  // try to cache that data to a file, now.
  std::ofstream mmcache(fname, std::ios::binary);
  if(!mmcache) {
    WARNING("could not open min/max cache file (%s); ignoring cache.",
            fname.c_str());
    return;
  }
  this->SaveMinMax(mmcache);
  mmcache.close();
}

void DynamicBrickingDS::dbinfo::SetCacheSize(size_t bytes) {
  this->cacheBytes = bytes;
  // shrink the cache to fit.
  while(this->cache.size() > this->cacheBytes) { this->cache.remove(); }
}

size_t DynamicBrickingDS::dbinfo::GetCacheSize() const {
  return this->cacheBytes;
}

/// @returns true if 'bytes' bytes will fit into the current cache
bool DynamicBrickingDS::dbinfo::FitsInCache(size_t bytes) const {
  return bytes < this->cacheBytes;
}

bool DynamicBrickingDS::GetBrick(const BrickKey& k, std::vector<uint8_t>& data) const
{
  return this->di->Brick<uint8_t>(*this, k, data);
}
bool DynamicBrickingDS::GetBrick(const BrickKey& k,
                                 std::vector<uint16_t>& data) const
{
  return this->di->Brick<uint16_t>(*this, k, data);
}
bool DynamicBrickingDS::GetBrick(const BrickKey& k,
                                 std::vector<uint32_t>& data) const
{
  return this->di->Brick<uint32_t>(*this, k, data);
}

void DynamicBrickingDS::SetRescaleFactors(const DOUBLEVECTOR3& scale) {
  this->di->ds->SetRescaleFactors(scale);
}
CFORWARDRET(DOUBLEVECTOR3, GetRescaleFactors)

/// If the underlying file format supports it, save the current scaling
/// factors to the file.  The format should implicitly load and apply the
/// scaling factors when opening the file.
FORWARDRET(bool, SaveRescaleFactors)
CFORWARDRET(DOUBLEVECTOR3, GetScale);

CFORWARDRET(unsigned, GetLODLevelCount)
CFORWARDRET(uint64_t, GetNumberOfTimesteps)
UINT64VECTOR3 DynamicBrickingDS::GetDomainSize(const size_t lod,
                                               const size_t ts) const {
  return this->di->ds->GetDomainSize(lod, ts);
}
CFORWARDRET(UINTVECTOR3, GetBrickOverlapSize)
/// @return the number of voxels for the given brick, per dimension, taking
///         into account any brick overlaps.
UINT64VECTOR3 DynamicBrickingDS::GetEffectiveBrickSize(const BrickKey&) const
{
  assert(false); // unused.
  return UINT64VECTOR3(0,0,0);
}

UINTVECTOR3 DynamicBrickingDS::GetMaxBrickSize() const {
  return UINTVECTOR3(this->di->brickSize[0], this->di->brickSize[1],
                     this->di->brickSize[2]);
}
UINTVECTOR3 DynamicBrickingDS::GetBrickLayout(size_t lod, size_t ts) const {
  const BrickLayout lout = this->di->TargetBrickLayout(lod, ts);
  return UINTVECTOR3(lout[0], lout[1], lout[2]);
}

CFORWARDRET(unsigned, GetBitWidth)
CFORWARDRET(uint64_t, GetComponentCount)
CFORWARDRET(bool, GetIsSigned)
CFORWARDRET(bool, GetIsFloat)
CFORWARDRET(bool, IsSameEndianness)
std::pair<double,double> DynamicBrickingDS::GetRange() const {
  return di->ds->GetRange();
}

/// Acceleration queries.
/// Right now, they just forward to the larger data set.  We might consider
/// recomputing this metadata, to get better performance at the expense of
/// memory.
///@{
bool DynamicBrickingDS::ContainsData(const BrickKey& bk, double isoval) const {
  assert(this->bricks.find(bk) != this->bricks.end());
  BrickKey skey = this->di->SourceBrickKey(bk);
  return di->ds->ContainsData(skey, isoval);
}
bool DynamicBrickingDS::ContainsData(const BrickKey& bk, double fmin,
                                     double fmax) const {
  assert(this->bricks.find(bk) != this->bricks.end());
  BrickKey skey = this->di->SourceBrickKey(bk);
  return di->ds->ContainsData(skey, fmin, fmax);
}
bool DynamicBrickingDS::ContainsData(const BrickKey& bk,
                                     double fmin, double fmax,
                                     double fminGradient,
                                     double fmaxGradient) const {
  assert(this->bricks.find(bk) != this->bricks.end());
  BrickKey skey = this->di->SourceBrickKey(bk);
  return di->ds->ContainsData(skey, fmin,fmax, fminGradient, fmaxGradient);
}

MinMaxBlock DynamicBrickingDS::MaxMinForKey(const BrickKey& bk) const {
  switch(this->di->mmMode) {
    case MM_SOURCE: {
      BrickKey skey = this->di->SourceBrickKey(bk);
      return di->ds->MaxMinForKey(skey);
    } break;
    case MM_DYNAMIC: return minmax_brick(bk, *this); break;
    case MM_PRECOMPUTE: {
      assert(this->di->minmax.find(bk) != this->di->minmax.end());
      return this->di->minmax.find(bk)->second;
    } break;
  }
  return MinMaxBlock();
}
///@}

bool DynamicBrickingDS::Export(uint64_t lod, const std::string& to,
                               bool append) const {
  return di->ds->Export(lod, to, append);
}

bool DynamicBrickingDS::ApplyFunction(uint64_t lod,
                        bool (*brickFunc)(void* pData,
                                          const UINT64VECTOR3& vBrickSize,
                                          const UINT64VECTOR3& vBrickOffset,
                                          void* pUserContext),
                        void *pUserContext,
                        uint64_t iOverlap) const {
  T_ERROR("This probably doesn't work.");
  return di->ds->ApplyFunction(lod, brickFunc, pUserContext, iOverlap);
}

const char* DynamicBrickingDS::Name() const {
  return "Rebricked Data";
}

// Virtual constructor.  Hard to make sense of this in the IOManager's
// context; this isn't a register-able Dataset type which tuvok can
// automatically instantiate to read a dataset.  Rather, the user must *have*
// such a dataset already and use this as a proxy for it.
DynamicBrickingDS* DynamicBrickingDS::Create(const std::string&, uint64_t,
                                             bool) const {
  abort(); return NULL;
}

std::string DynamicBrickingDS::Filename() const {
  const FileBackedDataset& f = dynamic_cast<const FileBackedDataset&>(
    *(this->di->ds.get())
  );
  return f.Filename();
}

bool DynamicBrickingDS::CanRead(const std::string&,
                                const std::vector<int8_t>&) const
{
  return false;
}
bool DynamicBrickingDS::Verify(const std::string&) const {
  T_ERROR("you shouldn't use a dynamic bricking DS to verify a file!");
  assert(false);
  return false;
}
std::list<std::string> DynamicBrickingDS::Extensions() const {
  WARNING("You should be calling this on the underlying DS.  I'll do that "
          "for you, I guess...");
  std::shared_ptr<FileBackedDataset> fbds =
    std::dynamic_pointer_cast<FileBackedDataset>(this->di->ds);
  return fbds->Extensions();
}

// computes the layout when transitioning to a new level.
// just convenience for dividing by 2 and checking to make sure a dimension
// doesn't go to 0.
static UINTVECTOR3 layout_next_level(UINTVECTOR3 layout) {
  layout = layout / 2;
  if(layout[0] == 0) { layout[0] = 1; }
  if(layout[1] == 0) { layout[1] = 1; }
  if(layout[2] == 0) { layout[2] = 1; }
  return layout;
}

// identifies the number of a bricks a data set will have, when divided into
// bricks.
static uint64_t nbricks(const VoxelLayout& voxels,
                        const BrickSize& bricksize) {
  assert(voxels[0] > 0); assert(bricksize[0] > 0);
  assert(voxels[1] > 0); assert(bricksize[1] > 0);
  assert(voxels[2] > 0); assert(bricksize[2] > 0);
  UINTVECTOR3 blayout(voxels[0] / bricksize[0], voxels[1] / bricksize[1],
                      voxels[2] / bricksize[2]);
  // if the brick size is bigger than the number of voxels, we'll end up with 0
  // in that dimension!
  if(blayout[0] == 0) { blayout[0] = 1; }
  if(blayout[1] == 0) { blayout[1] = 1; }
  if(blayout[2] == 0) { blayout[2] = 1; }

  uint64_t nb = 1;
  while(blayout != UINTVECTOR3(1,1,1)) {
    nb += blayout.volume();
    blayout = layout_next_level(blayout);
  }
  return nb;
}

// @returns true if a is a multiple of b
static bool integer_multiple(unsigned a, unsigned b) {
  // we need to limit it somewhere so that we ensure a*i doesn't overflow.
  // this is overly conservative, but more than okay for the uses right now.
  assert(a <= 256);
  for(size_t i=0; i < 128; ++i) {
    if(a*i == b) { return true; }
  }
  return false;
}

// what are the low/high points of our data set?  Interestingly, we don't have
// a way to query this from the Dataset itself.  So we find a LOD which is just
// one brick, and then see how big that brick is.
std::array<std::array<float,3>,2>
DatasetExtents(const std::shared_ptr<Dataset>& ds) {
  std::shared_ptr<BrickedDataset> bds =
    std::dynamic_pointer_cast<BrickedDataset>(ds);
  const size_t timestep = 0;
  const size_t lod = bds->GetLargestSingleBrickLOD(timestep);
  const BrickKey key(timestep, lod, 0);
  FLOATVECTOR3 extents = bds->GetBrickExtents(key);

  std::array<std::array<float,3>,2> rv;
  typedef std::array<float,3> vf;
  vf elow =  {{ -(extents[0]/2.0f), -(extents[1]/2.0f), -(extents[2]/2.0f) }};
  vf ehigh = {{  (extents[0]/2.0f),  (extents[1]/2.0f),  (extents[2]/2.0f) }};
  rv[0] = elow; rv[1] = ehigh;

  assert(rv[1][0] >= rv[0][0]);
  assert(rv[1][1] >= rv[0][1]);
  assert(rv[1][2] >= rv[0][2]);
  return rv;
}

void DynamicBrickingDS::dbinfo::VerifyBrick(
#ifndef NDEBUG
  const std::pair<BrickKey, BrickMD>& brk) const
#else
  const std::pair<BrickKey, BrickMD>&) const
#endif
{
  const BrickSize src_bs = SourceMaxBrickSize(*this->ds);

  if(this->brickSize[0] == src_bs[0] &&
     this->brickSize[1] == src_bs[1] &&
     this->brickSize[2] == src_bs[2]) {
    // if we "Re"brick to the same size bricks, then all
    // the bricks we create should also exist in the source
    // dataset.
    assert(brk.first == this->SourceBrickKey(brk.first));
  }
#ifndef NDEBUG
  const BrickKey& srckey = this->SourceBrickKey(brk.first);
#endif
  // brick we're creating can't be larger than the brick it reads from.
  assert(brk.second.n_voxels[0] <= SourceBrickSize(*this->ds, srckey)[0]);
  assert(brk.second.n_voxels[1] <= SourceBrickSize(*this->ds, srckey)[1]);
  assert(brk.second.n_voxels[2] <= SourceBrickSize(*this->ds, srckey)[2]);

  std::array<std::array<float,3>,2> extents = DatasetExtents(this->ds);
  const FLOATVECTOR3 fullexts(
    extents[1][0] - extents[0][0],
    extents[1][1] - extents[0][1],
    extents[1][2] - extents[0][2]
  );
  assert(brk.second.extents[0] <= fullexts[0]);
  assert(brk.second.extents[1] <= fullexts[1]);
  assert(brk.second.extents[2] <= fullexts[2]);
}

struct ExtCenter { FLOATVECTOR3 exts, center; };
ExtCenter BrickMetadata(size_t x, size_t y, size_t z,
                        const BrickSize& size,
                        const BrickSize& bsize,
                        const VoxelLayout voxels,
                        const std::array<std::array<float,3>,2>& extents) {
  // lower left coordinate of the brick, in voxels
  const UINT64VECTOR3 voxlow(x*bsize[0], y*bsize[1], z*bsize[2]);
  // the high coord is the low coord + the number of voxels in the brick
  const UINT64VECTOR3 voxhigh(voxlow[0]+size[0], voxlow[1]+size[1],
                              voxlow[2]+size[2]);
  // where the center of this brick would be, in voxels.  note this is FP:
  // the 'center' could be a half-voxel in, if the brick has an odd number of
  // voxels.
  const std::array<float,3> vox_center = {{
    ((voxhigh[0] - voxlow[0]) / 2.0f) + voxlow[0],
    ((voxhigh[1] - voxlow[1]) / 2.0f) + voxlow[1],
    ((voxhigh[2] - voxlow[2]) / 2.0f) + voxlow[2],
  }};
  // for interpolation purposes we need the # of voxels in floating point.
  const FLOATVECTOR3 voxelsf(static_cast<float>(voxels[0]),
                             static_cast<float>(voxels[1]),
                             static_cast<float>(voxels[2]));
  assert(vox_center[0] < voxelsf[0]);
  assert(vox_center[1] < voxelsf[1]);
  assert(vox_center[2] < voxelsf[2]);

  ExtCenter rv;
  rv.center = FLOATVECTOR3(
    MathTools::lerp(vox_center[0], 0.0f,voxelsf[0], extents[0][0],
                                                    extents[1][0]),
    MathTools::lerp(vox_center[1], 0.0f,voxelsf[1], extents[0][1],
                                                    extents[1][1]),
    MathTools::lerp(vox_center[2], 0.0f,voxelsf[2], extents[0][2],
                                                    extents[1][2])
  );
  assert(extents[0][0] <= rv.center[0] && rv.center[0] <= extents[1][0]);
  assert(extents[0][1] <= rv.center[1] && rv.center[1] <= extents[1][1]);
  assert(extents[0][2] <= rv.center[2] && rv.center[2] <= extents[1][2]);
  const uint64_t zero = 0ULL; // for type purposes.
  const FLOATVECTOR3 wlow(
    MathTools::lerp(voxlow[0], zero,voxels[0], extents[0][0],extents[1][0]),
    MathTools::lerp(voxlow[1], zero,voxels[1], extents[0][1],extents[1][1]),
    MathTools::lerp(voxlow[2], zero,voxels[2], extents[0][2],extents[1][2])
  );
  const FLOATVECTOR3 whigh(
    MathTools::lerp(voxhigh[0], zero,voxels[0], extents[0][0],extents[1][0]),
    MathTools::lerp(voxhigh[1], zero,voxels[1], extents[0][1],extents[1][1]),
    MathTools::lerp(voxhigh[2], zero,voxels[2], extents[0][2],extents[1][2])
  );
  rv.exts = whigh - wlow;
  assert(rv.exts[0] <= (extents[1][0] - extents[0][0]));
  assert(rv.exts[1] <= (extents[1][1] - extents[0][1]));
  assert(rv.exts[2] <= (extents[1][2] - extents[0][2]));
  return rv;
}

void DynamicBrickingDS::Rebrick() {
  // first make sure this makes sense.
  const BrickSize src_bs = SourceMaxBrickSize(*this->di->ds);

  if(!integer_multiple(this->di->brickSize[0]-ghost(*this), src_bs[0])) {
    throw std::runtime_error("x dimension is not an integer multiple of "
                             "original brick size.");
  }
  if(!integer_multiple(this->di->brickSize[1]-ghost(*this), src_bs[1])) {
    throw std::runtime_error("y dimension is not an integer multiple of "
                             "original brick size.");
  }
  if(!integer_multiple(this->di->brickSize[2]-ghost(*this), src_bs[2])) {
    throw std::runtime_error("z dimension is not an integer multiple of "
                             "original brick size.");
  }
  assert(this->di->brickSize[0] > 0);
  assert(this->di->brickSize[1] > 0);
  assert(this->di->brickSize[2] > 0);

  BrickedDataset::Clear();
  const VoxelLayout nvoxels = {{ // does not include ghost voxels.
    di->ds->GetDomainSize(0,0)[0],
    di->ds->GetDomainSize(0,0)[1],
    di->ds->GetDomainSize(0,0)[2]
  }};
  MESSAGE("Rebricking %llux%llux%llu dataset (with %ux%ux%u source bricks) "
          "with %ux%ux%u bricks.",
          nvoxels[0], nvoxels[1], nvoxels[2],
          static_cast<unsigned>(src_bs[0]), static_cast<unsigned>(src_bs[1]),
          static_cast<unsigned>(src_bs[2]),
          this->di->brickSize[0] - ghost(*this),
          this->di->brickSize[1] - ghost(*this),
          this->di->brickSize[2] - ghost(*this));

  assert(nvoxels[0] > 0 && nvoxels[1] > 0 && nvoxels[2] > 0);
  assert(this->di->brickSize[0] > 0);
  assert(this->di->brickSize[1] > 0);
  assert(this->di->brickSize[2] > 0);

  // give a hint as to how many bricks we'll have total.
  assert(nbricks(nvoxels, di->brickSize) > 0);
  this->NBricksHint(nbricks(nvoxels, di->brickSize));

  std::array<std::array<float,3>,2> extents = DatasetExtents(this->di->ds);
  MESSAGE("Extents are: [%g:%g x %g:%g x %g:%g]", extents[0][0],extents[1][0],
          extents[0][1],extents[1][0], extents[0][2],extents[1][2]);

  // don't create more LODs than the source data set (otherwise reading
  // the data is hard, we'd have to subsample on the fly)
  for(size_t lod=0; lod < di->ds->GetLODLevelCount(); ++lod) {
    const VoxelLayout voxels = {{
      this->di->ds->GetDomainSize(lod, 0)[0],
      this->di->ds->GetDomainSize(lod, 0)[1],
      this->di->ds->GetDomainSize(lod, 0)[2]
    }};
    const std::array<unsigned,3> blayout =
      GenericBrickLayout(voxels, this->di->BrickSansGhost());

    for(size_t x=0; x < blayout[0]; ++x) {
      for(size_t y=0; y < blayout[1]; ++y) {
        for(size_t z=0; z < blayout[2]; ++z) {
          const size_t idx = z*blayout[1]*blayout[0] + y*blayout[0] + x;

          BrickIndex bidx = {{unsigned(x), unsigned(y), unsigned(z)}};
          BrickSize cur_bs = ComputedTargetBrickSize(bidx,
            VoxelsInLOD(*this->di->ds.get(), lod), this->di->brickSize
          );
          BrickMD bmd;
          bmd.n_voxels = UINTVECTOR3(cur_bs[0], cur_bs[1], cur_bs[2]);
          const ExtCenter ec = BrickMetadata(x,y,z, BrickSansGhost(cur_bs),
            BrickSansGhost(this->di->brickSize), voxels, extents
          );

          bmd.extents = ec.exts;
          bmd.center = ec.center;

          const BrickKey key(0, lod, idx);
#ifndef NDEBUG
          this->di->VerifyBrick(std::make_pair(key, bmd));
#endif
          this->AddBrick(key, bmd);
        }
      }
    }
  }

  if(this->di->mmMode == MM_PRECOMPUTE) {
    this->di->ComputeMinMaxes(*this);
  }
}

#if !defined(NDEBUG) && !defined(_MSC_VER)
static bool test() {
  std::array<uint64_t,3> sz = {{192,200,16}};
  BrickSize th2 = {{32,32,32}};
  assert(layout(sz, th2)[0] == 6);
  assert(layout(sz, th2)[1] == 7);
  assert(layout(sz, th2)[2] == 1);
  assert(layout(VoxelLayout({{th2[0],th2[1],th2[2]}}), th2)[0] == 1);

  assert(to3d(sz, 0)[0] == 0);
  assert(to3d(sz, 0)[1] == 0);
  assert(to3d(sz, 0)[2] == 0);
  assert(to3d(sz, 191)[0] == 191);
  assert(to3d(sz, 191)[1] == 0);
  assert(to3d(sz, 191)[2] == 0);
  assert(to3d(sz, 192)[0] == 0);
  assert(to3d(sz, 192)[1] == 1);
  assert(to3d(sz, 192)[2] == 0);

  {
    std::array<uint64_t, 3> voxels = {{8,8,1}};
    BrickSize bsize = {{4,8,1}};
    std::array<float,3> low = {{ 0.0f, 0.0f, 0.0f }};
    std::array<float,3> high = {{ 10.0f, 5.0f, 19.0f }};
    std::array<std::array<float,3>,2> extents = {{ low, high }};
    auto beg = begin(voxels, bsize, extents);
    assert(beg != end());
    assert(std::get<0>((*beg).first) == 0); // timestep
    assert(std::get<1>((*beg).first) == 0); // LOD
    assert(std::get<2>((*beg).first) == 0); // index
    assert((*beg).second.n_voxels[0] ==  4);
    assert((*beg).second.n_voxels[1] ==  8);
    assert((*beg).second.n_voxels[2] ==  1);
    ++beg;
    assert(beg != end());
    assert(std::get<0>((*beg).first) == 0); // timestep
    assert(std::get<1>((*beg).first) == 0); // LOD
    assert(std::get<2>((*beg).first) == 1); // index
    assert((*beg).second.n_voxels[0] ==  4);
    assert((*beg).second.n_voxels[1] ==  8);
    assert((*beg).second.n_voxels[2] ==  1);
    ++beg;
    assert(beg != end());
    assert(std::get<0>((*beg).first) == 0); // timestep
    assert(std::get<1>((*beg).first) == 1); // LOD
    assert(std::get<2>((*beg).first) == 0); // index
    assert((*beg).second.n_voxels[0] ==  4);
    assert((*beg).second.n_voxels[1] ==  4);
    assert((*beg).second.n_voxels[2] ==  1);
    ++beg;
    assert(beg == end());
  }
  return true;
}
__attribute__((unused)) static const bool dybr = test();
#endif

}
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2013 IVDA Group


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/
