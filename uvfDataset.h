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
#include "FileBackedDataset.h"
#include "UVF/RasterDataBlock.h"
#include "UVF/MaxMinDataBlock.h"

/// For UVF, a brick key has to be a list for the LOD indicators and a
/// list of brick indices for the brick itself.
struct NDBrickKey {
  size_t timestep;
  std::vector<UINT64> lod;
  std::vector<UINT64> brick;
};
//typedef std::pair<std::vector<UINT64>,std::vector<UINT64> > NDBrickKey;


class VolumeDatasetInfo;
class KeyValuePairDataBlock;
class Histogram1DDataBlock;
class Histogram2DDataBlock;
class MaxMinDataBlock;
class GeometryDataBlock;
class UVF;

namespace tuvok {

class UVFDataset : public FileBackedDataset {
public:
  UVFDataset(const std::string& strFilename, UINT64 iMaxAcceptableBricksize, bool bVerify, bool bMustBeSameVersion = true);
  UVFDataset();
  virtual ~UVFDataset();

  // Brick Data
  virtual UINTVECTOR3 GetBrickVoxelCounts(const BrickKey&) const;
  virtual bool GetBrick(const BrickKey& k, std::vector<unsigned char>& vData) const;
  virtual UINT64VECTOR3 GetEffectiveBrickSize(const BrickKey &) const;

  /// Acceleration queries.
  virtual bool ContainsData(const BrickKey &k, double isoval) const;
  virtual bool ContainsData(const BrickKey &k, double fMin,double fMax) const;
  virtual bool ContainsData(const BrickKey &k, double fMin,double fMax, double fMinGradient,double fMaxGradient) const;

  // LOD Data
  /// @todo fixme -- this should take a brick key and just ignore the spatial
  /// indices.
  ///@{
  virtual BrickTable::size_type GetBrickCount(size_t lod, size_t ts) const;
  virtual UINT64VECTOR3 GetDomainSize(const size_t lod=0, const size_t ts=0) const;
  ///@}
  virtual UINT64 GetNumberOfTimesteps() const;

  // Global Data
  float MaxGradientMagnitude() const;
  virtual UINTVECTOR3 GetMaxBrickSize() const;
  virtual UINT64VECTOR3 GetMaxUsedBrickSizes() const;
  virtual UINTVECTOR3 GetBrickOverlapSize() const;
  virtual UINT64 GetLODLevelCount() const;
  virtual UINT64 GetBitWidth() const;
  virtual UINT64 GetComponentCount() const;
  virtual bool GetIsSigned() const;
  virtual bool GetIsFloat() const;
  virtual bool IsSameEndianness() const;
  virtual std::pair<double,double> GetRange();

  // Global "Operations" and additional data not from the UVF file
  virtual bool Export(UINT64 iLODLevel, const std::string& targetFilename,
                      bool bAppend,
                      bool (*brickFunc)(LargeRAWFile* pSourceFile,
                                        const std::vector<UINT64> vBrickSize,
                                        const std::vector<UINT64> vBrickOffset,
                                        void* pUserContext) = NULL,
                      void *pUserContext = NULL,
                      UINT64 iOverlap=0) const;

  virtual const std::vector< std::pair < std::string, std::string > > GetMetadata() const;

  virtual bool SaveRescaleFactors();
  bool AppendMesh(Mesh* m);
  bool RemoveMesh(size_t iMeshIndex);

  virtual bool CanRead(const std::string&, const std::vector<int8_t>&) const;
  virtual bool Verify(const std::string&) const;
  virtual FileBackedDataset* Create(const std::string&, UINT64, bool) const;
  virtual std::list<std::string> Extensions() const;
  const UVF* GetUVFFile() const {return m_pDatasetFile;}

  virtual const char* Name() const { 
    if (m_timesteps.size()) 
      return m_timesteps[0].m_pVolumeDataBlock->strBlockID.c_str(); 
    else
      return "Generic UVF Dataset"; 
  }

private:
  std::vector<UINT64> IndexToVector(const BrickKey &k) const;
  NDBrickKey IndexToVectorKey(const BrickKey &k) const;
  bool Open(bool bVerify, bool bReadWrite, bool bMustBeSameVersion=true);
  void Close();
  void FindSuitableRasterBlocks();
  void ComputeMetaData(size_t ts);
  void GetHistograms(size_t ts);

  void FixOverlap(UINT64& v, UINT64 brickIndex, UINT64 maxindex, UINT64 overlap) const;

  size_t DetermineNumberOfTimesteps();
  bool VerifyRasterDataBlock(const RasterDataBlock*) const;

private:
  struct Timestep {
    /// used for 2D TF scaling
    float                        m_fMaxGradMagnitude;
    const RasterDataBlock*       m_pVolumeDataBlock; ///< data
    const Histogram1DDataBlock*  m_pHist1DDataBlock;
    const Histogram2DDataBlock*  m_pHist2DDataBlock;
    MaxMinDataBlock*             m_pMaxMinData;      ///< acceleration info

    /// number of voxels of overlap with neighboring bricks
    UINTVECTOR3                  m_aOverlap;
    /// size of the domain for this timestep (i.e. n_voxels in finest LOD)
    std::vector<UINT64VECTOR3>   m_aDomainSize;
    /// max values for logical brick indices; std::vector index gives LOD.
    std::vector<UINT64VECTOR3>   m_vaBrickCount;
    /// the size of each individual brick.  Slowest moving dimension is LOD;
    /// then x,y,z.
    std::vector<std::vector<std::vector<std::vector<UINT64VECTOR3> > > >  m_vvaBrickSize;
    /// same layout as m_vvaBrickSize, but gives acceleration min/max info.
    std::vector<std::vector<std::vector<std::vector<InternalMaxMinElement> > > > m_vvaMaxMin;
    size_t                       block_number;
  };
  std::vector<Timestep>        m_timesteps;
  std::vector<GeometryDataBlock*> m_TriSoupBlocks;
  const KeyValuePairDataBlock* m_pKVDataBlock;
  UINTVECTOR3                  m_aMaxBrickSize;
  bool                         m_bIsSameEndianness;

  UVF*                         m_pDatasetFile;
  std::pair<double,double>     m_CachedRange;

  UINT64                       m_iMaxAcceptableBricksize;
};

}

#endif // TUVOK_UVF_DATASET_H
