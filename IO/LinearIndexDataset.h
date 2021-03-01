#ifndef TUVOK_LINEAR_INDEX_DATASET_H
#define TUVOK_LINEAR_INDEX_DATASET_H

#include "BrickedDataset.h"

namespace tuvok {

/// A LinearIndexDataset is simply a bricked dataset with a particular
/// algorithm for how the indexing is performed.  Namely, the 1D index is
/// actually the linearization of a 4D (LOD, z,y,x, from slowest to fasting
/// moving dimension) index.
/// This necessary implies that there are no 'holes' in the data: if a brick
/// <0, 1,0,0> exists, than the brick <0, 0,0,0> must exist as well.
class LinearIndexDataset : public BrickedDataset {
  public:
    LinearIndexDataset() {}
    virtual ~LinearIndexDataset() {}

    /// @returns the brick layout for a given LoD.  This is the number of
    /// bricks which exist (given per-dimension)
    virtual UINTVECTOR3 GetBrickLayout(size_t LoD, size_t timestep) const=0;

    /// @returns the brick key (1D brick index) derived from the 4D key.
    virtual BrickKey IndexFrom4D(const UINTVECTOR4& four,
                                 size_t timestep) const;
    virtual UINTVECTOR4 IndexTo4D(const BrickKey& key) const;
};

}
#endif
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
