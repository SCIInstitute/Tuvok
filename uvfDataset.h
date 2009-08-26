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
  \file    uvfDataset.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once
#ifndef TUVOK_UVF_DATASET_H
#define TUVOK_UVF_DATASET_H

#include <vector>
#include "BrickedDataset.h"
#include "UVF/RasterDataBlock.h"
#include "UVF/MaxMinDataBlock.h"

/// For UVF, a brick key has to be a list for the LOD indicators and a
/// list of brick indices for the brick itself.
typedef std::pair<std::vector<UINT64>,std::vector<UINT64> > NDBrickKey;


class VolumeDatasetInfo;
class Histogram1DDataBlock;
class Histogram2DDataBlock;
class MaxMinDataBlock;
class UVF;

namespace tuvok {

class UVFDataset : public BrickedDataset {
public:
  UVFDataset(const std::string& strFilename, bool bVerify);
  UVFDataset();
  virtual ~UVFDataset();

  // Brick Data
  virtual UINTVECTOR3 GetBrickVoxelCounts(const BrickKey&) const;
  virtual bool GetBrick(const BrickKey& k, std::vector<unsigned char>& vData) const;
  virtual bool BrickIsFirstInDimension(size_t, const BrickKey&) const;
  virtual bool BrickIsLastInDimension(size_t, const BrickKey&) const;
  virtual UINT64VECTOR3 GetEffectiveBrickSize(const BrickKey &) const;

  /// Acceleration queries.
  virtual bool ContainsData(const BrickKey &k, double isoval) const;
  virtual bool ContainsData(const BrickKey &k, double fMin,double fMax) const;
  virtual bool ContainsData(const BrickKey &k, double fMin,double fMax, double fMinGradient,double fMaxGradient) const;

  // LOD Data
  virtual BrickTable::size_type GetBrickCount(size_t lod) const;
  virtual UINT64VECTOR3 GetDomainSize(const UINT64 lod=0) const;

  // Global Data
  bool IsOpen() const { return m_bIsOpen; }
  std::string Filename() const { return m_strFilename; }
  float MaxGradientMagnitude() const { return m_fMaxGradMagnitude; }
  virtual UINTVECTOR3 GetMaxBrickSize() const;
  virtual UINTVECTOR3 GetBrickOverlapSize() const;
  virtual UINT64 GetLODLevelCount() const;
  virtual UINT64 GetBitWidth() const;
  virtual UINT64 GetComponentCount() const;
  virtual bool GetIsSigned() const;
  virtual bool GetIsFloat() const;
  virtual bool IsSameEndianness() const;
  /// Unsupported for UVF!  Wouldn't be hard to implement, though, just not
  /// currently needed.
  virtual std::pair<double,double> GetRange() const {
    assert(1 == 0);
    return std::make_pair(0.0, 0.0);
  }

  // Global "Operations" and additional data not from the UVF file
  virtual bool Export(UINT64 iLODLevel, const std::string& targetFilename,
                      bool bAppend,
                      bool (*brickFunc)(LargeRAWFile* pSourceFile,
                                        const std::vector<UINT64> vBrickSize,
                                        const std::vector<UINT64> vBrickOffset,
                                        void* pUserContext) = NULL,
                      void *pUserContext = NULL,
                      UINT64 iOverlap=0) const;


private:
  std::vector<UINT64> UVFDataset::IndexToVector(const BrickKey &k) const;
  NDBrickKey IndexToVectorKey(const BrickKey &k) const;
  bool Open(bool bVerify);
  UINT64 FindSuitableRasterBlock();
  void ComputeMetaData();
  void GetHistograms();

  void FixOverlap(UINT64& v, UINT64 brickIndex, UINT64 maxindex, UINT64 overlap) const;


private:
  float                 m_fMaxGradMagnitude;
  RasterDataBlock*      m_pVolumeDataBlock;
  Histogram1DDataBlock* m_pHist1DDataBlock;
  Histogram2DDataBlock* m_pHist2DDataBlock;
  MaxMinDataBlock*      m_pMaxMinData;
  UVF*                  m_pDatasetFile;
  bool                  m_bIsOpen;
  std::string           m_strFilename;

  UINTVECTOR3                m_aOverlap;
  bool                       m_bIsSameEndianness;
  std::vector<UINT64VECTOR3> m_aDomainSize;
  UINTVECTOR3                m_aMaxBrickSize;
  std::vector<UINT64VECTOR3> m_vaBrickCount;
  std::vector<std::vector<std::vector<std::vector<UINT64VECTOR3> > > >  m_vvaBrickSize;
  std::vector<std::vector<std::vector<std::vector<InternalMaxMinElement> > > > m_vvaMaxMin;

};

};

#endif // TUVOK_UVF_DATASET_H
