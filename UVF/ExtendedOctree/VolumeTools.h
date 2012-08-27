#pragma once

#ifndef VOLUMETOOLS_H
#define VOLUMETOOLS_H

#include "Basics/StdDefines.h"

// for the small fixed size vectors
#include "Basics/Vectors.h"

#include <algorithm>

namespace VolumeTools {

  /**
    Converts a brick into atlantified representation
    @param metaData the brick meta-data for the input brick
    @param atlasSize the size of the 2D texture atlas
    @param pData pointer to mem to hold non-atlantified data
    @param pData pointer to mem to hold atlantified data
    */
  void Atalasify(size_t iSizeInBytes,
                         const UINT64VECTOR3& vMaxBrickSize,
                         const UINT64VECTOR3& vCurrBrickSize,
                         const UINTVECTOR2& atlasSize,
                         uint8_t* pDataSource,
                         uint8_t* pDataTarget);

  /**
    Converts a brick into simple 3D representation
    @param metaData the brick meta-data for the input brick
    @param pData pointer to mem to hold atlantified data
    @param pData pointer to mem to hold non-atlantified data
    */
  void DeAtalasify(size_t iSizeInBytes,
                           const UINTVECTOR2& vCurrentAtlasSize,
                           const UINT64VECTOR3& vMaxBrickSize,
                           const UINT64VECTOR3& vCurrBrickSize,
                           uint8_t* pDataSource,
                           uint8_t* pDataTarget);

  /**
    This function takes a brick in 3D format and removes iRemove
    voxels in each dimension. The function changes the given
    brick in-place.

    @param pBrickData the voxels of the brick, changes are made to
                      this array in place
    @param vBrickSize the 3D size of the brick
    @param iVoxelSize the size (in bytes) of a voxel in the tree
    @param iRemove the number of voxels to be removed
  */
  void RemoveBoundary(uint8_t *pBrickData, 
                            const UINT64VECTOR3& vBrickSize,
                            size_t iVoxelSize, 
                            uint32_t iRemove);


  /**
   Computes the mean value ( (a+b)/2) of a and b or the median (just picking a) 
   the mean computations is carried out in F (usually double) precision to
   avoid clamping and-or quantization. This function is used when neighbors in
   two dimensions are missing
   
   @param a value to be filterd
   @param b value to be filterd
   @return the Filter/mean of values a and b
   */
   template<typename T, typename F, bool bComputeMedian> T Filter(T a, T b) {
    if (bComputeMedian) 
      return a;
    else
      return T((F(a) + F(b)) / F(2));
  }
  
  /**
   Computes the mean or the median value of inputs a to d for the mean computation
   intermediate computations are carried out in F (usually double) precision to
   avoid clamping and-or quantization. This function is used when neighbors in
   one dimension are missing
   
   @param a value to be filterd
   @param b value to be filterd
   @param c value to be filterd
   @param d value to be filterd
   @return the Filter/mean of values a to d
  */
   template<typename T, typename F, bool bComputeMedian> T Filter(T a, T b, T c, T d) {
    if (bComputeMedian) {
      if (a > b) std::swap(a,b);
      if (c > d) std::swap(c,d);
      if (a > c) {
        std::swap(a,c);
        std::swap(b,d);
      }

      return c;
      /*
      // to return always the 2nd out of 4 include the follwoing code
      // but as we don't care íf we get the 2nd or 3rd value we skip it

      if (b > d) 
        return c;
      else 
        return min(b,c);
      */

    } else
      return T((F(a) + F(b) + F(c) + F(d)) / F(4));
  }
  
  /**
   Computes the mean or median value of inputs a to h for the mean computation
   intermediate computations are carried out in F (usually double) precision to
   avoid clamping and-or quantization. This function is used for the majority
   of values when downsampling the bricks, only when no neighbors are present
   in one or multiple directions are the other Filter functions (with 4
   and 2 parameters) called
   
   @param a value to be filterd
   @param b value to be filterd
   @param c value to be filterd
   @param d value to be filterd
   @param e value to be filterd
   @param f value to be filterd
   @param g value to be filterd
   @param h value to be filterd
   @return the Filter/mean of values a to h
   */
   template<typename T, typename F, bool bComputeMedian> T Filter(T a, T b, T c, T d, 
                                                                  T e, T f, T g, T h) {
    if (bComputeMedian) {
      T elems[8] = {a,b,c,d,e,f,g,h};
      std::nth_element (elems, elems+3, elems+8);
      return elems[3];
   } else
      return T((F(a) + F(b) + F(c) + F(d) +
                F(e) + F(f) + F(g) + F(h)) / F(8));
  }

  template<typename T> void ComputeGradientVolumeFloat(T* pSourceData, T* pTargetData, const UINT64VECTOR3& vVolumeSize) {
    for (size_t z = 0;z<size_t(vVolumeSize[2]);z++) {
      for (size_t y = 0;y<size_t(vVolumeSize[1]);y++) {
        for (size_t x = 0;x<size_t(vVolumeSize[0]);x++) {

          // compute 3D positions
          size_t iCenter = x+size_t(vVolumeSize[0])*y+size_t(vVolumeSize[0])*size_t(vVolumeSize[1])*z;
          size_t iLeft   = iCenter;
          size_t iRight  = iCenter;
          size_t iTop    = iCenter;
          size_t iBottom = iCenter;
          size_t iFront  = iCenter;
          size_t iBack   = iCenter;

          VECTOR3<T> vScale(0,0,0);

          // handle borders
          if (x > 0)          {iLeft   = iCenter-1; vScale.x++;}
          if (x < vVolumeSize[0]-1) {iRight  = iCenter+1; vScale.x++;}
          if (y > 0)          {iTop    = iCenter-size_t(vVolumeSize[0]);vScale.y++;}
          if (y < vVolumeSize[1]-1) {iBottom = iCenter+size_t(vVolumeSize[0]);vScale.y++;}
          if (z > 0)          {iFront  = iCenter-size_t(vVolumeSize[0])*size_t(vVolumeSize[1]);vScale.z++;}
          if (z < vVolumeSize[2]-1) {iBack   = iCenter+size_t(vVolumeSize[0])*size_t(vVolumeSize[1]);vScale.z++;}

          // compte central differences
          VECTOR3<T> vGradient((pSourceData[iLeft] -pSourceData[iRight] )/vScale.x,
                               (pSourceData[iTop]  -pSourceData[iBottom])/vScale.y,
                               (pSourceData[iFront]-pSourceData[iBack]  )/vScale.z);
          // safe normalize
          vGradient.normalize(0);

          // store in expanded format
          pTargetData[0+iCenter*4] = vGradient.x;
          pTargetData[1+iCenter*4] = vGradient.y;
          pTargetData[2+iCenter*4] = vGradient.z;
          pTargetData[3+iCenter*4] = pSourceData[iCenter];
        }
      }
    }
  }

  template<typename T> void ComputeGradientVolumeUInt(T* pSourceData, T* pTargetData, const UINT64VECTOR3& vVolumeSize) {
    for (size_t z = 0;z<size_t(vVolumeSize[2]);z++) {
      for (size_t y = 0;y<size_t(vVolumeSize[1]);y++) {
        for (size_t x = 0;x<size_t(vVolumeSize[0]);x++) {

          // compute 3D positions
          size_t iCenter = x+size_t(vVolumeSize[0])*y+size_t(vVolumeSize[0])*size_t(vVolumeSize[1])*z;
          size_t iLeft   = iCenter;
          size_t iRight  = iCenter;
          size_t iTop    = iCenter;
          size_t iBottom = iCenter;
          size_t iFront  = iCenter;
          size_t iBack   = iCenter;

          DOUBLEVECTOR3 vScale(0,0,0);

          // handle borders
          if (x > 0)          {iLeft   = iCenter-1; vScale.x++;}
          if (x < vVolumeSize[0]-1) {iRight  = iCenter+1; vScale.x++;}
          if (y > 0)          {iTop    = iCenter-size_t(vVolumeSize[0]);vScale.y++;}
          if (y < vVolumeSize[1]-1) {iBottom = iCenter+size_t(vVolumeSize[0]);vScale.y++;}
          if (z > 0)          {iFront  = iCenter-size_t(vVolumeSize[0])*size_t(vVolumeSize[1]);vScale.z++;}
          if (z < vVolumeSize[2]-1) {iBack   = iCenter+size_t(vVolumeSize[0])*size_t(vVolumeSize[1]);vScale.z++;}

          // compte central differences in double
          DOUBLEVECTOR3 vGradient((double(pSourceData[iLeft]) -double(pSourceData[iRight]) )/(vScale.x),
                                  (double(pSourceData[iTop])  -double(pSourceData[iBottom]))/(vScale.y),
                                  (double(pSourceData[iFront])-double(pSourceData[iBack])  )/(vScale.z));
          // safe normalize
          vGradient.normalize(0);

          // store in expanded unsgined int format
          T halfMax = std::numeric_limits<T>::max;
          pTargetData[0+iCenter*4] = T(vGradient.x*halfMax+halfMax);
          pTargetData[1+iCenter*4] = T(vGradient.y*halfMax+halfMax);
          pTargetData[2+iCenter*4] = T(vGradient.z*halfMax+halfMax);
          pTargetData[3+iCenter*4] = pSourceData[iCenter];
        }
      }
    }
  }

  template<typename T> void ComputeGradientVolumeInt(T* pSourceData, T* pTargetData, const UINT64VECTOR3& vVolumeSize) {
    for (size_t z = 0;z<size_t(vVolumeSize[2]);z++) {
      for (size_t y = 0;y<size_t(vVolumeSize[1]);y++) {
        for (size_t x = 0;x<size_t(vVolumeSize[0]);x++) {

          // compute 3D positions
          size_t iCenter = x+size_t(vVolumeSize[0])*y+size_t(vVolumeSize[0])*size_t(vVolumeSize[1])*z;
          size_t iLeft   = iCenter;
          size_t iRight  = iCenter;
          size_t iTop    = iCenter;
          size_t iBottom = iCenter;
          size_t iFront  = iCenter;
          size_t iBack   = iCenter;

          DOUBLEVECTOR3 vScale(0,0,0);

          // handle borders
          if (x > 0)          {iLeft   = iCenter-1; vScale.x++;}
          if (x < vVolumeSize[0]-1) {iRight  = iCenter+1; vScale.x++;}
          if (y > 0)          {iTop    = iCenter-size_t(vVolumeSize[0]);vScale.y++;}
          if (y < vVolumeSize[1]-1) {iBottom = iCenter+size_t(vVolumeSize[0]);vScale.y++;}
          if (z > 0)          {iFront  = iCenter-size_t(vVolumeSize[0])*size_t(vVolumeSize[1]);vScale.z++;}
          if (z < vVolumeSize[2]-1) {iBack   = iCenter+size_t(vVolumeSize[0])*size_t(vVolumeSize[1]);vScale.z++;}

          // compte central differences in double
          DOUBLEVECTOR3 vGradient((double(pSourceData[iLeft]) -double(pSourceData[iRight]) )/(vScale.x),
                                  (double(pSourceData[iTop])  -double(pSourceData[iBottom]))/(vScale.y),
                                  (double(pSourceData[iFront])-double(pSourceData[iBack])  )/(vScale.z));
          // safe normalize
          vGradient.normalize(0);

          // store in expanded signed int format
          T fullMax = std::numeric_limits<T>::max;
          pTargetData[0+iCenter*4] = T(vGradient.x*fullMax);
          pTargetData[1+iCenter*4] = T(vGradient.y*fullMax);
          pTargetData[2+iCenter*4] = T(vGradient.z*fullMax);
          pTargetData[3+iCenter*4] = pSourceData[iCenter];
        }
      }
    }
  }
};

#endif // VOLUMETOOLS_H

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
