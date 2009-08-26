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
#include <sstream>

#include "uvfDataset.h"

#include "IOManager.h"
#include "Controller/Controller.h"
#include "UVF/UVF.h"
#include "UVF/Histogram1DDataBlock.h"
#include "UVF/Histogram2DDataBlock.h"

namespace tuvok {

UVFDataset::UVFDataset(const std::string& strFilename, bool bVerify) :
  m_fMaxGradMagnitude(0.0f),
  m_pVolumeDataBlock(NULL),
  m_pHist1DDataBlock(NULL),
  m_pHist2DDataBlock(NULL),
  m_pMaxMinData(NULL),
  m_pDatasetFile(NULL),
  m_bIsOpen(false),
  m_strFilename(strFilename)
{
  Open(bVerify);
}

UVFDataset::UVFDataset() :
  m_fMaxGradMagnitude(0.0f),
  m_pVolumeDataBlock(NULL),
  m_pHist1DDataBlock(NULL),
  m_pHist2DDataBlock(NULL),
  m_pMaxMinData(NULL),
  m_pDatasetFile(NULL),
  m_bIsOpen(false),
  m_strFilename("")
{
}

UVFDataset::~UVFDataset()
{
  delete m_pDatasetFile;
}

bool UVFDataset::Open(bool bVerify)
{
  // open the file
  std::wstring wstrFilename(m_strFilename.begin(),m_strFilename.end());
  m_pDatasetFile = new UVF(wstrFilename);
  std::string strError;
  m_bIsOpen = m_pDatasetFile->Open(true, bVerify,false, &strError);
  if (!m_bIsOpen) {
    T_ERROR(strError.c_str());
    return false;
  } 

  // analyze the main raster data blocks
  UINT64 iRasterBlockIndex = FindSuitableRasterBlock();
  if (iRasterBlockIndex == UINT64(-1)) {
    T_ERROR("No suitable volume block found in UVF file.  Check previous messages for rejected blocks.");
    return false;
  } 
  MESSAGE("Open successfully found a suitable data block in the UVF file analyzing data...");
  m_pVolumeDataBlock = (RasterDataBlock*)m_pDatasetFile->GetDataBlock(iRasterBlockIndex);

  // get the metadata and the histograms
  ComputeMetaData();
  GetHistograms();

  // print out data statistics
  MESSAGE("  Size %u x %u x %u", m_vaBrickCount[0].x, m_vaBrickCount[0].y, m_vaBrickCount[0].z );
  MESSAGE("  %i Bit, %i components", int(GetBitWidth()), int(GetComponentCount()));
  MESSAGE("  LOD down to %u x %u x %u found", m_vaBrickCount[m_vaBrickCount.size()-1].x, m_vaBrickCount[m_vaBrickCount.size()-1].y, m_vaBrickCount[m_vaBrickCount.size()-1].z);


  return true;
}

void UVFDataset::ComputeMetaData() {
  std::vector<double> vfScale;
  size_t iSize = m_pVolumeDataBlock->ulDomainSize.size();

  // we require the data to be at least 3D
  assert(iSize >= 3);

  // we also assume that x,y,z are in the first 3 components and
  // we have no anisotropy (i.e. ulLODLevelCount.size=1)
  size_t iLODLevel = static_cast<size_t>(m_pVolumeDataBlock->ulLODLevelCount[0]);
  for (size_t i = 0;i<3;i++) {
    m_aOverlap[i] = m_pVolumeDataBlock->ulBrickOverlap[i];
    m_aMaxBrickSize[i] = m_pVolumeDataBlock->ulBrickSize[i];
    m_DomainScale[i] = m_pVolumeDataBlock->dDomainTransformation[i+(iSize+1)*i];
  }

  /// @todo FIXME-IO: the brick information that's calculated below and stored
  /// in m_vvaBrickSize needs to end up in 'BrickedDataset', via its
  /// 'AddBrick' call.
  m_vvaBrickSize.resize(iLODLevel);
  if (m_pMaxMinData) m_vvaMaxMin.resize(iLODLevel);


  for (size_t j = 0;j<iLODLevel;j++) {
    std::vector<UINT64> vLOD;  vLOD.push_back(j);
    std::vector<UINT64> vDomSize = m_pVolumeDataBlock->GetLODDomainSize(vLOD);
    m_aDomainSize.push_back(UINT64VECTOR3(vDomSize[0], vDomSize[1],
                                          vDomSize[2]));

    std::vector<UINT64> vBrickCount = m_pVolumeDataBlock->GetBrickCount(vLOD);
    m_vaBrickCount.push_back(UINT64VECTOR3(vBrickCount[0], vBrickCount[1],
                                           vBrickCount[2]));

    m_vvaBrickSize[j].resize(size_t(m_vaBrickCount[j].x));
    if (m_pMaxMinData) {
      m_vvaMaxMin[j].resize(size_t(m_vaBrickCount[j].x));
    }

    FLOATVECTOR3 vBrickCorner;

    FLOATVECTOR3 vNormalizedDomainSize = FLOATVECTOR3(GetDomainSize(j));
    vNormalizedDomainSize /= vNormalizedDomainSize.maxVal();

    BrickMD bmd;
    for (UINT64 x=0; x < m_vaBrickCount[j].x; x++) {
      m_vvaBrickSize[j][size_t(x)].resize(size_t(m_vaBrickCount[j].y));
      if (m_pMaxMinData) {
        m_vvaMaxMin[j][size_t(x)].resize(size_t(m_vaBrickCount[j].y));
      }

      vBrickCorner.y = 0;
      for (UINT64 y=0; y < m_vaBrickCount[j].y; y++) {
        if (m_pMaxMinData) {
          m_vvaMaxMin[j][size_t(x)][size_t(y)].resize(size_t(m_vaBrickCount[j].z));
        }

        vBrickCorner.z = 0;
        for (UINT64 z=0; z < m_vaBrickCount[j].z; z++) {
          std::vector<UINT64> vBrick;
          vBrick.push_back(x);
          vBrick.push_back(y);
          vBrick.push_back(z);
          std::vector<UINT64> vBrickSize =
            m_pVolumeDataBlock->GetBrickSize(vLOD, vBrick);

          m_vvaBrickSize[j][size_t(x)][size_t(y)].push_back(UINT64VECTOR3(vBrickSize[0],
                                                          vBrickSize[1],
                                                          vBrickSize[2]));
          const BrickKey k = BrickKey(j, z*m_vaBrickCount[j].x*m_vaBrickCount[j].y + y*m_vaBrickCount[j].x + x);

          UINT64VECTOR3 effective = GetEffectiveBrickSize(k);

          bmd.extents  = FLOATVECTOR3(effective)/float(GetDomainSize(j).maxVal());
          bmd.center   = FLOATVECTOR3(vBrickCorner + bmd.extents/2.0f) - vNormalizedDomainSize*0.5f;
          bmd.n_voxels = UINTVECTOR3(vBrickSize[0], vBrickSize[1], vBrickSize[2]);

          AddBrick(k, bmd);
          vBrickCorner.z += bmd.extents.z;
        }
        vBrickCorner.y += bmd.extents.y;
      }
      vBrickCorner.x += bmd.extents.x;
    }
  }

  size_t iSerializedIndex = 0;
  if (m_pMaxMinData) {
    for (size_t lod=0; lod < iLODLevel; lod++) {
      for (UINT64 z=0; z < m_vaBrickCount[lod].z; z++) {
        for (UINT64 y=0; y < m_vaBrickCount[lod].y; y++) {
          for (UINT64 x=0; x < m_vaBrickCount[lod].x; x++) {
            // for four-component data we use the fourth component
            // (presumably the alpha channel); for all other data we use
            // the first component
            /// \todo we may have to change this if we add support for other
            /// kinds of multicomponent data.
            m_vvaMaxMin[lod][size_t(x)][size_t(y)][size_t(z)] =
              m_pMaxMinData->GetValue(iSerializedIndex++,
                 (m_pVolumeDataBlock->ulElementDimensionSize[0] == 4) ? 3 : 0
              );
          }
        }
      }
    }
  }

  SetRescaleFactors(DOUBLEVECTOR3(1.0,1.0,1.0));
}


// One dimensional brick shrinking for internal bricks that have some overlap
// with neighboring bricks.  Assumes overlap is constant per dataset: this
// brick's overlap with the brick to its right is the same as the overlap with
// the right brick's overlap with the brick to the left.
/// @param v            original brick size for this dimension
/// @param brickIndex   index of the brick in this dimension
/// @param maxindex     number-1 of bricks for this dimension
/// @param overlap      amount of per-brick overlap.
void UVFDataset::FixOverlap(UINT64& v, UINT64 brickIndex, UINT64 maxindex, UINT64 overlap) const {
  if(brickIndex > 0) {
    v -= static_cast<size_t>(overlap/2.0f);
  }
  if(brickIndex < maxindex) {
    v -= static_cast<size_t>(overlap/2.0f);
  }
}


// Gives the size of a brick in real space.
/// @todo FIXME-IO: obsolete: any query on bricks must be done in
/// ExternalDataset, with a pure-virtual for that call in Dataset.
UINT64VECTOR3 UVFDataset::GetEffectiveBrickSize(const BrickKey &k) const
{
  const NDBrickKey& key = IndexToVectorKey(k);
  UINT64 iLOD = k.first;
  UINT64VECTOR3 vBrickSize = m_vvaBrickSize[iLOD][key.second[0]][key.second[1]][key.second[2]];

  // If this is an internal brick, the size is a bit smaller based on the
  // amount of overlap per-brick.
  if (m_vaBrickCount[iLOD].x > 1) {
    FixOverlap(vBrickSize.x, key.second[0], m_vaBrickCount[iLOD].x-1, m_aOverlap.x);
  }
  if (m_vaBrickCount[iLOD].y > 1) {
    FixOverlap(vBrickSize.y, key.second[1], m_vaBrickCount[iLOD].y-1, m_aOverlap.y);
  }
  if (m_vaBrickCount[iLOD].z > 1) {
    FixOverlap(vBrickSize.z, key.second[2], m_vaBrickCount[iLOD].z-1, m_aOverlap.z);
  }

  return vBrickSize;
}


BrickTable::size_type UVFDataset::GetBrickCount(size_t lod) const
{
  return BrickTable::size_type(m_vaBrickCount[lod].volume());
}


UINT64VECTOR3 UVFDataset::GetDomainSize(const size_t lod) const 
{
  return m_aDomainSize[lod];
}

UINT64 UVFDataset::FindSuitableRasterBlock() {
  UINT64 iRasterBlockIndex = UINT64(-1);
  for (size_t iBlocks = 0;
       iBlocks < m_pDatasetFile->GetDataBlockCount();
       iBlocks++) {
    if (m_pDatasetFile->GetDataBlock(iBlocks)->GetBlockSemantic() ==
        UVFTables::BS_1D_Histogram) {
      if (m_pHist1DDataBlock != NULL) {
        WARNING("Multiple 1D Histograms found using last block.");
      }
      m_pHist1DDataBlock = (Histogram1DDataBlock*)m_pDatasetFile->GetDataBlock(iBlocks);
    } else if (m_pDatasetFile->GetDataBlock(iBlocks)->GetBlockSemantic() ==
               UVFTables::BS_2D_Histogram) {
      if (m_pHist2DDataBlock != NULL) {
        WARNING("Multiple 2D Histograms found using last block.");
      }
      m_pHist2DDataBlock = (Histogram2DDataBlock*)m_pDatasetFile->GetDataBlock(iBlocks);
    } else if (m_pDatasetFile->GetDataBlock(iBlocks)->GetBlockSemantic() ==
               UVFTables::BS_MAXMIN_VALUES) {
      if (m_pMaxMinData != NULL) {
        WARNING("Multiple MaxMinData Blocks found using last block.");
      }
      m_pMaxMinData = (MaxMinDataBlock*)m_pDatasetFile->GetDataBlock(iBlocks);
    } else if (m_pDatasetFile->GetDataBlock(iBlocks)->GetBlockSemantic() ==
               UVFTables::BS_REG_NDIM_GRID) {
      const RasterDataBlock* pVolumeDataBlock =
                         static_cast<const RasterDataBlock*>
                                    (m_pDatasetFile->GetDataBlock(iBlocks));

      // check if the block is at least 3 dimensional
      if (pVolumeDataBlock->ulDomainSize.size() < 3) {
        MESSAGE("%i-D raster data block found in UVF file, skipping.",
                int(pVolumeDataBlock->ulDomainSize.size()));
        continue;
      }

      // check if the ulElementDimension = 1 e.g. we can deal with scalars and vectors
      if (pVolumeDataBlock->ulElementDimension != 1) {
        MESSAGE("Non scalar/vector raster data block found in UVF file,"
                " skipping.");
        continue;
      }

      /// \todo: rethink this for time dependent data
      if (pVolumeDataBlock->ulLODGroups[0] != pVolumeDataBlock->ulLODGroups[1] ||
          pVolumeDataBlock->ulLODGroups[1] != pVolumeDataBlock->ulLODGroups[2]) {
        MESSAGE("Raster data block with unsupported LOD layout found in "
                "UVF file, skipping.");
        continue;
      }

      /// \todo: change this if we want to support vector data
      // check if we have anything other than scalars or color
      if (pVolumeDataBlock->ulElementDimensionSize[0] != 1 &&
          pVolumeDataBlock->ulElementDimensionSize[0] != 4) {
        MESSAGE("Skipping UVF raster data block with %u elements; "
                "only know how to handle scalar and color data.",
                pVolumeDataBlock->ulElementDimensionSize[0]);
        continue;
      }

      // check if the data's smallest LOD level is not larger than our bricksize
      /// \todo: if this fails we may want to convert the dataset
      std::vector<UINT64> vSmallLODBrick = pVolumeDataBlock->GetSmallestBrickSize();
      bool bToFewLODLevels = false;
      for (size_t i = 0;i<vSmallLODBrick.size();i++) {
        if (vSmallLODBrick[i] > BRICKSIZE) {
          MESSAGE("Raster data block with insufficient LOD levels found in "
                  "UVF file, skipping.");
          bToFewLODLevels = true;
          break;
        }
      }
      if (bToFewLODLevels) continue;

      if (iRasterBlockIndex != UINT64(-1)) {
        WARNING("Multiple volume blocks found using last block.");
      }
      iRasterBlockIndex = iBlocks;
    } else {
      MESSAGE("Non-volume block found in UVF file, skipping.");
    }
  }
  return iRasterBlockIndex;
}

void UVFDataset::GetHistograms() {

  m_pHist1D = NULL;
  if (m_pHist1DDataBlock != NULL) {
    const std::vector<UINT64>& vHist1D = m_pHist1DDataBlock->GetHistogram();
    m_pHist1D = new Histogram1D(vHist1D.size());
    for (size_t i = 0;i<m_pHist1D->GetSize();i++) {
      m_pHist1D->Set(i, UINT32(vHist1D[i]));
    }
  } else {
    // generate a zero 1D histogram (max 4k) if none is found in the file
    m_pHist1D = new Histogram1D(std::min(4096, 1<<GetBitWidth()));
    for (size_t i = 0;i<m_pHist1D->GetSize();i++) {
      // set all values to one so "getFilledsize" later does not return a
      // completely empty dataset
      m_pHist1D->Set(i, 1);
    }
  }

  m_pHist2D = NULL;
  if (m_pHist2DDataBlock != NULL) {
    const std::vector< std::vector<UINT64> >& vHist2D = m_pHist2DDataBlock->GetHistogram();

    VECTOR2<size_t> vSize(vHist2D.size(),vHist2D[0].size());

    m_pHist2D = new Histogram2D(vSize);
    for (size_t y = 0;y<m_pHist2D->GetSize().y;y++)
      for (size_t x = 0;x<m_pHist2D->GetSize().x;x++)
        m_pHist2D->Set(x,y,UINT32(vHist2D[x][y]));

    m_fMaxGradMagnitude = m_pHist2DDataBlock->GetMaxGradMagnitude();
  } else {
    // generate a zero 2D histogram (max 4k) if none is found in the file
    VECTOR2<size_t> vec(256, std::min(4096,
                                      1<<GetBitWidth()));
    m_pHist2D = new Histogram2D(vec);
    for (size_t y = 0;y<m_pHist2D->GetSize().y;y++) {
      for (size_t x = 0;x<m_pHist2D->GetSize().x;x++) {
        // set all values to one so "getFilledsize" later does not return a
        // completely empty dataset
        m_pHist2D->Set(x,y,1);
      }
    }

    m_fMaxGradMagnitude = 0;
  }
}

UINTVECTOR3 UVFDataset::GetBrickVoxelCounts(const BrickKey& k) const
{
  UINT64 iLOD = k.first;
  const NDBrickKey& key = IndexToVectorKey(k);
  return UINTVECTOR3(m_vvaBrickSize[iLOD][key.second[0]][key.second[1]][key.second[2]]);
}

bool UVFDataset::Export(UINT64 iLODLevel, const std::string& targetFilename,
                        bool bAppend,
                        bool (*brickFunc)(LargeRAWFile* pSourceFile,
                                          const std::vector<UINT64> vBrickSize,
                                          const std::vector<UINT64> vBrickOffset,
                                          void* pUserContext),
                        void *pUserContext,
                        UINT64 iOverlap) const
{
  std::vector<UINT64> vLOD; vLOD.push_back(iLODLevel);
  return m_pVolumeDataBlock->BrickedLODToFlatData(vLOD, targetFilename,
                                                  bAppend,
                                                  &Controller::Debug::Out(),
                                                  brickFunc, pUserContext,
                                                  iOverlap);
}


bool
UVFDataset::BrickIsFirstInDimension(size_t dim, const BrickKey& k) const
{
  const BrickMD& md = this->bricks.find(k)->second;
  for(BrickTable::const_iterator iter = this->BricksBegin();
      iter != this->BricksEnd(); ++iter) {
    if(iter->second.center[dim] < md.center[dim]) {
      return false;
    }
  }
  return true;
}

bool
UVFDataset::BrickIsLastInDimension(size_t dim, const BrickKey& k) const
{
  const BrickMD& md = this->bricks.find(k)->second;
  for(BrickTable::const_iterator iter = this->BricksBegin();
      iter != this->BricksEnd(); ++iter) {
    if(iter->second.center[dim] > md.center[dim]) {
      return false;
    }
  }
  return true;
}


bool
UVFDataset::GetBrick(const BrickKey& k,
                      std::vector<unsigned char>& vData) const {
  const NDBrickKey& key = this->IndexToVectorKey(k);
  return m_pVolumeDataBlock->GetData(vData, key.first, key.second);
}

std::vector<UINT64> 
UVFDataset::IndexToVector(const BrickKey &k) const {
  std::vector<UINT64> vBrick;

  UINT64 iIndex = UINT64(k.second);
  UINT64 iZIndex = UINT64(iIndex / (m_vaBrickCount[k.first].x*m_vaBrickCount[k.first].y));
  iIndex = iIndex % (m_vaBrickCount[k.first].x*m_vaBrickCount[k.first].y);
  UINT64 iYIndex = UINT64(iIndex / m_vaBrickCount[k.first].x);
  iIndex = iIndex % m_vaBrickCount[k.first].x;
  UINT64 iXIndex = iIndex;

  vBrick.push_back(iXIndex);
  vBrick.push_back(iYIndex);
  vBrick.push_back(iZIndex);

  return vBrick;
}

NDBrickKey UVFDataset::IndexToVectorKey(const BrickKey &k) const {
  std::vector<UINT64> vLOD;
  std::vector<UINT64> vBrick = IndexToVector(k);
  vLOD.push_back(k.first);
  return std::make_pair(vLOD, vBrick);
}

UINTVECTOR3 UVFDataset::GetMaxBrickSize() const
{
  return m_aMaxBrickSize;
}

UINTVECTOR3 UVFDataset::GetBrickOverlapSize() const
{
  return m_aOverlap;
}

UINT64 UVFDataset::GetLODLevelCount() const
{
  return m_vvaBrickSize.size();
}

/// \todo change this if we want to support data where elements are of
// different size
UINT64 UVFDataset::GetBitWidth() const
{
  return m_pVolumeDataBlock->ulElementBitSize[0][0];
}

UINT64 UVFDataset::GetComponentCount() const
{
  return m_pVolumeDataBlock->ulElementDimensionSize[0];
}

/// \todo change this if we want to support data where elements are of
/// different type
bool UVFDataset::GetIsSigned() const
{
  return m_pVolumeDataBlock->bSignedElement[0][0];
}

/// \todo change this if we want to support data where elements are of
///  different type
bool UVFDataset::GetIsFloat() const
{
  return GetBitWidth() != m_pVolumeDataBlock->ulElementBitSize[0][0];
}

bool UVFDataset::IsSameEndianness() const
{
  return m_bIsSameEndianness;
}


bool UVFDataset::ContainsData(const BrickKey &k, double isoval) const 
{
  // if we have no max min data we have to assume that every block is visible
  if(NULL == m_pMaxMinData) {return true;}

  const NDBrickKey& key = IndexToVectorKey(k);
  UINT64 iLOD = k.first;
  const InternalMaxMinElement& maxMinElement = m_vvaMaxMin[iLOD][key.second[0]][key.second[1]][key.second[2]];

  return (isoval <= maxMinElement.maxScalar);
}

bool UVFDataset::ContainsData(const BrickKey &k, double fMin,double fMax) const
{
  // if we have no max min data we have to assume that every block is visible
  if(NULL == m_pMaxMinData) {return true;}

  const NDBrickKey& key = IndexToVectorKey(k);
  UINT64 iLOD = k.first;
  const InternalMaxMinElement& maxMinElement = m_vvaMaxMin[iLOD][key.second[0]][key.second[1]][key.second[2]];

  return (fMax >= maxMinElement.minScalar && fMin <= maxMinElement.maxScalar);
}

bool UVFDataset::ContainsData(const BrickKey &k, double fMin,double fMax, double fMinGradient,double fMaxGradient) const 
{
  // if we have no max min data we have to assume that every block is visible
  if(NULL == m_pMaxMinData) {return true;}

  const NDBrickKey& key = IndexToVectorKey(k);
  UINT64 iLOD = k.first;
  const InternalMaxMinElement& maxMinElement = m_vvaMaxMin[iLOD][key.second[0]][key.second[1]][key.second[2]];

  return (fMax >= maxMinElement.minScalar &&
          fMin <= maxMinElement.maxScalar)
                         &&
         (fMaxGradient >= maxMinElement.minGradient &&
          fMinGradient <= maxMinElement.maxGradient);
}


}; // tuvok namespace.
