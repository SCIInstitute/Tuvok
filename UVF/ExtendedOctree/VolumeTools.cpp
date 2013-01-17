#include <cstring>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include "VolumeTools.h"
#include "Hilbert.h"

using namespace VolumeTools;

Layout::Layout(UINT64VECTOR3 const& vDomainSize)
  : m_vDomainSize(vDomainSize)
{}

bool Layout::ExceedsDomain(UINT64VECTOR3 const& vSpatialPosition)
{
  assert(vSpatialPosition.x < m_vDomainSize.x);
  assert(vSpatialPosition.y < m_vDomainSize.y);
  assert(vSpatialPosition.z < m_vDomainSize.z);
  if (vSpatialPosition.x <= m_vDomainSize.x)
    return true;
  if (vSpatialPosition.y <= m_vDomainSize.y)
    return true;
  if (vSpatialPosition.z <= m_vDomainSize.z)
    return true;
  return false;
}

ScanlineLayout::ScanlineLayout(UINT64VECTOR3 const& vDomainSize)
  : Layout(vDomainSize)
{}

uint64_t ScanlineLayout::GetLinearIndex(UINT64VECTOR3 const& vSpatialPosition)
{
  return vSpatialPosition.x + 
         vSpatialPosition.y * m_vDomainSize.x + 
         vSpatialPosition.z * m_vDomainSize.x * m_vDomainSize.y;
}

UINT64VECTOR3 ScanlineLayout::GetSpatialPosition(uint64_t iLinearIndex)
{
  UINT64VECTOR3 vPosition(0, 0, 0);

  vPosition.x = iLinearIndex % m_vDomainSize.x;
  vPosition.y = (iLinearIndex / m_vDomainSize.x) % m_vDomainSize.y;
  vPosition.z = iLinearIndex / (m_vDomainSize.x * m_vDomainSize.y);

  return vPosition;
}

MortonLayout::MortonLayout(UINT64VECTOR3 const& vDomainSize)
  : Layout(vDomainSize)
{}

uint64_t MortonLayout::GetLinearIndex(UINT64VECTOR3 const& vSpatialPosition)
{
  if (ExceedsDomain(vSpatialPosition))
    throw std::runtime_error("spatial position out of domain bounds");

  // TODO: check index overflow...

  // we use the z-order curve, so we have to interlace the bits
  // of the 3d spatial position to obtain a linear 1d index
  uint64_t const iIterations = (sizeof(uint64_t) * 8) / 3;
  uint64_t iIndex = 0;

  for (uint64_t i = 0; i < iIterations; ++i)
  {
    uint64_t const bit = 1u << i;

    iIndex |= (vSpatialPosition.x & bit) << ((i * 2) + 0);
    iIndex |= (vSpatialPosition.y & bit) << ((i * 2) + 1);
    iIndex |= (vSpatialPosition.z & bit) << ((i * 2) + 2);
  }

  return iIndex;
}

UINT64VECTOR3 MortonLayout::GetSpatialPosition(uint64_t iLinearIndex)
{
  // TODO: check if index is too large

  // we use the z-order curve, so we have to deinterlace the bits
  // of the 1d linear index to obtain the 3d spatial position
  uint64_t const iIterations = (sizeof(uint64_t) * 8) / 3;
  UINT64VECTOR3 vPosition(0, 0, 0);

  for (uint64_t i = 0u; i < iIterations; ++i)
  {
    vPosition.x |= (iLinearIndex & 1u) << i;
    vPosition.y |= (iLinearIndex & 2u) << i;
    vPosition.z |= (iLinearIndex & 4u) << i;

    iLinearIndex >>= 3u;
  }

  vPosition.y >>= 1u;
  vPosition.z >>= 2u;

  return vPosition;
}

HilbertLayout::HilbertLayout(UINT64VECTOR3 const& vDomainSize)
  : Layout(vDomainSize)
  , m_iBits(size_t(ceil(log(double(vDomainSize.maxVal()))/log(2.0))))
{}

uint64_t HilbertLayout::GetLinearIndex(UINT64VECTOR3 const& vSpatialPosition)
{
  if (ExceedsDomain(vSpatialPosition))
    throw std::runtime_error("spatial position out of domain bounds");

  std::array<uint64_t, 3> v = {{
    vSpatialPosition.x,
    vSpatialPosition.y,
    vSpatialPosition.z
  }};
  return Hilbert::Encode(m_iBits, v);
}

UINT64VECTOR3 HilbertLayout::GetSpatialPosition(uint64_t iLinearIndex)
{
  std::array<uint64_t, 3> v = {{0, 0, 0}};
  Hilbert::Decode(m_iBits, iLinearIndex, v);
  return UINT64VECTOR3(v[0], v[1], v[2]);
}

namespace {

  template<class T>
  struct UniqueNumber {
    T number;
    UniqueNumber() : number(0) {}
    T operator()() { return number++; }
  };

} // anonymous namespace

RandomLayout::RandomLayout(UINT64VECTOR3 const& vDomainSize)
  : ScanlineLayout(vDomainSize)
  , m_vLookUp((size_t)vDomainSize.volume())
{
  std::generate(m_vLookUp.begin(), m_vLookUp.end(), UniqueNumber<uint64_t>());
  std::random_shuffle(m_vLookUp.begin(), m_vLookUp.end());
}

uint64_t RandomLayout::GetLinearIndex(UINT64VECTOR3 const& vSpatialPosition)
{
  uint64_t const iIndex = ScanlineLayout::GetLinearIndex(vSpatialPosition);
  assert(iIndex < (uint64_t)m_vLookUp.size());
  return m_vLookUp[(size_t)iIndex];
}

UINT64VECTOR3 RandomLayout::GetSpatialPosition(uint64_t iLinearIndex)
{
  assert(iLinearIndex < (uint64_t)m_vLookUp.size());
  uint64_t const iIndex = m_vLookUp[(size_t)iLinearIndex];
  return ScanlineLayout::GetSpatialPosition(iIndex);
}

UINTVECTOR2 VolumeTools::Fit1DIndexTo2DArray(uint64_t iMax1DIndex,
                                             uint32_t iMax2DArraySize) {
  // check if 1D index exceeds given 2D array
  if (iMax1DIndex > uint64_t(iMax2DArraySize) * uint64_t(iMax2DArraySize)) {
    std::stringstream ss;
    ss << "element count of " << iMax1DIndex << " exceeds the addressable indices "
       << "of a " << iMax2DArraySize << "x" << iMax2DArraySize << " array";
    throw std::runtime_error(ss.str().c_str());
  }

  // 1D index fits into a row
  if (iMax1DIndex <= uint64_t(iMax2DArraySize))
    return UINTVECTOR2(uint32_t(iMax1DIndex), 1);

  // fit 1D index into the smallest possible square
  UINTVECTOR2 v2DArraySize;
  v2DArraySize.x = uint32_t(std::ceil(std::sqrt(double(iMax1DIndex))));
  v2DArraySize.y = uint32_t(std::ceil(double(iMax1DIndex)/double(v2DArraySize.x)));

  return v2DArraySize;
}

/*
 RemoveBoundary:
 
 This function takes a brick in 3D format and removes iRemove
 voxels in each dimension. The function changes the given
 brick in-place.
 */
void VolumeTools::RemoveBoundary(uint8_t *pBrickData, 
                                 const UINT64VECTOR3& vBrickSize, 
                                 size_t iVoxelSize, uint32_t iRemove) {
  const UINT64VECTOR3 vTargetBrickSize = vBrickSize-iRemove*2;
  
  for (uint32_t z = 0;z<vBrickSize.z-2*iRemove;++z) {
    for (uint32_t y = 0;y<vBrickSize.y-2*iRemove;++y) {
      // below is just the usual 3D to 1D conversion where
      // we skip iRemove elements in each input dimension
      // for the output we simply use the smaller size
      size_t inOffset = size_t(iVoxelSize * ( iRemove +
                                     (y+iRemove) * vBrickSize.x +
                                     (z+iRemove) * vBrickSize.x*vBrickSize.y));
      size_t outOffset = size_t(iVoxelSize * (y*vTargetBrickSize.x +
                                       z*vTargetBrickSize.x*vTargetBrickSize.y));
      memcpy(pBrickData+outOffset, pBrickData+inOffset, 
             size_t(iVoxelSize*vTargetBrickSize.x));
    }
  }
}

void VolumeTools::Atalasify(size_t iSizeInBytes,
                            const UINTVECTOR3& vMaxBrickSize,
                            const UINT64VECTOR3& vCurrBrickSize,
                            const UINTVECTOR2& atlasSize,
                            uint8_t* pDataSource,
                            uint8_t* pDataTarget) {
  
  // can't do in-place conversion 
  if (pDataSource == pDataTarget) {
    uint8_t* temp = new uint8_t[iSizeInBytes];
    memcpy(temp, pDataTarget, iSizeInBytes);
    Atalasify(iSizeInBytes, vMaxBrickSize, 
               vCurrBrickSize, atlasSize, temp, pDataTarget);
    delete [] temp;
    return;
  }
  
  // do the actual atlasify
  const size_t iSizePerElement = size_t(iSizeInBytes/vCurrBrickSize.volume());
  const unsigned int iTilesPerRow = (unsigned int)(atlasSize.x / vMaxBrickSize.x);
  uint8_t* pDataSourceIter = pDataSource;
  for (unsigned int z = 0;z<vCurrBrickSize.z;++z) {
    const unsigned int iTileX = z % iTilesPerRow;
    const unsigned int iTileY = z / iTilesPerRow;
    for (unsigned int y = 0;y<vCurrBrickSize.y;++y) {
      memcpy(pDataTarget+(iSizePerElement*(iTileX*vMaxBrickSize.x+(iTileY*vMaxBrickSize.y+y)*atlasSize.x)), 
             pDataSourceIter, 
             size_t(vCurrBrickSize.x*iSizePerElement));
      pDataSourceIter += vCurrBrickSize.x*iSizePerElement;
    }
  }
  
}



void VolumeTools::DeAtalasify(size_t iSizeInBytes,
                              const UINTVECTOR2& vCurrentAtlasSize,
                              const UINTVECTOR3& vMaxBrickSize,
                              const UINT64VECTOR3& vCurrBrickSize,
                              uint8_t* pDataSource,
                              uint8_t* pDataTarget) {
  
  // can't do in-place conversion 
  if (pDataSource == pDataTarget) {
    uint8_t* temp = new uint8_t[iSizeInBytes];
    memcpy(temp, pDataTarget, iSizeInBytes);
    DeAtalasify(iSizeInBytes, vCurrentAtlasSize, vMaxBrickSize, 
                 vCurrBrickSize, temp, pDataTarget);
    delete [] temp;
    return;
  }
  
  // do the actual de-atlasify
  const size_t iSizePerElement = size_t(iSizeInBytes/vCurrBrickSize.volume());
  const unsigned int iTilesPerRow = (unsigned int)(vCurrentAtlasSize.x /
                                                   vMaxBrickSize.x);
  uint8_t* pDataTargetIter = pDataTarget;
  for (unsigned int z = 0;z<vCurrBrickSize.z;++z) {
    const unsigned int iTileX = z % iTilesPerRow;
    const unsigned int iTileY = z / iTilesPerRow;
    for (unsigned int y = 0;y<vCurrBrickSize.y;++y) {
      memcpy(pDataTargetIter,
             pDataSource+(iSizePerElement*(iTileX*vMaxBrickSize.x+(iTileY*vMaxBrickSize.y+y)*vCurrentAtlasSize.x)),               
             size_t(vCurrBrickSize.x*iSizePerElement));
      pDataTargetIter += vCurrBrickSize.x*iSizePerElement;
    }
  }
}


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

