#include <cstring>
#include "VolumeTools.h"

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
                             const UINT64VECTOR3& vMaxBrickSize,
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
                               const UINT64VECTOR3& vMaxBrickSize,
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

