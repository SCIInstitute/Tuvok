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

#include <cstring>
#include <sstream>

#include "uvfDataset.h"

#include "RAWConverter.h"
#include "Basics/MathTools.h"
#include "Basics/SysTools.h"
#include "Controller/Controller.h"
#include "TuvokIOError.h"
#include "TuvokSizes.h"
#include "UVF/UVF.h"
#include "UVF/Histogram1DDataBlock.h"
#include "UVF/KeyValuePairDataBlock.h"
#include "UVF/Histogram2DDataBlock.h"
#include "UVF/GeometryDataBlock.h"
#include "uvfMesh.h"

using namespace boost;
using namespace std;

namespace tuvok {

UVFDataset::UVFDataset(const std::string& strFilename, uint64_t iMaxAcceptableBricksize, bool bVerify, bool bMustBeSameVersion) :
  FileBackedDataset(strFilename),
  m_bToCBlock(false),
  m_pKVDataBlock(NULL),
  m_bIsSameEndianness(true),
  m_pDatasetFile(NULL),
  m_CachedRange(make_pair(+1,-1)),
  m_iMaxAcceptableBricksize(iMaxAcceptableBricksize)
{
  Open(bVerify, false, bMustBeSameVersion);
}

UVFDataset::UVFDataset() :
  FileBackedDataset(""),
  m_bToCBlock(false),
  m_pKVDataBlock(NULL),
  m_bIsSameEndianness(true),
  m_pDatasetFile(NULL),
  m_CachedRange(make_pair(+1,-1)),
  m_iMaxAcceptableBricksize(DEFAULT_BRICKSIZE)
{
}

UVFDataset::~UVFDataset()
{
  Close();
}

bool UVFDataset::Open(bool bVerify, bool bReadWrite, bool bMustBeSameVersion)
{
  // open the file
  const std::string& fn = Filename();
  std::wstring wstrFilename(fn.begin(), fn.end());
  m_pDatasetFile = new UVF(wstrFilename);
  std::string strError;
  m_bIsOpen = m_pDatasetFile->Open(bMustBeSameVersion,bVerify,bReadWrite,&strError);
  if (!m_bIsOpen) {
    T_ERROR(strError.c_str());
    return false;
  }

  if(m_pDatasetFile->ms_ulReaderVersion !=
     m_pDatasetFile->GetGlobalHeader().ulFileVersion) {
    // bMustBeSameVersion must not be set otherwise Open would have thrown an error
    assert(!bMustBeSameVersion && "Open should have failed!");
    WARNING("Opening UVF file with a version (%u) "
            "different from this program's (%u)!",
            unsigned(m_pDatasetFile->GetGlobalHeader().ulFileVersion),
            unsigned(m_pDatasetFile->ms_ulReaderVersion));
    if (m_pDatasetFile->ms_ulReaderVersion < 4) 
      WARNING("Opening UVF file with a version"
              "older then 4 without TOCBlock support, "
              "some features may not be available.");
  }

  m_timesteps.clear();
  size_t n_timesteps = DetermineNumberOfTimesteps();
  if (n_timesteps == 0) {
    T_ERROR("No suitable volume block found in UVF file.  Check previous messages for rejected blocks.");
    Close();
    m_bIsOpen = false;
    return false;
  }

  for (size_t i = 0;i<m_timesteps.size();++i) 
    delete m_timesteps[i];
  m_timesteps.resize(n_timesteps);
  if (m_bToCBlock) {
    for (size_t i = 0;i<m_timesteps.size();++i) {
      m_timesteps[i] = new TOCTimestep();
    }
  } else {
    for (size_t i = 0;i<m_timesteps.size();++i) {
      m_timesteps[i] = new RDTimestep();
    }
  }

  // analyze the main data blocks
  FindSuitableDataBlocks();

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
  
  ComputeRange();

  // print out data statistics
  MESSAGE("  %u timesteps found in the UVF.",
          static_cast<unsigned>(n_timesteps));
  for(size_t tsi=0; tsi < n_timesteps; ++tsi) {
    ostringstream stats;
    if (m_bToCBlock) {
      const TOCTimestep* ts = (TOCTimestep*)m_timesteps[tsi];
      stats << "Timestep " << tsi << ":\n"
            << "  Dataset size: "
            << ts->GetDB()->GetLODDomainSize(0).x << " x "
            << ts->GetDB()->GetLODDomainSize(0).y << " x "
            << ts->GetDB()->GetLODDomainSize(0).z << "\n"
            << "  Brick layout of highest resolution level: "
            << ts->GetDB()->GetBrickCount(0).x << " x "
            << ts->GetDB()->GetBrickCount(0).y << " x "
            << ts->GetDB()->GetBrickCount(0).z << "\n  "
            << GetBitWidth() << " bit, "
            << GetComponentCount() << " components\n"
            << "  LOD down to "
            << ts->GetDB()->GetBrickCount(ts->GetDB()->GetLoDCount()-1).x << " x "
            << ts->GetDB()->GetBrickCount(ts->GetDB()->GetLoDCount()-1).y << " x "
            << ts->GetDB()->GetBrickCount(ts->GetDB()->GetLoDCount()-1).z 
            << " bricks found.";
    } else {
      const RDTimestep* ts = (RDTimestep*)m_timesteps[tsi];
      stats << "Timestep " << tsi << ":\n"
            << "  Dataset size: "
            << ts->m_aDomainSize[0].x << " x "
            << ts->m_aDomainSize[0].y << " x "
            << ts->m_aDomainSize[0].z << "\n"
            << "  Brick layout of highest resolution level: "
            << ts->m_vaBrickCount[0].x << " x "
            << ts->m_vaBrickCount[0].y << " x "
            << ts->m_vaBrickCount[0].z << "\n  "
            << (ts->GetDB()->bSignedElement[0][0] ?
                std::string("signed ") : std::string("unsigned "))
            << GetBitWidth() << " bit, "
            << GetComponentCount() << " components\n"
            << "  LOD down to "
            << ts->m_vaBrickCount[ts->m_vaBrickCount.size()-1].x << " x "
            << ts->m_vaBrickCount[ts->m_vaBrickCount.size()-1].y << " x "
            << ts->m_vaBrickCount[ts->m_vaBrickCount.size()-1].z
            << " bricks found.";
    }
    MESSAGE("%s", stats.str().c_str());
  }

  if (m_TriSoupBlocks.size()) {
    MESSAGE("Extracting Meshes.");
    for (vector<GeometryDataBlock*>::iterator tsb = m_TriSoupBlocks.begin();
         tsb != m_TriSoupBlocks.end();
         tsb++) {
      uvfMesh* m = new uvfMesh(**tsb);
      m_vpMeshList.push_back(m);
/*
      ostringstream stats;
      stats << "Mesh Statistics:\n"
            << "  Vertex count:"    << m->GetVertices().size() << "\n  "
            << "  Normal count:"    << m->GetNormals().size() << "\n  "
            << "  TexCoords count:" << m->GetTexCoords().size() << "\n  "
            << "  Color count:"     << m->GetColors().size() << "\n  "
            << "  Vertex Index count:"    << m->GetVertexIndices().size() << "\n  "
            << "  Normal Index count:"    << m->GetNormalIndices().size() << "\n  "
            << "  TexCoords Index count:" << m->GetTexCoordIndices().size() << "\n  "
            << "  Color Index count:"     << m->GetColorIndices().size();
      MESSAGE("%s", stats.str().c_str());*/
    }
  }

  return true;
}

void UVFDataset::Close() {
  delete m_pDatasetFile;

  for(std::vector<Timestep*>::iterator ts = m_timesteps.begin();
      ts != m_timesteps.end(); ++ts) {
    delete *ts;
    *ts = NULL;
  }
  m_timesteps.clear();

  m_TriSoupBlocks.clear();
  DeleteMeshes();

  m_pKVDataBlock= NULL;
  m_pDatasetFile= NULL;
  m_bIsOpen= false;
}

void UVFDataset::ComputeMetaData(size_t timestep) {
  if (m_bToCBlock) {
    TOCTimestep* ts = static_cast<TOCTimestep*>(m_timesteps[timestep]);
    const TOCBlock* pVolumeDataBlock = ts->GetDB();
    m_DomainScale = pVolumeDataBlock->GetScale();

    for (size_t j = 0;j<pVolumeDataBlock->GetLoDCount();j++) {
      UINT64VECTOR3 bc = pVolumeDataBlock->GetBrickCount(j);

      BrickMD bmd;
      FLOATVECTOR3 vBrickCorner(0,0,0);

      for (uint64_t x=0; x < bc.x; x++) {
        vBrickCorner.y = 0;
        for (uint64_t y=0; y < bc.y; y++) {
          vBrickCorner.z = 0;
          for (uint64_t z=0; z < bc.z; z++) {
            const UINT64VECTOR4 coords(x,y,z,j);
            const BrickKey k = BrickKey(timestep, j,
              static_cast<size_t>(z*bc.x*bc.y+y*bc.x + x));

            FLOATVECTOR3 vNormalizedDomainSize = FLOATVECTOR3(GetDomainSize(j, timestep)) * FLOATVECTOR3(pVolumeDataBlock->GetBrickAspect(coords));
            float maxVal = vNormalizedDomainSize.maxVal();
            vNormalizedDomainSize /= maxVal;

            bmd.extents  = FLOATVECTOR3(GetEffectiveBrickSize(k)) * FLOATVECTOR3(pVolumeDataBlock->GetBrickAspect(coords)) / maxVal;
            bmd.center   = FLOATVECTOR3(vBrickCorner + bmd.extents/2.0f) -
                           vNormalizedDomainSize * 0.5f;
            bmd.n_voxels = UINTVECTOR3(pVolumeDataBlock->GetBrickSize(coords));
            AddBrick(k, bmd);
            vBrickCorner.z += bmd.extents.z;
          }
          vBrickCorner.y += bmd.extents.y;
        }
        vBrickCorner.x += bmd.extents.x;
      }
    }
  } else {
    std::vector<double> vfScale;
    RDTimestep* ts = static_cast<RDTimestep*>(m_timesteps[timestep]);
    const RasterDataBlock* pVolumeDataBlock = ts->GetDB();

    size_t iSize = pVolumeDataBlock->ulDomainSize.size();

    // we require the data to be at least 3D
    assert(iSize >= 3);

    // we also assume that x,y,z are in the first 3 components and
    // we have no anisotropy (i.e. ulLODLevelCount.size=1)
    size_t iLODLevel = static_cast<size_t>(pVolumeDataBlock->ulLODLevelCount[0]);
    for (size_t i=0; i < 3 ; i++) {
      ts->m_aOverlap[i] = static_cast<uint32_t>(pVolumeDataBlock->ulBrickOverlap[i]);
      /// @todo FIXME badness -- assume domain scaling information is the
      /// same across all raster data blocks (across all timesteps)
      m_DomainScale[i] = pVolumeDataBlock->dDomainTransformation[i+(iSize+1)*i];
    }
    m_aMaxBrickSize.StoreMax(UINTVECTOR3(
      static_cast<unsigned>(pVolumeDataBlock->ulBrickSize[0]),
      static_cast<unsigned>(pVolumeDataBlock->ulBrickSize[1]),
      static_cast<unsigned>(pVolumeDataBlock->ulBrickSize[2])
    ));

    ts->m_vvaBrickSize.resize(iLODLevel);
    if (ts->m_pMaxMinData) {
      ts->m_vvaMaxMin.resize(iLODLevel);
    }

    for (size_t j = 0;j<iLODLevel;j++) {
      std::vector<uint64_t> vLOD;  vLOD.push_back(j);
      std::vector<uint64_t> vDomSize = pVolumeDataBlock->GetLODDomainSize(vLOD);
      ts->m_aDomainSize.push_back(UINT64VECTOR3(vDomSize[0], vDomSize[1], vDomSize[2]));

      std::vector<uint64_t> vBrickCount = pVolumeDataBlock->GetBrickCount(vLOD);
      ts->m_vaBrickCount.push_back(UINT64VECTOR3(vBrickCount[0], vBrickCount[1], vBrickCount[2]));

      ts->m_vvaBrickSize[j].resize(size_t(ts->m_vaBrickCount[j].x));
      if (ts->m_pMaxMinData) {
        ts->m_vvaMaxMin[j].resize(size_t(ts->m_vaBrickCount[j].x));
      }

      FLOATVECTOR3 vBrickCorner;

      FLOATVECTOR3 vNormalizedDomainSize = FLOATVECTOR3(GetDomainSize(j, timestep));
      vNormalizedDomainSize /= vNormalizedDomainSize.maxVal();

      BrickMD bmd;
      for (uint64_t x=0; x < ts->m_vaBrickCount[j].x; x++) {
        ts->m_vvaBrickSize[j][size_t(x)].resize(size_t(ts->m_vaBrickCount[j].y));
        if (ts->m_pMaxMinData) {
          ts->m_vvaMaxMin[j][size_t(x)].resize(size_t(ts->m_vaBrickCount[j].y));
        }

        vBrickCorner.y = 0;
        for (uint64_t y=0; y < ts->m_vaBrickCount[j].y; y++) {
          if (ts->m_pMaxMinData) {
            ts->m_vvaMaxMin[j][size_t(x)][size_t(y)].resize(size_t(ts->m_vaBrickCount[j].z));
          }

          vBrickCorner.z = 0;
          for (uint64_t z=0; z < ts->m_vaBrickCount[j].z; z++) {
            std::vector<uint64_t> vBrick;
            vBrick.push_back(x);
            vBrick.push_back(y);
            vBrick.push_back(z);
            std::vector<uint64_t> vBrickSize =
              pVolumeDataBlock->GetBrickSize(vLOD, vBrick);

            ts->m_vvaBrickSize[j][size_t(x)][size_t(y)].push_back(
              UINT64VECTOR3(vBrickSize[0], vBrickSize[1], vBrickSize[2]));

            const BrickKey k = BrickKey(timestep, j,
              static_cast<size_t>(z*ts->m_vaBrickCount[j].x*ts->m_vaBrickCount[j].y+
                                  y*ts->m_vaBrickCount[j].x + x));

            bmd.extents  = FLOATVECTOR3(GetEffectiveBrickSize(k)) /
                           float(GetDomainSize(j, timestep).maxVal());
            bmd.center   = FLOATVECTOR3(vBrickCorner + bmd.extents/2.0f) -
                           vNormalizedDomainSize * 0.5f;
            bmd.n_voxels = UINTVECTOR3(static_cast<unsigned>(vBrickSize[0]),
                                       static_cast<unsigned>(vBrickSize[1]),
                                       static_cast<unsigned>(vBrickSize[2]));
            AddBrick(k, bmd);
            vBrickCorner.z += bmd.extents.z;
          }
          vBrickCorner.y += bmd.extents.y;
        }
        vBrickCorner.x += bmd.extents.x;
      }
    }

    size_t iSerializedIndex = 0;
    if (ts->m_pMaxMinData) {
      for (size_t lod=0; lod < iLODLevel; lod++) {
        for (uint64_t z=0; z < ts->m_vaBrickCount[lod].z; z++) {
          for (uint64_t y=0; y < ts->m_vaBrickCount[lod].y; y++) {
            for (uint64_t x=0; x < ts->m_vaBrickCount[lod].x; x++) {
              // for four-component data we use the fourth component
              // (presumably the alpha channel); for all other data we use
              // the first component
              /// \todo we may have to change this if we add support for other
              /// kinds of multicomponent data.
              try {
                ts->m_vvaMaxMin[lod][size_t(x)][size_t(y)][size_t(z)] =
                  ts->m_pMaxMinData->GetValue(iSerializedIndex++,
                     (pVolumeDataBlock->ulElementDimensionSize[0] == 4) ? 3 : 0
                  );
              } catch(const std::length_error&) {
                InternalMaxMinElement elem;
                const std::pair<double,double> mm = std::make_pair(
                  -std::numeric_limits<double>::max(),
                   std::numeric_limits<double>::max()
                );
                elem.minScalar = elem.minGradient = mm.first;
                elem.maxScalar = elem.maxGradient = mm.second;
                ts->m_vvaMaxMin[lod][size_t(x)][size_t(y)][size_t(z)] = elem;
              }
            }
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
void UVFDataset::FixOverlap(uint64_t& v, uint64_t brickIndex, uint64_t maxindex, uint64_t overlap) const {
  if(brickIndex > 0) {
    v -= static_cast<size_t>(overlap/2.0f);
  }
  if(brickIndex < maxindex) {
    v -= static_cast<size_t>(overlap/2.0f);
  }
}

size_t UVFDataset::DetermineNumberOfTimesteps()
{
  size_t toc=0, raster=0, hist1d=0, hist2d=0, accel=0;
  bool is_color = false;
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
            if (rdb->ulElementDimensionSize[0] == 4 ||
                rdb->ulElementDimensionSize[0] == 3) {
              is_color = true;
            }
          }
        }
        break;
      case UVFTables::BS_TOC_BLOCK:
        {
          const TOCBlock *tb =
            static_cast<const TOCBlock*>
                       (m_pDatasetFile->GetDataBlock(block));
          if(VerifyTOCBlock(tb)) {
            ++toc;
            if (tb->GetComponentCount() == 4) { is_color = true; }
          }
        }
        break;
      default: break;
    }
  }
  MESSAGE("Block counts (toc, raster, hist1, hist2, accel): (%u, %u, %u, %u, %u)",
          static_cast<unsigned>(toc),
          static_cast<unsigned>(raster),
          static_cast<unsigned>(hist1d),
          static_cast<unsigned>(hist2d),
          static_cast<unsigned>(accel));

  if (toc > raster) m_bToCBlock = true;

  // color data is weird; none of our extra blocks are useful.
  if(is_color) {
    return std::max(raster,toc);
  }
  if(raster+toc == hist1d && hist1d == hist2d && hist2d == accel) {
    return std::max(raster,toc);
  }
  // if the number of blocks don't match, say we have 0 valid timesteps.
  T_ERROR("UVF Block combinations do not match; "
          "do not know how to interpret data.  "
          "Block counts (toc, raster, hist1, hist2, accel): (%u, %u, %u, %u, %u)",
          static_cast<unsigned>(toc),
          static_cast<unsigned>(raster),
          static_cast<unsigned>(hist1d),
          static_cast<unsigned>(hist2d),
          static_cast<unsigned>(accel));
  throw tuvok::io::DSParseFailed("No valid timesteps in UVF!");
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
  if (rdb->ulElementDimensionSize[0] == 1 ||
      rdb->ulElementDimensionSize[0] == 3 ||
      rdb->ulElementDimensionSize[0] == 4) {
    // check if the data's coarsest LOD level contains only one brick
    const vector<uint64_t>& vSmallestLODBrickCount = rdb->GetBrickCount(rdb->GetSmallestBrickIndex());
    uint64_t iSmallestLODBrickCount = vSmallestLODBrickCount[0];
    for (size_t i = 1;i<3;i++) iSmallestLODBrickCount *= vSmallestLODBrickCount[i]; // currently we only care about the first 3 dimensions
    if (iSmallestLODBrickCount > 1) {
      return false;
    }
  } else {
    return false;
  }
  return true;
}

bool UVFDataset::VerifyTOCBlock(const TOCBlock* tb) const
{
  /// \todo: change this if we want to support vector data
  // check if we have anything other than scalars or color
  if (tb->GetComponentCount() == 1 || tb->GetComponentCount() == 4) {
    // check if the data's coarsest LOD level contains only one brick
    // this should always be true by design of the TOC-Block but
    // we check it here in case we allow exceptionsto this in the future 
    const uint64_t vSmallestLODBrickCount = tb->GetBrickCount(tb->GetLoDCount()-1).volume();
    return vSmallestLODBrickCount == 1;
  } else {
    return false;
  }
}


// Gives the size of a brick in real space.
UINT64VECTOR3 UVFDataset::GetEffectiveBrickSize(const BrickKey &k) const
{
  if (m_bToCBlock) {
    const TOCTimestep* ts = static_cast<TOCTimestep*>(m_timesteps[std::get<0>(k)]);
    return UINT64VECTOR3(ts->GetDB()->GetBrickSize(KeyToTOCVector(k)) - 2*ts->GetDB()->GetOverlap());
  } else {
    const NDBrickKey& key = IndexToVectorKey(k);
    size_t iLOD = static_cast<size_t>(std::get<1>(k));
    const RDTimestep* ts = static_cast<RDTimestep*>(m_timesteps[key.timestep]);

    UINT64VECTOR3 vBrickSize =
      ts->m_vvaBrickSize[iLOD]
                      [static_cast<size_t>(key.brick[0])]
                      [static_cast<size_t>(key.brick[1])]
                      [static_cast<size_t>(key.brick[2])];


    // If this is an internal brick, the size is a bit smaller based on the
    // amount of overlap per-brick.
    if (ts->m_vaBrickCount[iLOD].x > 1) {
      FixOverlap(vBrickSize.x, key.brick[0], ts->m_vaBrickCount[iLOD].x-1, ts->m_aOverlap.x);
    }
    if (ts->m_vaBrickCount[iLOD].y > 1) {
      FixOverlap(vBrickSize.y, key.brick[1], ts->m_vaBrickCount[iLOD].y-1, ts->m_aOverlap.y);
    }
    if (ts->m_vaBrickCount[iLOD].z > 1) {
      FixOverlap(vBrickSize.z, key.brick[2], ts->m_vaBrickCount[iLOD].z-1, ts->m_aOverlap.z);
    }

    return vBrickSize;
  }
}

BrickTable::size_type UVFDataset::GetBrickCount(size_t lod, size_t iTs) const
{
  if (m_bToCBlock) {
    const TOCTimestep* ts = static_cast<TOCTimestep*>(m_timesteps[iTs]);
    return  BrickTable::size_type(ts->GetDB()->GetBrickCount(lod).volume());
  } else {
    const RDTimestep* ts = static_cast<RDTimestep*>(m_timesteps[iTs]);
    return BrickTable::size_type(ts->m_vaBrickCount[lod].volume());
  }
}

UINT64VECTOR3 UVFDataset::GetDomainSize(const size_t lod, const size_t iTs) const
{
  if (m_bToCBlock) {
    const TOCTimestep* ts = static_cast<TOCTimestep*>(m_timesteps[iTs]);
    return ts->GetDB()->GetLODDomainSize(lod);
  } else {
    const RDTimestep* ts = static_cast<RDTimestep*>(m_timesteps[iTs]);
    return ts->m_aDomainSize[lod];
  }
}

uint64_t UVFDataset::GetNumberOfTimesteps() const {
  return m_timesteps.size();
}

float UVFDataset::MaxGradientMagnitude() const
{
  float mx = -std::numeric_limits<float>::max();
  for(std::vector<Timestep*>::const_iterator ts = m_timesteps.begin();
      ts != m_timesteps.end(); ++ts) {
    mx = std::max(mx, (*ts)->m_fMaxGradMagnitude);
  }
  return mx;
}

void UVFDataset::FindSuitableDataBlocks() {
  // keep a count of each type of block.  We require that the number of blocks
  // match, or put another way, that all blocks exist for all timesteps.  This
  // isn't strictly necessary; we could still, technically, work with a
  // timestep that was missing acceleration structures.
  size_t data=0, hist1d=0, hist2d=0, accel=0;

  for (size_t iBlocks = 0;
       iBlocks < m_pDatasetFile->GetDataBlockCount();
       iBlocks++) {
    switch(m_pDatasetFile->GetDataBlock(iBlocks)->GetBlockSemantic()) {
      case UVFTables::BS_1D_HISTOGRAM:
        m_timesteps[hist1d++]->m_pHist1DDataBlock =
          static_cast<const Histogram1DDataBlock*>
                     (m_pDatasetFile->GetDataBlock(iBlocks));
        break;
      case UVFTables::BS_2D_HISTOGRAM:
        m_timesteps[hist2d++]->m_pHist2DDataBlock =
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
        m_timesteps[accel++]->m_pMaxMinData = (MaxMinDataBlock*)m_pDatasetFile->GetDataBlock(iBlocks);
        break;
      case UVFTables::BS_TOC_BLOCK:
        if (m_bToCBlock) {
          const TOCBlock* pVolumeDataBlock =
                                        static_cast<const TOCBlock*>
                                        (m_pDatasetFile->GetDataBlock(iBlocks));

          if(!VerifyTOCBlock(pVolumeDataBlock)) {
            WARNING("A TOCBlock failed verification; skipping it");
            continue;
          }

          m_timesteps[data]->block_number = iBlocks;
          m_timesteps[data++]->m_pVolumeDataBlock = pVolumeDataBlock;
        }
        break;
      case UVFTables::BS_REG_NDIM_GRID:
        if (!m_bToCBlock) {
          const RasterDataBlock* pVolumeDataBlock =
                             static_cast<const RasterDataBlock*>
                                        (m_pDatasetFile->GetDataBlock(iBlocks));

          if(!VerifyRasterDataBlock(pVolumeDataBlock)) {
            WARNING("A RasterDataBlock failed verification; skipping it");
            continue;
          }

          // check if the data's biggest brick dimensions are
          // smaller than m_iMaxAcceptableBricksize
          std::vector<uint64_t> vMaxBrickSizes = pVolumeDataBlock->GetLargestBrickSizes();
          for (size_t i=0; i < 3; i++) {  // currently we only care about the first 3 dimensions
            if (vMaxBrickSizes[i] > m_iMaxAcceptableBricksize) {
              std::stringstream large;
              large << "Brick size used in UVF file is too large (" << vMaxBrickSizes[i]
                    << " > " << m_iMaxAcceptableBricksize << "); rebricking necessary.";
              WARNING("%s", large.str().c_str());
              throw tuvok::io::DSBricksOversized(
                large.str().c_str(),static_cast<size_t>(m_iMaxAcceptableBricksize), _func_, __LINE__
              );
            }
          }

          m_timesteps[data]->block_number = iBlocks;
          m_timesteps[data++]->m_pVolumeDataBlock = pVolumeDataBlock;
        } 
        break;
      case UVFTables::BS_GEOMETRY: {
        MESSAGE("Found triangle mesh.");
        m_TriSoupBlocks.push_back((GeometryDataBlock*)m_pDatasetFile->GetDataBlock(iBlocks));
      }
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

  Timestep* ts = m_timesteps[0];
  if (ts->m_pHist1DDataBlock != NULL) {
    const std::vector<uint64_t>& vHist1D = ts->m_pHist1DDataBlock->GetHistogram();

    m_pHist1D = new Histogram1D(std::min<size_t>(vHist1D.size(),
                                    std::min<size_t>(MAX_TRANSFERFUNCTION_SIZE,
                                                     1<<GetBitWidth())));

    if (m_pHist1D->GetSize() != vHist1D.size()) {
      MESSAGE("1D Histogram too big to be drawn efficiently, resampling.");
      // "resample" the histogram

      float sampleFactor = static_cast<float>(vHist1D.size()) /
                           static_cast<float>(m_pHist1D->GetSize());

      float accWeight = 0.0f;
      float currWeight = 1.0f;
      float accValue = 0.0f;
      size_t j  = 0;
      bool bLast = false;

      for (size_t i = 0;i < vHist1D.size(); i++) {
        if (bLast) {
            m_pHist1D->Set(j, uint32_t( accValue ));

            currWeight = 1.0f - currWeight;
            j++;
            i--;
            bLast = false;
            accValue = 0;
            accWeight = 0;
        } else {
          if (sampleFactor-accWeight > 1) {
            currWeight = 1.0f;
          } else {
            currWeight = sampleFactor-accWeight;
            bLast = true;
          }
        }

        accValue  += static_cast<float>(vHist1D[i]) * currWeight;
        accWeight += currWeight;

        // make sure we are not writing beyond m_pHist1D's end
        // due to accumulated float errors in the sampling computation above
        if (j == m_pHist1D->GetSize() - 1) break;
      }
    } else {
      for (size_t i = 0;i < m_pHist1D->GetSize(); i++) {
        m_pHist1D->Set(i, uint32_t(vHist1D[i]));
      }
    }
  } else {
    // generate a zero 1D histogram (max 4k) if none is found in the file
    m_pHist1D = new Histogram1D(std::min(MAX_TRANSFERFUNCTION_SIZE, 1<<GetBitWidth()));

    // set all values to one so "getFilledsize" later does not return a
    // completely empty dataset
    for (size_t i = 0;i<m_pHist1D->GetSize();i++) {
      m_pHist1D->Set(i, 1);
    }
  }

  m_pHist2D = NULL;
  if (ts->m_pHist2DDataBlock != NULL) {
    const std::vector< std::vector<uint64_t> >& vHist2D =
      ts->m_pHist2DDataBlock->GetHistogram();

    VECTOR2<size_t> vSize(vHist2D.size(),vHist2D[0].size());

    vSize.x = min<size_t>(MAX_TRANSFERFUNCTION_SIZE, vSize.x);
    vSize.y = min<size_t>(256, vSize.y);
    m_pHist2D = new Histogram2D(vSize);

    if (vSize.x != vHist2D.size() || vSize.y != vHist2D[0].size() ) {
      MESSAGE("2D Histogram too big to be drawn efficiently, resampling.");

      // TODO: implement the same linear resampling as above
      //       for now we just clear the histogram with ones

      for (size_t y = 0;y<m_pHist2D->GetSize().y;y++)
        for (size_t x = 0;x<m_pHist2D->GetSize().x;x++)
          m_pHist2D->Set(x,y,1);
    } else {
      for (size_t y = 0;y<m_pHist2D->GetSize().y;y++)
        for (size_t x = 0;x<m_pHist2D->GetSize().x;x++)
          m_pHist2D->Set(x,y,uint32_t(vHist2D[x][y]));
    }

    ts->m_fMaxGradMagnitude = ts->m_pHist2DDataBlock->GetMaxGradMagnitude();
  } else {
    // generate a zero 2D histogram (max 4k) if none is found in the file
    VECTOR2<size_t> vec(256, std::min(MAX_TRANSFERFUNCTION_SIZE, 1<<GetBitWidth()));

    m_pHist2D = new Histogram2D(vec);
    for (size_t y=0; y < m_pHist2D->GetSize().y; y++) {
      // set all values to one so "getFilledsize" later does not return a
      // completely empty dataset
      for (size_t x=0; x < m_pHist2D->GetSize().x; x++) {
        m_pHist2D->Set(x,y,1);
      }
    }

    ts->m_fMaxGradMagnitude = 0;
  }
}

UINTVECTOR3 UVFDataset::GetBrickVoxelCounts(const BrickKey& k) const
{
 if (m_bToCBlock) {
    const UINT64VECTOR4 coords = KeyToTOCVector(k);
    const TOCTimestep* ts = static_cast<TOCTimestep*>(m_timesteps[std::get<0>(k)]);
    return UINTVECTOR3(ts->GetDB()->GetBrickSize(coords));
  } else {
    size_t iLOD = static_cast<size_t>(std::get<1>(k));
    const NDBrickKey& key = IndexToVectorKey(k);
    const RDTimestep* ts = static_cast<RDTimestep*>(m_timesteps[key.timestep]);
    return UINTVECTOR3(ts->m_vvaBrickSize[iLOD]
                                         [static_cast<size_t>(key.brick[0])]
                                         [static_cast<size_t>(key.brick[1])]
                                         [static_cast<size_t>(key.brick[2])]);
  }  
}



bool UVFDataset::Export(uint64_t iLODLevel, const std::string& targetFilename,
                        bool bAppend) const {
 if (m_bToCBlock) {
    bool okay = true;
    for(std::vector<Timestep*>::const_iterator ts = m_timesteps.begin();
      ts != m_timesteps.end(); ++ts) {
      const TOCTimestep* toc_ts = static_cast<TOCTimestep*>(*ts);
      // Unbrick each timestep.  Append the data if the user asks, but we must
      // always append on second and subsequent timesteps!
      okay &= toc_ts->GetDB()->BrickedLODToFlatData(
                iLODLevel, targetFilename,
                bAppend || ts != m_timesteps.begin(),
                &Controller::Debug::Out()
               );
    }
    return okay;
  } else {
    std::vector<uint64_t> vLOD; vLOD.push_back(iLODLevel);
    bool okay = true;
    for(std::vector<Timestep*>::const_iterator ts = m_timesteps.begin();
        ts != m_timesteps.end(); ++ts) {
      const RDTimestep* rd_ts = static_cast<RDTimestep*>(*ts);
      // Unbrick each timestep.  Append the data if the user asks, but we must
      // always append on second and subsequent timesteps!
      okay &= rd_ts->GetDB()->BrickedLODToFlatData(
                vLOD, targetFilename, 
                bAppend || ts != m_timesteps.begin(),
                &Controller::Debug::Out()
                );
    }
    return okay; 
  }
}

bool UVFDataset::ApplyFunction(uint64_t iLODLevel, 
                      bool (*brickFunc)(void* pData, 
                                        const UINT64VECTOR3& vBrickSize,
                                        const UINT64VECTOR3& vBrickOffset,
                                        void* pUserContext),
                      void *pUserContext,
                      uint64_t iOverlap) const {

 if (m_bToCBlock) {
    bool okay = true;
    for(std::vector<Timestep*>::const_iterator ts = m_timesteps.begin();
      ts != m_timesteps.end(); ++ts) {
      const TOCTimestep* toc_ts = static_cast<TOCTimestep*>(*ts);
      okay &= toc_ts->GetDB()->ApplyFunction(
                iLODLevel,             
                brickFunc, pUserContext, 
                uint32_t(iOverlap), &Controller::Debug::Out()
              );
    }
    return okay;
  } else {
    std::vector<uint64_t> vLOD; vLOD.push_back(iLODLevel);
    bool okay = true;
    for(std::vector<Timestep*>::const_iterator ts = m_timesteps.begin();
        ts != m_timesteps.end(); ++ts) {
      const RDTimestep* rd_ts = static_cast<RDTimestep*>(*ts);
      okay &= rd_ts->GetDB()->ApplyFunction(
                vLOD, brickFunc, pUserContext, 
                iOverlap, &Controller::Debug::Out()
                ); 
    }
    return okay;  
  }
}

// BrickKey's index is 1D. For UVF's RDB, we've got a 3D index.  When we create the
// brick index to satisfy the interface, we do so in a reversible way.  This
// methods reverses the 1D into into UVF's 3D index.
std::vector<uint64_t>
UVFDataset::IndexToVector(const BrickKey &k) const {
  std::vector<uint64_t> vBrick;
  if (!m_bToCBlock) {
    const RDTimestep* ts = static_cast<RDTimestep*>(m_timesteps[std::get<0>(k)]);
    const size_t lod = std::get<1>(k);

    uint64_t iIndex = uint64_t(std::get<2>(k));
    uint64_t iZIndex = uint64_t(iIndex / (ts->m_vaBrickCount[lod].x *
                                      ts->m_vaBrickCount[lod].y));
    iIndex = iIndex % (ts->m_vaBrickCount[lod].x *
                       ts->m_vaBrickCount[lod].y);
    uint64_t iYIndex = uint64_t(iIndex / ts->m_vaBrickCount[lod].x);
    iIndex = iIndex % ts->m_vaBrickCount[lod].x;
    uint64_t iXIndex = iIndex;

    vBrick.push_back(iXIndex);
    vBrick.push_back(iYIndex);
    vBrick.push_back(iZIndex);
  }
  return vBrick;
}

UINT64VECTOR4 UVFDataset::KeyToTOCVector(const BrickKey &k) const {
  if (m_bToCBlock) {
    const TOCTimestep* ts = static_cast<TOCTimestep*>(m_timesteps[std::get<0>(k)]);

    uint64_t iLOD = std::get<1>(k);
    uint64_t iLinearIndex = std::get<2>(k);
    UINT64VECTOR3 iBricks = ts->GetDB()->GetBrickCount(iLOD);

    uint64_t x = iLinearIndex % iBricks.x;
    uint64_t y = (iLinearIndex % (iBricks.x*iBricks.y)) / iBricks.x;
    uint64_t z = iLinearIndex / (iBricks.x*iBricks.y); 

    return UINT64VECTOR4(x,y,z,iLOD);
  } else {
    return UINT64VECTOR4();
  }

}

NDBrickKey UVFDataset::IndexToVectorKey(const BrickKey &k) const {
  NDBrickKey ndk;
  ndk.timestep = std::get<0>(k);
  ndk.lod.push_back(std::get<1>(k));
  ndk.brick = IndexToVector(k);
  return ndk;
}

// determines the largest actually used brick dimensions
// in the current dataset
UINT64VECTOR3 UVFDataset::GetMaxUsedBrickSizes() const
{
  UINT64VECTOR3 vMaxSize(1,1,1);
  UINT64VECTOR3 vAbsoluteMax(m_aMaxBrickSize);
  if (m_bToCBlock) {
    for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
      const TOCTimestep* ts = (TOCTimestep*)m_timesteps[tsi];

      // the first brick in the highest LoD is the biggest brick (usually of size vAbsoluteMax)
      vMaxSize.StoreMax(UINT64VECTOR3(ts->GetDB()->GetBrickSize(UINT64VECTOR4(0,0,0,0))));

      if (vMaxSize == vAbsoluteMax) return vAbsoluteMax;
    }
    return vMaxSize;
  } else {
    UINT64VECTOR3 vAbsoluteMax(m_aMaxBrickSize);

    for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
      const RDTimestep* ts = (RDTimestep*)m_timesteps[tsi];

      for (size_t iLOD=0;iLOD < ts->m_vvaBrickSize.size(); iLOD++) {
        for (size_t iX=0; iX < ts->m_vvaBrickSize[iLOD].size(); iX++) {
          for (size_t iY=0; iY < ts->m_vvaBrickSize[iLOD][iX].size(); iY++) {
            for (size_t iZ=0; iZ < ts->m_vvaBrickSize[iLOD][iX][iY].size(); iZ++) {
              vMaxSize.StoreMax(ts->m_vvaBrickSize[iLOD][iX][iY][iZ]);

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

  if (m_bToCBlock) {
    uint32_t overlap = ((TOCTimestep*)(m_timesteps[0]))->GetDB()->GetOverlap();
    return UINTVECTOR3(overlap,overlap,overlap);
  } else
    return ((RDTimestep*)(m_timesteps[0]))->m_aOverlap;
}

uint64_t UVFDataset::GetLODLevelCount() const
{
  /// @todo HACK fixme: should take into account the timestep, needed as arg
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");

  if (m_bToCBlock)
    return ((TOCTimestep*)(m_timesteps[0]))->GetDB()->GetLoDCount();
  else
    return ((RDTimestep*)(m_timesteps[0]))->m_vvaBrickSize.size();
}

/// \todo change this if we want to support data where elements are of
// different size
uint64_t UVFDataset::GetBitWidth() const
{
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  // All data in the time series should have the same bit width, so any
  // timestep we choose to query the bit width from should be fine.

  if (m_bToCBlock)
    return ((TOCTimestep*)(m_timesteps[0]))->GetDB()->GetComponentTypeSize()*8;
  else
    return ((RDTimestep*)(m_timesteps[0]))->GetDB()->ulElementBitSize[0][0];

}

uint64_t UVFDataset::GetComponentCount() const
{
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  // All data in the time series should have the same bit number of
  // components, so any timestep we choose to query the component count
  // from should be fine.

  if (m_bToCBlock)
    return ((TOCTimestep*)(m_timesteps[0]))->GetDB()->GetComponentCount();
  else
    return ((RDTimestep*)(m_timesteps[0]))->GetDB()->ulElementDimensionSize[0];
}

/// \todo change this if we want to support data where elements are of
/// different type
bool UVFDataset::GetIsSigned() const
{
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  // All data in the time series should have the same signedness,
  // so any timestep we choose to query the signedness from should be fine.

  if (m_bToCBlock)
    return ((TOCTimestep*)(m_timesteps[0]))->GetDB()->GetIsSigned();
  else
    return ((RDTimestep*)(m_timesteps[0]))->GetDB()->bSignedElement[0][0];
}

/// \todo change this if we want to support data where elements are of
///  different type
bool UVFDataset::GetIsFloat() const
{
  assert(!m_timesteps.empty() && "no data, haven't analyzed UVF?");
  // All data in the time series should have the same type, so any timestep
  // we choose to query the type from should be fine.
  if (m_bToCBlock)
    return ((TOCTimestep*)(m_timesteps[0]))->GetDB()->GetIsFloat();
  else
    return GetBitWidth() != ((RDTimestep*)(m_timesteps[0]))->GetDB()->ulElementMantissa[0][0];
}

bool UVFDataset::IsSameEndianness() const
{
  return m_bIsSameEndianness;
}

std::pair<double,double> UVFDataset::GetRange() const { 
  return m_CachedRange;
}

void UVFDataset::ComputeRange() {

  // If we're missing MaxMin data for any timestep, we don't have maxmin data.
  bool have_maxmin_data = true;
  for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
    if(m_timesteps[tsi]->m_pMaxMinData == NULL) {
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

    if (m_bToCBlock) {
      for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
        for (size_t i=0; i < GetBrickCount(0, tsi); i++) {
          const TOCTimestep* ts = static_cast<TOCTimestep*>(m_timesteps[tsi]);
          const InternalMaxMinElement& maxMinElement = ts->m_pMaxMinData->GetValue(i, ts->GetDB()->GetComponentCount() == 4 ? 3 : 0);

          if (i>0) {
            limits.first  = min(limits.first, maxMinElement.minScalar);
            limits.second = max(limits.second, maxMinElement.maxScalar);
          } else {
            limits = make_pair(maxMinElement.minScalar, maxMinElement.maxScalar);
          }
        }
      }
      m_CachedRange = limits;

    } else {
      for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
        const RDTimestep* ts = static_cast<RDTimestep*>(m_timesteps[tsi]);

        for (size_t i=0; i < GetBrickCount(0, tsi); i++) {
          BrickKey k(tsi,0,i);
          const NDBrickKey& key = IndexToVectorKey(k);

          const InternalMaxMinElement& maxMinElement =
            ts->m_vvaMaxMin[0]
                          [static_cast<size_t>(key.brick[0])]
                          [static_cast<size_t>(key.brick[1])]
                          [static_cast<size_t>(key.brick[2])];

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
  }
}

InternalMaxMinElement UVFDataset::MaxMinForKey(const BrickKey &k) const {
  InternalMaxMinElement maxMinElement;
  if (m_bToCBlock) {
    const TOCTimestep* ts = static_cast<TOCTimestep*>(m_timesteps[std::get<0>(k)]);
    size_t iLinIndex = size_t(ts->GetDB()->GetLinearBrickIndex(KeyToTOCVector(k)));
    return  ts->m_pMaxMinData->GetValue(iLinIndex, ts->GetDB()->GetComponentCount() == 4 ? 3 : 0 );
  } else {

    const NDBrickKey& key = IndexToVectorKey(k);
    size_t iLOD = static_cast<size_t>(std::get<1>(k));
    
    const RDTimestep* ts = static_cast<RDTimestep*>(m_timesteps[key.timestep]);
    return ts->m_vvaMaxMin[iLOD]
                          [static_cast<size_t>(key.brick[0])]
                          [static_cast<size_t>(key.brick[1])]
                          [static_cast<size_t>(key.brick[2])];
  }
}

bool UVFDataset::ContainsData(const BrickKey &k, double isoval) const
{
  // if we have no max min data we have to assume that every block is visible
  if(NULL == m_timesteps[std::get<0>(k)]->m_pMaxMinData) {return true;}
  const InternalMaxMinElement maxMinElement = MaxMinForKey(k);
  return (isoval <= maxMinElement.maxScalar);
}

bool UVFDataset::ContainsData(const BrickKey &k, double fMin,double fMax) const
{
  // if we have no max min data we have to assume that every block is visible
  if(NULL == m_timesteps[std::get<0>(k)]->m_pMaxMinData) {return true;}
  const InternalMaxMinElement maxMinElement = MaxMinForKey(k);
  return (fMax >= maxMinElement.minScalar && fMin <= maxMinElement.maxScalar);
}

bool UVFDataset::ContainsData(const BrickKey &k, double fMin,double fMax, double fMinGradient,double fMaxGradient) const
{
  // if we have no max min data we have to assume that every block is visible
  if(NULL == m_timesteps[std::get<0>(k)]->m_pMaxMinData) {return true;}
  const InternalMaxMinElement maxMinElement = MaxMinForKey(k);
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

bool UVFDataset::GeometryTransformToFile(size_t iMeshIndex, const FLOATMATRIX4& m) {
  Close();

  MESSAGE("Attempting to reopen file in readwrite mode.");

  if (!Open(false,true,false)) {
    T_ERROR("Readwrite mode failed, maybe file is write protected?");

    Open(false,false,false);
    return false;
  } else {
    MESSAGE("Successfully reopened file in readwrite mode.");

    // turn meshindex into block index, those are different as the
    // uvf file most likely also contains data other than meshes
    // such as the volume or histograms, etc.
    size_t iBlockIndex = 0;
    bool bFound = false;
        
    for(size_t block=0; block < m_pDatasetFile->GetDataBlockCount(); ++block) {
      if (m_pDatasetFile->GetDataBlock(block)->GetBlockSemantic() 
                                                  == UVFTables::BS_GEOMETRY) {        
        if (iMeshIndex == 0) {
          iBlockIndex = block;
          bFound = true;
          break;
        }          
        iMeshIndex--;
      }
    }

    if (!bFound) {
      T_ERROR("Unable to locate mesh data block %u", static_cast<unsigned>(iBlockIndex));
      return false;
    }

    GeometryDataBlock* block = dynamic_cast<GeometryDataBlock*>(m_pDatasetFile->GetDataBlockRW(iBlockIndex,false));
    if (!block) {
      T_ERROR("Inconsistent UVF block at index %u", static_cast<unsigned>(iBlockIndex));
      return false;
    }

    MESSAGE("Transforming Vertices ...");
    std::vector< float > vertices  = block->GetVertices();
    if (vertices.size() % 3) {
      T_ERROR("Inconsistent data vertex in UVF block at index %u", static_cast<unsigned>(iBlockIndex));
      return false;
    }  
    for (size_t i = 0;i<vertices.size();i+=3) {
      FLOATVECTOR3 v = (FLOATVECTOR4(vertices[i+0],vertices[i+1],vertices[i+2],1)*m).xyz();
      vertices[i+0] = v.x;
      vertices[i+1] = v.y;
      vertices[i+2] = v.z;
    }
    block->SetVertices(vertices);

    MESSAGE("Transforming Normals ...");
    FLOATMATRIX4 invTranspose(m);
    invTranspose = invTranspose.inverse();
    invTranspose = invTranspose.Transpose();

    std::vector< float > normals  = block->GetNormals();
    if (normals.size() % 3) {
      T_ERROR("Inconsistent normal data in UVF block at index %u", static_cast<unsigned>(iBlockIndex));
      return false;
    }  
    for (size_t i = 0;i<normals.size();i+=3) {
      FLOATVECTOR3 n = (FLOATVECTOR4(normals[i+0],normals[i+1],normals[i+2],0)*invTranspose).xyz();
      n.normalize();
      normals[i+0] = n.x;
      normals[i+1] = n.y;
      normals[i+2] = n.z;
    }
    block->SetNormals(normals);

    MESSAGE("Writing changes to disk");
    Close();
    MESSAGE("Reopening in read-only mode");
    
    Open(false,false,false);
    return true;
  }
}

bool UVFDataset::RemoveMesh(size_t iMeshIndex) {
  Close();

  MESSAGE("Attempting to reopen file in readwrite mode.");

  if (!Open(false,true,false)) {
    T_ERROR("Readwrite mode failed, maybe file is write protected?");

    Open(false,false,false);
    return false;
  } else {
    MESSAGE("Successfully reopened file in readwrite mode.");

    // turn meshindex into block index, those are different as the
    // uvf file most likely also contains data other than meshes
    // such as the volume or histograms, etc.
    size_t iBlockIndex = 0;
    bool bFound = false;
        
    for(size_t block=0; block < m_pDatasetFile->GetDataBlockCount(); ++block) {
      if (m_pDatasetFile->GetDataBlock(block)->GetBlockSemantic() 
                                                  == UVFTables::BS_GEOMETRY) {        
        if (iMeshIndex == 0) {
          iBlockIndex = block;
          bFound = true;
          break;
        }          
        iMeshIndex--;
      }
    }

    if (!bFound) {
      T_ERROR("Unable to locate mesh data block %u", static_cast<unsigned>(iMeshIndex));
      return false;
    }

    bool bResult = m_pDatasetFile->DropBlockFromFile(iBlockIndex);

    MESSAGE("Writing changes to disk");
    Close();
    MESSAGE("Reopening in read-only mode");
    
    Open(false,false,false);
    return bResult;
  }
}

bool UVFDataset::AppendMesh(const Mesh& m) {
  Close();

  MESSAGE("Attempting to reopen file in readwrite mode.");

  if (!Open(false,true,false)) {
    T_ERROR("Readwrite mode failed, maybe file is write protected?");

    Open(false,false,false);
    return false;
  } else {
    MESSAGE("Successfully reopened file in readwrite mode.");

    // now create a GeometryDataBlock ...
    std::shared_ptr<GeometryDataBlock> tsb(new GeometryDataBlock());

    // ... and transfer the data from the mesh object
    // source data
    const VertVec&      v = m.GetVertices();
    const NormVec&      n = m.GetNormals();
    const TexCoordVec&  t = m.GetTexCoords();
    const ColorVec&     c = m.GetColors();

    // target data
    vector<float> fVec;
    size_t iVerticesPerPoly = m.GetVerticesPerPoly();
    tsb->SetPolySize(iVerticesPerPoly);

    if (v.size()) {
      fVec.resize(v.size()*3);
      memcpy(&fVec[0],&v[0],v.size()*3*sizeof(float));
      tsb->SetVertices(fVec);
    } else {
      // even if the vectors are empty still let the datablock know
      tsb->SetVertices(vector<float>()); 
    }

    if (n.size()) {
      fVec.resize(n.size()*3);
      memcpy(&fVec[0],&n[0],n.size()*3*sizeof(float));
      tsb->SetNormals(fVec);
    } else {
      // even if the vectors are empty still let the datablock know
      tsb->SetNormals(vector<float>());
    }
    if (t.size()) {
      fVec.resize(t.size()*2);
      memcpy(&fVec[0],&t[0],t.size()*2*sizeof(float));
      tsb->SetTexCoords(fVec);
    } else {
      // even if the vectors are empty still let the datablock know
      tsb->SetTexCoords(vector<float>());
    }
    if (c.size()) {
      fVec.resize(c.size()*4);
      memcpy(&fVec[0],&c[0],c.size()*4*sizeof(float));
      tsb->SetColors(fVec);
    } else {
      // even if the vectors are empty still let the datablock know
      tsb->SetColors(vector<float>());
    }

    tsb->SetVertexIndices(m.GetVertexIndices());
    tsb->SetNormalIndices(m.GetNormalIndices());
    tsb->SetTexCoordIndices(m.GetTexCoordIndices());
    tsb->SetColorIndices(m.GetColorIndices());

    tsb->m_Desc = m.Name();
    
    m_pDatasetFile->AppendBlockToFile(tsb);

    MESSAGE("Writing changes to disk");
    Close();
    MESSAGE("Reopening in read-only mode");
    
    Open(false,false,false);
    return true;
  }
}



bool UVFDataset::Crop( const PLANE<float>& plane, const std::string& strTempDir, bool bKeepOldData)
{
  MESSAGE("Flattening dataset");
  string strTempRawFilename = SysTools::FindNextSequenceName(strTempDir + "crop-tmp.raw");
  Export(0, strTempRawFilename , false);

  MESSAGE("Cropping at plane (%g %g %g %g)", plane.x, plane.y, plane.z, plane.w);

  FLOATMATRIX4 m;
  m.Scaling(FLOATVECTOR3(GetScale()/GetScale().maxVal()) * FLOATVECTOR3(GetDomainSize()) /float(GetDomainSize().maxVal()) );
  PLANE<float> scaleInvariantPlane = plane;
  scaleInvariantPlane.transformIT(m);

  LargeRAWFile dataFile(strTempRawFilename);
  if (!dataFile.Open(true)) {
    T_ERROR("Unable to open flattened data.");
    if(remove(strTempRawFilename.c_str()) == -1) WARNING("Unable to delete temp file %s", strTempRawFilename.c_str());
    return false;
  }
  
  // crop data
  bool bCroppingOK = false;
  switch (GetBitWidth()) {
    case 8:
      if(GetIsSigned()) {
        bCroppingOK = CropData<int8_t>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
      } else {
        bCroppingOK = CropData<uint8_t>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
      }
      break;
    case 16 :
      if(GetIsSigned()) {
        bCroppingOK = CropData<int16_t>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
      } else {
        bCroppingOK = CropData<uint16_t>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
      }
      break;
    case 32 :
      if (GetIsFloat()) {
        bCroppingOK = CropData<float>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
      } else {
        if(GetIsSigned()) {
          bCroppingOK = CropData<int32_t>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
        } else {
          bCroppingOK = CropData<uint32_t>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
        }
      }
      break;
    case 64 :
      if (GetIsFloat()) {
        bCroppingOK = CropData<double>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
      } else {
        if(GetIsSigned()) {
          bCroppingOK = CropData<int64_t>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
        } else {
          bCroppingOK = CropData<uint64_t>(dataFile, scaleInvariantPlane, GetDomainSize(), GetComponentCount());
        }
      }
      break;
  }

  if (!bCroppingOK)  {
    if(remove(strTempRawFilename.c_str()) == -1) WARNING("Unable to delete temp file %s", strTempRawFilename.c_str());
    return false;
  }


  // TODO shrink volume to the largest non-zero aabb


  dataFile.Close();

  MESSAGE("Rebuilding UVF data");
  string strTempFilename = SysTools::FindNextSequenceName(Filename());

  std::string strDesc = std::string("Cropped ") + std::string(Name());
  std::string strSource = SysTools::GetFilename(Filename());

  if (!RAWConverter::ConvertRAWDataset(strTempRawFilename, strTempFilename, strTempDir, 0,
    GetBitWidth(), size_t(GetComponentCount()), 1, !IsSameEndianness(), GetIsSigned(), GetIsFloat(),
    GetDomainSize(), FLOATVECTOR3(GetScale()), strDesc, strSource, 
    Controller::Instance().IOMan()->GetMaxBrickSize(), 
    Controller::Instance().IOMan()->GetBrickOverlap())) {
    T_ERROR("Unable to convert cropped data back to UVF");
    if(remove(strTempRawFilename.c_str()) == -1) WARNING("Unable to delete temp file %s", strTempRawFilename.c_str());
    return false;
  }
  if(remove(strTempRawFilename.c_str()) == -1) WARNING("Unable to delete temp file %s", strTempRawFilename.c_str());

  MESSAGE("Replacing original UVF by the new one");
  Close();
  
  if (bKeepOldData) {
    std::string newFilename = SysTools::AppendFilename(Filename(),"-beforeCropping");
    if (SysTools::FileExists(newFilename)) newFilename = SysTools::FindNextSequenceName(newFilename);
    rename(Filename().c_str(),newFilename.c_str());
  } else {
    remove(Filename().c_str());
  }


  if (SysTools::FileExists(Filename())) {
    T_ERROR("Unable to delete original UVF file, a new file (%s) has be created alongside the old.", strTempFilename.c_str());
    Open(false,false,false);
    return false;
  }

  rename(strTempFilename.c_str(), Filename().c_str());

  MESSAGE("Opening new file");
  Open(false,false,false);

  return true;
}


bool UVFDataset::SaveRescaleFactors() {
  DOUBLEVECTOR3 saveUserScale = m_UserScale;
  Close();

  MESSAGE("Attempting to reopen file in readwrite mode.");

  if (!Open(false,true,false)) {
    T_ERROR("Readwrite mode failed, maybe file is write protected?");

    Open(false,false,false);
    return false;
  } else {
    MESSAGE("Successfully reopened file in readwrite mode.");
    
    if (m_bToCBlock) {
      for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
        TOCBlock* tocb =
          static_cast<TOCBlock*>(
            m_pDatasetFile->GetDataBlockRW(m_timesteps[tsi]->block_number, true)
          );
        tocb->SetScale(saveUserScale);
      }
    } else {
      for(size_t tsi=0; tsi < m_timesteps.size(); ++tsi) {
        RasterDataBlock* rdb =
          static_cast<RasterDataBlock*>(
            m_pDatasetFile->GetDataBlockRW(m_timesteps[tsi]->block_number, true)
          );

        size_t iSize = rdb->ulDomainSize.size();
        for (size_t i=0; i < 3; i++) {
          m_DomainScale[i] = saveUserScale[i];
          // matrix multiplication with scale factors
          rdb->dDomainTransformation[0+(iSize+1)*i] *= saveUserScale[0];
          rdb->dDomainTransformation[1+(iSize+1)*i] *= saveUserScale[1];
          rdb->dDomainTransformation[2+(iSize+1)*i] *= saveUserScale[2];
        }
      }
    }

    MESSAGE("Writing changes to disk");
    Close();
    MESSAGE("Reopening in read-only mode");
    Open(false,false,false);

    return true;
  }
}

bool UVFDataset::CanRead(const std::string&,
                         const std::vector<int8_t>& bytes) const
{
  return bytes[0] == 0x55 && // 'U'
         bytes[1] == 0x56 && // 'V'
         bytes[2] == 0x46 && // 'F'
         bytes[3] == 0x2D && // '-'
         bytes[4] == 0x44 && // 'D'
         bytes[5] == 0x41 && // 'A'
         bytes[6] == 0x54 && // 'T'
         bytes[7] == 0x41;   // 'A'
}

bool UVFDataset::Verify(const std::string& filename) const
{
  std::wstring wstrFilename(filename.begin(), filename.end());
  bool checksumFail=false;
  UVF::IsUVFFile(wstrFilename, checksumFail);
  // negate it; IsUVFFile sets the argument if the checksum *fails*!
  return !checksumFail;
}

FileBackedDataset* UVFDataset::Create(const std::string& filename,
                                      uint64_t max_brick_size, bool verify) const
{
  return new UVFDataset(filename, max_brick_size, verify, false);
}

std::list<std::string> UVFDataset::Extensions() const
{
  std::list<std::string> retval;
  retval.push_back("UVF");
  return retval;
}

bool UVFDataset::GetBrick(const BrickKey& k, std::vector<uint8_t>& vData) const {
  return GetBrickTemplate<uint8_t>(k,vData);
}

bool UVFDataset::GetBrick(const BrickKey& k, std::vector<int8_t>& vData) const {
  return GetBrickTemplate<int8_t>(k,vData);
}

bool UVFDataset::GetBrick(const BrickKey& k, std::vector<uint16_t>& vData) const {
  return GetBrickTemplate<uint16_t>(k,vData);
}

bool UVFDataset::GetBrick(const BrickKey& k, std::vector<int16_t>& vData) const {
  return GetBrickTemplate<int16_t>(k,vData);
}

bool UVFDataset::GetBrick(const BrickKey& k, std::vector<uint32_t>& vData) const {
  return GetBrickTemplate<uint32_t>(k,vData);
}

bool UVFDataset::GetBrick(const BrickKey& k, std::vector<int32_t>& vData) const {
  return GetBrickTemplate<int32_t>(k,vData);
}

bool UVFDataset::GetBrick(const BrickKey& k, std::vector<float>& vData) const {
  return GetBrickTemplate<float>(k,vData);
}

bool UVFDataset::GetBrick(const BrickKey& k, std::vector<double>& vData) const {
  return GetBrickTemplate<double>(k,vData);
}

std::pair<FLOATVECTOR3, FLOATVECTOR3> UVFDataset::GetTextCoords(BrickTable::const_iterator brick, bool bUseOnlyPowerOfTwo) const {

  if (m_bToCBlock) {
    const UINT64VECTOR4 coords = KeyToTOCVector(brick->first);
    const TOCBlock* tb = static_cast<TOCTimestep*>(m_timesteps[std::get<0>(brick->first)])->GetDB();

    uint32_t iOverlap = tb->GetOverlap();
    FLOATVECTOR3 brickAspect = FLOATVECTOR3(tb->GetBrickAspect(coords));
    FLOATVECTOR3 vTexcoordsMin;
    FLOATVECTOR3 vTexcoordsMax;

    if (bUseOnlyPowerOfTwo) {
      UINTVECTOR3 vRealVoxelCount(MathTools::NextPow2(brick->second.n_voxels.x),
        MathTools::NextPow2(brick->second.n_voxels.y),
        MathTools::NextPow2(brick->second.n_voxels.z)
        );
      vTexcoordsMin = float(iOverlap) / FLOATVECTOR3(vRealVoxelCount);
      vTexcoordsMax = 1.0f-vTexcoordsMin;
      vTexcoordsMax -= FLOATVECTOR3(vRealVoxelCount - brick->second.n_voxels) / FLOATVECTOR3(vRealVoxelCount);
    } else {
      vTexcoordsMin = float(iOverlap) / FLOATVECTOR3(brick->second.n_voxels);
      vTexcoordsMax = 1.0f-vTexcoordsMin;
    }

    return std::make_pair(vTexcoordsMin, vTexcoordsMax*brickAspect);
   
  } else {
    return Dataset::GetTextCoords(brick, bUseOnlyPowerOfTwo);
  }
}

}; // tuvok namespace.
