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