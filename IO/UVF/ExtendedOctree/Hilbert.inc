#ifdef WIN32
  #pragma warning(disable:4146)
#endif

/*
  Hilbert code implementation is inspired by:
  http://web.archive.org/web/20040811200015/http://www.caam.rice.edu/~dougm/twiddle/Hilbert/
*/

/* implementation of the Hilbert functions */

#define adjustRotation(rotation, nDims, bits)\
  do {\
    /* rotation = (rotation + 1 + ffs(bits)) % nDims; */\
    bits &= -bits & nd1Ones;\
    while (bits)\
      bits >>= 1, ++rotation;\
    if (++rotation >= nDims)\
      rotation -= nDims;\
  } while (0)

#define ones(T, k)\
  ((((T)2) << (k-1)) - 1)

#define rdbit(w, k)\
  (((w) >> (k)) & 1)

#define rotateRight(arg, nRots, nDims)\
  ((((arg) >> (nRots)) | ((arg) << ((nDims)-(nRots)))) & ones(Bitmask, nDims))

#define rotateLeft(arg, nRots, nDims)\
  ((((arg) << (nRots)) | ((arg) >> ((nDims)-(nRots)))) & ones(Bitmask, nDims))

#define DLOGB_BIT_TRANSPOSE
template<size_t nDims, size_t nBits, class Bitmask, class Halfmask>
template<size_t iDims, size_t iBits>
Bitmask Curve<nDims, nBits, Bitmask, Halfmask>::BitTranspose(Bitmask inCoords)
#if defined(DLOGB_BIT_TRANSPOSE)
{
  size_t const nDims1 = iDims-1;
  size_t inB = iBits;
  size_t utB;
  Bitmask inFieldEnds = 1;
  Bitmask inMask = ones(Bitmask, inB);
  Bitmask coords = 0;

  while ((utB = inB / 2) != 0)
  {
    size_t const shiftAmt = nDims1 * utB;
    Bitmask const utFieldEnds =
      inFieldEnds | (inFieldEnds << (shiftAmt+utB));
    Bitmask const utMask =
      (utFieldEnds << utB) - utFieldEnds;
    Bitmask utCoords = 0;
    size_t d;
    if (inB & 1)
    {
      Bitmask const inFieldStarts = inFieldEnds << (inB-1);
      size_t oddShift = 2*shiftAmt;
      for (d = 0; d < iDims; ++d)
      {
        Bitmask in = inCoords & inMask;
        inCoords >>= inB;
        coords |= (in & inFieldStarts) <<	oddShift++;
        in &= ~inFieldStarts;
        in = (in | (in << shiftAmt)) & utMask;
        utCoords |= in << (d*utB);
      }
    }
    else
    {
      for (d = 0; d < iDims; ++d)
      {
        Bitmask in = inCoords & inMask;
        inCoords >>= inB;
        in = (in | (in << shiftAmt)) & utMask;
        utCoords |= in << (d*utB);
      }
    }
    inCoords = utCoords;
    inB = utB;
    inFieldEnds = utFieldEnds;
    inMask = utMask;
  }
  coords |= inCoords;
  return coords;
}
#else
{
  Bitmask coords = 0;
  size_t d;
  for (d = 0; d < iDims; ++d)
  {
    size_t b;
    Bitmask in = inCoords & ones(Bitmask, iBits);
    Bitmask out = 0;
    inCoords >>= iBits;
    for (b = iBits; b--;)
    {
      out <<= iDims;
      out |= rdbit(in, b);
    }
    coords |= out << d;
  }
  return coords;
}
#endif

template<size_t nDims, size_t nBits, class Bitmask, class Halfmask>
void
Curve<nDims, nBits, Bitmask, Halfmask>::Decode(
  typename Curve<nDims, nBits, Bitmask, Halfmask>::Index index,
  typename Curve<nDims, nBits, Bitmask, Halfmask>::Point& point
  )
{
  static_assert(std::is_unsigned<Bitmask>::value, "Bitmask must be an unsigned type");
  static_assert(std::is_unsigned<Halfmask>::value, "Halfmask must be an unsigned type");
  static_assert(nDims * nBits <= sizeof(Bitmask) * 8, "Bitmask of insufficient size");
  static_assert(sizeof(Bitmask) <= sizeof(Halfmask) * 2, "Halfmask must be at least half the size of Bitmask");
  
  if (nDims > 1) {
    Bitmask coords;
    Halfmask const nbOnes = ones(Halfmask, nBits);
    size_t d;

    if (nBits > 1) {
      size_t const nDimsBits = nDims * nBits;
      Halfmask const ndOnes = ones(Halfmask, nDims);
      Halfmask const nd1Ones= ndOnes >> 1; /* for adjust_rotation */
      size_t b = nDimsBits;
      size_t rotation = 0;
      Halfmask flipBit = 0;
      Bitmask const nthbits = ones(Bitmask, nDimsBits) / ndOnes;
      index ^= (index ^ nthbits) >> 1;
      coords = 0;
      do {
        Halfmask bits = (index >> (b-=nDims)) & ndOnes;
        coords <<= nDims;
        coords |= rotateLeft(bits, rotation, nDims) ^ flipBit;
        flipBit = (Halfmask)1 << rotation;
        adjustRotation(rotation, nDims, bits);
      } while (b);
      for (b = nDims; b < nDimsBits; b *= 2)
        coords ^= coords >> b;
      coords = BitTranspose<nBits, nDims>(coords); // we need to swap nBits and nDims here for decoding!
    } else
      coords = index ^ (index >> 1);

    for (d = 0; d < nDims; ++d) {
      point[d] = coords & nbOnes;
      coords >>= nBits;
    }
  } else
    point[0] = index;
}

template<size_t nDims, size_t nBits, class Bitmask, class Halfmask>
typename Curve<nDims, nBits, Bitmask, Halfmask>::Index
Curve<nDims, nBits, Bitmask, Halfmask>::Encode(
  typename Curve<nDims, nBits, Bitmask, Halfmask>::Point const& point
  )
{
  static_assert(std::is_unsigned<Bitmask>::value, "Bitmask must be an unsigned type");
  static_assert(std::is_unsigned<Halfmask>::value, "Halfmask must be an unsigned type");
  static_assert(nDims * nBits <= sizeof(Bitmask) * 8, "Bitmask of insufficient size");
  static_assert(sizeof(Bitmask) <= sizeof(Halfmask) * 2, "Halfmask must be at least half the size of Bitmask");

  if (nDims > 1) {
    size_t const nDimsBits = nDims * nBits;
    Bitmask index;
    size_t d;
    Bitmask coords = 0;
    for (d = nDims; d--;) {
      coords <<= nBits;
      coords |= point[d];
    }

    if (nBits > 1) {
      Halfmask const ndOnes = ones(Halfmask, nDims);
      Halfmask const nd1Ones= ndOnes >> 1; /* for adjust_rotation */
      size_t b = nDimsBits;
      size_t rotation = 0;
      Halfmask flipBit = 0;
      Bitmask const nthbits = ones(Bitmask, nDimsBits) / ndOnes;
      coords = BitTranspose<nDims, nBits>(coords);
      coords ^= coords >> nDims;
      index = 0;
      do {
        Halfmask bits = (coords >> (b-=nDims)) & ndOnes;
        bits = rotateRight(flipBit ^ bits, rotation, nDims);
        index <<= nDims;
        index |= bits;
        flipBit = (Halfmask)1 << rotation;
        adjustRotation(rotation, nDims, bits);
      } while (b);
      index ^= nthbits >> 1;
    } else
      index = coords;
    
    for (d = 1; d < nDimsBits; d *= 2)
      index ^= index >> d;
    return index;
  } else
    return point[0];
}

template<>
void Decode<uint16_t>(size_t nBits, uint16_t index, std::array<uint16_t, 2>& point)
{
  switch (nBits) {
  case  0: point.fill(0); break;
  case  1: Curve<2,  1, uint16_t, uint8_t>::Decode(index, point); break;
  case  2: Curve<2,  2, uint16_t, uint8_t>::Decode(index, point); break;
  case  3: Curve<2,  3, uint16_t, uint8_t>::Decode(index, point); break;
  case  4: Curve<2,  4, uint16_t, uint8_t>::Decode(index, point); break;
  case  5: Curve<2,  5, uint16_t, uint8_t>::Decode(index, point); break;
  case  6: Curve<2,  6, uint16_t, uint8_t>::Decode(index, point); break;
  case  7: Curve<2,  7, uint16_t, uint8_t>::Decode(index, point); break;
  case  8: Curve<2,  8, uint16_t, uint8_t>::Decode(index, point); break;
  default: assert(false); break;
  }
}

template<>
uint16_t Encode<uint16_t>(size_t nBits, std::array<uint16_t, 2> const& point)
{
  switch (nBits) {
  case  0: return 0;
  case  1: return Curve<2,  1, uint16_t, uint8_t>::Encode(point);
  case  2: return Curve<2,  2, uint16_t, uint8_t>::Encode(point);
  case  3: return Curve<2,  3, uint16_t, uint8_t>::Encode(point);
  case  4: return Curve<2,  4, uint16_t, uint8_t>::Encode(point);
  case  5: return Curve<2,  5, uint16_t, uint8_t>::Encode(point);
  case  6: return Curve<2,  6, uint16_t, uint8_t>::Encode(point);
  case  7: return Curve<2,  7, uint16_t, uint8_t>::Encode(point);
  case  8: return Curve<2,  8, uint16_t, uint8_t>::Encode(point);
  default: assert(false); break;
  }
  return 0;
}

template<>
void Decode<uint32_t>(size_t nBits, uint32_t index, std::array<uint32_t, 2>& point)
{
  switch (nBits) {
  case  0: point.fill(0); break;
  case  1: Curve<2,  1, uint32_t, uint16_t>::Decode(index, point); break;
  case  2: Curve<2,  2, uint32_t, uint16_t>::Decode(index, point); break;
  case  3: Curve<2,  3, uint32_t, uint16_t>::Decode(index, point); break;
  case  4: Curve<2,  4, uint32_t, uint16_t>::Decode(index, point); break;
  case  5: Curve<2,  5, uint32_t, uint16_t>::Decode(index, point); break;
  case  6: Curve<2,  6, uint32_t, uint16_t>::Decode(index, point); break;
  case  7: Curve<2,  7, uint32_t, uint16_t>::Decode(index, point); break;
  case  8: Curve<2,  8, uint32_t, uint16_t>::Decode(index, point); break;
  case  9: Curve<2,  9, uint32_t, uint16_t>::Decode(index, point); break;
  case 10: Curve<2, 10, uint32_t, uint16_t>::Decode(index, point); break;
  case 11: Curve<2, 11, uint32_t, uint16_t>::Decode(index, point); break;
  case 12: Curve<2, 12, uint32_t, uint16_t>::Decode(index, point); break;
  case 13: Curve<2, 13, uint32_t, uint16_t>::Decode(index, point); break;
  case 14: Curve<2, 14, uint32_t, uint16_t>::Decode(index, point); break;
  case 15: Curve<2, 15, uint32_t, uint16_t>::Decode(index, point); break;
  case 16: Curve<2, 16, uint32_t, uint16_t>::Decode(index, point); break;
  default: assert(false); break;
  }
}

template<>
uint32_t Encode<uint32_t>(size_t nBits, std::array<uint32_t, 2> const& point)
{
  switch (nBits) {
  case  0: return 0;
  case  1: return Curve<2,  1, uint32_t, uint16_t>::Encode(point);
  case  2: return Curve<2,  2, uint32_t, uint16_t>::Encode(point);
  case  3: return Curve<2,  3, uint32_t, uint16_t>::Encode(point);
  case  4: return Curve<2,  4, uint32_t, uint16_t>::Encode(point);
  case  5: return Curve<2,  5, uint32_t, uint16_t>::Encode(point);
  case  6: return Curve<2,  6, uint32_t, uint16_t>::Encode(point);
  case  7: return Curve<2,  7, uint32_t, uint16_t>::Encode(point);
  case  8: return Curve<2,  8, uint32_t, uint16_t>::Encode(point);
  case  9: return Curve<2,  9, uint32_t, uint16_t>::Encode(point);
  case 10: return Curve<2, 10, uint32_t, uint16_t>::Encode(point);
  case 11: return Curve<2, 11, uint32_t, uint16_t>::Encode(point);
  case 12: return Curve<2, 12, uint32_t, uint16_t>::Encode(point);
  case 13: return Curve<2, 13, uint32_t, uint16_t>::Encode(point);
  case 14: return Curve<2, 14, uint32_t, uint16_t>::Encode(point);
  case 15: return Curve<2, 15, uint32_t, uint16_t>::Encode(point);
  case 16: return Curve<2, 16, uint32_t, uint16_t>::Encode(point);
  default: assert(false); break;
  }
  return 0;
}

template<>
void Decode<uint64_t>(size_t nBits, uint64_t index, std::array<uint64_t, 2>& point)
{
  switch (nBits) {
  case  0: point.fill(0); break;
  case  1: Curve<2,  1, uint64_t, uint32_t>::Decode(index, point); break;
  case  2: Curve<2,  2, uint64_t, uint32_t>::Decode(index, point); break;
  case  3: Curve<2,  3, uint64_t, uint32_t>::Decode(index, point); break;
  case  4: Curve<2,  4, uint64_t, uint32_t>::Decode(index, point); break;
  case  5: Curve<2,  5, uint64_t, uint32_t>::Decode(index, point); break;
  case  6: Curve<2,  6, uint64_t, uint32_t>::Decode(index, point); break;
  case  7: Curve<2,  7, uint64_t, uint32_t>::Decode(index, point); break;
  case  8: Curve<2,  8, uint64_t, uint32_t>::Decode(index, point); break;
  case  9: Curve<2,  9, uint64_t, uint32_t>::Decode(index, point); break;
  case 10: Curve<2, 10, uint64_t, uint32_t>::Decode(index, point); break;
  case 11: Curve<2, 11, uint64_t, uint32_t>::Decode(index, point); break;
  case 12: Curve<2, 12, uint64_t, uint32_t>::Decode(index, point); break;
  case 13: Curve<2, 13, uint64_t, uint32_t>::Decode(index, point); break;
  case 14: Curve<2, 14, uint64_t, uint32_t>::Decode(index, point); break;
  case 15: Curve<2, 15, uint64_t, uint32_t>::Decode(index, point); break;
  case 16: Curve<2, 16, uint64_t, uint32_t>::Decode(index, point); break;
  case 17: Curve<2, 17, uint64_t, uint32_t>::Decode(index, point); break;
  case 18: Curve<2, 18, uint64_t, uint32_t>::Decode(index, point); break;
  case 19: Curve<2, 19, uint64_t, uint32_t>::Decode(index, point); break;
  case 20: Curve<2, 20, uint64_t, uint32_t>::Decode(index, point); break;
  case 21: Curve<2, 21, uint64_t, uint32_t>::Decode(index, point); break;
  case 22: Curve<2, 22, uint64_t, uint32_t>::Decode(index, point); break;
  case 23: Curve<2, 23, uint64_t, uint32_t>::Decode(index, point); break;
  case 24: Curve<2, 24, uint64_t, uint32_t>::Decode(index, point); break;
  case 25: Curve<2, 25, uint64_t, uint32_t>::Decode(index, point); break;
  case 26: Curve<2, 26, uint64_t, uint32_t>::Decode(index, point); break;
  case 27: Curve<2, 27, uint64_t, uint32_t>::Decode(index, point); break;
  case 28: Curve<2, 28, uint64_t, uint32_t>::Decode(index, point); break;
  case 29: Curve<2, 29, uint64_t, uint32_t>::Decode(index, point); break;
  case 30: Curve<2, 30, uint64_t, uint32_t>::Decode(index, point); break;
  case 31: Curve<2, 31, uint64_t, uint32_t>::Decode(index, point); break;
  case 32: Curve<2, 32, uint64_t, uint32_t>::Decode(index, point); break;
  default: assert(false); break;
  }
}

template<>
uint64_t Encode<uint64_t>(size_t nBits, std::array<uint64_t, 2> const& point)
{
  switch (nBits) {
  case  0: return 0;
  case  1: return Curve<2,  1, uint64_t, uint32_t>::Encode(point);
  case  2: return Curve<2,  2, uint64_t, uint32_t>::Encode(point);
  case  3: return Curve<2,  3, uint64_t, uint32_t>::Encode(point);
  case  4: return Curve<2,  4, uint64_t, uint32_t>::Encode(point);
  case  5: return Curve<2,  5, uint64_t, uint32_t>::Encode(point);
  case  6: return Curve<2,  6, uint64_t, uint32_t>::Encode(point);
  case  7: return Curve<2,  7, uint64_t, uint32_t>::Encode(point);
  case  8: return Curve<2,  8, uint64_t, uint32_t>::Encode(point);
  case  9: return Curve<2,  9, uint64_t, uint32_t>::Encode(point);
  case 10: return Curve<2, 10, uint64_t, uint32_t>::Encode(point);
  case 11: return Curve<2, 11, uint64_t, uint32_t>::Encode(point);
  case 12: return Curve<2, 12, uint64_t, uint32_t>::Encode(point);
  case 13: return Curve<2, 13, uint64_t, uint32_t>::Encode(point);
  case 14: return Curve<2, 14, uint64_t, uint32_t>::Encode(point);
  case 15: return Curve<2, 15, uint64_t, uint32_t>::Encode(point);
  case 16: return Curve<2, 16, uint64_t, uint32_t>::Encode(point);
  case 17: return Curve<2, 17, uint64_t, uint32_t>::Encode(point);
  case 18: return Curve<2, 18, uint64_t, uint32_t>::Encode(point);
  case 19: return Curve<2, 19, uint64_t, uint32_t>::Encode(point);
  case 20: return Curve<2, 20, uint64_t, uint32_t>::Encode(point);
  case 21: return Curve<2, 21, uint64_t, uint32_t>::Encode(point);
  case 22: return Curve<2, 22, uint64_t, uint32_t>::Encode(point);
  case 23: return Curve<2, 23, uint64_t, uint32_t>::Encode(point);
  case 24: return Curve<2, 24, uint64_t, uint32_t>::Encode(point);
  case 25: return Curve<2, 25, uint64_t, uint32_t>::Encode(point);
  case 26: return Curve<2, 26, uint64_t, uint32_t>::Encode(point);
  case 27: return Curve<2, 27, uint64_t, uint32_t>::Encode(point);
  case 28: return Curve<2, 28, uint64_t, uint32_t>::Encode(point);
  case 29: return Curve<2, 29, uint64_t, uint32_t>::Encode(point);
  case 30: return Curve<2, 30, uint64_t, uint32_t>::Encode(point);
  case 31: return Curve<2, 31, uint64_t, uint32_t>::Encode(point);
  case 32: return Curve<2, 32, uint64_t, uint32_t>::Encode(point);
  default: assert(false); break;
  }
  return 0;
}

template<>
void Decode<uint16_t>(size_t nBits, uint16_t index, std::array<uint16_t, 3>& point)
{
  switch (nBits) {
  case  0: point.fill(0); break;
  case  1: Curve<3,  1, uint16_t, uint8_t>::Decode(index, point); break;
  case  2: Curve<3,  2, uint16_t, uint8_t>::Decode(index, point); break;
  case  3: Curve<3,  3, uint16_t, uint8_t>::Decode(index, point); break;
  case  4: Curve<3,  4, uint16_t, uint8_t>::Decode(index, point); break;
  case  5: Curve<3,  5, uint16_t, uint8_t>::Decode(index, point); break;
  default: assert(false); break;
  }
}

template<>
uint16_t Encode<uint16_t>(size_t nBits, std::array<uint16_t, 3> const& point)
{
  switch (nBits) {
  case  0: return 0;
  case  1: return Curve<3,  1, uint16_t, uint8_t>::Encode(point);
  case  2: return Curve<3,  2, uint16_t, uint8_t>::Encode(point);
  case  3: return Curve<3,  3, uint16_t, uint8_t>::Encode(point);
  case  4: return Curve<3,  4, uint16_t, uint8_t>::Encode(point);
  case  5: return Curve<3,  5, uint16_t, uint8_t>::Encode(point);
  default: assert(false); break;
  }
  return 0;
}

template<>
void Decode<uint32_t>(size_t nBits, uint32_t index, std::array<uint32_t, 3>& point)
{
  switch (nBits) {
  case  0: point.fill(0); break;
  case  1: Curve<3,  1, uint32_t, uint16_t>::Decode(index, point); break;
  case  2: Curve<3,  2, uint32_t, uint16_t>::Decode(index, point); break;
  case  3: Curve<3,  3, uint32_t, uint16_t>::Decode(index, point); break;
  case  4: Curve<3,  4, uint32_t, uint16_t>::Decode(index, point); break;
  case  5: Curve<3,  5, uint32_t, uint16_t>::Decode(index, point); break;
  case  6: Curve<3,  6, uint32_t, uint16_t>::Decode(index, point); break;
  case  7: Curve<3,  7, uint32_t, uint16_t>::Decode(index, point); break;
  case  8: Curve<3,  8, uint32_t, uint16_t>::Decode(index, point); break;
  case  9: Curve<3,  9, uint32_t, uint16_t>::Decode(index, point); break;
  case 10: Curve<3, 10, uint32_t, uint16_t>::Decode(index, point); break;
  default: assert(false); break;
  }
}

template<>
uint32_t Encode<uint32_t>(size_t nBits, std::array<uint32_t, 3> const& point)
{
  switch (nBits) {
  case  0: return 0;
  case  1: return Curve<3,  1, uint32_t, uint16_t>::Encode(point);
  case  2: return Curve<3,  2, uint32_t, uint16_t>::Encode(point);
  case  3: return Curve<3,  3, uint32_t, uint16_t>::Encode(point);
  case  4: return Curve<3,  4, uint32_t, uint16_t>::Encode(point);
  case  5: return Curve<3,  5, uint32_t, uint16_t>::Encode(point);
  case  6: return Curve<3,  6, uint32_t, uint16_t>::Encode(point);
  case  7: return Curve<3,  7, uint32_t, uint16_t>::Encode(point);
  case  8: return Curve<3,  8, uint32_t, uint16_t>::Encode(point);
  case  9: return Curve<3,  9, uint32_t, uint16_t>::Encode(point);
  case 10: return Curve<3, 10, uint32_t, uint16_t>::Encode(point);
  default: assert(false); break;
  }
  return 0;
}

template<>
void Decode<uint64_t>(size_t nBits, uint64_t index, std::array<uint64_t, 3>& point)
{
  switch (nBits) {
  case  0: point.fill(0); break;
  case  1: Curve<3,  1, uint64_t, uint32_t>::Decode(index, point); break;
  case  2: Curve<3,  2, uint64_t, uint32_t>::Decode(index, point); break;
  case  3: Curve<3,  3, uint64_t, uint32_t>::Decode(index, point); break;
  case  4: Curve<3,  4, uint64_t, uint32_t>::Decode(index, point); break;
  case  5: Curve<3,  5, uint64_t, uint32_t>::Decode(index, point); break;
  case  6: Curve<3,  6, uint64_t, uint32_t>::Decode(index, point); break;
  case  7: Curve<3,  7, uint64_t, uint32_t>::Decode(index, point); break;
  case  8: Curve<3,  8, uint64_t, uint32_t>::Decode(index, point); break;
  case  9: Curve<3,  9, uint64_t, uint32_t>::Decode(index, point); break;
  case 10: Curve<3, 10, uint64_t, uint32_t>::Decode(index, point); break;
  case 11: Curve<3, 11, uint64_t, uint32_t>::Decode(index, point); break;
  case 12: Curve<3, 12, uint64_t, uint32_t>::Decode(index, point); break;
  case 13: Curve<3, 13, uint64_t, uint32_t>::Decode(index, point); break;
  case 14: Curve<3, 14, uint64_t, uint32_t>::Decode(index, point); break;
  case 15: Curve<3, 15, uint64_t, uint32_t>::Decode(index, point); break;
  case 16: Curve<3, 16, uint64_t, uint32_t>::Decode(index, point); break;
  case 17: Curve<3, 17, uint64_t, uint32_t>::Decode(index, point); break;
  case 18: Curve<3, 18, uint64_t, uint32_t>::Decode(index, point); break;
  case 19: Curve<3, 19, uint64_t, uint32_t>::Decode(index, point); break;
  case 20: Curve<3, 20, uint64_t, uint32_t>::Decode(index, point); break;
  case 21: Curve<3, 21, uint64_t, uint32_t>::Decode(index, point); break;
  default: assert(false); break;
  }
}

template<>
uint64_t Encode<uint64_t>(size_t nBits, std::array<uint64_t, 3> const& point)
{
  switch (nBits) {
  case  0: return 0;
  case  1: return Curve<3,  1, uint64_t, uint32_t>::Encode(point);
  case  2: return Curve<3,  2, uint64_t, uint32_t>::Encode(point);
  case  3: return Curve<3,  3, uint64_t, uint32_t>::Encode(point);
  case  4: return Curve<3,  4, uint64_t, uint32_t>::Encode(point);
  case  5: return Curve<3,  5, uint64_t, uint32_t>::Encode(point);
  case  6: return Curve<3,  6, uint64_t, uint32_t>::Encode(point);
  case  7: return Curve<3,  7, uint64_t, uint32_t>::Encode(point);
  case  8: return Curve<3,  8, uint64_t, uint32_t>::Encode(point);
  case  9: return Curve<3,  9, uint64_t, uint32_t>::Encode(point);
  case 10: return Curve<3, 10, uint64_t, uint32_t>::Encode(point);
  case 11: return Curve<3, 11, uint64_t, uint32_t>::Encode(point);
  case 12: return Curve<3, 12, uint64_t, uint32_t>::Encode(point);
  case 13: return Curve<3, 13, uint64_t, uint32_t>::Encode(point);
  case 14: return Curve<3, 14, uint64_t, uint32_t>::Encode(point);
  case 15: return Curve<3, 15, uint64_t, uint32_t>::Encode(point);
  case 16: return Curve<3, 16, uint64_t, uint32_t>::Encode(point);
  case 17: return Curve<3, 17, uint64_t, uint32_t>::Encode(point);
  case 18: return Curve<3, 18, uint64_t, uint32_t>::Encode(point);
  case 19: return Curve<3, 19, uint64_t, uint32_t>::Encode(point);
  case 20: return Curve<3, 20, uint64_t, uint32_t>::Encode(point);
  case 21: return Curve<3, 21, uint64_t, uint32_t>::Encode(point);
  default: assert(false); break;
  }
  return 0;
}

#ifdef WIN32
  #pragma warning(default:4146)
#endif