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

// for find_if
#include <algorithm>
#include <memory>
#include <map>
#include <unordered_map>
#include "Basics/MathTools.h"
#include "Basics/nonstd.h"
#include "DebugOut/AbstrDebugOut.h"
#include "ExtendedOctreeConverter.h"
#include "zlib-compression.h"
#include "Basics/ProgressTimer.h"

// simple/generic progress update message
#define PROGRESS \
  do { \
    m_Progress.Message(_func_, "Generating Hierarchy ... %5.2f%% (%s)", \
      m_fProgress * 100.0f,\
      m_pProgressTimer->GetProgressMessage(m_fProgress).c_str() \
    );\
  } while(0)

#include "ExtendedOctreeConverter.inc"

ExtendedOctreeConverter::ExtendedOctreeConverter(
                          const UINT64VECTOR3& vBrickSize,
                          uint32_t iOverlap, uint64_t iMemLimit,
                          AbstrDebugOut& progress) :
    m_fProgress(0.0f),
    m_pProgressTimer(new ProgressTimer()),
    m_vBrickSize(vBrickSize),
    m_iOverlap(iOverlap),
    m_iMemLimit(iMemLimit),
    m_iCacheAccessCounter(0),
    m_pBrickStatVec(NULL),
    m_Progress(progress)
{
  m_pProgressTimer->Start();
}

ExtendedOctreeConverter::~ExtendedOctreeConverter() {
  delete m_pProgressTimer;
}

/*
  Convert (string):

  Convenience function that calls the "Convert" below with a large raw file
  constructed from the given string
*/
bool ExtendedOctreeConverter::Convert(const std::string& filename,
              uint64_t iOffset,
              ExtendedOctree::COMPONENT_TYPE eComponentType,
              uint64_t iComponentCount,
              const UINT64VECTOR3& vVolumeSize,
              const DOUBLEVECTOR3& vVolumeAspect,
              const std::string& targetFilename,
              uint64_t iOutOffset,
              BrickStatVec* stats,
              COMPRESSION_TYPE compression,
              bool bComputeMedian,
              bool bClampToEdge,
              LAYOUT_TYPE layout) {
  LargeRAWFile_ptr inFile(new LargeRAWFile(filename));
  LargeRAWFile_ptr outFile(new LargeRAWFile(targetFilename));

  if (!inFile->Open()) {
    return false;
  }
  if (!outFile->Create()) {
    inFile->Close();
    return false;
  }

  return Convert(inFile, iOffset, eComponentType, iComponentCount, vVolumeSize,
                 vVolumeAspect, outFile, iOutOffset, stats, compression,
                 bComputeMedian, bClampToEdge, layout);
}

/*
  Convert:

  This is the main function where the conversion from a raw file to
  a bricked octree happens. It starts by creating a new ExtendedOctree
  object, passes the user's parameters on to that object and computes
  the header data. It creates LoD zero by permuting/bricking the input
  data, then it computes the hierarchy. As this hierarchy computation
  involves averaging we choose an appropriate template at this point.
  Next, the function writes the header to disk and flushes the
  write cache. Finally, the file is truncated to the appropriate length
  as it may have grown bigger than necessary as compression is applied
  only after an entire level is completed.
*/
bool ExtendedOctreeConverter::Convert(LargeRAWFile_ptr pLargeRAWFileIn,
                      uint64_t iInOffset,
                      ExtendedOctree::COMPONENT_TYPE eComponentType,
                      const uint64_t iComponentCount,
                      const UINT64VECTOR3& vVolumeSize,
                      const DOUBLEVECTOR3& vVolumeAspect,
                      LargeRAWFile_ptr pLargeRAWFileOut,
                      uint64_t iOutOffset,
                      BrickStatVec* stats,
                      COMPRESSION_TYPE compression,
                      bool bComputeMedian,
                      bool bClampToEdge,
                      LAYOUT_TYPE layout) {
  m_pBrickStatVec = stats;
  m_fProgress = 0.0f;
  PROGRESS;

  ExtendedOctree e;

  // compute metadata
  e.m_eComponentType = eComponentType;
  e.m_iComponentCount = iComponentCount;
  e.m_vVolumeSize = vVolumeSize;
  e.m_vVolumeAspect = vVolumeAspect;
  e.m_iBrickSize = m_vBrickSize;
  e.m_iOverlap = m_iOverlap;
  e.m_iOffset = iOutOffset;
  e.m_pLargeRAWFile = pLargeRAWFileOut;
  e.ComputeMetadata();
  m_fProgress = 0.0f;

  m_eCompression = compression;
  m_eLayout = layout;

  SetupCache(e);

  // brick (permute) the input data
  PermuteInputData(e, pLargeRAWFileIn, iInOffset, bClampToEdge);
  m_fProgress = 0.4f;

  // compute hierarchy

  // now comes the really nasty part where we convert the input arguments
  // to template parameters, effectively writing out all branches

  if (bComputeMedian)  {
    switch (e.m_eComponentType) {
      case ExtendedOctree::CT_UINT8:
        ComputeHierarchy<uint8_t, true>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_UINT16:
        ComputeHierarchy<uint16_t, true>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_UINT32:
        ComputeHierarchy<uint32_t, true>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_UINT64:
        ComputeHierarchy<uint64_t, true>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_INT8:
        ComputeHierarchy<int8_t, true>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_INT16:
        ComputeHierarchy<int16_t, true>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_INT32:
        ComputeHierarchy<int32_t, true>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_INT64:
        ComputeHierarchy<int64_t, true>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_FLOAT32:
        ComputeHierarchy<float, true>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_FLOAT64:
        ComputeHierarchy<double, true>(e, bClampToEdge);
        break;
    }
  } else {
    switch (e.m_eComponentType) {
      case ExtendedOctree::CT_UINT8:
        ComputeHierarchy<uint8_t, false>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_UINT16:
        ComputeHierarchy<uint16_t, false>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_UINT32:
        ComputeHierarchy<uint32_t, false>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_UINT64:
        ComputeHierarchy<uint64_t, false>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_INT8:
        ComputeHierarchy<int8_t, false>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_INT16:
        ComputeHierarchy<int16_t, false>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_INT32:
        ComputeHierarchy<int32_t, false>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_INT64:
        ComputeHierarchy<int64_t, false>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_FLOAT32:
        ComputeHierarchy<float, false>(e, bClampToEdge);
        break;
      case ExtendedOctree::CT_FLOAT64:
        ComputeHierarchy<double, false>(e, bClampToEdge);
        break;
    }
  }
  // write bricks in the cache to disk
  FlushCache(e);
  {
    // we store the total octree size including header length
    // we know that the last brick in ToC must be at the end of the octree data
    // this assumption won't be valid anymore after permuting the brick ordering
    // that's why we store the total length explicitly
    TOCEntry const& lastBrickInFile = e.m_vTOC.back();
    e.m_iSize = lastBrickInFile.m_iOffset + lastBrickInFile.m_iLength;
  }

  if (m_eLayout >= LT_UNKNOWN) {
    m_Progress.Warning(_func_, "Unknown brick layout requested (%d), resetting "
                       "to default scanline order", m_eLayout);
    m_eLayout = LT_SCANLINE;
  }

  if (m_eLayout != LT_SCANLINE)
    ComputeStatsCompressAndPermuteAll(e);
  else
    ComputeStatsAndCompressAll(e);

  // add header to file
  e.WriteHeader(pLargeRAWFileOut, iOutOffset);
  PROGRESS;

  // remove part of the file used only for temp calculations
  pLargeRAWFileOut->Truncate(iOutOffset + e.m_iSize);

  m_fProgress = 1.0f;
  PROGRESS;

  return true;
}

/*
  GetInputBrick:

  This function extracts data for a specified brick from the linear raw input file.
  Index magic is explained in the function.
*/
void ExtendedOctreeConverter::GetInputBrick(std::vector<uint8_t>& vData,
                                            ExtendedOctree &tree,
                                            LargeRAWFile_ptr pLargeRAWFileIn,
                                            uint64_t iInOffset, 
                                            const UINT64VECTOR4& coords,
                                            bool bClampToEdge) {
  const UINT64VECTOR3 vBrickSize = tree.ComputeBrickSize(coords);
  const uint64_t iBricksSize = (tree.m_vTOC.end()-1)->m_iLength;
  if (vData.size() != size_t(iBricksSize)) vData.resize(size_t(iBricksSize));

  // zero out the data (this makes sure boundaries are zero)
  // if we use bClampToEdge then we need to init the borders
  // here as they are overriden later
  if (!bClampToEdge)
    memset (&vData[0],0,size_t(iBricksSize));

  const uint64_t iVoxelSize = tree.GetComponentTypeSize() * tree.GetComponentCount();
  const uint64_t iMaxLineSize = vBrickSize.x * iVoxelSize;

  const UINT64VECTOR4 bricksInZeroLevel = tree.GetBrickCount(0);

  // first we figure out if the requested brick is a boundary brick
  // for boundary bricks we have to skip the overlap regions
  const uint64_t xStart = (coords.x == 0) ? m_iOverlap : 0;
  const uint64_t yStart = (coords.y == 0) ? m_iOverlap : 0;
  const uint64_t zStart = (coords.z == 0) ? m_iOverlap : 0;
  const uint64_t yEnd = vBrickSize.y - ((coords.y == bricksInZeroLevel.y-1) ? m_iOverlap : 0);
  const uint64_t zEnd = vBrickSize.z - ((coords.z == bricksInZeroLevel.z-1) ? m_iOverlap : 0);

  // now iterate over the x-scanlines (as x is stored continuous in the
  // input file we only need to loop over y and z)
  for (uint64_t z = 0;z<zEnd-zStart;z++) {
    for (uint64_t y = 0;y<yEnd-yStart;y++) {

      // the offset into the large raw file:
      // as usual we skip beyond the user specified iInOffset next we compute the
      // voxel coordinates multiplied with the size of a voxel to get to bytes.
      // The voxel coordinates are computed as follows for all but the starting bricks
      // we fill the overlap (so step m_iOverlap steps back) from the x,y, and z positions
      // next add the coordinates of the brick to fetch multiplied with the effective
      // brick size (i.e. the brick size without the overlap regions on each side)
      // then do the usual 3D to 1D conversion by multiplying y coordinates with the
      // length of a line (tree.m_vVolumeSize.x) and the z coordinate with the size
      // of a slice (tree.m_vVolumeSize.x * tree.m_vVolumeSize.y) since we are indexing
      // into the input volume we need to use tree.m_vVolumeSize for this
      const uint64_t iCurrentInOffset = iInOffset + iVoxelSize * (0-(m_iOverlap-xStart) + coords.x * (m_vBrickSize.x-m_iOverlap*2) +
                                                         (y-(m_iOverlap-yStart) + coords.y * (m_vBrickSize.y-m_iOverlap*2)) * tree.m_vVolumeSize.x +
                                                         (z-(m_iOverlap-zStart) + coords.z * (m_vBrickSize.z-m_iOverlap*2)) * tree.m_vVolumeSize.x * tree.m_vVolumeSize.y);

      // the offset into the target array
      // this is just a simple 3D to 1D conversion of the current position
      // y & z plus the overlap skip (xStart, yStart, yStart) which we need
      // to add if this is the first brick (otherwise these are all 0
      const size_t iOutOffset = size_t(iVoxelSize * (  xStart +
                                              (y+yStart) * vBrickSize.x +
                                              (z+zStart) * vBrickSize.x * vBrickSize.y));

      // now for the amount of data to be copied:
      // this is a full scanline unless its the first brick
      // or if it's the last brick (and possibly both) in those
      // cases there exists no overlap an we shorten the line
      uint64_t iLineSize = iMaxLineSize;
      if (coords.x == 0) {
        iLineSize -= m_iOverlap*iVoxelSize;
      }
      if (coords.x == bricksInZeroLevel.x-1)  {
        iLineSize -= m_iOverlap*iVoxelSize;
      }

      pLargeRAWFileIn->SeekPos(iCurrentInOffset);
      pLargeRAWFileIn->ReadRAW((uint8_t*)&vData[iOutOffset], iLineSize);
    }
  }

  if (bClampToEdge) {
    ClampToEdge(vData,
                coords.x == 0, 
                coords.y == 0, 
                coords.z == 0,
                coords.x == bricksInZeroLevel.x-1, 
                coords.y == bricksInZeroLevel.y-1, 
                coords.z == bricksInZeroLevel.z-1,
                iVoxelSize,
                vBrickSize);
  }
}


void ExtendedOctreeConverter::ClampToEdge(std::vector<uint8_t>& vData,
                                          bool bCopyXs,
                                          bool bCopyYs,
                                          bool bCopyZs,
                                          bool bCopyXe,
                                          bool bCopyYe,
                                          bool bCopyZe,
                                          uint64_t iVoxelSize,
                                          const UINT64VECTOR3& vBrickSize) {

  // copy left (line-Start) border-plane into left overlap region
  if (bCopyXs) {
    for (uint64_t z = 0;z<vBrickSize.z;z++) {
      for (uint64_t y = 0;y<vBrickSize.y;y++) {
        auto pSource = vData.begin() + iVoxelSize * (m_iOverlap + y * vBrickSize.x + z * vBrickSize.x * vBrickSize.y);
        auto pTarget = pSource;
        for (uint64_t o = 0;o<m_iOverlap;o++) {
          pTarget-=iVoxelSize;
          std::copy(pSource, pSource+iVoxelSize, pTarget);
        }
      }
    }
  }
  
  // copy right (line-End) border-plane into right overlap region
  if (bCopyXe) {
    for (uint64_t z = 0;z<vBrickSize.z;z++) {
      for (uint64_t y = 0;y<vBrickSize.y;y++) {
        auto pSource = vData.begin() + iVoxelSize * (((vBrickSize.x-1)-m_iOverlap) + y * vBrickSize.x + z * vBrickSize.x * vBrickSize.y);
        auto pTarget = pSource;
        for (uint64_t o = 0;o<m_iOverlap;o++) {
          pTarget+=iVoxelSize;
          std::copy(pSource, pSource+iVoxelSize, pTarget);
        }
      }
    }
  }
  
  // copy top border-plane into top overlap region
  if (bCopyYs) {
    for (uint64_t z = 0;z<vBrickSize.z;z++) {
      auto pSource = vData.begin() + iVoxelSize * (m_iOverlap*vBrickSize.x + z * vBrickSize.x * vBrickSize.y);
      auto pTarget = pSource;
      for (uint64_t o = 0;o<m_iOverlap;o++) {
        pTarget-=iVoxelSize*vBrickSize.x;
        std::copy(pSource, pSource+iVoxelSize*vBrickSize.x, pTarget);
      }
    }
  }


  // copy bottom border-plane into bottom overlap region
  if (bCopyYe) {
    for (uint64_t z = 0;z<vBrickSize.z;z++) {
      auto pSource = vData.begin() + iVoxelSize * ( ((vBrickSize.y-1)-m_iOverlap)*vBrickSize.x + z * vBrickSize.x * vBrickSize.y);
      auto pTarget = pSource;
      for (uint64_t o = 0;o<m_iOverlap;o++) {
        pTarget+=iVoxelSize*vBrickSize.x;
        std::copy(pSource, pSource+iVoxelSize*vBrickSize.x, pTarget);
      }
    }
  }

  // copy front border-plane into front overlap region
  if (bCopyZs) {
    for (uint64_t y = 0;y<vBrickSize.y;y++) {
      auto pSource = vData.begin() + iVoxelSize * (m_iOverlap*(vBrickSize.x*vBrickSize.y) + y * vBrickSize.x);
      auto pTarget = pSource;
      for (uint64_t o = 0;o<m_iOverlap;o++) {
        pTarget-=iVoxelSize*(vBrickSize.x*vBrickSize.y);
        std::copy(pSource, pSource+iVoxelSize*vBrickSize.x, pTarget);
      }
    }
  }

  // copy back border-plane into back overlap region
  if (bCopyZe) {
    for (uint64_t y = 0;y<vBrickSize.y;y++) {
      auto pSource = vData.begin() + iVoxelSize * (((vBrickSize.z-1)-m_iOverlap)*(vBrickSize.x*vBrickSize.y) + y * vBrickSize.x);
      auto pTarget = pSource;
      for (uint64_t o = 0;o<m_iOverlap;o++) {
        pTarget+=iVoxelSize*(vBrickSize.x*vBrickSize.y);
        std::copy(pSource, pSource+iVoxelSize*vBrickSize.x, pTarget);
      }
    }
  }
}

/// Computes max min statistics for each brick and rewrites 
/// it using compression, if desired.
void ExtendedOctreeConverter::ComputeStatsAndCompressAll(ExtendedOctree& tree)
{
  FlushCache(tree); // be sure we've got everything on disk.
  m_vBrickCache.clear(); // be double sure we don't use the cache anymore.

  const size_t iVoxelSize = tree.GetComponentTypeSize() *
                            size_t(tree.m_iComponentCount);
  const size_t maxbricksize = static_cast<size_t>(tree.m_iBrickSize.volume() *
                                                  iVoxelSize);
  std::shared_ptr<uint8_t> BrickData(new uint8_t[maxbricksize],
                                     nonstd::DeleteArray<uint8_t>());
  std::shared_ptr<uint8_t> compressed(new uint8_t[maxbricksize],
                                      nonstd::DeleteArray<uint8_t>());

  size_t iReportInterval = std::max<size_t>(1, tree.m_vTOC.size()/2000);

  if(m_eCompression == CT_NONE) {
    // compression is disabled.  That makes our job pretty easy: only compute
    // the brick stats
    for(size_t i=0; i < tree.m_vTOC.size(); ++i) {
      tree.GetBrickData(BrickData.get(), i);
      BrickStat(m_pBrickStatVec, i, BrickData.get(), BrickSize(tree, i),
                tree.m_iComponentCount, tree.m_eComponentType);
      const float progress = float(i) / tree.m_vTOC.size();
      m_fProgress = MathTools::lerp(progress, 0.0f,1.0f, 0.8f,1.0f);

      if (i % iReportInterval == 0) {
        m_fProgress = MathTools::lerp(progress, 0.0f,1.0f, 0.8f,1.0f);
        PROGRESS;
      }
    }
  } else {

    // foreach brick:
    //   load it up
    //   compress it
    //   write compressed payload
    //   update brick metadata based on what compression changed
    for(size_t i=0; i < tree.m_vTOC.size(); ++i) {
      tree.GetBrickData(BrickData.get(), i);
      BrickStat(m_pBrickStatVec, i, BrickData.get(), BrickSize(tree, i),
                tree.m_iComponentCount, tree.m_eComponentType);

      uint64_t newlen = zcompress(BrickData, BrickSize(tree, i), compressed);
      std::shared_ptr<uint8_t> data;

      if(newlen < BrickSize(tree, i)) {
        tree.m_vTOC[i].m_iLength = newlen;
        tree.m_vTOC[i].m_eCompression = CT_ZLIB;
        data = compressed;
      } else {
        tree.m_vTOC[i].m_iLength = BrickSize(tree, i);
        tree.m_vTOC[i].m_eCompression = CT_NONE;
        data = BrickData;
      }
      if(i > 0) {
        tree.m_vTOC[i].m_iOffset = tree.m_vTOC[i-1].m_iOffset +
                                   tree.m_vTOC[i-1].m_iLength;
      }
      tree.m_pLargeRAWFile->SeekPos(tree.m_vTOC[i].m_iOffset);
      tree.m_pLargeRAWFile->WriteRAW(data.get(), tree.m_vTOC[i].m_iLength);
      const float progress = float(i) / tree.m_vTOC.size();
      
      if (i % iReportInterval == 0) {
        m_fProgress = MathTools::lerp(progress, 0.0f,1.0f, 0.8f,1.0f);
        PROGRESS;
      }
    }
  }

  // do not forget to set new octree size
  tree.m_iSize = tree.m_vTOC.back().m_iOffset + tree.m_vTOC.back().m_iLength;
}

std::shared_ptr<uint8_t>
ExtendedOctreeConverter::Fetch(ExtendedOctree& tree,
                               uint64_t iIndex,
                               std::shared_ptr<uint8_t> const pBuffer)
{
  TOCEntry& record = tree.m_vTOC[(size_t)iIndex];
  std::shared_ptr<uint8_t> pData = pBuffer;
  if (!pData)
    pData.reset(new uint8_t[record.m_iLength], nonstd::DeleteArray<uint8_t>());
  tree.m_pLargeRAWFile->SeekPos(tree.m_iOffset + record.m_iOffset);
  tree.m_pLargeRAWFile->ReadRAW(pData.get(), record.m_iLength);

  // if we are touching the brick the first time compute statistics and compress
  if ((m_pBrickStatVec->size() < (iIndex+1) * tree.m_iComponentCount) ||
      !m_pBrickStatVec->at(iIndex * tree.m_iComponentCount).IsValid())
  {
    assert(record.m_eCompression == CT_NONE);
    assert(record.m_iLength == BrickSize(tree, iIndex));

    BrickStat(m_pBrickStatVec, iIndex, pData.get(), record.m_iLength,
              tree.m_iComponentCount, tree.m_eComponentType);

    // compress if desired
    if (m_eCompression != CT_NONE) {
      std::shared_ptr<uint8_t> pCompressed;
      // zcompress will always create a buffer sized like the input data
      uint64_t iCompressed = zcompress(pData, record.m_iLength, pCompressed);
      if (iCompressed < record.m_iLength) {
        if (!pBuffer) {
          pData.reset(new uint8_t[iCompressed], nonstd::DeleteArray<uint8_t>());
          memcpy(pData.get(), pCompressed.get(), iCompressed);
        } else
          pData = pCompressed;
        record.m_iLength = iCompressed;
        record.m_eCompression = CT_ZLIB;
      }
    }
  }
  return pData;
}

void ExtendedOctreeConverter::ComputeStatsCompressAndPermuteAll(ExtendedOctree& tree)
{
  FlushCache(tree); // be sure we've got everything on disk.
  m_vBrickCache.clear(); // be double sure we don't use the cache anymore.

  size_t const iVoxelSize = tree.GetComponentTypeSize() * size_t(tree.m_iComponentCount);
  size_t const iMaxBrickSize = static_cast<size_t>(tree.m_iBrickSize.volume() * iVoxelSize);
  std::shared_ptr<uint8_t> const pUncompressed(new uint8_t[iMaxBrickSize], nonstd::DeleteArray<uint8_t>());
  size_t const iReportInterval = std::max<size_t>(1, tree.m_vTOC.size()/2000);

  uint64_t const IN_CORE = std::numeric_limits<uint64_t>::max();

  uint64_t const treeOffset = tree.ComputeHeaderSize();
  uint64_t tempOffset = tree.GetSize();
  uint64_t writeOffset = treeOffset;
  uint64_t emptyLength = 0;

  typedef std::map<uint64_t, uint64_t> Uint64Map;
  Uint64Map occupiedSpace; // maps offset to brick index
  Uint64Map emptySpace;    // maps offset to empty length

  typedef std::unordered_map<uint64_t, std::shared_ptr<uint8_t>> SimpleCache;
  SimpleCache cache;
  uint64_t cacheSize = 0;

  // build occupied space table, should be very efficient if ToC is ordered
  for (size_t i = 0; i < tree.m_vTOC.size(); ++i)
    occupiedSpace.emplace_hint(occupiedSpace.cend(), Uint64Map::value_type(tree.m_vTOC[i].m_iOffset, i));

  uint64_t iProgress = 0; // global brick progress counter
  for (uint64_t lod = 0; lod < tree.GetLODCount(); ++lod)
  {
    UINT64VECTOR3 const domain = tree.GetBrickCount(lod);
    uint64_t const brickCount = domain.volume();

    // instantiate the layout we want to use for the current level of detail
    std::shared_ptr<VolumeTools::Layout> pLayout;
    switch (m_eLayout) {
    default:
    case LT_SCANLINE: pLayout.reset(new VolumeTools::ScanlineLayout(domain)); break;
    case LT_MORTON:   pLayout.reset(new VolumeTools::MortonLayout(domain));   break;
    case LT_HILBERT:  pLayout.reset(new VolumeTools::HilbertLayout(domain));  break;
    case LT_RANDOM:   pLayout.reset(new VolumeTools::RandomLayout(domain));   break;
    }

    // follow the layout and permute bricks until we completely filled
    // the domain of the current level (brickCounter == brickCount)
    uint64_t brickCounter = 0;
    uint64_t layoutIndex  = 0;
    while (brickCounter < brickCount)
    {
      UINT64VECTOR3 const position = pLayout->GetSpatialPosition(layoutIndex++);
      if (position.x < domain.x &&
          position.y < domain.y &&
          position.z < domain.z)
      {
        // convert valid spatial position to our internal brick index
        uint64_t const thisIndex = tree.BrickCoordsToIndex(UINT64VECTOR4(position, lod));
        TOCEntry& thisRecord = tree.m_vTOC[(size_t)thisIndex];
        std::shared_ptr<uint8_t> thisData;

        // retrieve next brick in layout order
        auto c = cache.find(thisIndex);
        if (c != cache.cend()) {
          // found cached (compressed) brick
          thisData = c->second;
          assert(thisData && thisData.get());
          cache.erase(c);
        } else {
          // load (compressed) brick from disk
          uint64_t const iLength = thisRecord.m_iLength; // disk length before fetch
          thisData = Fetch(tree, thisIndex, pUncompressed);
          if (occupiedSpace.begin()->second != thisIndex) {
            emptySpace.emplace(Uint64Map::value_type(thisRecord.m_iOffset, iLength));
          } else {
            emptyLength += iLength; // we just fetched the next brick in file
          }
          size_t iSuccess = occupiedSpace.erase(thisRecord.m_iOffset);
          assert(iSuccess);
          if (iSuccess) {} // suppress local variable is initialized but not referenced warning
          thisRecord.m_iOffset = IN_CORE;
        }

        // eat empty space that might have opened up by removing some last occupier
        {
          uint64_t emptyOffset = writeOffset + emptyLength;
          while (!emptySpace.empty() &&
                 emptySpace.begin()->first <= emptyOffset)
          {
            auto e = emptySpace.begin();
            assert(e->first == emptyOffset); // just check to know if e->first < emptyOffset occurs sometimes
            //emptyLength += e->second - (emptyOffset - e->first); // see above
            emptyLength += e->second;
            emptySpace.erase(e);
            emptyOffset = writeOffset + emptyLength;
          }
        }

        // free up occupied space until the current brick fits
        while (thisRecord.m_iLength > emptyLength)
        {
          // fetch next occupier
          auto o = occupiedSpace.begin();
          uint64_t const pageInIndex = o->second;
          TOCEntry& pageInRecord = tree.m_vTOC[(size_t)pageInIndex];
          assert(pageInRecord.m_iOffset != IN_CORE);
          assert(o->first == pageInRecord.m_iOffset);
          assert(writeOffset + emptyLength == pageInRecord.m_iOffset);
          uint64_t const iLength = pageInRecord.m_iLength; // disk length before fetch
          std::shared_ptr<uint8_t> pageInData = Fetch(tree, pageInIndex);
          emptyLength += iLength; // we just fetched the next brick in file
          pageInRecord.m_iOffset = IN_CORE;
          occupiedSpace.erase(o);

          // eat empty space that might have opened up by removing the last occupier
          uint64_t emptyOffset = writeOffset + emptyLength;
          while (!emptySpace.empty() &&
                  emptySpace.begin()->first <= emptyOffset)
          {
            auto e = emptySpace.begin();
            assert(e->first == emptyOffset); // just check to know if e->first < emptyOffset occurs sometimes
            //emptyLength += e->second - (emptyOffset - e->first); // see above
            emptyLength += e->second;
            emptySpace.erase(e);
            emptyOffset = writeOffset + emptyLength;
          }

          // push paged in brick to cache
          cacheSize += pageInRecord.m_iLength;
          bool bSuccess = cache.insert(SimpleCache::value_type(pageInIndex, pageInData)).second;
          if (bSuccess) {} // suppress local variable is initialized but not referenced warning
          assert(bSuccess);

          // check cache limit and page out as much bricks as necessary
          while (!cache.empty() && cacheSize > m_iMemLimit)
          {
            // pop random brick from cache to be paged out
            auto c = cache.begin();
            uint64_t const pageOutIndex = c->first;
            TOCEntry& pageOutRecord = tree.m_vTOC[(size_t)pageOutIndex];
            assert(pageOutRecord.m_iOffset == IN_CORE);
            std::shared_ptr<uint8_t> pageOutData = c->second;
            assert(pageOutData && pageOutData.get());
            cacheSize -= pageOutRecord.m_iLength;
            cache.erase(c);

            // 1) try to find next suitable empty space location from the back of the file
            for (auto e = emptySpace.rbegin(); e != emptySpace.rend(); ++e) {
              if (pageOutRecord.m_iLength > e->second) {
                continue;
              } else if (pageOutRecord.m_iLength < e->second) {
                // Yay! we found suitable empty space but we need to split empty
                // space from behind to not change the EST entry
                e->second -= pageOutRecord.m_iLength;
                pageOutRecord.m_iOffset = e->first + e->second;
              } else {
                // Yay! we found suitable empty space that fits perfectly
                pageOutRecord.m_iOffset = e->first;
                emptySpace.erase(--e.base()); // erasing a reverse iterator
              }
              break;
            }
            // 2) try to use 1st order empty space until we barely fit thisRecord in there
            if (pageOutRecord.m_iOffset == IN_CORE) {
              if (emptyLength > thisRecord.m_iLength &&
                  emptyLength - thisRecord.m_iLength >= pageOutRecord.m_iLength)
              {
                pageOutRecord.m_iOffset = emptyOffset - pageOutRecord.m_iLength;
                emptyOffset -= pageOutRecord.m_iLength;
                emptyLength -= pageOutRecord.m_iLength;
              }
            }
            // 3) final chance use tempOffset at the end of the file to page out memory
            if (pageOutRecord.m_iOffset == IN_CORE) {
              pageOutRecord.m_iOffset = tempOffset;
              tempOffset += pageOutRecord.m_iLength;
            }
            // add new occupier even if we page out to the temp region at the end of file
            bool bSuccess = occupiedSpace.insert(Uint64Map::value_type(pageOutRecord.m_iOffset, pageOutIndex)).second;
            if (bSuccess) {} // suppress local variable is initialized but not referenced warning
            assert(bSuccess);

            // write brick to temporary position
            tree.m_pLargeRAWFile->SeekPos(tree.m_iOffset + pageOutRecord.m_iOffset);
            tree.m_pLargeRAWFile->WriteRAW(pageOutData.get(), pageOutRecord.m_iLength);

          } // free up some cache
        } // free up occupied space

        // write brick to the correct layout position
        thisRecord.m_iOffset = writeOffset;
        tree.m_pLargeRAWFile->SeekPos(tree.m_iOffset + thisRecord.m_iOffset);
        tree.m_pLargeRAWFile->WriteRAW(thisData.get(), thisRecord.m_iLength);
        writeOffset += thisRecord.m_iLength;
        emptyLength -= thisRecord.m_iLength;

        // increment brick counter for the termination criterion
        ++brickCounter;

        // report progress
        if (iProgress % iReportInterval == 0) {
          m_fProgress = MathTools::lerp(float(iProgress) / tree.m_vTOC.size(), 0.0f,1.0f, 0.8f,1.0f);
          PROGRESS;
        }
        ++iProgress;

      } // valid spatial position check
    } // reordering per level loop
  } // level loop

  uint64_t const temporarySpace = tempOffset - tree.m_iSize;
  uint64_t const compressionGain = tree.m_iSize - writeOffset;

  m_Progress.Other(_func_, "Temporary disk space required during brick reordering: %.3f MB", (float)temporarySpace / (1024.f*1024.f));
  m_Progress.Other(_func_, "Space savings due to data compression: %.2f%%", (100.f - ((float)compressionGain / tree.m_iSize) * 100.f));

  // do not forget to set new octree size
  tree.m_iSize = writeOffset;
  assert(cache.empty());
  assert(occupiedSpace.empty());
  assert(emptySpace.empty()); // may be not true
}

/*
  SetBrick (vector):

  Convenience function that takes vector coordinates of a brick
  turns them into a 1D index and calls the scalar SetBrick
*/
void ExtendedOctreeConverter::SetBrick(uint8_t* pData, ExtendedOctree &tree,
                                      const UINT64VECTOR4& vBrickCoords,
                                      bool bForceWrite) {
  SetBrick(pData, tree, tree.BrickCoordsToIndex(vBrickCoords), bForceWrite);
}

/*
  GetBrick (vector):

  Convenience function that takes vector coordinates of a brick
  turns them into a 1D index and calls the scalar GetBrick
*/
void ExtendedOctreeConverter::GetBrick(uint8_t* pData, ExtendedOctree &tree,
                                       const UINT64VECTOR4& vBrickCoords) {
  GetBrick(pData, tree, tree.BrickCoordsToIndex(vBrickCoords));
}

/*
  SetupCache:

  Computes the size of the cache: simply as available size divided
  by the size of a cache element
*/
void ExtendedOctreeConverter::SetupCache(ExtendedOctree &tree) {
  size_t CacheElementDataSize = size_t(tree.GetComponentTypeSize() * 
                                tree.GetComponentCount() * 
                                tree.m_iBrickSize.volume());
  uint64_t iCacheElemCount = m_iMemLimit / (CacheElementDataSize + sizeof(CacheEntry));
  iCacheElemCount = std::min(iCacheElemCount, tree.ComputeBrickCount());
  m_vBrickCache.resize(size_t(iCacheElemCount));

  for (BrickCacheIter i = m_vBrickCache.begin();i != m_vBrickCache.end();++i) {
    i->SetSize(CacheElementDataSize);
  }
}

/*
  FlushCache:

  Write all bricks to disk that have not been committed yet
*/
void ExtendedOctreeConverter::FlushCache(ExtendedOctree &tree) {
  for (BrickCacheIter i = m_vBrickCache.begin();i != m_vBrickCache.end();++i) {
    if (i->m_bDirty) {
      WriteBrickToDisk(tree, i);
    }
  }
}

// @returns the number of bytes needed to store the (uncompressed) given brick.
uint64_t ExtendedOctreeConverter::BrickSize(const ExtendedOctree& tree,
                                            uint64_t index) {
  // if the brick is compressed, then its m_iLength field gives the
  // length of the compressed brick, not the expanded brick...
  if(tree.m_vTOC[index].m_eCompression != CT_NONE) {
    // ... but it's pretty simple to compute how big it'll expand into:
    return tree.ComputeBrickSize(tree.IndexToBrickCoords(index)).volume() *
           tree.GetComponentTypeSize() *
           tree.GetComponentCount();
  }
  return tree.m_vTOC[index].m_iLength;
}

void ExtendedOctreeConverter::BrickStat(
  BrickStatVec* bs, uint64_t index, const uint8_t* pData, uint64_t length,
  size_t components, enum ExtendedOctree::COMPONENT_TYPE type
) {
  assert(bs);
  if(bs->size() < (index+1)*components) {
    bs->resize((index+1)*components);
  }

  BrickStatVec elem;

  switch (type) {
    case ExtendedOctree::CT_UINT8:
      elem = ComputeBrickStats<uint8_t>(pData, length, components);
      break;
    case ExtendedOctree::CT_UINT16:
      elem = ComputeBrickStats<uint16_t>(pData, length, components);
      break;
    case ExtendedOctree::CT_UINT32:
      elem = ComputeBrickStats<uint32_t>(pData, length, components);
      break;
    case ExtendedOctree::CT_UINT64:
      elem = ComputeBrickStats<uint64_t>(pData, length, components);
      break;
    case ExtendedOctree::CT_INT8:
      elem = ComputeBrickStats<int8_t>(pData, length, components);
      break;
    case ExtendedOctree::CT_INT16:
      elem = ComputeBrickStats<int16_t>(pData, length, components);
      break;
    case ExtendedOctree::CT_INT32:
      elem = ComputeBrickStats<int32_t>(pData, length, components);
      break;
    case ExtendedOctree::CT_INT64:
      elem = ComputeBrickStats<int64_t>(pData, length, components);
      break;
    case ExtendedOctree::CT_FLOAT32:
      elem = ComputeBrickStats<float>(pData, length, components);
      break;
    case ExtendedOctree::CT_FLOAT64:
      elem = ComputeBrickStats<double>(pData, length, components);
      break;
  }

  for (size_t c=0; c < components ;++c)
    (*bs)[index*components+c] = elem[c];
}

void ExtendedOctreeConverter::WriteBrickToDisk(ExtendedOctree &tree, BrickCacheIter element)
{
  WriteBrickToDisk(tree, element->m_pData, element->m_index);
  element->m_bDirty = false;
}

void ExtendedOctreeConverter::WriteBrickToDisk(ExtendedOctree &tree, uint8_t* pData, size_t index)
{
  tree.m_pLargeRAWFile->SeekPos(tree.m_iOffset+tree.m_vTOC[index].m_iOffset);
  const uint64_t length = BrickSize(tree, index);

  tree.m_vTOC[index].m_iLength = length;
  tree.m_vTOC[index].m_eCompression = CT_NONE;
  tree.m_pLargeRAWFile->WriteRAW(pData, tree.m_vTOC[index].m_iLength);
}

/*
  HasIndex:

  Function used for find_if, returns true iff a cache entry has a certain index
*/
struct HasIndex : public std::binary_function <CacheEntry, uint64_t, bool> {
  bool operator() (const CacheEntry& cacheEntry, uint64_t index) const {
    return cacheEntry.m_index == index;
  }
};

/*
  GetBrick:

  Retrieves a brick from the tree. First we check if the cache is
  enabled, if not we simply request the brick from the tree. Otherwise
  we check the cache and, if we have a hit, return the cache copy
  otherwise we fetch the data from disk and put a copy into the cache,
  therefore we search for a suitable cache entry (the entry with the
  oldest access counter). In either case (hit or miss) we update the
  access counter, i.e. we use true LRU as caching strategy. */
void ExtendedOctreeConverter::GetBrick(uint8_t* pData, ExtendedOctree &tree, uint64_t index) {
  if (m_vBrickCache.empty()) {
    tree.GetBrickData(pData, index);
    return;
  }

  BrickCacheIter cacheEntry = std::find_if(m_vBrickCache.begin(), m_vBrickCache.end(), std::bind2nd(HasIndex(), index));

  if (cacheEntry == m_vBrickCache.end()) {
    // cache miss

    // read data from disk
    tree.GetBrickData(pData, index);

    // find cache entry to evict from cache
    cacheEntry = m_vBrickCache.begin();
    for (BrickCacheIter i = m_vBrickCache.begin();i != m_vBrickCache.end();++i) {
      if (i->m_iAccess < cacheEntry->m_iAccess)
        cacheEntry = i;
    }

    if (cacheEntry->m_iAccess == 0) {
      // if this is a never before used cache entry allocate memory
      cacheEntry->Allocate();
    } else {
      // if it's dirty, write to disk
      if (cacheEntry->m_bDirty) WriteBrickToDisk(tree, cacheEntry);
    }
    uint64_t uncompressedLength = BrickSize(tree, index);

    // put new entry into cache
    cacheEntry->m_bDirty = false;
    cacheEntry->m_index = size_t(index);
    cacheEntry->m_iAccess = ++m_iCacheAccessCounter;
    memcpy(cacheEntry->m_pData, pData, size_t(uncompressedLength));
  } else {
    // cache hit
    uint64_t uncompressedLength = BrickSize(tree, cacheEntry->m_index);
    memcpy(pData, cacheEntry->m_pData, size_t(uncompressedLength));
    cacheEntry->m_iAccess = ++m_iCacheAccessCounter;
  }
}

/*
  SetBrick:

  Writes a brick to the tree. First we check if the cache is enabled, if not we simply write out
  the brick to disk. Otherwise we check the cache and, if we have a hit, write into the cache copy
  otherwise we search for a suitable cache entry (the entry with the oldest access counter).
  In either case (hit or miss) we update the access counter, i.e. we use true LRU as caching strategy.
  If bForceWrite is enabled we write the data to disk directly bypassing the write cache, if in this
  case a cache miss occurs we only write to disk and don't update the cache in a cache hit case we update
  the data and write to disk.
*/
void ExtendedOctreeConverter::SetBrick(uint8_t* pData, ExtendedOctree &tree, uint64_t index, bool bForceWrite) {
  if (m_vBrickCache.empty()) {
    WriteBrickToDisk(tree, pData, size_t(index));
    return;
  }

  BrickCacheIter cacheEntry = std::find_if(m_vBrickCache.begin(), m_vBrickCache.end(), std::bind2nd(HasIndex(), index));

  tree.m_vTOC[size_t(index)].m_iLength =
    tree.ComputeBrickSize(tree.IndexToBrickCoords(index)).volume() *
    tree.GetComponentTypeSize() *
    tree.GetComponentCount();
  if (cacheEntry == m_vBrickCache.end()) {
    // cache miss

    if (bForceWrite) {
      WriteBrickToDisk(tree, pData, size_t(index));
      return;
    }

    // find cache entry to evict from cache
    cacheEntry = m_vBrickCache.begin();
    for (BrickCacheIter i = m_vBrickCache.begin();i != m_vBrickCache.end();++i) {
      if (i->m_iAccess < cacheEntry->m_iAccess)
        cacheEntry = i;
    }

    if (cacheEntry->m_iAccess == 0) {
      // if this is a never before used cache entry allocate memory
      cacheEntry->Allocate();
    } else {
      // if it's dirty, write to disk
      if (cacheEntry->m_bDirty) WriteBrickToDisk(tree, cacheEntry);
    }

    // put new entry into cache
    cacheEntry->m_bDirty = true;
    cacheEntry->m_index = size_t(index);
    cacheEntry->m_iAccess = ++m_iCacheAccessCounter;
    memcpy(cacheEntry->m_pData, pData, size_t(tree.m_vTOC[cacheEntry->m_index].m_iLength));
  } else {
    // cache hit
    cacheEntry->m_bDirty = true;
    cacheEntry->m_iAccess = ++m_iCacheAccessCounter;
    memcpy(cacheEntry->m_pData, pData, size_t(tree.m_vTOC[size_t(index)].m_iLength));
    if (bForceWrite) WriteBrickToDisk(tree, cacheEntry);
  }
}


/*
  CopyBrickToBrick:

  Copies (parts) of one brick into another. This routine is used to fill the
  overlap regions. Index Magic explained in the function.
*/
void ExtendedOctreeConverter::CopyBrickToBrick(std::vector<uint8_t>& vSourceData, const UINT64VECTOR3& sourceBrickSize,
                                               std::vector<uint8_t>& vTargetData, const UINT64VECTOR3& targetBrickSize,
                                               const UINT64VECTOR3& sourceOffset, const UINT64VECTOR3& targetOffset,
                                               const UINT64VECTOR3& regionSize, size_t voxelSize) {
  for (uint32_t z = 0;z<regionSize.z;z++) {
    for (uint32_t y = 0;y<regionSize.y;y++) {

      // the index computation for both the source and target
      // are mostly just the usual 3D to 1D conversion
      // as coordinates we simply add the current coordinates
      // of the scanline (y,z) to the offset coordinates
      // also as usual the voxel coordinates are multiplied by the
      // voxel size
      size_t iSourcePos = size_t(voxelSize * (
                                      (sourceOffset.x) +
                                      (sourceOffset.y + y) * sourceBrickSize.x +
                                      (sourceOffset.z + z) * sourceBrickSize.x * sourceBrickSize.y));

      // same coordinate computation as above, only that we now use
      // the targetOffset and the targetBrickSize
      size_t iTargetPos = size_t(voxelSize * (
                                      (targetOffset.x) +
                                      (targetOffset.y + y) * targetBrickSize.x +
                                      (targetOffset.z + z) * targetBrickSize.x * targetBrickSize.y));

      memcpy(&vTargetData[iTargetPos], &vSourceData[iSourcePos], size_t(voxelSize*regionSize.x));
    }
  }
}

/*
  FillOverlap:

  This somewhat lengthy method fills the overlap regions of the bricks
  of a given LoD level, this function should only be called if the
  computation of the LoD level is otherwise complete.
  The basic idea of this method is to step through all bricks of the level
  (hence the three for-loops) and copy the overlapping regions from the
  brick's neighbors. Therefore we first compute if we have neighbors at all
  and then inside the innermost loop perform the copy operation. There is one
  little trick that we use here to avoid having to consider all 26 neighbors
  of the brick, but reduce the maximum number of neighbors to consider to 10.
  The idea works as follows: When we march in x,y,z order through the volume
  we can always assume that bricks with a lower x,y, or z coordinate are
  completely processed (including overlap). That means if we copy from the
  x-1 brick for example it already has a full overlap already including data
  from the diagonal neighbors. Consequently, after we have copied the data
  of the six direct neighbors we only need to consider one diagonal neighbors
  in the same plane and the three neighbors to fill the bottom right corner.
*/
void ExtendedOctreeConverter::FillOverlap(ExtendedOctree &tree, uint64_t iLoD, bool bClampToEdge) {
  UINT64VECTOR3 baseBricks = tree.GetBrickCount(iLoD);

  size_t iElementSize = tree.GetComponentTypeSize() * size_t(tree.GetComponentCount());
  std::vector<uint8_t> vTargetData(size_t(tree.m_iBrickSize.volume() * iElementSize));
  std::vector<uint8_t> vSourceData(size_t(tree.m_iBrickSize.volume() * iElementSize));

  for (uint64_t z = 0;z<baseBricks.z;z++) {

    bool bHasFrontNeighbour = z > 0;
    bool bHasBackNeighbour = z < baseBricks.z-1;

    for (uint64_t y = 0;y<baseBricks.y;y++) {

      bool bHasTopNeighbour = y > 0;
      bool bHasBottomNeighbour = y < baseBricks.y-1;

      for (uint64_t x = 0;x<baseBricks.x;x++) {

        bool bHasLeftNeighbour = x > 0;
        bool bHasRightNeighbour = x < baseBricks.x-1;

        UINT64VECTOR4 coords(x,y,z,iLoD);
        UINT64VECTOR3 targetBrickSize = tree.ComputeBrickSize(coords);
        GetBrick(&vTargetData[0], tree,  coords);

        // first the six direct neighbors
        if (bHasRightNeighbour) {
          UINT64VECTOR4 sourceCoords(x+1,y,z,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
                           UINT64VECTOR3(tree.m_iOverlap,0,0), UINT64VECTOR3(targetBrickSize.x-tree.m_iOverlap,0,0),
                           UINT64VECTOR3(tree.m_iOverlap, sourceBrickSize.y, sourceBrickSize.z),
                           iElementSize);
        }
        if (bHasBottomNeighbour) {
          UINT64VECTOR4 sourceCoords(x,y+1,z,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
                           UINT64VECTOR3(0,tree.m_iOverlap,0), UINT64VECTOR3(0,targetBrickSize.y-tree.m_iOverlap,0),
                           UINT64VECTOR3(sourceBrickSize.x, tree.m_iOverlap, sourceBrickSize.z),
                           iElementSize);
        }
        if (bHasBackNeighbour) {
          UINT64VECTOR4 sourceCoords(x,y,z+1,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
            UINT64VECTOR3(0,0,tree.m_iOverlap), UINT64VECTOR3(0,0,targetBrickSize.z-tree.m_iOverlap),
            UINT64VECTOR3(sourceBrickSize.x, sourceBrickSize.y, tree.m_iOverlap),
            iElementSize);
        }
        if (bHasLeftNeighbour) {
          UINT64VECTOR4 sourceCoords(x-1,y,z,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
            UINT64VECTOR3(sourceBrickSize.x-tree.m_iOverlap*2,0,0), UINT64VECTOR3(0,0,0),
            UINT64VECTOR3(tree.m_iOverlap, sourceBrickSize.y, sourceBrickSize.z),
            iElementSize);
        }
        if (bHasTopNeighbour) {
          UINT64VECTOR4 sourceCoords(x,y-1,z,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
            UINT64VECTOR3(0,sourceBrickSize.y-tree.m_iOverlap*2,0), UINT64VECTOR3(0,0,0),
            UINT64VECTOR3(sourceBrickSize.x, tree.m_iOverlap, sourceBrickSize.z),
            iElementSize);
        }
        if (bHasFrontNeighbour) {
          UINT64VECTOR4 sourceCoords(x,y,z-1,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
                           UINT64VECTOR3(0,0,sourceBrickSize.z-tree.m_iOverlap*2), UINT64VECTOR3(0,0,0),
                           UINT64VECTOR3(sourceBrickSize.x, sourceBrickSize.y, tree.m_iOverlap),
                           iElementSize);
        }

        // then the bottom left neighbor (the other four corners are included in the
        // previous cases)
        if (bHasBottomNeighbour && bHasRightNeighbour) {
          UINT64VECTOR4 sourceCoords(x+1,y+1,z,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
                           UINT64VECTOR3(tree.m_iOverlap,tree.m_iOverlap,0), UINT64VECTOR3(targetBrickSize.x-tree.m_iOverlap,targetBrickSize.y-tree.m_iOverlap,0),
                           UINT64VECTOR3(tree.m_iOverlap, tree.m_iOverlap, sourceBrickSize.z),
                           iElementSize);
        }


        // finally, the three diagonal neighbors in the next brick plane to fill the
        // bottom right corner
        if (bHasRightNeighbour && bHasBackNeighbour) {
          UINT64VECTOR4 sourceCoords(x+1,y,z+1,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
                           UINT64VECTOR3(tree.m_iOverlap,0,tree.m_iOverlap), UINT64VECTOR3(targetBrickSize.x-tree.m_iOverlap,0,targetBrickSize.z-tree.m_iOverlap),
                           UINT64VECTOR3(tree.m_iOverlap, sourceBrickSize.y, tree.m_iOverlap),
                           iElementSize);
        }
        if (bHasBottomNeighbour && bHasBackNeighbour) {
          UINT64VECTOR4 sourceCoords(x,y+1,z+1,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
                           UINT64VECTOR3(0,tree.m_iOverlap,tree.m_iOverlap), UINT64VECTOR3(0,targetBrickSize.y-tree.m_iOverlap,targetBrickSize.z-tree.m_iOverlap),
                           UINT64VECTOR3(sourceBrickSize.x, tree.m_iOverlap, tree.m_iOverlap),
                           iElementSize);
        }
        if (bHasRightNeighbour && bHasBottomNeighbour && bHasBackNeighbour) {
          UINT64VECTOR4 sourceCoords(x+1,y+1,z+1,iLoD);
          UINT64VECTOR3 sourceBrickSize = tree.ComputeBrickSize(sourceCoords);
          GetBrick(&vSourceData[0], tree, sourceCoords);

          CopyBrickToBrick(vSourceData, sourceBrickSize, vTargetData, targetBrickSize,
                           UINT64VECTOR3(tree.m_iOverlap,tree.m_iOverlap,tree.m_iOverlap), UINT64VECTOR3(targetBrickSize.x-tree.m_iOverlap,targetBrickSize.y-tree.m_iOverlap,targetBrickSize.z-tree.m_iOverlap),
                           UINT64VECTOR3(tree.m_iOverlap, tree.m_iOverlap, tree.m_iOverlap),
                           iElementSize);
        }


        if (bClampToEdge) {
          ClampToEdge(vTargetData,
                      !bHasLeftNeighbour,
                      !bHasTopNeighbour, 
                      !bHasFrontNeighbour,
                      !bHasRightNeighbour, 
                      !bHasBottomNeighbour, 
                      !bHasBackNeighbour,
                      iElementSize,
                      targetBrickSize);
        }
        


        // now that brick is complete, write it back to the tree
        SetBrick(&vTargetData[0], tree, coords);
      }
    }
  }

}

/*
  This method reorders the large input raw file into smaller bricks
  of maximum size m_vBrickSize with an overlap of m_iOverlap i.e.
  it computes LoD level zero. Therefore, it iterates over all bricks
  to be created and grabs each from the source data. In the process it
  also fills the tree ToC.
*/
void ExtendedOctreeConverter::PermuteInputData(ExtendedOctree &tree, LargeRAWFile_ptr 
                                               pLargeRAWFileIn, uint64_t iInOffset,
                                               bool bClampToEdge) {
  std::vector<uint8_t> vData;
  UINT64VECTOR3 baseBricks = tree.GetBrickCount(0);
  const float progress_start = m_fProgress;

  uint64_t iCurrentOutOffset = tree.ComputeHeaderSize();
  for (uint64_t z = 0;z<baseBricks.z;z++) {
    for (uint64_t y = 0;y<baseBricks.y;y++) {
      for (uint64_t x = 0;x<baseBricks.x;x++) {
        UINT64VECTOR4 coords(x,y,z,0);
        uint64_t iUncompressedBrickSize =
          tree.ComputeBrickSize(UINT64VECTOR4(x,y,z,0)).volume() *
          tree.GetComponentTypeSize() *
          tree.GetComponentCount();
        TOCEntry t = {iCurrentOutOffset, iUncompressedBrickSize, CT_NONE,
                      iUncompressedBrickSize, UINTVECTOR2(0,0)};
        tree.m_vTOC.push_back(t);

        GetInputBrick(vData, tree, pLargeRAWFileIn, iInOffset, coords,
                      bClampToEdge);
        SetBrick(&(vData[0]), tree, coords);

        iCurrentOutOffset += iUncompressedBrickSize;
        const float xf = float(x);
        const float p = xf + y*baseBricks.x + z*baseBricks.x*baseBricks.y;
        m_fProgress = MathTools::lerp(p / float(baseBricks.volume()),
                                      0.0f,1.0f, progress_start,0.4f);
      }
      PROGRESS;
    }
  }
}

/*
 ExportToRAW:

 Flattens/un-bricks a given LoD level into a file, for example a call with
 iLODLevel = 0 will recover the exact original data file used tho build this
 tree. This method is very simple, it iterates over all bricks of the given
 LoD level (x,y,z for-loops) and for each brick it writes it's non-overlap
 values into the target file. Index magic is explained inside the function.
*/
bool ExtendedOctreeConverter::ExportToRAW(const ExtendedOctree &tree,
                                 const LargeRAWFile_ptr pLargeRAWFile,
                                 uint64_t iLODLevel, uint64_t iOffset) {
  if (iLODLevel >= tree.GetLODCount()) return false;

  const size_t iVoxelSize =tree. GetComponentTypeSize() * size_t(tree.m_iComponentCount);
  uint8_t *pBrickData = new uint8_t[size_t(tree.m_iBrickSize.volume() * iVoxelSize)];

  const UINT64VECTOR3 outSize = tree.m_vLODTable[size_t(iLODLevel)].m_iLODPixelSize;

  UINT64VECTOR3 bricksToExport = tree.GetBrickCount(iLODLevel);
  for (uint64_t z = 0;z<bricksToExport.z;++z) {
    for (uint64_t y = 0;y<bricksToExport.y;++y) {
      for (uint64_t x = 0;x<bricksToExport.x;++x) {
        const UINT64VECTOR4 coords(x,y,z, iLODLevel);
        const UINT64VECTOR3 brickSize = tree.ComputeBrickSize(coords);

        tree.GetBrickData(pBrickData, coords);

        // compute the length of a scanline that is the non-overlap size
        // times the size of a voxel
        const size_t iLineSize = (size_t(brickSize.x)-tree.m_iOverlap*2) *iVoxelSize;

        for (uint64_t bz = 0;bz<brickSize.z-2*tree.m_iOverlap;++bz) {
          for (uint64_t by = 0;by<brickSize.y-2*tree.m_iOverlap;++by) {

            // the offset into the target file is computed as follows:
            // first the global offset into the file as specified by the user
            // plus the scanline coordinate within th current brick (by, by)
            // and the coordinates of the non-overlap part of the current
            // brick x,y,z as usual x is used as is y is multiplied with x size
            // z is multiplied with x- times y-size, since we are placing the
            // brick inside the output file we have to use outSize
            // finally we multiply the brick voxels with the voxelsize to
            // get the offset in bytes
            const uint64_t iOutOffset =  iOffset +
              (
                (    (x*(tree.m_iBrickSize.x-tree.m_iOverlap*2))) +
                ((by+(y*(tree.m_iBrickSize.y-tree.m_iOverlap*2))) * outSize.x) +
                ((bz+(z*(tree.m_iBrickSize.z-tree.m_iOverlap*2))) * outSize.x * outSize.y)
              ) * iVoxelSize;

            // the offset in the source data is computed as follows:
            // skip the overlap in the scanline then skip to the current line by
            // and the current slice by also skip their overlap as usual
            // x is used as is, y is multiplied with x size, and
            // z is multiplied with x- times y-size, since we are offsetting in
            // inside the brick  we have to use brick's size
            // finally we multiply the brick voxels with the voxel size to
            // get the offset in bytes
            const uint64_t iInOffset = (
                                     tree.m_iOverlap  +
                                ((by+tree.m_iOverlap) * brickSize.x) +
                                ((bz+tree.m_iOverlap) * brickSize.x * brickSize.y)
                               ) * iVoxelSize;

            pLargeRAWFile->SeekPos(iOutOffset);
            pLargeRAWFile->WriteRAW(pBrickData + iInOffset, iLineSize);
          }
        }
      }
    }
  }

  delete [] pBrickData;
  return true;
}

/*
 ExportToRAW (string):

 Convenience function that calls the above method with a large raw file
 constructed from the given string
*/
bool ExtendedOctreeConverter::ExportToRAW(const ExtendedOctree &tree,
                                  const std::string& filename,
                                 uint64_t iLODLevel, uint64_t iOffset) {
  uint64_t iElementSize = tree.GetComponentTypeSize() * uint64_t(tree.m_iComponentCount);
  UINT64VECTOR3 outsize = tree.m_vLODTable[size_t(iLODLevel)].m_iLODPixelSize;

  LargeRAWFile_ptr outFile(new LargeRAWFile(filename));
  if (!outFile->Create(iOffset + outsize.volume()*iElementSize)) {
    return false;
  }
  return ExportToRAW(tree, outFile, iLODLevel, iOffset);
}


/*
 ApplyFunction:

 Applies a function to each brick of a given LoD level.
 This method simply iterates over all bricks of the given LoD
 level (x,y,z for-loops) and for each brick it writes hands the
 brick with (possibly modified) overlap to the supplied function.
*/
bool ExtendedOctreeConverter::ApplyFunction(const ExtendedOctree &tree, uint64_t iLODLevel,
                                            bool (*brickFunc)(void* pData,
                                            const UINT64VECTOR3& vBrickSize,
                                            const UINT64VECTOR3& vBrickOffset,
                                            void* pUserContext),
                                            void* pUserContext, uint32_t iOverlap) {

  if (iLODLevel >= tree.GetLODCount() || iOverlap > tree.m_iOverlap) return false;

  uint32_t skipOverlap = tree.m_iOverlap-iOverlap;

  const size_t iVoxelSize = tree.GetComponentTypeSize() * size_t(tree.m_iComponentCount);
  uint8_t *pBrickData = new uint8_t[size_t(tree.m_iBrickSize.volume() * iVoxelSize)];

  UINT64VECTOR3 bricksToExport = tree.GetBrickCount(iLODLevel);
  for (uint64_t z = 0;z<bricksToExport.z;++z) {
    for (uint64_t y = 0;y<bricksToExport.y;++y) {
      for (uint64_t x = 0;x<bricksToExport.x;++x) {
        const UINT64VECTOR4 coords(x,y,z, iLODLevel);
        const UINT64VECTOR3 brickSize = tree.ComputeBrickSize(coords);

        tree.GetBrickData(pBrickData, coords);
        if (skipOverlap != 0) VolumeTools::RemoveBoundary(pBrickData, brickSize, iVoxelSize, skipOverlap);

        if(!brickFunc(pBrickData, brickSize, coords.xyz(),pUserContext)) {
          delete [] pBrickData;
          return false;
        }
      }
    }
  }
  delete [] pBrickData;
  return true;
}


void ExtendedOctreeConverter::Atalasify(const ExtendedOctree &tree,
                                         const UINT64VECTOR4& vBrickCoords,
                                         const UINTVECTOR2& atlasSize,
                                         uint8_t* pData) {

  Atalasify(tree, size_t(tree.BrickCoordsToIndex(vBrickCoords)), atlasSize, pData);
}

void ExtendedOctreeConverter::Atalasify(const ExtendedOctree &tree,
                                         size_t index,
                                         const UINTVECTOR2& atlasSize,
                                         uint8_t* pData) {


  const TOCEntry& metaData = tree.GetBrickToCData(index);
  const UINTVECTOR3 maxBrickSize  = tree.GetMaxBrickSize();
  const UINT64VECTOR3 currBrickSize = tree.ComputeBrickSize(tree.IndexToBrickCoords(index));

  tree.GetBrickData(pData, index);

  // bail out if brick is atlantified and the size
  // is correct
  if (metaData.m_iAtlasSize == atlasSize) return;

  // if the the brick is already atlantified differently
  // then convert it back to plain format first 
  if (metaData.m_iAtlasSize.area() != 0) 
    VolumeTools::DeAtalasify(size_t(currBrickSize.volume()*tree.GetComponentTypeSize()*tree.GetComponentCount()), metaData.m_iAtlasSize, maxBrickSize, currBrickSize, pData, pData);

  // finally atlantify
  VolumeTools::Atalasify(size_t(currBrickSize.volume()*tree.GetComponentTypeSize()*tree.GetComponentCount()), maxBrickSize, currBrickSize, atlasSize, pData, pData);
}

bool ExtendedOctreeConverter::Atalasify(ExtendedOctree &tree,
                                        const UINTVECTOR2& atlasSize,
                                        LargeRAWFile_ptr pLargeRAWFile,
                                        uint64_t iOffset)
{
  ExtendedOctree e;

  // setup target metadata
  e.m_eComponentType = tree.m_eComponentType;
  e.m_iComponentCount = tree.m_iComponentCount;
  e.m_vVolumeSize = tree.m_vVolumeSize;
  e.m_vVolumeAspect = tree.m_vVolumeAspect;
  e.m_iBrickSize = tree.m_iBrickSize;
  e.m_iOverlap = tree.m_iOverlap;
  e.m_iOffset = iOffset;
  e.m_pLargeRAWFile = pLargeRAWFile;
  e.ComputeMetadata();

  size_t CacheElementDataSize = size_t(tree.GetComponentTypeSize() * tree.GetComponentCount() * tree.m_iBrickSize.volume());
  unsigned char* pData = new unsigned char[CacheElementDataSize];


  // go through all bricks and convert them to deatlantified format
  for (size_t iBrick = 0;iBrick<tree.m_vTOC.size();iBrick++) {
    // convert
    Atalasify(tree, iBrick, atlasSize, pData);
    
    // write updated data to disk
    const uint64_t iUncompressedBrickSize = tree.ComputeBrickSize(tree.IndexToBrickCoords(iBrick)).volume() * tree.GetComponentTypeSize() * tree.GetComponentCount();
    const TOCEntry t = {(e.m_vTOC.end()-1)->m_iLength+(e.m_vTOC.end()-1)->m_iOffset, iUncompressedBrickSize, CT_NONE, iUncompressedBrickSize, atlasSize};
    e.m_vTOC.push_back(t);

    WriteBrickToDisk(e, pData, iBrick);
  }


  delete [] pData;

  // write updated ToC to file
  e.WriteHeader(pLargeRAWFile, iOffset);

  return false;}


bool ExtendedOctreeConverter::Atalasify(ExtendedOctree &tree,                         
                                         const UINTVECTOR2& atlasSize) {

  bool bTreeWasInRWModeAlready = tree.IsInRWMode();

  if (!bTreeWasInRWModeAlready)
    if (!tree.ReOpenRW()) return false;

  size_t CacheElementDataSize = size_t(tree.GetComponentTypeSize() * tree.GetComponentCount() * tree.m_iBrickSize.volume());
  unsigned char* pData = new unsigned char[CacheElementDataSize];

  // go throught all bricks and convert them to atlantified format
  for (size_t iBrick = 0;iBrick<tree.m_vTOC.size();iBrick++) {
    // if brick is ok -> skip it
    if (tree.m_vTOC[iBrick].m_iAtlasSize == atlasSize) continue;

    // this method shall not be called on trees with compressed bricks
    if (tree.m_vTOC[iBrick].m_eCompression != CT_NONE)  {
      if (!bTreeWasInRWModeAlready) tree.ReOpenR();
      return false;
    }

    // convert
    Atalasify(tree, iBrick, atlasSize, pData);

    // write updated data to disk
    tree.m_pLargeRAWFile->SeekPos(tree.m_iOffset+tree.m_vTOC[iBrick].m_iOffset);
    tree.m_vTOC[iBrick].m_iAtlasSize = atlasSize;
    tree.m_pLargeRAWFile->WriteRAW(pData, tree.m_vTOC[iBrick].m_iLength);
  }

  delete [] pData;

  // write updated ToC to file
  tree.WriteHeader(tree.m_pLargeRAWFile, tree.m_iOffset);

  if (!bTreeWasInRWModeAlready)
    if (!tree.ReOpenR()) return false;

  return true;
}

void ExtendedOctreeConverter::DeAtalasify(const ExtendedOctree &tree,
                                         const UINT64VECTOR4& vBrickCoords,
                                         uint8_t* pData) {

  DeAtalasify(tree, size_t(tree.BrickCoordsToIndex(vBrickCoords)), pData);
}

void ExtendedOctreeConverter::DeAtalasify(const ExtendedOctree &tree,
                                         size_t index,
                                         uint8_t* pData) {


  const TOCEntry& metaData = tree.GetBrickToCData(index);
  const UINTVECTOR3 maxBrickSize  = tree.GetMaxBrickSize();
  const UINT64VECTOR3 currBrickSize = tree.ComputeBrickSize(tree.IndexToBrickCoords(index));
  tree.GetBrickData(pData, index);

  // bail out if brick is atlantified and the size
  // is correct
  if (metaData.m_iAtlasSize.area() == 0) return;

  VolumeTools::DeAtalasify(size_t(currBrickSize.volume()*tree.GetComponentTypeSize()*tree.GetComponentCount()), metaData.m_iAtlasSize, maxBrickSize, currBrickSize, pData, pData);
}


bool ExtendedOctreeConverter::DeAtalasify(const ExtendedOctree &tree,
                                          LargeRAWFile_ptr pLargeRAWFile,
                                          uint64_t iOffset)
{
  ExtendedOctree e;

  // setup target metadata
  e.m_eComponentType = tree.m_eComponentType;
  e.m_iComponentCount = tree.m_iComponentCount;
  e.m_vVolumeSize = tree.m_vVolumeSize;
  e.m_vVolumeAspect = tree.m_vVolumeAspect;
  e.m_iBrickSize = tree.m_iBrickSize;
  e.m_iOverlap = tree.m_iOverlap;
  e.m_iOffset = iOffset;
  e.m_pLargeRAWFile = pLargeRAWFile;
  e.ComputeMetadata();

  size_t CacheElementDataSize = size_t(tree.GetComponentTypeSize() * tree.GetComponentCount() * tree.m_iBrickSize.volume());
  unsigned char* pData = new unsigned char[CacheElementDataSize];


  // go through all bricks and convert them to deatlantified format
  for (size_t iBrick = 0;iBrick<tree.m_vTOC.size();iBrick++) {
    // convert
    DeAtalasify(tree, iBrick, pData);
    
    // write updated data to disk
    const uint64_t iUncompressedBrickSize = tree.ComputeBrickSize(tree.IndexToBrickCoords(iBrick)).volume() * tree.GetComponentTypeSize() * tree.GetComponentCount();
    const TOCEntry t = {(e.m_vTOC.end()-1)->m_iLength+(e.m_vTOC.end()-1)->m_iOffset, iUncompressedBrickSize, CT_NONE, iUncompressedBrickSize, UINTVECTOR2(0,0)};
    e.m_vTOC.push_back(t);

    WriteBrickToDisk(e, pData, iBrick);
  }


  delete [] pData;

  // write updated ToC to file
  e.WriteHeader(pLargeRAWFile, iOffset);

  return false;
}


bool ExtendedOctreeConverter::DeAtalasify(ExtendedOctree &tree) {

  bool bTreeWasInRWModeAlready = tree.IsInRWMode();

  if (!bTreeWasInRWModeAlready)
    if (!tree.ReOpenRW()) return false;

  size_t CacheElementDataSize = size_t(tree.GetComponentTypeSize() * tree.GetComponentCount() * tree.m_iBrickSize.volume());
  unsigned char* pData = new unsigned char[CacheElementDataSize];

  // go throught all bricks and convert them to 3D brick format
  for (size_t iBrick = 0;iBrick<tree.m_vTOC.size();iBrick++) {
    // if brick is ok -> skip it
    if (tree.m_vTOC[iBrick].m_iAtlasSize.area() == 0) continue;

    // this method shall not be called on trees with compressed bricks
    if (tree.m_vTOC[iBrick].m_eCompression != CT_NONE)  {
      if (!bTreeWasInRWModeAlready) tree.ReOpenR();
      return false;
    }

    // convert
    DeAtalasify(tree, iBrick, pData);
    // write updated data to disk

    tree.m_pLargeRAWFile->SeekPos(tree.m_iOffset+tree.m_vTOC[iBrick].m_iOffset);
    tree.m_vTOC[iBrick].m_iAtlasSize = UINTVECTOR2(0,0);
    tree.m_pLargeRAWFile->WriteRAW(pData, tree.m_vTOC[iBrick].m_iLength);
  }

  delete [] pData;

  // write updated ToC to file
  tree.WriteHeader(tree.m_pLargeRAWFile, tree.m_iOffset);

  if (!bTreeWasInRWModeAlready)
    if (!tree.ReOpenR()) return false;

  return true;
}

