// * Dataset's "GetBitWidth" should return an unsigned or something
// * can we chain these recursively?  e.g.:
//      DynBrickingDS a(ds, {{128, 128, 128}});
//      DynBrickingDS b(a, {{16, 16, 16}});
// * tuvok::Dataset derives from boost::noncopyable... c++11 ?
// * hack: only generate LODs which exist in source data (keep?)
// * Dataset::GetLODLevelCount returns a uint64_t?
// * MasterController::IOMan returns a pointer instead of a reference
// * Dataset::GetComponentCount returns a uint64_t?
// * dynamically generate/handle more LODs than the source data
// * interface for this is strange: would be nicer if it took the voxel ranges
//   we need, per-dimension, and the resolution it wants (instead of 'keys')?
// * 'GetMaxBrickSize' needs to be moved up to Dataset
// * rename/fix creation of the iterator functions ("begin"/"end" too general)
// * stop dynamic_cast'ing to UVFDataset; move those methods to BrickedDataset.
// * implement GetBrick for other bit widths!!
// * ContainsData: deal with new metadata appropriately
#include <algorithm>
#include <array>
#include <cassert>
#include <stdexcept>
#include "Controller/Controller.h"
#include "const-brick-iterator.h"
#include "DynamicBrickingDS.h"
#include "FileBackedDataset.h"
#include "IOManager.h"
#include "uvfDataset.h"

// This file deals with some tricky indexing.  The convention here is that a
// std::array<unsigned,3> refers to a BRICK index, whereas a
// std::array<uint64_t,3> refers to a VOXEL index.  We also try to use "source"
// in variable names which refer to indices from the data set which actually
// exists, and "target" to refer to indices in the faux/rebricked data set.

namespace tuvok {

#ifndef NDEBUG
static bool test();
#endif

struct DynamicBrickingDS::dbinfo {
  std::shared_ptr<Dataset> ds;
  std::array<unsigned, 3> brickSize;

  dbinfo(std::shared_ptr<Dataset> d,
         std::array<unsigned,3> bs) : ds(d), brickSize(bs) { }

  // assumes timestep=0
  std::vector<uint8_t> ReadSourceBrick(unsigned lod, std::array<uint64_t,3> idx);

  // given the brick key in the dynamic DS, return the corresponding BrickKey
  // in the source data.
  BrickKey SourceBrickKey(const BrickKey&);
};

std::array<unsigned,3> GenericSourceBrickSize(const Dataset&);

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
static uint64_t to1d(const std::array<uint64_t,3>& loc,
                     const std::array<uint64_t,3>& size) {
  assert(loc[2] < size[2]);
  assert(loc[1] < size[1]);
  assert(loc[0] < size[0]);
  return loc[2]*size[1]*size[0] + loc[1]*size[0] + loc[0];
}
// ditto, but the location is unsigned instead of uint64_t.
// remember unsigned is for brick indices, uint64_t is for voxel indices
static uint64_t to1d(const std::array<unsigned,3>& loc,
                     const std::array<uint64_t,3>& size) {
  assert(loc[2] < size[2]);
  assert(loc[1] < size[1]);
  assert(loc[0] < size[0]);
  return loc[2]*size[1]*size[0] + loc[1]*size[0] + loc[0];
}

DynamicBrickingDS::DynamicBrickingDS(std::shared_ptr<Dataset> ds,
                                     std::array<unsigned, 3> maxBrickSize) :
  FileBackedDataset(dynamic_cast<const FileBackedDataset*>
                                (ds.get())->Filename()),
  di(new DynamicBrickingDS::dbinfo(ds, maxBrickSize))
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

// Removes all the cache information we've made so far.
void DynamicBrickingDS::Clear() {
  di->ds->Clear();
  /// @todo FIXME should also clear our own internal stuff here.
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
std::array<unsigned,3> SourceBrickLayout(const std::shared_ptr<Dataset> ds,
                                         size_t lod, size_t timestep) {
  const std::shared_ptr<UVFDataset> uvf =
    std::dynamic_pointer_cast<UVFDataset>(ds);
  const UINT64VECTOR3 layout = uvf->GetBrickLayout(lod, timestep);
  std::array<unsigned,3> tmp = {{
    static_cast<unsigned>(layout[0]),
    static_cast<unsigned>(layout[1]),
    static_cast<unsigned>(layout[2])
  }};
  return tmp;
}

// gives the number of bricks in each dimension.
static const std::array<unsigned,3> BrickLayout(
  const std::array<uint64_t,3> voxels,
  const std::array<unsigned,3> bsize
) {
  const std::array<unsigned,3> tgt_layout = {{
    static_cast<unsigned>(layout(voxels, bsize)[0]),
    static_cast<unsigned>(layout(voxels, bsize)[1]),
    static_cast<unsigned>(layout(voxels, bsize)[2])
  }};
  return tgt_layout;
}

// identifies the number of bricks we have in the target dataset for
// each brick in the source dataset. that is, how many bricks we're
// stuffing into one brick.
// this fits in integer numbers because we enforce that rebricking subdivides
// the original volume/bricks nicely.
std::array<unsigned,3> TargetBricksPerSource(
  const std::shared_ptr<Dataset> ds, const size_t lod,
  const std::array<unsigned,3> bsize
) {
  const size_t timestep = 0; /// @fixme support time..
  const std::array<uint64_t, 3> voxels = {{
    ds->GetDomainSize(lod, timestep)[0],
    ds->GetDomainSize(lod, timestep)[1],
    ds->GetDomainSize(lod, timestep)[2]
  }};
  std::array<unsigned,3> tgt_blayout = BrickLayout(voxels, bsize);
  std::array<unsigned,3> src_blayout = BrickLayout(
    voxels, GenericSourceBrickSize(*ds)
  );
  // the rebricked data set can't have *fewer* bricks:
  assert(tgt_blayout[0] >= src_blayout[0]);
  assert(tgt_blayout[1] >= src_blayout[1]);
  assert(tgt_blayout[2] >= src_blayout[2]);
  std::array<unsigned,3> rv = {{
    tgt_blayout[0] / src_blayout[0],
    tgt_blayout[1] / src_blayout[1],
    tgt_blayout[2] / src_blayout[2]
  }};
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
std::array<unsigned,3> SourceBrickIndex(const BrickKey& k,
                                        const std::shared_ptr<Dataset> ds,
                                        const std::array<unsigned,3> bsize)
{
  // See the comment Rebrick: we shouldn't have more LODs than the source data.
  assert(std::get<1>(k) < ds->GetLODLevelCount());
  const size_t lod = std::min(std::get<1>(k),
                              static_cast<size_t>(ds->GetLODLevelCount()));
  const size_t timestep = std::get<0>(k);
  // identify how many voxels we've got
  const std::array<uint64_t, 3> voxels = {{
    ds->GetDomainSize(lod, timestep)[0],
    ds->GetDomainSize(lod, timestep)[1],
    ds->GetDomainSize(lod, timestep)[2]
  }};
  // now we know how many voxels we've got.  we can use that to convert the 1D
  // index we have back into the 3D index.
  const size_t idx1d = std::get<2>(k);
  std::array<unsigned,3> idx = to3d(layout(voxels, bsize), idx1d);

  const std::array<unsigned,3> bricks_per_src =
    TargetBricksPerSource(ds, lod, bsize);

  std::array<unsigned,3> tmp = {{0}};
  for(size_t i=0; i < 3; ++i) {
    double rio = static_cast<double>(idx[i] / bricks_per_src[i]);
    tmp[i] = static_cast<unsigned>(floor(rio));
  }
  return tmp;
}

// @returns the number of voxels in the given level of detail.
std::array<uint64_t,3> VoxelsInLOD(const Dataset& ds, size_t lod) {
  const size_t timestep = 0; /// @todo properly implement.
  UINT64VECTOR3 domain = ds.GetDomainSize(lod, timestep);
  std::array<uint64_t,3> tmp = {{ domain[0], domain[1], domain[2] }};
  return tmp;
}

// @return the brick size which the given dataset *tries* to use.  Of course,
// if the bricks don't fit evenly, there will be some bricks on the edge which
// are smaller.
std::array<unsigned,3> GenericSourceBrickSize(const Dataset& ds) {
  const UVFDataset& uvf = dynamic_cast<const UVFDataset&>(ds);
  const std::array<unsigned,3> src_bs = {{
    static_cast<unsigned>(uvf.GetMaxUsedBrickSizes()[0] - ghost()),
    static_cast<unsigned>(uvf.GetMaxUsedBrickSizes()[1] - ghost()),
    static_cast<unsigned>(uvf.GetMaxUsedBrickSizes()[2] - ghost())
  }};
  assert(src_bs[0] > 0 && src_bs[0] < 65535); // must make sense.
  assert(src_bs[1] > 0 && src_bs[1] < 65535);
  assert(src_bs[2] > 0 && src_bs[2] < 65535);
  return src_bs;
}

// with the source brick index, give a brick key for the source dataset.
BrickKey SourceKey(const std::array<unsigned,3> brick_idx, size_t lod,
                   const Dataset& ds) {
  const std::array<uint64_t, 3> src_voxels = VoxelsInLOD(ds, lod);
  const std::array<unsigned,3> src_bricksize = GenericSourceBrickSize(ds);
  const size_t timestep = 0; /// @todo properly implement
  return BrickKey(timestep, lod,
                  to1d(brick_idx, layout(src_voxels, src_bricksize)));
}

// figure out the voxel index of the upper left corner of a brick
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

// basically a cast from UINTVECTOR3 to a 3-elem array.
std::array<unsigned,3> ua(const UINTVECTOR3& v) {
  const std::array<unsigned,3> tmp = {{ v[0], v[1], v[2] }};
  return tmp;
}

// index of the first voxel in the current brick, among the whole level
std::array<uint64_t,3> SourceIndex(const BrickKey& k,
                                   const Dataset& ds) {
  const size_t lod = std::get<1>(k);
  const size_t idx1d = std::get<2>(k);


  const std::array<unsigned,3> src_bs = GenericSourceBrickSize(ds);
  return Index(ds, lod, idx1d, src_bs);
}

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

// given the brick key in the dynamic DS, return the corresponding BrickKey
// in the source data.
BrickKey DynamicBrickingDS::dbinfo::SourceBrickKey(const BrickKey& k) {
  const size_t lod = std::get<1>(k);
  const std::array<unsigned,3> src_bidx =
    SourceBrickIndex(k, this->ds, this->brickSize);
  BrickKey skey = SourceKey(src_bidx, lod, *(this->ds));
#ifndef NDEBUG
  // brick key should make sense.
  std::shared_ptr<FileBackedDataset> fbds =
    std::dynamic_pointer_cast<FileBackedDataset>(this->ds);
  assert(std::get<2>(skey) < fbds->GetTotalBrickCount());
  assert(std::get<0>(skey) < fbds->GetNumberOfTimesteps());
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

// Because of how we done the re-bricking, we know that all target bricks will
// fit nicely inside a source brick: so we know we only need to read one brick.
bool DynamicBrickingDS::GetBrick(const BrickKey& k, std::vector<uint8_t>& data) const
{
  assert(this->bricks.find(k) != this->bricks.end());
  BrickKey skey = this->di->SourceBrickKey(k);

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

bool DynamicBrickingDS::GetBrick(const BrickKey& k, std::vector<int8_t>&) const
{
  assert(this->bricks.find(k) != this->bricks.end());
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey& k,
                                 std::vector<uint16_t>&) const
{
  assert(this->bricks.find(k) != this->bricks.end());
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey& k, std::vector<int16_t>&) const
{
  assert(this->bricks.find(k) != this->bricks.end());
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey& k,
                                 std::vector<uint32_t>&) const
{
  assert(this->bricks.find(k) != this->bricks.end());
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey& k, std::vector<int32_t>&) const
{
  assert(this->bricks.find(k) != this->bricks.end());
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey& k, std::vector<float>&) const
{
  assert(this->bricks.find(k) != this->bricks.end());
  abort(); return false;
}
bool DynamicBrickingDS::GetBrick(const BrickKey& k, std::vector<double>&) const
{
  assert(this->bricks.find(k) != this->bricks.end());
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
UINT64VECTOR3 DynamicBrickingDS::GetEffectiveBrickSize(const BrickKey& k) const
{
  assert(this->bricks.find(k) != this->bricks.end());
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

CFORWARDRET(const char*, Name)

// Virtual constructor.  Hard to make sense of this in the IOManager's
// context; this isn't a register-able Dataset type which tuvok can
// automatically instantiate to read a dataset.  Rather, the user must *have*
// such a dataset already and use this as a proxy for it.
DynamicBrickingDS* DynamicBrickingDS::Create(const std::string&, uint64_t,
                                             bool) const {
  abort(); return NULL;
}

bool DynamicBrickingDS::IsOpen() const {
  const FileBackedDataset& f = dynamic_cast<const FileBackedDataset&>(
    *(this->di->ds.get())
  );
  return f.IsOpen();
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
static uint64_t nbricks(const std::array<uint64_t,3>& voxels,
                        const std::array<unsigned,3>& bricksize) {
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
  typedef std::array<float,3> v;
  v elow =  {{ -(extents[0]/2.0f), -(extents[1]/2.0f), -(extents[2]/2.0f) }};
  v ehigh = {{  (extents[0]/2.0f),  (extents[1]/2.0f),  (extents[2]/2.0f) }};
  rv[0] = elow; rv[1] = ehigh;
  return rv;
}

void DynamicBrickingDS::Rebrick() {
  // first make sure this makes sense.
  const std::array<unsigned,3> src_bs = GenericSourceBrickSize(*this->di->ds);
  this->di->brickSize[0] = std::min(this->di->brickSize[0], src_bs[0]);
  this->di->brickSize[1] = std::min(this->di->brickSize[1], src_bs[1]);
  this->di->brickSize[2] = std::min(this->di->brickSize[2], src_bs[2]);
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
  assert(this->di->brickSize[0] > 0);
  assert(this->di->brickSize[1] > 0);
  assert(this->di->brickSize[2] > 0);

  BrickedDataset::Clear();
  const std::array<uint64_t, 3> nvoxels = {{ // does not include ghost.
    di->ds->GetDomainSize(0,0)[0],
    di->ds->GetDomainSize(0,0)[1],
    di->ds->GetDomainSize(0,0)[2]
  }};
  MESSAGE("Rebricking %llux%llux%llu data set with %ux%ux%u bricks.",
          nvoxels[0], nvoxels[1], nvoxels[2],
          this->di->brickSize[0], this->di->brickSize[1],
          this->di->brickSize[2]);

  assert(nvoxels[0] > 0 && nvoxels[1] > 0 && nvoxels[2]);

  std::array<std::array<float,3>,2> extents = DatasetExtents(this->di->ds);
  MESSAGE("Extents are: [%g:%g x %g:%g x %g:%g]", extents[0][0],extents[1][0],
          extents[0][1],extents[1][0], extents[0][2],extents[1][2]);
  assert(extents[1][0] >= extents[0][0]);
  assert(extents[1][1] >= extents[0][1]);
  assert(extents[1][2] >= extents[0][2]);

  // give a hint as to how many bricks we'll have total.
  assert(nbricks(nvoxels, di->brickSize) > 0);
  this->NBricksHint(nbricks(nvoxels, di->brickSize));
  std::for_each(begin(nvoxels, di->brickSize, extents), end(),
                [&](const std::pair<BrickKey,BrickMD>& b) {
                    const size_t lod = std::get<1>(b.first);
                    assert(this->di->brickSize[0] > 0);
                    assert(this->di->brickSize[1] > 0);
                    assert(this->di->brickSize[2] > 0);
                    assert(std::get<0>(b.first) == 0); // timestep unused
#ifndef NDEBUG
                    const FLOATVECTOR3 fullexts(
                      extents[1][0] - extents[0][0],
                      extents[1][1] - extents[0][1],
                      extents[1][2] - extents[0][2]
                    );
                    assert(b.second.extents[0] <= fullexts[0]);
                    assert(b.second.extents[1] <= fullexts[1]);
                    assert(b.second.extents[2] <= fullexts[2]);
#endif
                    // since our brick sizes are smaller, and in both our
                    // DS and the DS we use we continue creating LODs
                    // until we get to a single brick, we could have
                    // more LODs in our Dataset than we do in the source
                    // Dataset.
                    // We could dynamically generate that lower-res data.  We
                    // probably do want to do that eventually.  But for now,
                    // let's just stop generating data when we hit the source
                    // data's LOD.
                    if(lod < di->ds->GetLODLevelCount()) {
                      // add in the ghost data.
                      std::pair<BrickKey, BrickMD> brk = b;
                      brk.second.n_voxels[0] += ghost();
                      brk.second.n_voxels[1] += ghost();
                      brk.second.n_voxels[2] += ghost();
#ifndef NDEBUG
                      BrickKey srckey = this->di->SourceBrickKey(b.first);
                      MESSAGE("adding brick w/ srckey: <%zu,%zu,%zu>",
                              std::get<0>(srckey), std::get<1>(srckey),
                              std::get<2>(srckey));
                      if(this->di->brickSize[0] == src_bs[0] &&
                         this->di->brickSize[1] == src_bs[1] &&
                         this->di->brickSize[2] == src_bs[2]) {
                        // if we "Re"brick to the same size bricks, then all
                        // the bricks we create should also exist in the source
                        // dataset.
                        assert(b.first == srckey);
                      }
#endif
                      this->AddBrick(brk.first, brk.second);
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

  // 'floor' is giving absurd results in gdb; make sure it's sane here.
  assert(unsigned(floor(1.2)) == 1);
  assert(unsigned(floor(1.8)) == 1);
  {
    std::array<uint64_t, 3> voxels = {{8,8,1}};
    std::array<unsigned,3> bsize = {{4,8,1}};
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
    assert((*beg).second.n_voxels[1] ==  8);
    assert((*beg).second.n_voxels[2] ==  1);
    ++beg;
    assert(beg == end());
  }
  return true;
}
static bool dybr = test();
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
