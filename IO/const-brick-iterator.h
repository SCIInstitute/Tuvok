#ifndef TUVOK_CONST_BRICK_ITERATOR_H
#define TUVOK_CONST_BRICK_ITERATOR_H

#include <array>
#include <cstdint>
#include <iterator>
#include "Brick.h"

namespace tuvok {

/// Takes a number of voxels and a given brick size, and yields a brick
/// whenever it is dereferenced.  Continues on to multiple LODs, until we
/// create an LOD which is a single brick.
/// Note that this is completely ignorant of ghost data!
/// Example usage:
///   for(auto b=begin({{128,128,128}}, {{16,16,16}}, ...); b != end(); ++b) {
///     MESSAGE("working with brick <%u,%u,%u>",
///             static_cast<unsigned>(std::get<0>(b.first)),
///             static_cast<unsigned>(std::get<1>(b.first)),
///             static_cast<unsigned>(std::get<2>(b.first)));
///   }
class const_brick_iterator :
  public std::iterator<std::forward_iterator_tag,
                       const std::pair<BrickKey, BrickMD>, int> {
  public:
    ///@parameter voxels number of voxels in the data set
    ///@parameter bricksize size of the bricks to use
    ///@parameter extent low/high of the data in world space
    explicit const_brick_iterator(
      const std::array<uint64_t,3>& voxels,
      const std::array<size_t,3>& bricksize,
      const std::array<std::array<float,3>,2>& extent
    );
#ifdef _MSC_VER
    const_brick_iterator() : MaxLODs(0), LOD(0) {
#else
    const_brick_iterator() : bsize({{0,0,0}}), MaxLODs(0), voxels({{0,0,0}}),
                             LOD(0), location({{0,0,0}}),
                             extents({{ {{0.0f,0.0f,0.0f}},
                                        {{1.0f,1.0f,1.0f}} }}) {
#endif
      voxels[0] = voxels[1] = voxels[2] = 0ULL;
      location[0] = location[1] = location[2] = 0ULL;
    }

    const_brick_iterator& advance();
    const_brick_iterator& operator++();
    const std::pair<BrickKey, BrickMD> dereference() const;
    const std::pair<BrickKey, BrickMD> operator*() const;
    bool equals(const const_brick_iterator& iter) const;
    bool operator==(const const_brick_iterator& iter) const;
    bool operator!=(const const_brick_iterator& iter) const;

  private:
    const std::array<size_t,3> bsize;
    const size_t MaxLODs; ///< number of LODs we'll have total
    std::array<uint64_t,3> voxels; ///< in the current LOD
    size_t LOD; ///< what LOD we're on.  0 is fine. +1 is coarser, ...
    // current brick, in layout coords (not voxels)
    std::array<uint64_t,3> location;
    const std::array<std::array<float,3>,2> extents;
};

const_brick_iterator begin(const std::array<uint64_t,3>& voxels,
                           const std::array<size_t,3>& bricksize,
                           const std::array<std::array<float,3>,2>& extents);
const_brick_iterator end();

}
#endif // TUVOK_CONST_BRICK_ITERATOR_H
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012-2013 IVDA Group


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
