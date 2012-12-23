/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2009 Scientific Computing and Imaging Institute,
   University of Utah.


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
/**
  \file    BrickedDataset.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#include <cassert>
#include "BrickedDataset.h"
#include "Controller/Controller.h"

namespace tuvok {

BrickedDataset::BrickedDataset() { }
BrickedDataset::~BrickedDataset() { }

void BrickedDataset::NBricksHint(size_t n) {

// The following line implements bricks.reserve(n);
// in a portable way. unordered_map does not define
// a reserve function but is seems gcc's tr1 does
  bricks.rehash(size_t(ceil(float(n) / bricks.max_load_factor())));
}

/// Adds a brick to the dataset.
void BrickedDataset::AddBrick(const BrickKey& bk,
                              const BrickMD& brick)
{
/*  MESSAGE("adding brick (%u, %u, %u) -> ((%g,%g,%g), (%g,%g,%g), (%u,%u,%u))",
          static_cast<unsigned>(std::get<0>(bk)),
          static_cast<unsigned>(std::get<1>(bk)),
          static_cast<unsigned>(std::get<2>(bk)),
          brick.center[0], brick.center[1], brick.center[2],
          brick.extents[0], brick.extents[1], brick.extents[2],
          static_cast<unsigned>(brick.n_voxels[0]),
          static_cast<unsigned>(brick.n_voxels[1]),
          static_cast<unsigned>(brick.n_voxels[2]));*/
  this->bricks.insert(std::make_pair(bk, brick));
}

/// Looks up the spatial range of a brick.
FLOATVECTOR3 BrickedDataset::GetBrickExtents(const BrickKey &bk) const
{
  BrickTable::const_iterator iter = this->bricks.find(bk);
  if(iter == this->bricks.end()) {
    T_ERROR("Unknown brick (%u, %u, %u)",
            static_cast<unsigned>(std::get<0>(bk)),
            static_cast<unsigned>(std::get<1>(bk)),
            static_cast<unsigned>(std::get<2>(bk)));
    return FLOATVECTOR3(0.0f, 0.0f, 0.0f);
  }
  return iter->second.extents;
}

/// @return an iterator that can be used to visit every brick in the dataset.
BrickTable::const_iterator BrickedDataset::BricksBegin() const
{
  return this->bricks.begin();
}

BrickTable::const_iterator BrickedDataset::BricksEnd() const
{
  return this->bricks.end();
}

/// @return the number of bricks at the given LOD.
BrickTable::size_type BrickedDataset::GetBrickCount(size_t lod, size_t ts) const
{
  BrickTable::size_type count = 0;
  BrickTable::const_iterator iter = this->bricks.begin();
  for(; iter != this->bricks.end(); ++iter) {
    if(std::get<0>(iter->first) == ts &&
       std::get<1>(iter->first) == lod) {
      ++count;
    }
  }
  return count;
}

size_t BrickedDataset::GetLargestSingleBrickLod(size_t ts) const {
  const size_t n_lods = this->GetLODLevelCount();
  for(size_t lod=0; lod < n_lods; ++lod) {
    if(this->GetBrickCount(lod, ts) == 1) {
      return lod;
    }
  }
  assert("not reachable");
  return 0;
}

const BrickMD& BrickedDataset::GetBrickMetadata(const BrickKey& k) const
{
  return this->bricks.find(k)->second;
}

bool
BrickedDataset::BrickIsFirstInDimension(size_t dim, const BrickKey& k) const
{
  const BrickMD& md = this->bricks.find(k)->second;
  for(BrickTable::const_iterator iter = this->BricksBegin();
      iter != this->BricksEnd(); ++iter) {
    if(iter->second.center[dim] < md.center[dim]) {
      return false;
    }
  }
  return true;
}

bool
BrickedDataset::BrickIsLastInDimension(size_t dim, const BrickKey& k) const
{
  const BrickMD& md = this->bricks.find(k)->second;
  for(BrickTable::const_iterator iter = this->BricksBegin();
      iter != this->BricksEnd(); ++iter) {
    if(iter->second.center[dim] > md.center[dim]) {
      return false;
    }
  }
  return true;
}

void BrickedDataset::Clear() {
  MESSAGE("Clearing brick metadata.");
  bricks.clear();
}

} // namespace tuvok
