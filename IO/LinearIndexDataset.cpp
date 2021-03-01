#include <cassert>
#include "LinearIndexDataset.h"

namespace tuvok {
// given a (x,y,z,LOD) tuple, convert that to one of our Brickkeys, which use a
// 1D (brick) index internally.
BrickKey LinearIndexDataset::IndexFrom4D(const UINTVECTOR4& four,
                                         size_t timestep) const {
  // the fourth component represents the LOD.
  const size_t lod = static_cast<size_t>(four.w);
  UINTVECTOR3 layout = this->GetBrickLayout(lod, timestep);

  const BrickKey k = BrickKey(timestep, lod, four.x +
                                             four.y*layout.x +
                                             four.z*layout.x*layout.y);
  // it must be an actual brick we know about!
  assert(this->bricks.find(k) != this->bricks.end());
  return k;
}

// our brick Keys have 1D indices internally; compute the (x,y,z,LOD) tuple
// index based on the 1D index and the dataset size.
UINTVECTOR4 LinearIndexDataset::IndexTo4D(const BrickKey& key) const {
  assert(this->bricks.find(key) != this->bricks.end());

  const size_t timestep = std::get<0>(key);
  const size_t lod = std::get<1>(key);
  const size_t idx1d = std::get<2>(key);

  const UINTVECTOR3 layout = this->GetBrickLayout(lod, timestep);
  const UINTVECTOR4 rv = UINTVECTOR4(
    idx1d % layout.x,
    (idx1d / layout.x) % layout.y,
    idx1d / (layout.x*layout.y),
    lod
  );
  assert(rv[2] < layout[2]);
  return rv;
}
}
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
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
