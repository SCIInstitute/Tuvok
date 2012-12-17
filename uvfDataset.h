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
#include "Controller/Controller.h"
#include "AbstrConverter.h"

/// For UVF, a brick key has to be a list for the LOD indicators and a
/// list of brick indices for the brick itself.
struct NDBrickKey {
  size_t timestep;
  std::vector<uint64_t> lod;
  std::vector<uint64_t> brick;
};

class VolumeDatasetInfo;
class KeyValuePairDataBlock;
class Histogram1DDataBlock;
class Histogram2DDataBlock;
class MaxMinDataBlock;
class GeometryDataBlock;
class UVF;

namespace tuvok {

  class Timestep  {
  public:
  	Timestep() : 
      m_fMaxGradMagnitude(0), 
      m_pVolumeDataBlock(NULL),
      m_pHist1DDataBlock(NULL), 
      m_pHist2DDataBlock(NULL),
      m_pMaxMinData(NULL)
    {}  
    float                        m_fMaxGradMagnitude;
    const DataBlock*             m_pVolumeDataBlock; ///< data
    const Histogram1DDataBlock*  m_pHist1DDataBlock;
    const Histogram2DDataBlock*  m_pHist2DDataBlock;
    const MaxMinDataBlock*       m_pMaxMinData;      ///< acceleration info
    size_t                       block_number;
  };

  class RDTimestep : public Timestep   {
  public:
    RDTimestep() : Timestep() {}

    const RasterDataBlock* GetDB() const {return dynamic_cast<const RasterDataBlock*>(m_pVolumeDataBlock);}

    /// number of voxels of overlap with neighboring bricks
    UINTVECTOR3                  m_aOverlap;
    /// size of the domain for this timestep (i.e. n_voxels in finest LOD)
    std::vector<UINT64VECTOR3>   m_aDomainSize;
    /// max values for logical brick indices; std::vector index gives LOD.
    std::vector<UINT64VECTOR3>   m_vaBrickCount;
    /// the size of each individual brick.  Slowest moving dimension is LOD;
    /// then x,y,z.
    std::vector<std::vector<std::vector<std::vector<UINT64VECTOR3>>>>  m_vvaBrickSize;
    /// same layout as m_vvaBrickSize, but gives acceleration min/max info.
    std::vector<std::vector<std::vector<InternalMaxMinVoxel>>> m_vvaMaxMin;
  };

  class TOCTimestep : public Timestep   {
  public:
    TOCTimestep() : Timestep() {}
    const TOCBlock* GetDB() const {return dynamic_cast<const TOCBlock*>(m_pVolumeDataBlock);}
  };

class UVFDataset : public FileBackedDataset {
public:
  UVFDataset(const std::string& strFilename, uint64_t iMaxAcceptableBricksize, bool bVerify, bool bMustBeSameVersion = true);
  UVFDataset();
  virtual ~UVFDataset();

  // Brick Data
  virtual UINTVECTOR3 GetBrickVoxelCounts(const BrickKey&) const;
  virtual UINT64VECTOR3 GetEffectiveBrickSize(const BrickKey &) const;

  virtual bool GetBrick(const BrickKey&, std::vector<uint8_t>&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<int8_t>&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<uint16_t>&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<int16_t>&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<uint32_t>&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<int32_t>&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<float>&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<double>&) const;

  /// Acceleration queries.
  virtual bool ContainsData(const BrickKey &k, double isoval) const;
  virtual bool ContainsData(const BrickKey &k, double fMin,double fMax) const;
  virtual bool ContainsData(const BrickKey &k, double fMin,double fMax, double fMinGradient,double fMaxGradient) const;

  // Used by GLVolumePool to speed up visibility computations
  InternalMaxMinComponent MaxMinForKey(const BrickKey &k) const;

  // LOD Data
  /// @todo fixme -- this should take a brick key and just ignore the spatial
  /// indices.
  ///@{
  virtual BrickTable::size_type GetBrickCount(size_t lod, size_t ts) const;
  virtual size_t GetLargestSingleBrickLod(size_t ts) const;
  virtual UINT64VECTOR3 GetDomainSize(const size_t lod=0, const size_t ts=0) const;
  ///@}
  virtual uint64_t GetNumberOfTimesteps() const;

  UINT64VECTOR3 GetBrickLayout(size_t lod, size_t ts) const;

  // Global Data
  float MaxGradientMagnitude() const;
  virtual UINTVECTOR3 GetMaxBrickSize() const;
  virtual UINT64VECTOR3 GetMaxUsedBrickSizes() const;
  virtual UINTVECTOR3 GetBrickOverlapSize() const;
  virtual uint64_t GetLODLevelCount() const;
  virtual uint64_t GetBitWidth() const;
  virtual uint64_t GetComponentCount() const;
  virtual bool GetIsSigned() const;
  virtual bool GetIsFloat() const;
  virtual bool IsSameEndianness() const;
  virtual std::pair<double,double> GetRange() const;
  // computes the range and caches it internally for the next call to
  // 'GetRange'.
  void ComputeRange();

  // Global "Operations" and additional data not from the UVF file
  virtual bool Export(uint64_t iLODLevel, const std::string& targetFilename,
    bool bAppend) const;

  virtual bool ApplyFunction(uint64_t iLODLevel, 
                        bool (*brickFunc)(void* pData, 
                                          const UINT64VECTOR3& vBrickSize,
                                          const UINT64VECTOR3& vBrickOffset,
                                          void* pUserContext),
                        void *pUserContext= NULL,
                        uint64_t iOverlap=0) const;

  virtual const std::vector<std::pair<std::string, std::string>> GetMetadata() const;

  virtual bool SaveRescaleFactors();
  virtual bool Crop( const PLANE<float>& plane, const std::string& strTempDir, 
                     bool bKeepOldData, bool bUseMedianFilter, bool bClampToEdge);

  bool AppendMesh(std::shared_ptr<const Mesh> m);
  bool RemoveMesh(size_t iMeshIndex);
  bool GeometryTransformToFile(size_t iMeshIndex, const FLOATMATRIX4& m);

  virtual bool CanRead(const std::string&, const std::vector<int8_t>&) const;
  virtual bool Verify(const std::string&) const;
  virtual FileBackedDataset* Create(const std::string&, uint64_t, bool) const;
  virtual std::list<std::string> Extensions() const;
  const UVF* GetUVFFile() const {return m_pDatasetFile;}

  virtual const char* Name() const { 
    if (m_timesteps.size())
      return m_timesteps[0]->m_pVolumeDataBlock->strBlockID.c_str(); 
    else
      return "Generic UVF Dataset"; 
  }

  NDBrickKey IndexToVectorKey(const BrickKey &k) const;
  UINT64VECTOR4 KeyToTOCVector(const BrickKey &k) const;
  BrickKey TOCVectorToKey(const UINTVECTOR4& hash, size_t timestep) const;

  bool IsTOCBlock() const {return m_bToCBlock;}

  /// this function computes the texture coordinates for a given brick
  /// this may be non trivial with power of two padding, overlap handling
  /// and per brick rescale
  virtual  std::pair<FLOATVECTOR3, FLOATVECTOR3> GetTextCoords(BrickTable::const_iterator brick, bool bUseOnlyPowerOfTwo) const;

private:
  std::vector<uint64_t> IndexToVector(const BrickKey &k) const;
  bool Open(bool bVerify, bool bReadWrite, bool bMustBeSameVersion=true);
  void Close();
  void FindSuitableDataBlocks();
  void ComputeMetaData(size_t ts);
  void ComputeMetadataTOC(size_t ts);
  void ComputeMetadataRDB(size_t ts);
  void GetHistograms(size_t ts);

  void FixOverlap(uint64_t& v, uint64_t brickIndex, uint64_t maxindex, uint64_t overlap) const;

  size_t DetermineNumberOfTimesteps();
  bool VerifyRasterDataBlock(const RasterDataBlock*) const;
  bool VerifyTOCBlock(const TOCBlock* tb) const;

  template <class T> bool GetBrickTemplate(const BrickKey& k,
                            std::vector<T>& vData) const
  {
   if (m_bToCBlock) {
      const UINT64VECTOR4 coords = KeyToTOCVector(k);
      const TOCTimestep* ts = static_cast<TOCTimestep*>(m_timesteps[std::get<0>(k)]);

      size_t targetSize = size_t(ts->GetDB()->GetComponentTypeSize() *
                                 ts->GetDB()->GetComponentCount() *
                                 ts->GetDB()->GetBrickSize(coords).volume())/sizeof(T);
      if (vData.size() < targetSize)
        vData.resize(targetSize);
      uint8_t* pData = (uint8_t*)&vData[0];
      ts->GetDB()->GetData(pData,coords);
      if (ts->GetDB()->GetAtlasSize(coords).area() != 0) {
        VolumeTools::DeAtalasify(targetSize, ts->GetDB()->GetAtlasSize(coords), 
                                 ts->GetDB()->GetMaxBrickSize(),
                                 ts->GetDB()->GetBrickSize(coords), pData,
                                 pData);
      }
      return true;
    } else {
      const NDBrickKey& key = this->IndexToVectorKey(k);
      const RDTimestep* ts = static_cast<RDTimestep*>(m_timesteps[key.timestep]);
      return ts->GetDB()->GetData(vData, key.lod, key.brick);
    }
  }

private:
  bool                         m_bToCBlock;
  std::vector< Timestep* >     m_timesteps;
  std::vector<GeometryDataBlock*> m_TriSoupBlocks;
  const KeyValuePairDataBlock* m_pKVDataBlock;
  UINTVECTOR3                  m_aMaxBrickSize;
  bool                         m_bIsSameEndianness;

  UVF*                         m_pDatasetFile;
  std::pair<double,double>     m_CachedRange;

  uint64_t                     m_iMaxAcceptableBricksize;

  FLOATVECTOR3 GetVolCoord(uint64_t pos, const UINT64VECTOR3& domSize) {
    UINT64VECTOR3 domCoords;

    domCoords.x = pos % domSize.x;
    domCoords.y = (pos / domSize.x) % domSize.y;
    domCoords.z = pos / (domSize.x*domSize.y);

    FLOATVECTOR3 normCoords(float(domCoords.x) / float(domSize.x),
                            float(domCoords.y) / float(domSize.y),
                            float(domCoords.z) / float(domSize.z));
    normCoords -= FLOATVECTOR3(0.5, 0.5, 0.5);
    return normCoords;
  }

  template<typename T> 
  bool CropData(LargeRAWFile& dataFile, const PLANE<float>& plane, const UINT64VECTOR3& domSize, const uint64_t iComponentCount) {

    assert(iComponentCount == size_t(iComponentCount));

    size_t iInCoreElemCount = AbstrConverter::GetIncoreSize()/sizeof(T);

    // make sure iInCoreElemCount is a multiple of iComponentCount to
    // read only entire tuples
    iInCoreElemCount = size_t(iComponentCount)*(iInCoreElemCount/size_t(iComponentCount));

    uint64_t iFileSize = dataFile.GetCurrentSize();
    if (sizeof(T)*iComponentCount*domSize.volume() != iFileSize) {
      return false;
    }

    T* data = new T[iInCoreElemCount];
    size_t iElemsRead;
    uint64_t iFilePos = 0;
    do {
      iElemsRead = dataFile.ReadRAW((unsigned char*)data, iInCoreElemCount*sizeof(T))/sizeof(T);

      // march through the data tuple by tuple
      // TODO: optimize this by computing the start and end of a scan line and perform block operations
      for (size_t elem = 0;elem<iElemsRead;elem+=size_t(iComponentCount)) {
        FLOATVECTOR3 volCoord = GetVolCoord((iFilePos/sizeof(T)+elem)/iComponentCount, domSize);

        if (plane.clip(volCoord)) {
          for (size_t comp = 0;comp<size_t(iComponentCount);++comp)
            data[elem+comp] = T(0);
        } 

      }

      // in-place write data back
      dataFile.SeekPos(iFilePos);
      dataFile.WriteRAW((unsigned char*)data, iElemsRead*sizeof(T));
      iFilePos += iElemsRead*sizeof(T);

      MESSAGE("Cropping voxels (%g%% completed)", 100.0f*float(iFilePos)/float(iFileSize));


    } while (iFilePos < iFileSize && iElemsRead > 0);
    
    delete [] data;
    return true;
  }
};

}

#endif // TUVOK_UVF_DATASET_H
