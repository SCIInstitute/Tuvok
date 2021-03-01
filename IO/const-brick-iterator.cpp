#include <array>
#include <cmath>
#include "Basics/MathTools.h"
#include "const-brick-iterator.h"

namespace tuvok {

// converts a 3D index ('loc') into a 1D index.
static uint64_t to1d(const std::array<uint64_t,3>& loc,
                     const std::array<uint64_t,3>& size) {
  return loc[2]*size[1]*size[0] + loc[1]*size[0] + loc[0];
}

// just converts a 3-element array into a UINTVECTOR3.
static UINTVECTOR3 va(const std::array<uint32_t,3>& a) {
  return UINTVECTOR3(a[0], a[1], a[2]);
}

/// @param l the current location, in brick coords
/// @param bsz size of the bricks
/// @param voxels number of voxels
/// @returns the number of voxels 'l' has.  this is normally 'bsz', but can be
///          smaller when the 'l' abuts the side of a dimension
static std::array<uint32_t,3> nvoxels(const std::array<uint64_t,3> l,
                                      const std::array<size_t,3> bsz,
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
  std::array<uint32_t,3> nvox = {{
    difference[0] ? std::min(bs[0], difference[0]) : bs[0],
    difference[1] ? std::min(bs[1], difference[1]) : bs[1],
    difference[2] ? std::min(bs[2], difference[2]) : bs[2],
  }};

  return nvox;
}

// The dimension with the largest ratio of size-to-bricksize
#define DIM \
    (vox[0] / bsize[0] > vox[1] / bsize[1] ? \
      (vox[0] / bsize[0] > vox[2] / bsize[2] ? 0 : 2) \
    : (vox[1] / bsize[1] > vox[2] / bsize[2] ? 1 : 2))
const_brick_iterator::const_brick_iterator(
  const std::array<uint64_t,3>& vox,
  const std::array<size_t,3>& bricksize,
  const std::array<std::array<float,3>,2>& exts
) : bsize(bricksize),
    // a bit intense for an initializer list, but it needs to be here so this
    // can be const.
    MaxLODs(static_cast<size_t>(ceil(double(vox[DIM]) / bricksize[DIM]))),
    voxels(vox), LOD(0), extents(exts)
#undef DIM
{
  location[0] = location[1] = location[2] = 1ULL;
}

/// gives the brick layout for a given decomposition. i.e. the number of bricks
/// in each dimension
static std::array<uint64_t,3> layout(const std::array<uint64_t,3> voxels,
                                     const std::array<size_t,3> bsize) {
  std::array<uint64_t,3> tmp = {{
    static_cast<uint64_t>(ceil(static_cast<float>(voxels[0]) / bsize[0])),
    static_cast<uint64_t>(ceil(static_cast<float>(voxels[1]) / bsize[1])),
    static_cast<uint64_t>(ceil(static_cast<float>(voxels[2]) / bsize[2])),
  }};
  return tmp;
}

const_brick_iterator& const_brick_iterator::advance() {
  // the layout for this level
  const std::array<uint64_t,3> ly = layout(this->voxels, this->bsize);

  // increment the x coord. if it goes beyond the end, then switch to
  // Y... and so on.
  this->location[0]++;
  if(this->location[0] > ly[0]) {
    this->location[0] = 1;
    this->location[1]++;
  }
  if(this->location[1] > ly[1]) {
    this->location[1] = 1;
    this->location[2]++;
  }
  if(this->location[2] > ly[2]) {
    this->location[2] = 1;
    this->LOD++;
    // we also have to decrease the number of voxels for the new level
    this->voxels[0] = voxels[0] /= 2; if(voxels[0] == 0) { voxels[0] = 1; }
    this->voxels[1] = voxels[1] /= 2; if(voxels[1] == 0) { voxels[1] = 1; }
    this->voxels[2] = voxels[2] /= 2; if(voxels[2] == 0) { voxels[2] = 1; }
  }
  if(this->LOD >= this->MaxLODs) {
    // invalidate the iterator.
    this->voxels[0] = this->voxels[1] = this->voxels[2] = 0ULL;
    this->location[0] = this->location[1] = this->location[2] = 0ULL;
  }
  return *this;
}

const_brick_iterator& const_brick_iterator::operator++() {
  return this->advance();
}

const std::pair<BrickKey, BrickMD> const_brick_iterator::dereference() const {
  const size_t timestep = 0; // unsupported/unimplemented.
  std::array<uint64_t,3> loc_sub1 = {{
    this->location[0] - 1, this->location[1] - 1, this->location[2] - 1
  }};
  uint64_t index = to1d(
    loc_sub1, layout(this->voxels, this->bsize)
  );
  BrickKey bk(timestep, this->LOD, index);

  FLOATVECTOR3 exts, center;
  { // what percentage of the way through the volume are we?
    const std::array<uint64_t,3> vlow = {{
      loc_sub1[0] * this->bsize[0],
      loc_sub1[1] * this->bsize[1],
      loc_sub1[2] * this->bsize[2]
    }};
    // the high coord is the low coord + the number of voxels in the brick
    const std::array<uint32_t,3> bsz = nvoxels(loc_sub1, this->bsize,
                                               this->voxels);
    const std::array<uint64_t,3> vhigh = {{
      vlow[0]+bsz[0], vlow[1]+bsz[1], vlow[2]+bsz[2]
    }};
    // where the center of this brick would be, in voxels.  note this is FP:
    // the 'center' could be a half-voxel in, if the brick has an odd number of
    // voxels.
    const std::array<float,3> vox_center = {{
      ((vhigh[0]-vlow[0])/2.0f) + vlow[0],
      ((vhigh[1]-vlow[1])/2.0f) + vlow[1],
      ((vhigh[2]-vlow[2])/2.0f) + vlow[2],
    }};
    const std::array<float,3> voxelsf = {{ // for type purposes.
      static_cast<float>(this->voxels[0]), static_cast<float>(this->voxels[1]),
      static_cast<float>(this->voxels[2])
    }};
    assert(vox_center[0] < voxelsf[0]);
    assert(vox_center[1] < voxelsf[1]);
    assert(vox_center[2] < voxelsf[2]);
    // we know the center in voxels, and we know the width of the domain in
    // world space.  interpolate to get the center in world space.
    center = FLOATVECTOR3(
      MathTools::lerp(vox_center[0], 0.0f,voxelsf[0], this->extents[0][0],
                                                      this->extents[1][0]),
      MathTools::lerp(vox_center[1], 0.0f,voxelsf[1], this->extents[0][1],
                                                      this->extents[1][1]),
      MathTools::lerp(vox_center[2], 0.0f,voxelsf[2], this->extents[0][2],
                                                      this->extents[1][2])
    );
    assert(extents[0][0] <= center[0] && center[0] <= extents[1][0]);
    assert(extents[0][1] <= center[1] && center[1] <= extents[1][1]);
    assert(extents[0][2] <= center[2] && center[2] <= extents[1][2]);
    const uint64_t zero = 0ULL; // for type purposes.
    const FLOATVECTOR3 wlow(
      MathTools::lerp(vlow[0], zero,this->voxels[0],
                      this->extents[0][0],this->extents[1][0]),
      MathTools::lerp(vlow[1], zero,this->voxels[1],
                      this->extents[0][1],this->extents[1][1]),
      MathTools::lerp(vlow[2], zero,this->voxels[2],
                      this->extents[0][2],this->extents[1][2])
    );
    const FLOATVECTOR3 whigh(
      MathTools::lerp(vhigh[0], zero,this->voxels[0],
                      this->extents[0][0],this->extents[1][0]),
      MathTools::lerp(vhigh[1], zero,this->voxels[1],
                      this->extents[0][1],this->extents[1][1]),
      MathTools::lerp(vhigh[2], zero,this->voxels[2],
                      this->extents[0][2],this->extents[1][2])
    );
    exts = FLOATVECTOR3(whigh[0]-wlow[0], whigh[1]-wlow[1], whigh[2]-wlow[2]);
    assert(exts[0] <= (this->extents[1][0]-this->extents[0][0]));
    assert(exts[1] <= (this->extents[1][1]-this->extents[0][1]));
    assert(exts[2] <= (this->extents[1][2]-this->extents[0][2]));
  }

  UINTVECTOR3 nvox = va(nvoxels(loc_sub1, this->bsize, this->voxels));
  BrickMD md = { center, exts, nvox };
  return std::make_pair(bk, md);
}
const std::pair<BrickKey, BrickMD> const_brick_iterator::operator*() const {
  return this->dereference();
}

bool const_brick_iterator::equals(const const_brick_iterator& iter) const {
  return this->location == iter.location;
}
bool const_brick_iterator::operator==(const const_brick_iterator& that) const {
  return this->equals(that);
}
bool const_brick_iterator::operator!=(const const_brick_iterator& that) const {
  return !this->equals(that);
}

const_brick_iterator begin(const std::array<uint64_t,3>& voxels,
                           const std::array<size_t,3>& bricksize,
                           const std::array<std::array<float,3>,2>& extents) {
  return const_brick_iterator(voxels, bricksize, extents);
}
const_brick_iterator end() { return const_brick_iterator(); }

typedef std::array<size_t,3> BrickSize;

static bool test() {
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
  return true;
}
static bool cbiter = test();
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
