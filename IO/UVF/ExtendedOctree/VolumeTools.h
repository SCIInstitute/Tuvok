#pragma once

#ifndef VOLUMETOOLS_H
#define VOLUMETOOLS_H

#include "Basics/StdDefines.h"

// for the small fixed size vectors
#include "Basics/Vectors.h"

#include <algorithm>

namespace VolumeTools {

  class Layout {
  public:
    /**
      Abstract layout that maps a 3D domain to a linear 1D index
      @param vDomainSize spatial 3D domain size where a linear 1D index
      @                  should be defined in
      */
    Layout(UINT64VECTOR3 const& vDomainSize);

    /**
      Convert spatial 3D brick position to linear index (position)
      @param vSpatialPosition spatial 3D position
      @return linear index
      @throws std::runtime_error if vSpatialPosition exceeds domain boundaries
      */
    virtual uint64_t GetLinearIndex(UINT64VECTOR3 const& vSpatialPosition) = 0;

    /**
      Convert linear index (position) to spatial 3D brick position
      @param iLinearIndex linear index
      @return spatial 3D position
      */
    virtual UINT64VECTOR3 GetSpatialPosition(uint64_t iLinearIndex) = 0;

  protected:
    /**
      Test if spatial 3D brick position is not part of the domain
      @param vSpatialPosition spatial 3D position
      @return true if given spatial position is not part of the domain
      */
    inline bool ExceedsDomain(UINT64VECTOR3 const& vSpatialPosition);

    UINT64VECTOR3 m_vDomainSize;
  };

  class ScanlineLayout : public Layout {
  public:
    ScanlineLayout(UINT64VECTOR3 const& vDomainSize);
    uint64_t GetLinearIndex(UINT64VECTOR3 const& vSpatialPosition);
    UINT64VECTOR3 GetSpatialPosition(uint64_t iLinearIndex);
  };

  // NOTICE: The current implementation works for cubic power of two domains.
  //         Using a non cubic domain with non power of two axes will generate
  //         some undefined indices. The host code needs to take care of this!
  //
  // TODO:   Morton numbers can be applied to non-square domains by simply not
  //         interleaving bits from an axis when they have been exhausted.
  // SEE:    http://blog.gmane.org/gmane.games.devel.algorithms/month=20080801/page=10
  //         http://comments.gmane.org/gmane.games.devel.algorithms/20013
  class MortonLayout : public Layout {
  public:
    MortonLayout(UINT64VECTOR3 const& vDomainSize);
    uint64_t GetLinearIndex(UINT64VECTOR3 const& vSpatialPosition);
    UINT64VECTOR3 GetSpatialPosition(uint64_t iLinearIndex);
  };

  // NOTICE: The current implementation works for cubic power of two domains.
  //         Using a non cubic domain with non power of two axes will generate
  //         some undefined indices. The host code needs to take care of this!
  //
  // TODO:   C.H. Hamilton, A. Rau-Chaplin "Compact Hilbert Indices:
  //         Space-filling curves for domains with unequal side lengths."
  //         Information Processing Letters, 105(5), 155--163, February 2008.
  // SEE:    http://web.cs.dal.ca/~chamilto/hilbert/ipl.pdf
  //         http://web.cs.dal.ca/~chamilto/hilbert/index.html
  class HilbertLayout : public Layout {
  public:
    HilbertLayout(UINT64VECTOR3 const& vDomainSize);
    uint64_t GetLinearIndex(UINT64VECTOR3 const& vSpatialPosition);
    UINT64VECTOR3 GetSpatialPosition(uint64_t iLinearIndex);
  private:
    size_t m_iBits;
  };

  class RandomLayout : public ScanlineLayout {
  public:
    RandomLayout(UINT64VECTOR3 const& vDomainSize);
    uint64_t GetLinearIndex(UINT64VECTOR3 const& vSpatialPosition);
    UINT64VECTOR3 GetSpatialPosition(uint64_t iLinearIndex);
  private:
    std::vector<uint64_t> m_vLookUp;
  };

  /**
    Compute the necessary 2D array size to fit a linear index minimizing wasted indices
    @param iMax1DIndex number of elements we want to fit into a 2d array
    @param iMax2DArraySize maximal dimension of a square 2d array
    @return optimal 2d array size to host at least the given amount of numbers
    @throws std::runtime_error if 1d index does not fit given array
    */
  UINTVECTOR2 Fit1DIndexTo2DArray(uint64_t iMax1DIndex,
                                  uint32_t iMax2DArraySize);

  /**
    Converts a brick into atlantified representation
    @param iSizeInBytes total size of the current brick
    @param vMaxBrickSize maximum size of a brick
    @param vCurrBrickSize actual size of current brick
    @param atlasSize the size of the 2D texture atlas
    @param pDataSource pointer to mem to hold non-atlantified data
    @param pDataTarget pointer to mem to hold atlantified data
    */
  void Atalasify(size_t iSizeInBytes,
                 const UINTVECTOR3& vMaxBrickSize,
                 const UINT64VECTOR3& vCurrBrickSize,
                 const UINTVECTOR2& atlasSize,
                 uint8_t* pDataSource,
                 uint8_t* pDataTarget);

  /**
    Converts a brick into simple 3D representation
    @param iSizeInBytes total size of the current brick
    @param vCurrentAtlasSize the size of the 2D texture atlas
    @param vMaxBrickSize maximum size of a brick
    @param vCurrBrickSize actual size of current brick
    @param pDataSource pointer to mem to hold atlantified data
    @param pDataTarget pointer to mem to hold non-atlantified data
    */
  void DeAtalasify(size_t iSizeInBytes,
                   const UINTVECTOR2& vCurrentAtlasSize,
                   const UINTVECTOR3& vMaxBrickSize,
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
   
   @param a value to be filtered
   @param b value to be filtered
   @return the Filter/mean of values a and b
   */
  template<typename T, typename F, bool bComputeMedian> T Filter(T a, T b) {
    if (bComputeMedian) 
      return a;
    else
      return T((F(a) + F(b)) / F(2));
  }

  template<typename T> void Order(T& a, T& b) {
    if (a > b) std::swap(a, b);
  }
  
  /**
   Computes the mean or the median value of inputs a to d for the mean computation
   intermediate computations are carried out in F (usually double) precision to
   avoid clamping and-or quantization. This function is used when neighbors in
   one dimension are missing
   
   @param a value to be filtered
   @param b value to be filtered
   @param c value to be filtered
   @param d value to be filtered
   @return the Filter/mean of values a to d
  */
  template<typename T, typename F, bool bComputeMedian> T Filter(T a, T b, T c, T d) {
    if (bComputeMedian) {
      // here we compute the median of a,b,c (ignoring d) which means
      // that we will either get the second or third smallest value
      // in the original a,b,c,d sequence
      Order(a,b);
      Order(b,c);
      return std::max(a,b);
    } else
      return T((F(a) + F(b) + F(c) + F(d)) / F(4));
  }

  template<typename T> void InsertIntoQuadruple(T& a, T& b, T& c, T& d, T& p) {
    if (p > c) {
      Order(d, p);
    } else {
      if (p < b) {
        d = c; 
        c = b;
        b = p;
        Order(a, b);				
      }	else {
        d = c;
        c = p;
      }
    }
  }
  
  /**
   Computes the mean or median value of inputs a to h for the mean computation
   intermediate computations are carried out in F (usually double) precision to
   avoid clamping and-or quantization. This function is used for the majority
   of values when downsampling the bricks, only when no neighbors are present
   in one or multiple directions are the other Filter functions (with 4
   and 2 parameters) called
   
   @param a value to be filtered
   @param b value to be filtered
   @param c value to be filtered
   @param d value to be filtered
   @param e value to be filtered
   @param f value to be filtered
   @param g value to be filtered
   @param h value to be filtered
   @return the Filter/mean of values a to h
   */
  template<typename T, typename F, bool bComputeMedian> T Filter(T a, T b, T c, T d, 
                                                                 T e, T f, T g, T h) {
    if (bComputeMedian) {
      /* the std solution should be equivalent to the optimized version below
      T elems[8] = {a,b,c,d,e,f,g,h};
      std::nth_element (elems, elems+3, elems+8);
      return elems[3]; */

      // this version considers only 7 values, the computed median is thus the lower or the upper median for 8
      // sort first 4 values
      Order(a, b);
      Order(c, d);
      Order(a, c);
      Order(b, d);
      Order(b, c);

      //find 4 minimum values out of 6
      InsertIntoQuadruple(a, b, c, d, e);
      InsertIntoQuadruple(a, b, c, d, f);

      // 7th value is only relevant when it is smaller than d and larger than c
      return std::max(std::min(d, g), c);
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

          // compute central differences in double
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
