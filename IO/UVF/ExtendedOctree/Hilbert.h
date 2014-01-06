#pragma once

#ifndef HILBERT_H
#define HILBERT_H

#include <array>
#include <type_traits>
#include <cstdint>
#include <cassert>

namespace Hilbert {

/**
  Hilbert code implementation is inspired by:
  http://web.archive.org/web/20040811200015/http://www.caam.rice.edu/~dougm/twiddle/Hilbert/
  Original Hilbert Curve implementation copyright 1998, Rice University.
  This implementation is only modified by the use of templates to quickly track
  down bit precision problems and to speed up computations a little.
  @param nDims number of coordinate axes
  @param nBits number of bits per axis
  @param Bitmask type as an unsigned integer of sufficient size
  @param Halfmask type as an unsigned integer of at least 1/2 the size of Bitmask
  */
template<size_t nDims, size_t nBits, class Bitmask, class Halfmask = Bitmask>
class Curve {
public:
  typedef Bitmask Index;
  typedef typename std::array<Bitmask, nDims> Point;

  /**
    Convert an index into a Hilbert curve to a set of coordinates
    @param index contains nDims*nBits bits
    @param point list of nDims coordinates, each with nBits bits
    */
  static void Decode(Index index, Point& point);

  /**
    Convert coordinates of a point on a Hilbert curve to its index
    @param point array of n nBits-bit coordinates
    @return output index value of nDims*nBits bits
    */
  static Index Encode(Point const& point);

private:
  template<size_t iDims, size_t iBits>
  static Bitmask BitTranspose(Bitmask inCoords);
};

// Convenient 2D decode wrapper that takes the number of bits as an argument
template<class T>
void Decode(size_t nBits, T index, typename std::array<T, 2>& point) {
  static_assert(sizeof(T) != sizeof(T), "template specialization required");
}

// Convenient 2D encode wrapper that takes the number of bits as an argument
template<class T>
T Encode(size_t nBits, typename std::array<T, 2> const& point) {
  static_assert(sizeof(T) != sizeof(T), "template specialization required");
}

// Convenient 3D decode wrapper that takes the number of bits as an argument
template<class T>
void Decode(size_t nBits, T index, typename std::array<T, 3>& point) {
  static_assert(sizeof(T) != sizeof(T), "template specialization required");
}

// Convenient 3D encode wrapper that takes the number of bits as an argument
template<class T>
T Encode(size_t nBits, typename std::array<T, 3> const& point) {
  static_assert(sizeof(T) != sizeof(T), "template specialization required");
}

#include "Hilbert.inc"

} // namespace Hilbert

#endif // HILBERT_H

/*
 The MIT License
 
 Copyright (c) 2011 Interactive Visualization and Data Analysis Group
 
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
