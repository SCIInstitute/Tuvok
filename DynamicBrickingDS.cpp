// * not considering brick overlap now, will need voxel overlap
// * calculate / fix extents && centers!
// * Dataset's "GetBitWidth" should return an unsigned or something
// * can we chain these recursively?  e.g.:
//      DynBrickingDS a(ds, {{128, 128, 128}});
//      DynBrickingDS b(a, {{16, 16, 16}});
// * tuvok::Dataset derives from boost::noncopyable... c++11 ?
// * OriginalKey is hacking '0' for the timestep
// * hack: only generate LODs which exist in source data (keep?)
// * Dataset::GetLODLevelCount returns a uint64_t?
// * size_t -> unsigned for brick sizes
// * MasterController::IOMan returns a pointer instead of a reference
// * Dataset::GetComponentCount returns a uint64_t?
// * dynamically generate/handle more LODs than the source data
// * interface for this is strange: would be nicer if it took the voxel ranges
//   we need, per-dimension, and the resolution it wants (instead of 'keys')?
// * 'GetMaxBrickSize' needs to be moved up to Dataset
// * remove 'overlap' from dbinfo (not used anyway) and just hardcode 'ghost()'
#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>
#include <stdexcept>
#include "Controller/Controller.h"
#include "const-brick-iterator.h"
#include "DynamicBrickingDS.h"
#include "IOManager.h"
#include "uvfDataset.h"

namespace tuvok {

#ifndef NDEBUG
static bool test();
#endif

struct DynamicBrickingDS::dbinfo {
  std::shared_ptr<Dataset> ds;
  std::array<unsigned, 3> brickSize;
  /// number of voxels of ghost data, per-SIDE (not dimension)
  const unsigned overlap;

  dbinfo(std::shared_ptr<Dataset> d,
         std::array<unsigned,3> bs,
         const unsigned olap) : ds(d), brickSize(bs), overlap(olap) { }

  // assumes timestep=0
  std::vector<uint8_t> ReadSourceBrick(unsigned lod, std::array<uint64_t,3> idx);
};

// gives the number of ghost voxels (per dimension) in a brick.  must be the
// same for both source and target.
static unsigned ghost() { return 4; }

/// gives the brick layout for a given decomposition. i.e. the number of bricks
/// in each dimension
static std::array<uint64_t,3> layout(const std::array<uint64_t,3> voxels,
                                     const std::array<unsigned,3> bsize) {
  std::array<uint64_t,3> tmp = {{
    static_cast<uint64_t>(ceil(static_cast<float>(voxels[0]) / bsize[0])),
    static_cast<uint64_t>(ceil(static_cast<float>(voxels[1]) / bsize[1])),
    static_cast<uint64_t>(ceil(static_cast<float>(voxels[2]) / bsize[2])),
  }};
  return tmp;
}

// converts a 3D index ('loc') into a 1D index.
static uint64_t to1d(const std::array<uint64_t,3> loc,
                     const std::array<uint64_t,3> size) {
  return loc[2]*size[1]*size[0] + loc[1]*size[0] + loc[0];
}

/// @param l the current location, in brick coords
/// @param bsz size of the bricks
/// @param voxels number of voxels
/// @returns the number of voxels 'l' has.  this is normally 'bsz', but can be
///          smaller when the 'l' abuts the side of a dimension
static std::array<uint32_t,3> nvoxels(const std::array<uint64_t,3> l,
                                      const std::array<unsigned,3> bsz,
                                      const std::array<uint64_t,3> voxels) {
  // the brick starts at 'v' and goes to 'v+bricksize'.  But 'v+bricksize'
  // might exceed beyond the bounds of the domain ('voxels'), and therefore
  // it should shrink.
  std::array<uint64_t,3> v = {{
    l[0] * bsz[0], l[1] * bsz[1], l[2] * bsz[2]
  }};
  const std::array<uint32_t,3> difference = {{
    static_cast<uint32_t>(voxels[0] - v[0]),
    static_cast<uint32_t>(voxels[1] - v[1]),
    static_cast<uint32_t>(voxels[2] - v[2])
  }};
  // get a uint32 version so we don't have to cast w/ e.g. min below.
  const std::array<uint32_t,3> bs = {{
    static_cast<uint32_t>(bsz[0]),
    static_cast<uint32_t>(bsz[1]),
    static_cast<uint32_t>(bsz[2]),
  }};
  const std::array<uint32_t,3> nvox = {{
    difference[0] ? std::min(bs[0], difference[0]) : bs[0],
    difference[1] ? std::min(bs[1], difference[1]) : bs[1],
    difference[2] ? std::min(bs[2], difference[2]) : bs[2],
  }};

  return nvox;
}

DynamicBrickingDS::DynamicBrickingDS(std::shared_ptr<Dataset> ds,
                                     std::array<unsigned, 3> maxBrickSize) :
  di(new DynamicBrickingDS::dbinfo(ds, maxBrickSize, 4))
{
  di->ds = ds;
  di->brickSize = maxBrickSize;
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

// Removes all the cache information we've made so far.
void DynamicBrickingDS::Clear() {
  di->ds->Clear();
  /// @todo FIXME should also clear our own internal stuff here.
}

UINTVECTOR3 DynamicBrickingDS::GetBrickVoxelCounts(const BrickKey&) const {
  abort(); return UINTVECTOR3();
}

static std::array<unsigned,3>
to3d(const std::array<uint64_t,3> dim, uint64_t idx) {
  std::array<unsigned,3> tmp = {{
    static_cast<unsigned>(idx % dim[0]),
    static_cast<unsigned>((idx / dim[0]) % dim[1]),
    static_cast<unsigned>(idx / (dim[1] * dim[2]))
  }};
  return tmp;
}

// with a brick identifier from the target dataset, find the 3D brick index in
// the source dataset.
std::array<uint64_t,3> SourceBrickIndex(const BrickKey& k,
                                        const std::shared_ptr<Dataset> ds,
                                        const std::array<unsigned,3> bsize)
{
  // See the comment ReBrick: we shouldn't have more LODs than the source data.
  assert(std::get<1>(k) < ds->GetLODLevelCount());
  const size_t lod = std::min(std::get<1>(k),
                              static_cast<size_t>(ds->GetLODLevelCount()));
  const size_t timestep = 0; // not handled presently...
  // identify how many voxels we've got
  const std::array<uint64_t, 3> voxels = {{
    ds->GetDomainSize(lod, timestep)[0],
    ds->GetDomainSize(lod, timestep)[1],
    ds->GetDomainSize(lod, timestep)[2]
  }};
  // now we know how many voxels we've got.  we can use that to convert the 1D
  // index we have back into the 3D index.
  std::array<unsigned,3> idx = to3d(layout(voxels, bsize), std::get<2>(k));

  // first, calculate the ratio of our layout to the actual layout
  const std::array<uint64_t,3> ratio = {{
    layout(voxels, bsize)[0] / ds->GetDomainSize(lod, timestep)[0],
    layout(voxels, bsize)[1] / ds->GetDomainSize(lod, timestep)[1],
    layout(voxels, bsize)[2] / ds->GetDomainSize(lod, timestep)[2]
  }};
  // the layout of the brick in the actual data set is floor(loc, ratio[i])
  std::array<uint64_t,3> tmp = {{
    static_cast<uint64_t>(floor(static_cast<double>(idx[0])/ratio[0])),
    static_cast<uint64_t>(floor(static_cast<double>(idx[1])/ratio[1])),
    static_cast<uint64_t>(floor(static_cast<double>(idx[2])/ratio[2]))
  }};
  return tmp;
}

std::array<uint64_t,3> VoxelsInLOD(const Dataset& ds, size_t lod) {
  const size_t timestep = 0; /// @todo properly implement.
  UINT64VECTOR3 domain = ds.GetDomainSize(lod, timestep);
  std::array<uint64_t,3> tmp = {{ domain[0], domain[1], domain[2] }};
  return tmp;
}

// with the source brick index, give a brick key for the source dataset.
BrickKey SourceKey(const std::array<uint64_t,3> brick_idx, size_t lod,
                   const Dataset& ds) {
  const std::array<uint64_t, 3> src_voxels = VoxelsInLOD(ds, lod);
  const std::array<unsigned,3> src_bricksize = {{
    static_cast<unsigned>(Controller::Instance().IOMan()->GetMaxBrickSize()),
    static_cast<unsigned>(Controller::Instance().IOMan()->GetMaxBrickSize()),
    static_cast<unsigned>(Controller::Instance().IOMan()->GetMaxBrickSize())
  }};
  const size_t timestep = 0; /// @todo properly implement
  return BrickKey(timestep, lod,
                  to1d(brick_idx, layout(src_voxels, src_bricksize)));
}

#if 0
ReadSourceBrick(...) {
  const std::array<unsigned,3> src_bricksize = {{
    static_cast<unsigned>(Controller::Instance().IOMan()->GetMaxBrickSize()),
    static_cast<unsigned>(Controller::Instance().IOMan()->GetMaxBrickSize()),
    static_cast<unsigned>(Controller::Instance().IOMan()->GetMaxBrickSize())
  }};

  // idx3d is where we are in 3d; 'layout(voxels, bsize)' is our
  // "layout size": the number of bricks in each dimension. we can
  // 'to1d' them to get our 1D brick index.
  return BrickKey(
    timestep, lod, to1d(idx3d, layout(voxels, original_bricksize))
  );
}
#endif

static std::array<uint64_t,3> Index(
  const Dataset& ds, size_t lod, uint64_t idx1d,
  const std::array<unsigned,3> bricksize
) {
  const std::array<unsigned,3> idx3d = to3d(
    layout(VoxelsInLOD(ds, lod), bricksize), idx1d
  );

  std::array<uint64_t,3> tmp = {{
    idx3d[0] * bricksize[0],
    idx3d[1] * bricksize[1],
    idx3d[2] * bricksize[2]
  }};
  return tmp;
}

// index of the first voxel of the given brick, among the whole level.
std::array<uint64_t,3> TargetIndex(const BrickKey& k,
                                   const Dataset& ds,
                                   const std::array<unsigned,3> bricksize) {
  const size_t idx1d = std::get<2>(k);
  const size_t lod = std::get<1>(k);

  return Index(ds, lod, idx1d, bricksize);
}

std::array<unsigned,3> ua(const UINTVECTOR3& v) {
  std::array<unsigned,3> tmp = {{ v[0], v[1], v[2] }};
  return tmp;
}

// index of the first voxel in the current brick, among the whole level
std::array<uint64_t,3> SourceIndex(const BrickKey& k,
                                   const Dataset& ds) {
  const size_t lod = std::get<1>(k);
  const size_t idx1d = std::get<2>(k);

  const UVFDataset& uvf = dynamic_cast<const UVFDataset&>(ds);
  return Index(ds, lod, idx1d, ua(uvf.GetMaxBrickSize()));
}

#if 0
void CopyFromIntersectingBrick() {
  std::vector<uint8_t> tmpdata;
  std::array<uint64_t,3> src_bidx = SourceBrickIndex(k, this->di->ds,
                                                     this->di->brickSize);
  tmpdata = ReadSourceBrick(lod, src_bidx);
  // need to figure out the voxel index of target brick's upper left and src's
  // bricks upper left, that tells us how many voxels to go 'in' before we
  // start copying.
  std::array<uint64_t,3> tgt_index = TargetIndex(...);
  std::array<uint64_t,3> src_index = SourceIndex(...);
  // it should always be the case that tgt_index >= src_index: we looked up the
  // brick so it would do that!
  assert(tgt_index[0] >= src_index[0]);
  assert(tgt_index[1] >= src_index[1]);
  assert(tgt_index[2] >= src_index[2]);

  data.resize(voxels in target);
  // we need to do copies from 'tmpdata' into 'data', but both of them will
  // have holes.  the approach we take is to iterate through the target,
  // copying the elements of the source.

  std::array<uint64_t,3> src_bs = SourceBrickSize(src_idx);
  std::array<uint64_t,3> tgt_bs = TargetBrickSize(tgt_idx);

  // how wide can our scanline be? its the intersection of our overlaps.
  size_t scanline;
  if(tgt_index[0]+tgt_bs[0] <= src_index[0]+src_bs[0]) { // tgt fits in src
    scanline = tgt_bs[0];
  } else {
    scanline = src_bs[0];
  }
  assert(scanline <= tgt_bs[0]); // no wider than brick we're copying into

  // unless the target brick shares a corner with the target brick, we'll need
  // to begin reading from it offset in a little bit.  but from where?
  std::array<uint64_t,3> src_offset = {{
    tgt_index[2] - src_index[2],
    tgt_index[1] - src_index[1],
    tgt_index[0] - src_index[0]
  }};

  for(uint64_t z=0; z < tgt_bs[2]; ++z, ++src_offset[2]) {
    for(uint64_t y=0; y < tgt_bs[1]; ++y, ++src_offset[1]) {
      uint64_t x = tgt_index[0]-src_index[0];
      uint64_t tgt_offset = z*tgt_bs[2]*tgt_bs[1] + y*tgt_bs[1] + x;
      assert(x + scanline <= tgt_bs[0]); // else our scanline width is wrong

      src_offset[0] = tgt_index[0]-src_index[0];
      const uint64_t src_o = src_offset[2]*src_bs[1]*src_bs[0] +
                             src_offset[1]*src_bs[0] + src_offset[0];
      std::copy(tmpdata.data()+src_o, data.data()+tgt_offset, scanline);
    }
  }
}
#endif

// a 3D box with an upper and lower coordinate; used for testing if two regions
// overlap.
// though technically we could throw anything in there, we use Box's in this
// code to talk about *voxel* coordinates (as opposed to brick coords).
struct Box {
  std::array<uint64_t,3> lower;
  std::array<uint64_t,3> upper;

  static bool intersect(const std::pair<uint64_t,uint64_t>& a,
                        const std::pair<uint64_t,uint64_t>& b) {
    assert(a.second >= a.first);
    assert(b.second >= b.first);

    return (a.first <= b.first && a.second >= b.first) ||
           (a.second >= b.first && a.second <= b.second);
  }
  bool intersect(const Box& b) const {
    return intersect(std::make_pair(lower[0],upper[0]),
                     std::make_pair(b.lower[0], b.upper[0])) &&
           intersect(std::make_pair(lower[1],upper[1]),
                     std::make_pair(b.lower[1], b.upper[1])) &&
           intersect(std::make_pair(lower[2],upper[2]),
                     std::make_pair(b.lower[2], b.upper[2]));
  }
};

#if 0
// gives back a box from the given brick coordinates.  the box's high/low is
// given in voxel coordinates
// if no brick exists at those coords, returns a Box which always intersects
// false.
Box SourceBox(...) {
}
#endif

// gives the size of the given brick from the target DS
std::array<unsigned,3> TargetBrickSize(const Dataset& ds, const BrickKey& k) {
  const BrickedDataset& b = dynamic_cast<const BrickedDataset&>(ds);
  UINTVECTOR3 sz = b.GetBrickMetadata(k).n_voxels;
  std::array<unsigned,3> tmp = {{
    static_cast<unsigned>(sz[0]),
    static_cast<unsigned>(sz[1]),
    static_cast<unsigned>(sz[2]),
  }};
  return tmp;
}
// gives the size of the given brick from the source DS
std::array<unsigned,3> SourceBrickSize(const Dataset& ds, const BrickKey& k) {
  const BrickedDataset& b = dynamic_cast<const BrickedDataset&>(ds);
  UINTVECTOR3 sz = b.GetBrickMetadata(k).n_voxels;
  std::array<unsigned,3> tmp = {{
    static_cast<unsigned>(sz[0]),
    static_cast<unsigned>(sz[1]),
    static_cast<unsigned>(sz[2])
  }};
  return tmp;
}

// Because of how we done the re-bricking, we know that all target bricks will
// fit nicely inside a source brick: so we know we only need to read one brick.
bool DynamicBrickingDS::GetBrick(const BrickKey& k, std::vector<uint8_t>& data) const
{
  const size_t lod = std::get<1>(k);
  BrickKey skey = SourceKey(SourceBrickIndex(k, this->di->ds,
                                             this->di->brickSize),
                            lod, *(this->di->ds));

  std::vector<uint8_t> srcdata;
  this->di->ds->GetBrick(skey, srcdata);

  std::array<unsigned,3> tgt_bs = TargetBrickSize(*this, k);
  std::array<unsigned,3> src_bs = SourceBrickSize(*this->di->ds, skey);

  // now we need to copy parts of 'srcdata' into 'data'.

  // our copy size/scanline size is the width of our target brick.
  const size_t scanline = tgt_bs[0];

  // need to figure out the voxel index of target brick's upper left and src's
  // bricks upper left, that tells us how many voxels to go 'in' before we
  // start copying.
  // these are both in the same space, because we have the same number of
  // voxels in both data sets---just more bricks in our target DS.
  std::array<uint64_t,3> tgt_index = TargetIndex(k, *this,
                                                 this->di->brickSize);
  std::array<uint64_t,3> src_index = SourceIndex(skey, *this->di->ds);
  // it should always be the case that tgt_index >= src_index: we looked up the
  // brick so it would do that!
  assert(tgt_index[0] >= src_index[0]);
  assert(tgt_index[1] >= src_index[1]);
  assert(tgt_index[2] >= src_index[2]);

  const size_t voxels_in_target = tgt_bs[0] * tgt_bs[1] * tgt_bs[2];
  data.resize(voxels_in_target);

  // unless the target brick shares a corner with the target brick, we'll need
  // to begin reading from it offset inwards a little bit.  where, exactly?
  std::array<uint64_t,3> src_offset = {{
    tgt_index[0] - src_index[0],
    tgt_index[1] - src_index[1],
    tgt_index[2] - src_index[2]
  }};

  for(uint64_t z=0; z < tgt_bs[2]; ++z) {
    for(uint64_t y=0; y < tgt_bs[1]; ++y) {
      const uint64_t x = 0;
      const uint64_t tgt_offset = z*tgt_bs[0]*tgt_bs[1] + y*tgt_bs[0] + x;

      const uint64_t src_o = src_offset[2]*src_bs[0]*src_bs[1] +
                             src_offset[1]*src_bs[0] + src_offset[0];
      std::copy(srcdata.data()+src_o, srcdata.data()+src_o+scanline,
                data.data()+tgt_offset);
      src_offset[1]++; // should follow 'y' increment.
    }
    src_offset[1] = tgt_index[1]-src_index[1]; // reset y ..
    src_offset[2]++; // .. and increment z
  }
  return true;
}
    

#if 0
// this is outdated because it tries too hard, doesn't assume any overlap
// between data sets.
void OldReadBrickCrap() {
  std::array<uint64_t,3> src_bidx = SourceBrickIndex(...);
  const Box target = {{
    .lower = {{ tgt_index[0], tgt_index[1], tgt_index[2] }},
    .upper = {{ tgt_index[0]+tgt_bs[0], tgt_index[1]+tgt_bs[1],
                tgt_index[2]+tgt_bs[2] }}
  }};
   
  std::array<uint64_t,3> tgt_index = TargetIndex(...);
  std::array<uint64_t,3> src_index = SourceIndex(...);
  std::array<uint64_t,3> src_bs = SourceBrickSize(src_idx);
  std::array<uint64_t,3> tgt_bs = TargetBrickSize(tgt_idx);
  std::array<uint64_t,3> src_bidx = SourceBrickIndex(k, this->di->ds,
                                                     this->di->brickSize);

  // our target box can span up to two bricks in the source space---per
  // dimension.  So we have to check intersection with 2^3=8 potential pairs.
  // (these should count in binary, and the "+X"s should in the SourceBox
  // should match what goes on inside the if.)
  Box src = SourceBox(src_bidx[0]+0, src_bidx[1]+0, src_bidx[2]+0);
  if(Intersects(target, src)) {
    CopyFromIntersectingBrick(tgt_index,
                              src_bidx[0]+0, src_bidx[1]+0, src_bidx[2]+0);
  }
  src = SourceBox(src_bidx[0]+0, src_bidx[1]+0, src_bidx[2]+1);
  if(Intersects(target, src)) {
    CopyFromIntersectingBrick(tgt_index,
                              src_bidx[0]+0, src_bidx[1]+0, src_bidx[2]+1);
  }
  src = SourceBox(src_bidx[0]+0, src_bidx[1]+1, src_bidx[2]+0);
  if(Intersects(target, src)) {
    CopyFromIntersectingBrick(tgt_index,
                              src_bidx[0]+0, src_bidx[1]+1, src_bidx[2]+0);
  }
  src = SourceBox(src_bidx[0]+0, src_bidx[1]+1, src_bidx[2]+1);
  if(Intersects(target, src)) {
    CopyFromIntersectingBrick(tgt_index,
                              src_bidx[0]+0, src_bidx[1]+1, src_bidx[2]+1);
  }

  src = SourceBox(src_bidx[0]+1, src_bidx[1]+0, src_bidx[2]+0);
  if(Intersects(target, src)) {
    CopyFromIntersectingBrick(tgt_index,
                              src_bidx[0]+1, src_bidx[1]+0, src_bidx[2]+0);
  }
  src = SourceBox(src_bidx[0]+1, src_bidx[1]+0, src_bidx[2]+1);
  if(Intersects(target, src)) {
    CopyFromIntersectingBrick(tgt_index,
                              src_bidx[0]+1, src_bidx[1]+0, src_bidx[2]+1);
  }
  src = SourceBox(src_bidx[0]+1, src_bidx[1]+1, src_bidx[2]+0);
  if(Intersects(target, src)) {
    CopyFromIntersectingBrick(tgt_index,
                              src_bidx[0]+1, src_bidx[1]+1, src_bidx[2]+0);
  }
  src = SourceBox(src_bidx[0]+1, src_bidx[1]+1, src_bidx[2]+1);
  if(Intersects(target, src)) {
    CopyFromIntersectingBrick(tgt_index,
                              src_bidx[0]+1, src_bidx[1]+1, src_bidx[2]+1);
  }
}
#endif

bool DynamicBrickingDS::GetBrick(const BrickKey&, std::vector<int8_t>&) const
{
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey&, std::vector<uint16_t>&) const
{
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey&, std::vector<int16_t>&) const
{
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey&, std::vector<uint32_t>&) const
{
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey&, std::vector<int32_t>&) const
{
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey&, std::vector<float>&) const
{
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey&, std::vector<double>&) const
{
  abort(); return false;
}

void DynamicBrickingDS::SetRescaleFactors(const DOUBLEVECTOR3& scale) {
  di->ds->SetRescaleFactors(scale);
}
CFORWARDRET(DOUBLEVECTOR3, GetRescaleFactors)

/// If the underlying file format supports it, save the current scaling
/// factors to the file.  The format should implicitly load and apply the
/// scaling factors when opening the file.
FORWARDRET(bool, SaveRescaleFactors)

CFORWARDRET(uint64_t, GetLODLevelCount)
CFORWARDRET(uint64_t, GetNumberOfTimesteps)
UINT64VECTOR3 DynamicBrickingDS::GetDomainSize(const size_t lod,
                                               const size_t ts) const {
  return di->ds->GetDomainSize(lod, ts);
}
CFORWARDRET(UINTVECTOR3, GetBrickOverlapSize)
/// @return the number of voxels for the given brick, per dimension, taking
///         into account any brick overlaps.
UINT64VECTOR3 DynamicBrickingDS::GetEffectiveBrickSize(const BrickKey&) const
{
  abort();
  return UINT64VECTOR3(0,0,0);
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
bool DynamicBrickingDS::ContainsData(const BrickKey& bk, double isoval) const {
  return di->ds->ContainsData(bk, isoval);
}
bool DynamicBrickingDS::ContainsData(const BrickKey& bk, double fmin,
                                     double fmax) const {
  return di->ds->ContainsData(bk, fmin, fmax);
}
bool DynamicBrickingDS::ContainsData(const BrickKey& bk,
                                     double fmin, double fmax,
                                     double fminGradient,
                                     double fmaxGradient) const {
  return di->ds->ContainsData(bk, fmin,fmax, fminGradient, fmaxGradient);
}

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
  return di->ds->ApplyFunction(lod, brickFunc, pUserContext, iOverlap);
}

CFORWARDRET(const char*, Name)

// Virtual constructor.  Hard to make sense of this in the IOManager's
// context; this isn't a register-able Dataset type which tuvok can
// automatically instantiate to read a dataset.  Rather, the user must *have*
// such a dataset already and use this as a proxy for it.
DynamicBrickingDS* DynamicBrickingDS::Create(const std::string&, uint64_t,
                                             bool) const {
  abort(); return NULL;
}

std::pair<FLOATVECTOR3, FLOATVECTOR3>
DynamicBrickingDS::GetTextCoords(BrickTable::const_iterator brick,
                                 bool PoT) const {
  return di->ds->GetTextCoords(brick, PoT);
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
static uint64_t nbricks(const std::array<uint64_t,3> voxels,
                        const std::array<unsigned,3> bricksize) {
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

void DynamicBrickingDS::Rebrick() {
  // first make sure this makes sense.
  const UVFDataset& uvf = dynamic_cast<const UVFDataset&>(*this->di->ds);
  std::array<unsigned,3> src_bs = {{
    (unsigned)uvf.GetMaxUsedBrickSizes()[0] - ghost(),
    (unsigned)uvf.GetMaxUsedBrickSizes()[1] - ghost(),
    (unsigned)uvf.GetMaxUsedBrickSizes()[2] - ghost()
  }};
  this->di->brickSize[0] = std::min(this->di->brickSize[0], src_bs[0]);
  this->di->brickSize[1] = std::min(this->di->brickSize[1], src_bs[1]);
  this->di->brickSize[2] = std::min(this->di->brickSize[2], src_bs[2]);
  if(this->di->brickSize[0] > src_bs[0] ||
     this->di->brickSize[1] > src_bs[1] ||
     this->di->brickSize[2] > src_bs[2]) {
    throw std::runtime_error("brick size is too big");
  }
  if(!integer_multiple(this->di->brickSize[0], src_bs[0])) {
    throw std::runtime_error("x dimension is not an integer multiple of "
                             "original brick size.");
  }
  if(!integer_multiple(this->di->brickSize[1], src_bs[1])) {
    throw std::runtime_error("y dimension is not an integer multiple of "
                             "original brick size.");
  }
  if(!integer_multiple(this->di->brickSize[2], src_bs[2])) {
    throw std::runtime_error("z dimension is not an integer multiple of "
                             "original brick size.");
  }

  BrickedDataset::Clear();
  // FIXME read these from di->ds
  std::array<uint64_t, 3> nvoxels = {{
    di->ds->GetDomainSize(0,0)[0],
    di->ds->GetDomainSize(0,0)[1],
    di->ds->GetDomainSize(0,0)[2]
  }};

  assert(nvoxels[0] > 0 && nvoxels[1] > 0 && nvoxels[2]);

  // give a hint as to how many bricks we'll have total.
  assert(nbricks(nvoxels, di->brickSize) > 0);
  this->NBricksHint(nbricks(nvoxels, di->brickSize));
  std::for_each(begin(nvoxels, di->brickSize), end(),
                [&](const std::pair<BrickKey,BrickMD>& b) {
                    // since our brick sizes are smaller, and in both our
                    // DS and the DS we use we continue creating LODs
                    // until we get to a single brick, we could have
                    // more LODs in our Dataset than we do in the source
                    // Dataset.
                    // We could dynamically generate that lower-res data.  We
                    // probably do want to do that eventually.  But for now,
                    // let's just stop generating data when we hit the source
                    // data's LOD.
                    const size_t lod = std::get<1>(b.first);
                    if(lod < di->ds->GetLODLevelCount()) {
                      this->AddBrick(b.first, b.second);
                    }
                  });
}

#if !defined(NDEBUG) && !defined(_MSC_VER)
static bool test() {
  std::array<uint64_t,3> sz = {{192,200,16}};
  std::array<unsigned,3> th2 = {{32,32,32}};
  assert(layout(sz, th2)[0] == 6);
  assert(layout(sz, th2)[1] == 7);
  assert(layout(sz, th2)[2] == 1);
  assert(layout(std::array<uint64_t,3>({{th2[0],th2[1],th2[2]}}), th2)[0] == 1);

  assert(to3d(sz, 0)[0] == 0);
  assert(to3d(sz, 0)[1] == 0);
  assert(to3d(sz, 0)[2] == 0);
  assert(to3d(sz, 191)[0] == 191);
  assert(to3d(sz, 191)[1] == 0);
  assert(to3d(sz, 191)[2] == 0);
  assert(to3d(sz, 192)[0] == 0);
  assert(to3d(sz, 192)[1] == 1);
  assert(to3d(sz, 192)[2] == 0);
#if 0
  {
    std::array<uint64_t, 3> voxels = {{8,8,1}};
    std::array<size_t,3> bsize = {{8,4,1}};
    size_t i=0;
    for(auto b=begin(voxels, bsize); b != end(); ++b) {
      WARNING("brick %zu: %zu %zu %zu", i, std::get<0>((*b).first),
              std::get<1>((*b).first), std::get<2>((*b).first));
      ++i;
    }
  }
#endif
  {
    std::array<uint64_t, 3> voxels = {{8,8,1}};
    std::array<unsigned,3> bsize = {{4,8,1}};
    auto beg = begin(voxels, bsize);
    assert(beg != end());
    assert(std::get<0>((*beg).first) == 0); // timestep
    assert(std::get<1>((*beg).first) == 0); // LOD
    assert(std::get<2>((*beg).first) == 0); // index
    assert((*beg).second.n_voxels[0] == 4);
    assert((*beg).second.n_voxels[1] == 8);
    assert((*beg).second.n_voxels[2] == 1);
    ++beg;
    assert(beg != end());
    assert(std::get<0>((*beg).first) == 0); // timestep
    assert(std::get<1>((*beg).first) == 0); // LOD
    assert(std::get<2>((*beg).first) == 1); // index
    assert((*beg).second.n_voxels[0] == 4);
    assert((*beg).second.n_voxels[1] == 8);
    assert((*beg).second.n_voxels[2] == 1);
    ++beg;
    assert(beg != end());
    assert(std::get<0>((*beg).first) == 0); // timestep
    assert(std::get<1>((*beg).first) == 1); // LOD
    assert(std::get<2>((*beg).first) == 0); // index
    assert((*beg).second.n_voxels[0] == 4);
    assert((*beg).second.n_voxels[1] == 8);
    assert((*beg).second.n_voxels[2] == 1);
    ++beg;
    assert(beg == end());
  }
  return true;
}
//static bool dybr = test();
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
