#include <array>
#include <cmath>
#include "const-brick-iterator.h"

namespace tuvok {

// converts a 3D index ('loc') into a 1D index.
static uint64_t to1d(const std::array<uint64_t,3> loc,
                     const std::array<uint64_t,3> size) {
  return loc[2]*size[1]*size[0] + loc[1]*size[0] + loc[0];
}

// just converts a 3-element array into a UINTVECTOR3.
static UINTVECTOR3 va(const std::array<uint32_t,3> a) {
  return UINTVECTOR3(a[0], a[1], a[2]);
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

// The dimension with the largest ratio of size-to-bricksize
#define DIM \
    (vox[0] / bsize[0] > vox[1] / bsize[1] ? \
      (vox[0] / bsize[0] > vox[2] / bsize[2] ? 0 : 2) \
    : (vox[1] / bsize[1] > vox[2] / bsize[2] ? 1 : 2))
const_brick_iterator::const_brick_iterator(
  const std::array<uint64_t,3> vox,
  const std::array<unsigned,3> bricksize
) : bsize(bricksize),
    // a bit intense for an initializer list, but it needs to be here so this
    // can be const.
    MaxLODs(static_cast<size_t>(ceil(double(vox[DIM]) / bricksize[DIM]))),
    voxels(vox), LOD(0)
#undef DIM
{
  location[0] = location[1] = location[2] = 1ULL;
}

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
    if(this->voxels[0] > this->bsize[0]) { this->voxels[0] = voxels[0] /= 2; }
    if(this->voxels[1] > this->bsize[1]) { this->voxels[1] = voxels[1] /= 2; }
    if(this->voxels[2] > this->bsize[2]) { this->voxels[2] = voxels[2] /= 2; }
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

  BrickMD md = {
    FLOATVECTOR3(0.0f, 0.0f, 0.0f), // fixme
    FLOATVECTOR3(0.0f, 0.0f, 0.0f), // fixme
    UINTVECTOR3(va(nvoxels(this->location, this->bsize, this->voxels)))
  };
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

const_brick_iterator begin(const std::array<uint64_t,3> voxels,
                           const std::array<unsigned,3> bricksize) {
  return const_brick_iterator(voxels, bricksize);
}
const_brick_iterator end() { return const_brick_iterator(); }

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
