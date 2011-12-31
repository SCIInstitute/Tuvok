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
  \file    Brick.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once
#ifndef TUVOK_BRICK_H
#define TUVOK_BRICK_H

#include <StdTuvokDefines.h>

#include <utility>
#ifdef DETECTED_OS_WINDOWS
# include <functional>
# include <memory>
# include <tuple>
# include <unordered_map>
#else
# include <tr1/functional>
# include <tr1/memory>
# include <tr1/tuple>
# include <tr1/unordered_map>
#endif
#include "Basics/Vectors.h"

namespace tuvok {

/// Datasets are organized as a set of bricks, stored in a hash table.  A key
/// into this table consists of an LOD index plus a brick index. 
/// An element in the table contains
/// brick metadata, but no data; to obtain the data one must query the dataset.
/// @todo FIXME: this can be a tuple internally, but the interface should be a
///  struct or similar: if a new component gets added to the key which is not
///  the last component, it shifts all the indices, necessitating massive code
///  changes.
typedef std::tr1::tuple<size_t, size_t, size_t> BrickKey; ///< timestep + LOD + 1D brick index
struct BrickMD {
  FLOATVECTOR3 center; ///< center of the brick, in world coords
  FLOATVECTOR3 extents; ///< width/height/depth of the brick.
  UINTVECTOR3  n_voxels; ///< number of voxels, per dimension.
};
struct BKeyHash : std::unary_function<BrickKey, std::size_t> {
  std::size_t operator()(const BrickKey& bk) const {
    size_t ts    = std::tr1::hash<size_t>()(std::tr1::get<0>(bk));
    size_t h_lod = std::tr1::hash<size_t>()(std::tr1::get<1>(bk));
    size_t brick = std::tr1::hash<size_t>()(std::tr1::get<2>(bk));
    size_t seed = h_lod;
    seed ^= brick + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= ts + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }
};
typedef std::tr1::unordered_map<BrickKey, BrickMD, BKeyHash> BrickTable;

} // namespace tuvok

#endif // TUVOK_BRICK_H
