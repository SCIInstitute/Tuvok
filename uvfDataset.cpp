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
#include "UVF/KeyValuePairDataBlock.h"
#include "UVF/Histogram2DDataBlock.h"

using namespace std;

namespace tuvok {

UVFDataset::UVFDataset(const std::string& strFilename, UINT64 iMaxAcceptableBricksize, bool bVerify, bool bMustBeSameVersion) :
  m_pKVDataBlock(NULL),
  m_bIsSameEndianness(true),
  m_pDatasetFile(NULL),
  m_bIsOpen(false),
  m_strFilename(strFilename),
  m_CachedRange(make_pair(+1,-1)),
  m_iMaxAcceptableBricksize(iMaxAcceptableBricksize),
  m_bOnlyBricksizeCheckFailed(false)
{
  Open(bVerify, false, bMustBeSameVersion);
}

UVFDataset::UVFDataset() :
  m_pKVDataBlock(NULL),
  m_bIsSameEndianness(true),
  m_pDatasetFile(NULL),
  m_bIsOpen(false),
  m_strFilename(""),
  m_CachedRange(make_pair(+1,-1)),
  m_iMaxAcceptableBricksize(DEFAULT_BRICKSIZE),
  m_bOnlyBricksizeCheckFailed(false)
{
}

UVFDataset::~UVFDataset()
{
  Close();
}

bool UVFDataset::Open(bool bVerify, bool bReadWrite, bool bMustBeSameVersion)
{
  // open the file
  std::wstring wstrFilename(m_strFilename.begin(),m_strFilename.end());
  m_pDatasetFile = new UVF(wstrFilename);
  std::string strError;
  m_bIsOpen = m_pDatasetFile->Open(bMustBeSameVersion,bVerify,bReadWrite,&strError);
  if (!m_bIsOpen) {
    T_ERROR(strError.c_str());
    return false;
  }

  if (m_pDatasetFile->ms_ulReaderVersion != m_pDatasetFile->GetGlobalHeader().ulFileVersion) {
    // bMustBeSameVersion must not be set otherwise Open would have thrown an error
    WARNING("WARNING: Opening UVF file with a version (%u) different from this program's (%u)!!!",
            unsigned(m_pDatasetFile->ms_ulReaderVersion),
            unsigned(m_pDatasetFile->GetGlobalHeader().ulFileVersion));
  }

  size_t n_timesteps = DetermineNumberOfTimesteps();
  if (n_timesteps == 0) {
    T_ERROR("No suitable volume block found in UVF file.  Check previous messages for rejected blocks.");
    Close();
    m_bIsOpen = false;
    return false;
  }
  m_timesteps.resize(n_timesteps);

  // analyze the main raster data blocks
  FindSuitableRasterBlocks(m_bOnlyBricksizeCheckFailed);

  if (m_bOnlyBricksizeCheckFailed) {
    WARNING("No suitable volume block found in UVF file but rebricking would make this file usable.");
    Close();
    m_bIsOpen = false;
    return false;
  }
  MESSAGE("Open successfully found %u suitable data block in the UVF file.",
          static_cast<unsigned>(n_timesteps));

  MESSAGE("Analyzing data...");

  m_bIsSameEndianness = m_pDatasetFile->GetGlobalHeader().bIsBigEndian ==
                          EndianConvert::IsBigEndian();

  SetRescaleFactors(DOUBLEVECTOR3(1.0,1.0,1.0));
  // get the metadata and the histograms
  for(size_t i=0; i < n_timesteps; ++i) {
    ComputeMetaData(i);
    GetHistograms(i);
  }

  // print out data statistics
  MESSAGE("  %u timesteps found in the UVF.",
          static_cast<unsigned>(n_timesteps));
  for(size_t tsi=0; tsi < n_timesteps; ++tsi) {
    Timestep& ts = m_timesteps[tsi];
    ostringstream stats;
    stats << "Timestep " << tsi << ":\n"
          << "  Dataset size: "
          << ts.m_aDomainSize[0].x << " x "
          << ts.m_aDomainSize[0].y << " x "
          << ts.m_aDomainSize[0].z << "\n"
          << "  Brick layout of highest resolution level: "
          << ts.m_vaBrickCount[0].x << " x "
          << ts.m_vaBrickCount[0].y << " x "
          << ts.m_vaBrickCount[0].z << "\n  "
          << GetBitWidth() << " bit, "
          << GetComponentCount() << " components\n"
          << "  LOD down to "
          << ts.m_vaBrickCount[ts.m_vaBrickCount.size()-1].x << " x "
          << ts.m_vaBrickCount[ts.m_vaBrickCount.size()-1].y << " x "
          << ts.m_vaBrickCount[ts.m_vaBrickCount.size()-1].z
          << " found.";
    MESSAGE("%s", stats.str().c_str());
  }
  return true;
}

void UVFDataset::Close() {
  delete m_pDatasetFile;

  for(std::vector<Timestep>::iterator ts = m_timesteps.begin();
      ts != m_timesteps.end(); ++ts) {
    ts->m_fMaxGradMagnitude = 0.0f;
    ts->m_pVolumeDataBlock = NULL;
    ts->m_pHist1DDataBlock = NULL;
    ts->m_pHist2DDataBlock = NULL;
    ts->m_pMaxMinData= NULL;
  }

  m_pKVDataBlock= NULL;
  m_pDatasetFile= NULL;
  m_bIsOpen= false;
}

void UVFDataset::ComputeMetaData(size_t timestep) {
  std::vector<double> vfScale;
  const RasterDataBlock* pVolumeDataBlock =
    m_timesteps[timestep].m_pVolumeDataBlock;

  struct Timestep& ts = m_timesteps[timestep];

  size_t iSize = pVolumeDataBlock->ulDomainSize.size();

  // we require the data to be at least 3D
  assert(iSize >= 3);

  // we also assume that x,y,z are in the first 3 components and
  // we have no anisotropy (i.e. ulLODLevelCount.size=1)
  size_t iLODLevel = static_cast<size_t>(pVolumeDataBlock->ulLODLevelCount[0]);
  for (size_t i=0; i < 3 ; i++) {
    ts.m_aOverlap[i] = pVolumeDataBlock->ulBrickOverlap[i];
    /// @todo FIXME badness -- assume domain scaling information is the
    /// same across all raster data blocks (across all timesteps
    m_DomainScale[i] = pVolumeDataBlock->dDomainTransformation[i+(iSize+1)*i];
  }
  m_aMaxBrickSize.StoreMax(UINTVECTOR3(pVolumeDataBlock->ulBrickSize[0],
                                       pVolumeDataBlock->ulBrickSize[1],
                                       pVolumeDataBlock->ulBrickSize[2]));

  ts.m_vvaBrickSize.resize(iLODLevel);
  if (ts.m_pMaxMinData) {
    ts.m_vvaMaxMin.resize(iLODLevel);
  }

  for (size_t j = 0;j<iLODLevel;j++) {
    std::vector<UINT64> vLOD;  vLOD.push_back(j);
    std::vector<UINT64> vDomSize = pVolumeDataBlock->GetLODDomainSize(vLOD);
    ts.m_aDomainSize.push_back(UINT64VECTOR3(vDomSize[0], vDomSize[1], vDomSize[2]));

    std::vector<UINT64> vBrickCount = pVolumeDataBlock->GetBrickCount(vLOD);
    ts.m_vaBrickCount.push_back(UINT64VECTOR3(vBrickCount[0], vBrickCount[1], vBrickCount[2]));

    ts.m_vvaBrickSize[j].resize(size_t(ts.m_vaBrickCount[j].x));
    if (ts.m_pMaxMinData) {
      ts.m_vvaMaxMin[j].resize(size_t(ts.m_vaBrickCount[j].x));
    }

    FLOATVECTOR3 vBrickCorner;

    FLOATVECTOR3 vNormalizedDomainSize = FLOATVECTOR3(GetDomainSize(j, timestep));
    vNormalizedDomainSize /= vNormalizedDomainSize.maxVal();

    BrickMD bmd;
    for (UINT64 x=0; x < ts.m_vaBrickCount[j].x; x++) {
      ts.m_vvaBrickSize[j][size_t(x)].resize(size_t(ts.m_vaBrickCount[j].y));
      if (ts.m_pMaxMinData) {
        ts.m_vvaMaxMin[j][size_t(x)].resize(size_t(ts.m_vaBrickCount[j].y));
      }

      vBrickCorner.y = 0;
      for (UINT64 y=0; y < ts.m_vaBrickCount[j].y; y++) {
        if (ts.m_pMaxMinData) {
          ts.m_vvaMaxMin[j][size_t(x)][size_t(y)].resize(size_t(ts.m_vaBrickCount[j].z));
        }

        vBrickCorner.z = 0;
        for (UINT64 z=0; z < ts.m_vaBrickCount[j].z; z++) {
          std::vector<UINT64> vBrick;
          vBrick.push_back(x);
          vBrick.push_back(y);
          vBrick.push_back(z);
          std::vector<UINT64> vBrickSize =
            pVolumeDataBlock->GetBrickSize(vLOD, vBrick);

          ts.m_vvaBrickSize[j][size_t(x)][size_t(y)].push_back(
            UINT64VECTOR3(vBrickSize[0], vBrickSize[1], vBrickSize[2]));

          const BrickKey k = BrickKey(timestep, j, z*ts.m_vaBrickCount[j].x*ts.m_vaBrickCount[j].y +
                                                   y*ts.m_vaBrickCount[j].x + x);

          bmd.extents  = FLOATVECTOR3(GetEffectiveBrickSize(k)) /
                         float(GetDomainSize(j, timestep).maxVal());
          bmd.center   = FLOATVECTOR3(vBrickCorner + bmd.extents/2.0f) -
                         vNormalizedDomainSize * 0.5f;
          bmd.n_voxels = UINTVECTOR3(vBrickSize[0], vBrickSize[1],
                                     vBrickSize[2]);
          AddBrick(k, bmd);
          vBrickCorner.z += bmd.extents.z;
        }
        vBrickCorner.y += bmd.extents.y;
      }
      vBrickCorner.x += bmd.extents.x;
    }
  }

  size_t iSerializedIndex = 0;
  if (ts.m_pMaxMinData) {
    for (size_t lod=0; lod < iLODLevel; lod++) {
      for (UINT64 z=0; z < ts.m_vaBrickCount[lod].z; z++) {
        for (UINT64 y=0; y < ts.m_vaBrickCount[lod].y; y++) {
          for (UINT64 x=0; x < ts.m_vaBrickCount[lod].x; x++) {
            // for four-component data we use the fourth component
            // (presumably the alpha channel); for all other data we use
            // the first component
            /// \todo we may have to change this if we add support for other
            /// kinds of multicomponent data.
            ts.m_vvaMaxMin[lod][size_t(x)][size_t(y)][size_t(z)] =
              ts.m_pMaxMinData->GetValue(iSerializedIndex++,
                 (pVolumeDataBlock->ulElementDimensionSize[0] == 4) ? 3 : 0
              );
          }
        }
      }
    }
  }
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

size_t UVFDataset::DetermineNumberOfTimesteps()
{
  size_t raster, hist1d, hist2d, accel;
  bool is_color = false;
  raster = hist1d = hist2d = accel = 0;
  for(size_t block=0; block < m_pDatasetFile->GetDataBlockCount(); ++block) {
    switch(m_pDatasetFile->GetDataBlock(block)->GetBlockSemantic()) {
      case UVFTables::BS_1D_HISTOGRAM: hist1d++; break;
      case UVFTables::BS_2D_HISTOGRAM: hist2d++; break;
      case UVFTables::BS_MAXMIN_VALUES: accel++; break;
      case UVFTables::BS_REG_NDIM_GRID:
        {
          const RasterDataBlock *rdb =
            static_cast<const RasterDataBlock*>
                       (m_pDatasetFile->GetDataBlock(block));
          if(VerifyRasterDataBlock(rdb)) {
            ++raster;
            if (rdb->ulElementDimensionSize[0] == 4) { is_color = true; }
          }
        }
        break;
      default: break;
    }
  }
  MESSAGE("Block counts (raster, hist1, hist2, accel): (%u, %u, %u, %u)",
          static_cast<unsigned>(raster),
          static_cast<unsigned>(hist1d),
          static_cast<unsigned>(hist2d),
          static_cast<unsigned>(accel));
  if(raster == hist1d && hist1d == hist2d && hist2d == accel) {
    return raster;
  }
  // if the number of blocks don't match, say we have 0 valid timesteps.
  T_ERROR("UVF Block combinations do not match; "
          "do not know how to interpret data."
          "Block counts (raster, hist1, hist2, accel): (%u, %u, %u, %u)",
          static_cast<unsigned>(raster),
          static_cast<unsigned>(hist1d),
          static_cast<unsigned>(hist2d),
          static_cast<unsigned>(accel));
  if(is_color) {
    WARNING("Hack -- assuming 1 timestep in color data");
    return 1;
  }
  return 0;
}

bool UVFDataset::VerifyRasterDataBlock(const RasterDataBlock* rdb) const
{
  if((rdb->ulDomainSize.size() < 3) ||
     (rdb->ulElementDimension != 1) ||
     (rdb->ulLODGroups[0] != rdb->ulLODGroups[1]) ||
     (rdb->ulLODGroups[1] != rdb->ulLODGroups[2])) {
    return false;
  }

  /// \todo: change this if we want to support vector data
  // check if we have anything other than scalars or color
  if (rdb->ulElementDimensionSize[0] != 1 &&
      rdb->ulElementDimensionSize[0] != 4) {
    // check if the data's coarsest LOD level contains only one brick
    const vector<UINT64>& vSmallestLODBrickCount = rdb->GetBrickCount(rdb->GetSmallestBrickIndex());
    UINT64 iSmallestLODBrickCount = vSmallestLODBrickCount[0];
    for (size_t i = 1;i<3;i++) iSmallestLODBrickCount *= vSmallestLODBrickCount[i]; // currently we only care about the first 3 dimensions
    if (iSmallestLODBrickCount > 1) {
      return false;
    }
  }
  return true;
}


// Gives the size of a brick in real space.
UINT64VECTOR3 UVFDataset::GetEffectiveBrickSize(const BrickKey &k) const
{
  const NDBrickKey& key = IndexToVectorKey(k);
  UINT64 iLOD = std::tr1::get<1>(k);
  UINT64VECTOR3 vBrickSize =
    m_timesteps[key.timestep].
      m_vvaBrickSize[iLOD][key.brick[0]][key.brick[1]][key.brick[2]];

  const Timestep& ts = m_timesteps[key.timestep];

  // If this is an internal brick, the size is a bit smaller based on the
  // amount of overlap per-brick.
  if (ts.m_vaBrickCount[iLOD].x > 1) {
    FixOverlap(vBrickSize.x, key.brick[0], ts.m_vaBrickCount[iLOD].x-1, ts.m_aOverlap.x);
  }
  if (ts.m_vaBrickCount[iLOD].y > 1) {
    FixOverlap(vBrickSize.y, key.brick[1], ts.m_vaBrickCount[iLOD].y-1, ts.m_aOverlap.y);
  }
  if (ts.m_vaBrickCount[iLOD].z > 1) {
    FixOverlap(vBrickSize.z, key.brick[2], ts.m_vaBrickCount[iLOD].z-1, ts.m_aOverlap.z);
  }

  return vBrickSize;
}


BrickTable::size_type UVFDataset::GetBrickCount(size_t lod, size_t ts) const
{
  return BrickTable::size_type(m_timesteps[ts].m_vaBrickCount[lod].volume());
}


UINT64VECTOR3 UVFDataset::GetDomainSize(const size_t lod, const size_t ts) const
{
  return m_timesteps[ts].m_aDomainSize[lod];
}

float UVFDataset::MaxGradientMagnitude() const
{
  float mx = -std::numeric_limits<float>::max();
  for(std::vector<Timestep>::const_iterator ts = m_timesteps.begin();
      ts != m_timesteps.end(); ++ts) {
    mx = std::max(mx, ts->m_fMaxGradMagnitude);
  }
  return mx;
}

void UVFDataset::FindSuitableRasterBlocks(bool &bOnlyBricksizeCheckFailed) {
  bOnlyBricksizeCheckFailed = false;
  // keep a count of each type of block.  We require that the number of blocks
  // match, or put another way, that all blocks exist for all timesteps.  This
  // isn't strictly necessary; we could still, technically, work with a
  // timestep that was missing acceleration structures.
  size_t raster=0, hist1d=0, hist2d=0, accel=0;

  for (size_t iBlocks = 0;
       iBlocks < m_pDatasetFile->GetDataBlockCount();
       iBlocks++) {
    switch(m_pDatasetFile->GetDataBlock(iBlocks)->GetBlockSemantic()) {
      case UVFTables::BS_1D_HISTOGRAM:
        m_timesteps[hist1d++].m_pHist1DDataBlock =
          static_cast<const Histogram1DDataBlock*>
                     (m_pDatasetFile->GetDataBlock(iBlocks));
        break;
      case UVFTables::BS_2D_HISTOGRAM:
        m_timesteps[hist2d++].m_pHist2DDataBlock =
          static_cast<const Histogram2DDataBlock*>
                     (m_pDatasetFile->GetDataBlock(iBlocks));
        break;
      case UVFTables::BS_KEY_VALUE_PAIRS:
        if(m_pKVDataBlock != NULL) {
          WARNING("Multiple Key-Value pair blocks; using first!");
          continue;
        }
        m_pKVDataBlock = (KeyValuePairDataBlock*)m_pDatasetFile->GetDataBlock(iBlocks);
        break;
      case UVFTables::BS_MAXMIN_VALUES:
        m_timesteps[accel++].m_pMaxMinData = (MaxMinDataBlock*)m_pDatasetFile->GetDataBlock(iBlocks);
        break;
      case UVFTables::BS_REG_NDIM_GRID:
        {
          const RasterDataBlock* pVolumeDataBlock =
                             static_cast<const RasterDataBlock*>
                                        (m_pDatasetFile->GetDataBlock(iBlocks));

          if(!VerifyRasterDataBlock(pVolumeDataBlock)) {
            WARNING("A RasterDataBlock failed verification; skipping it");
            continue;
          }

          // check if the data's biggest brick dimensions are
          // smaller than m_iMaxAcceptableBricksize
          std::vector<UINT64> vMaxBrickSizes = pVolumeDataBlock->GetLargestBrickSizes();
          for (size_t i=0; i < 3; i++) {  // currently we only care about the first 3 dimensions
            if (vMaxBrickSizes[i] > m_iMaxAcceptableBricksize) {
              WARNING("Raster data with too large bricks found in UVF file,"
                      "re-bricking may be necessary.");
              bOnlyBricksizeCheckFailed = true;
              break;
            }
          }

          m_timesteps[raster++].m_pVolumeDataBlock = pVolumeDataBlock;
        }
        break;
      default:
        MESSAGE("Non-volume block found in UVF file, skipping.");
        break;
    }
  }
}


/// @todo fixme (hack): we only look at the first timestep for the
/// histograms.  should really set a vector of histograms, one per timestep.
void UVFDataset::GetHistograms(size_t) {
  m_pHist1D = NULL;


  struct Timestep& ts = m_timesteps[0];
  if (ts.m_pHist1DDataBlock != NULL) {
    const std::vector<UINT64>& vHist1D = ts.m_pHist1DDataBlock->GetHistogram();
    m_pHist1D = new Histogram1D(vHist1D.size());
    for (size_t i = 0;i < m_pHist1D->GetSize(); i++) {
      m_pHist1D->Set(i, UINT32(vHist1D[i]));
    }
  } else {
    // generate a zero 1D histogram (max 4k) if none is found in the file
    m_pHist1D = new Histogram1D(std::min(4096, 1<<GetBitWidth()));

    // set all values to one so "getFilledsize" later does not return a
    // completely empty dataset
    for (size_t i = 0;i<m_pHist1D->GetSize();i++) {
      m_pHist1D->Set(i, 1);
    }
  }

  m_pHist2D = NULL;
  if (ts.m_pHist2DDataBlock != NULL) {
    const std::vector< std::vector<UINT64> >& vHist2D =
      ts.m_pHist2DDataBlock->GetHistogram();

    VECTOR2<size_t> vSize(vHist2D.size(),vHist2D[0].size());

    m_pHist2D = new Histogram2D(vSize);
    for (size_t y = 0;y<m_pHist2D->GetSize().y;y++)
      for (size_t x = 0;x<m_pHist2D->GetSize().x;x++)
        m_pHist2D->Set(x,y,UINT32(vHist2D[x][y]));

    ts.m_fMaxGradMagnitude = ts.m_pHist2DDataBlock->GetMaxGradMagnitude();
  } else {
    // generate a zero 2D histogram (max 4k) if none is found in the file
    VECTOR2<size_t> vec(256, std::min(4096, 1<<GetBitWidth()));

    m_pHist2D = new Histogram2D(vec);
    for (size_t y=0; y < m_pHist2D->GetSize().y; y++) {
      // set all values to one so "getFilledsize" later does not return a
      // completely empty dataset
      for (size_t x=0; x < m_pHist2D->GetSize().x; x++) {
        m_pHist2D->Set(x,y,1);
      }
    }

    ts.m_fMaxGradMagnitude = 0;
  }
}

UINTVECTOR3 UVFDataset::GetBrickVoxelCounts(const BrickKey& k) const
{
  UINT64 iLOD = std::tr1::get<1>(k);
  const NDBrickKey& key = IndexToVectorKey(k);
  return UINTVECTOR3(m_timesteps[key.timestep].m_vvaBrickSize[iLOD][key.brick[0]][key.brick[1]][key.brick[2]]);
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

  bool okay = true;
  for(std::vector<Timestep>::const_iterator ts = m_timesteps.begin();
      ts != m_timesteps.end(); ++ts) {
    // Unbrick each timestep.  Append the data if the user asks, but we must
    // always append on second and subsequent timesteps!
    okay &= ts->m_pVolumeDataBlock->BrickedLODToFlatData(
              vLOD, targetFilename, bAppend || ts != m_timesteps.begin(),
              &Controller::Debug::Out(), brickFunc, pUserContext, iOverlap
            );
  }
  return okay;
}


bool
UVFDataset::GetBrick(const BrickKey& k,
                     std::vector<unsigned char>& vData) const {
  const NDBrickKey& key = this->IndexToVectorKey(k);
  const Timestep& ts = m_timesteps[key.timestep];
  return ts.m_pVolumeDataBlock->GetData(vData, key.lod, key.brick);
}

// BrickKey's index is 1D.  For UVF, we've got a 3D index.  When we create the
// brick index to satisfy the interface, we do so in a reversible way.  This
// methods reverses the 1D into into UVF's 3D index.
std::vector<UINT64>
UVFDataset::IndexToVector(const BrickKey &k) const {
  std::vector<UINT64> vBrick;
  const Timestep& ts = m_timesteps[std::tr1::get<0>(k)];
  const size_t lod = std::tr1::get<1>(k);

  UINT64 iIndex = UINT64(std::tr1::get<2>(k));
  UINT64 iZIndex = UINT64(iIndex / (ts.m_vaBrickCount[lod].x *
                                    ts.m_vaBrickCount[lod].y));
  iIndex = iIndex % (ts.m_vaBrickCount[lod].x *
                     ts.m_vaBrickCount[lod].y);
  UINT64 iYIndex = UINT64(iIndex / ts.m_vaBrickCount[lod].x);
  iIndex = iIndex % ts.m_vaBrickCount[lod].x;
  UINT64 iXIndex = iIndex;

  vBrick.push_back(iXIndex);
  vBrick.push_back(iYIndex);
  vBrick.push_back(iZIndex);

  return vBrick;
}

NDBrickKey UVFDataset::IndexToVectorKey(const BrickKey &k) const {
  NDBrickKey ndk;
  ndk.timestep = std::tr1::get<0>(k);
  ndk.lod.push_back(std::tr1::get<1>(k));
  ndk.brick = IndexToVector(k);
  return ndk;
}

// determines the largest actually used brick dimensions
// in the current dataset
UINT64VECTOR3 UVFDataset::GetMaxUsedBrickSizes() const
{
  UINT64VECTOR3 vMaxSize(1,1,1);
  UINT64VECTOR3 vAbsoluteMax(m_aMaxBrickSize);

  for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
    const Timestep& ts = m_timesteps[tsi];

    for (size_t iLOD=0;iLOD < ts.m_vvaBrickSize.size(); iLOD++) {
      for (size_t iX=0; iX < ts.m_vvaBrickSize[iLOD].size(); iX++) {
        for (size_t iY=0; iY < ts.m_vvaBrickSize[iLOD][iX].size(); iY++) {
          for (size_t iZ=0; iZ < ts.m_vvaBrickSize[iLOD][iX][iY].size(); iZ++) {
            vMaxSize.StoreMax(ts.m_vvaBrickSize[iLOD][iX][iY][iZ]);

            // as no brick should be larger than vAbsoluteMax
            // we can terminate the scan if we reached that size
            if (vMaxSize == vAbsoluteMax) return vAbsoluteMax;
          }
        }
      }
    }
  }

  return vMaxSize;
}

UINTVECTOR3 UVFDataset::GetMaxBrickSize() const
{
  return m_aMaxBrickSize;
}

UINTVECTOR3 UVFDataset::GetBrickOverlapSize() const
{
  /// @todo HACK fixme: should take into account the LOD and timestep, probably
  /// need them as arguments.
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  return m_timesteps[0].m_aOverlap;
}

UINT64 UVFDataset::GetLODLevelCount() const
{
  /// @todo HACK fixme: should take into account the timestep, needed as arg
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  return m_timesteps[0].m_vvaBrickSize.size();
}

/// \todo change this if we want to support data where elements are of
// different size
UINT64 UVFDataset::GetBitWidth() const
{
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  // All data in the time series should have the same bit width, so any
  // timestep we choose to query the bit width from should be fine.
  return m_timesteps[0].m_pVolumeDataBlock->ulElementBitSize[0][0];
}

UINT64 UVFDataset::GetComponentCount() const
{
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  // All data in the time series should have the same bit number of
  // components, so any timestep we choose to query the component count
  // from should be fine.
  return m_timesteps[0].m_pVolumeDataBlock->ulElementDimensionSize[0];
}

/// \todo change this if we want to support data where elements are of
/// different type
bool UVFDataset::GetIsSigned() const
{
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  // All data in the time series should have the same signedness,
  // so any timestep we choose to query the signedness from should be fine.
  return m_timesteps[0].m_pVolumeDataBlock->bSignedElement[0][0];
}

/// \todo change this if we want to support data where elements are of
///  different type
bool UVFDataset::GetIsFloat() const
{
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  // All data in the time series should have the same type, so any timestep
  // we choose to query the type from should be fine.
  return GetBitWidth() !=
         m_timesteps[0].m_pVolumeDataBlock->ulElementBitSize[0][0];
}

bool UVFDataset::IsSameEndianness() const
{
  return m_bIsSameEndianness;
}

std::pair<double,double> UVFDataset::GetRange() {
  // If we're missing MaxMin data for any timestep, we don't have maxmin data.
  bool have_maxmin_data = true;
  for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
    if(m_timesteps[tsi].m_pMaxMinData == NULL) {
      WARNING("Missing acceleration structure for timestep %u",
              static_cast<unsigned>(tsi));
      have_maxmin_data = false;
    }
  }

  // second < first is a convention we use to indicate "haven't figured this
  // out yet".  We might not have MaxMin data though; in some cases, we'll
  // never figure it out.
  if (have_maxmin_data && m_CachedRange.second < m_CachedRange.first) {
    // to find the range of values we simply traverse all the bricks in
    // LOD level 0 (highest res) and compute the max & min
    std::pair<double,double> limits;

    for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
      const Timestep& ts = m_timesteps[tsi];
      for (size_t i=0; i < GetBrickCount(0, tsi); i++) {
        BrickKey k(tsi,0,i);
        const NDBrickKey& key = IndexToVectorKey(k);

        const InternalMaxMinElement& maxMinElement =
          ts.m_vvaMaxMin[0][key.brick[0]][key.brick[1]][key.brick[2]];

        if (i>0) {
          limits.first  = min(limits.first, maxMinElement.minScalar);
          limits.second = max(limits.second, maxMinElement.maxScalar);
        } else {
          limits = make_pair(maxMinElement.minScalar, maxMinElement.maxScalar);
        }
      }
      m_CachedRange = limits;
    }
  }

  return m_CachedRange;
}


bool UVFDataset::ContainsData(const BrickKey &k, double isoval) const
{
  // if we have no max min data we have to assume that every block is visible
  if(NULL == m_timesteps[std::tr1::get<0>(k)].m_pMaxMinData) {return true;}

  const NDBrickKey& key = IndexToVectorKey(k);
  UINT64 iLOD = std::tr1::get<1>(k);
  const InternalMaxMinElement& maxMinElement =
    m_timesteps[key.timestep].m_vvaMaxMin[iLOD][key.brick[0]][key.brick[1]][key.brick[2]];

  return (isoval <= maxMinElement.maxScalar);
}

bool UVFDataset::ContainsData(const BrickKey &k, double fMin,double fMax) const
{
  // if we have no max min data we have to assume that every block is visible
  if(NULL == m_timesteps[std::tr1::get<0>(k)].m_pMaxMinData) {return true;}

  const NDBrickKey& key = IndexToVectorKey(k);
  UINT64 iLOD = std::tr1::get<1>(k);
  const InternalMaxMinElement& maxMinElement =
    m_timesteps[key.timestep].m_vvaMaxMin[iLOD][key.brick[0]][key.brick[1]][key.brick[2]];

  return (fMax >= maxMinElement.minScalar && fMin <= maxMinElement.maxScalar);
}

bool UVFDataset::ContainsData(const BrickKey &k, double fMin,double fMax, double fMinGradient,double fMaxGradient) const
{
  // if we have no max min data we have to assume that every block is visible
  if(NULL == m_timesteps[std::tr1::get<0>(k)].m_pMaxMinData) {return true;}

  const NDBrickKey& key = IndexToVectorKey(k);
  UINT64 iLOD = std::tr1::get<1>(k);
  const InternalMaxMinElement& maxMinElement =
    m_timesteps[key.timestep].m_vvaMaxMin[iLOD][key.brick[0]][key.brick[1]][key.brick[2]];

  return (fMax >= maxMinElement.minScalar &&
          fMin <= maxMinElement.maxScalar)
                         &&
         (fMaxGradient >= maxMinElement.minGradient &&
          fMinGradient <= maxMinElement.maxGradient);
}

const std::vector< std::pair < std::string, std::string > > UVFDataset::GetMetadata() const {
  std::vector< std::pair < std::string, std::string > > v;

  if (m_pKVDataBlock)  {
    for (size_t i = 0;i<m_pKVDataBlock->GetKeyCount();i++) {
      v.push_back(std::make_pair(m_pKVDataBlock->GetKeyByIndex(i), m_pKVDataBlock->GetValueByIndex(i)));
    }
  }

  return v;
}

bool UVFDataset::SaveRescaleFactors() {
  DOUBLEVECTOR3 saveUserScale = m_UserScale;
  Close();

  MESSAGE("Attempting to reopen file in readwrite mode.");

  if (!Open(false,true)) {
    T_ERROR("Readwrite mode failed, maybe file is write protected?");

    Open(false,false);
    return false;
  } else {
    MESSAGE("Successfully reopened file in readwrite mode.");

    std::vector<RasterDataBlock*> RW_blocks;
    for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
      RasterDataBlock* rdb =
        static_cast<RasterDataBlock*>
                   (m_pDatasetFile->GetDataBlockRW(tsi, true));

      size_t iSize = rdb->ulDomainSize.size();
      for (size_t i=0; i < 3; i++) {
        m_DomainScale[i] = saveUserScale[i];
        // matrix multiplication with scale factors
        rdb->dDomainTransformation[0+(iSize+1)*i] *= saveUserScale[0];
        rdb->dDomainTransformation[1+(iSize+1)*i] *= saveUserScale[1];
        rdb->dDomainTransformation[2+(iSize+1)*i] *= saveUserScale[2];
      }
      RW_blocks.push_back(rdb);
    }

    MESSAGE("Writing changes to disk");
    Close();
    MESSAGE("Reopening in read-only mode");
    Open(false,false);

    return true;
  }
}

}; // tuvok namespace.
