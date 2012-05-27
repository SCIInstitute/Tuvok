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
#include "ExtendedOctreeConverter.h"

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
                                      COMPORESSION_TYPE compression) {
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
                 vVolumeAspect, outFile, iOutOffset, stats, compression);
}

/*
  Convert:

  This is the main function where the conversion from a raw file to
  a bricked octree happens. It starts by creating a new ExtendedOctree
  object, passes the user's parameters on to that object and computes
  the header data. It creates LoD zero by permuting/bricking the input
  data, then it computes the hierarchy. As this hierarchy computation
  involves averaging we choose appropriate template at this point.
  Next, the function writes the header to file and flushes the
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
                                      COMPORESSION_TYPE compression) {
  m_pBrickStatVec = stats;
  m_fProgress = 0; // TODO: do something smart with the progress

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

  m_eCompression = compression;

  SetupCache(e);

  // brick (permute) the input data
  PermuteInputData(e, pLargeRAWFileIn, iInOffset);

  // compute hierarchy
  switch (e.m_eComponentType) {
    case ExtendedOctree::CT_UINT8:
      ComputeHierarchy<uint8_t>(e);
      break;
    case ExtendedOctree::CT_UINT16:
      ComputeHierarchy<uint16_t>(e);
      break;
    case ExtendedOctree::CT_UINT32:
      ComputeHierarchy<uint32_t>(e);
      break;
    case ExtendedOctree::CT_UINT64:
      ComputeHierarchy<uint64_t>(e);
      break;
    case ExtendedOctree::CT_INT8:
      ComputeHierarchy<int8_t>(e);
      break;
    case ExtendedOctree::CT_INT16:
      ComputeHierarchy<int16_t>(e);
      break;
    case ExtendedOctree::CT_INT32:
      ComputeHierarchy<int32_t>(e);
      break;
    case ExtendedOctree::CT_INT64:
      ComputeHierarchy<int64_t>(e);
      break;
    case ExtendedOctree::CT_FLOAT32:
      ComputeHierarchy<float>(e);
      break;
    case ExtendedOctree::CT_FLOAT64:
      ComputeHierarchy<double>(e);
      break;
  }

  // add header to file
  e.WriteHeader(pLargeRAWFileOut, iOutOffset);

  // write bricks in the cache to disk
  FlushCache(e);

  // remove part of the file used only for temp calculations
  TOCEntry lastBrickInFile = *(e.m_vTOC.end()-1);
  pLargeRAWFileOut->Truncate(iOutOffset+lastBrickInFile.m_iOffset+lastBrickInFile.m_iLength);

  m_fProgress = 1.0f;

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
                                            uint64_t iInOffset, const UINT64VECTOR4& coords) {
  UINT64VECTOR3 vBrickSize = tree.ComputeBrickSize(coords);
  uint64_t iBricksSize = (tree.m_vTOC.end()-1)->m_iLength;
  if (vData.size() != size_t(iBricksSize)) vData.resize(size_t(iBricksSize));

  // zero out the data (this makes sure boundaries are zero
  memset (&vData[0],0,size_t(iBricksSize));

  uint64_t iVoxelSize = tree.GetComponentTypeSize() * tree.GetComponentCount();
  uint64_t iMaxLineSize = vBrickSize.x * iVoxelSize;

  const UINT64VECTOR4 bricksInZeroLevel = tree.GetBrickCount(0);

  // first we figure out if the requested brick is a boundary brick
  // for boundary bricks we have to skip the overlap regions
  uint64_t xStart = (coords.x == 0) ? m_iOverlap : 0;
  uint64_t yStart = (coords.y == 0) ? m_iOverlap : 0;
  uint64_t zStart = (coords.z == 0) ? m_iOverlap : 0;
  uint64_t yEnd = vBrickSize.y - ((coords.y == bricksInZeroLevel.y-1) ? m_iOverlap : 0);
  uint64_t zEnd = vBrickSize.z - ((coords.z == bricksInZeroLevel.z-1) ? m_iOverlap : 0);

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
      uint64_t iCurrentInOffset = iInOffset + iVoxelSize * (0-(m_iOverlap-xStart) + coords.x * (m_vBrickSize.x-m_iOverlap*2) +
                                                         (y-(m_iOverlap-yStart) + coords.y * (m_vBrickSize.y-m_iOverlap*2)) * tree.m_vVolumeSize.x +
                                                         (z-(m_iOverlap-zStart) + coords.z * (m_vBrickSize.z-m_iOverlap*2)) * tree.m_vVolumeSize.x * tree.m_vVolumeSize.y);

      // the offset into the target array
      // this is just a simple 3D to 1D conversion of the current position
      // y & z plus the overlap skip (xStart, yStart, yStart) which we need
      // to add if this is the first brick (otherwise these are all 0
      size_t iOutOffset = size_t(iVoxelSize * (  xStart +
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
}

/*
  Compress:

  Apply compression requests and fill holes between compressed
  bricks by shifting bricks to the front.
*/
void ExtendedOctreeConverter::Compress(ExtendedOctree &tree, size_t iBrickSkip) {
  if (m_eCompression != CT_NONE) {
    const size_t iVoxelSize =tree.GetComponentTypeSize() * size_t(tree.m_iComponentCount);
    uint8_t *pBrickData = new uint8_t[size_t(tree.m_iBrickSize.volume() * iVoxelSize)];

    for (size_t i = iBrickSkip;i<tree.m_vTOC.size();++i) {
      GetBrick(pBrickData, tree, i);
      if (i>0)
        tree.m_vTOC[i].m_iOffset = tree.m_vTOC[i-1].m_iOffset+tree.m_vTOC[i-1].m_iLength;
      SetBrick(pBrickData, tree, i, true);
    }

    delete [] pBrickData;
  }
}

/*
  SetBrick (vector):

  Convenience function that takes vector coordinates of a brick
  turns them into a 1D index and calls the scalar SetBrick
*/
void ExtendedOctreeConverter::SetBrick(uint8_t* pData, ExtendedOctree &tree, const UINT64VECTOR4& vBrickCoords, bool bForceWrite) {
  SetBrick(pData, tree, tree.BrickCoordsToIndex(vBrickCoords), bForceWrite);
}

/*
  GetBrick (vector):

  Convenience function that takes vector coordinates of a brick
  turns them into a 1D index and calls the scalar GetBrick
*/
void ExtendedOctreeConverter::GetBrick(uint8_t* pData, ExtendedOctree &tree, const UINT64VECTOR4& vBrickCoords) {
  GetBrick(pData, tree, tree.BrickCoordsToIndex(vBrickCoords));
}

/*
  SetupCache:

  Computes the size of the cache: simply as available size divided by the size of a cache element
*/
void ExtendedOctreeConverter::SetupCache(ExtendedOctree &tree) {
  size_t CacheElementDataSize = size_t(tree.GetComponentTypeSize() * tree.GetComponentCount() * tree.m_iBrickSize.volume());
  uint64_t iCacheElemCount = m_iMemLimit / (CacheElementDataSize + sizeof(CacheEntry));
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
      WriteBrickToDisk(tree, i, m_pBrickStatVec, m_eCompression);
    }
  }
}

void ExtendedOctreeConverter::WriteBrickToDisk(ExtendedOctree &tree, BrickCacheIter element, BrickStatVec* pBrickStatVec, COMPORESSION_TYPE eCompression) {
  WriteBrickToDisk(tree, element->m_pData, element->m_index, pBrickStatVec, eCompression);
  element->m_bDirty = false;
}

void ExtendedOctreeConverter::WriteBrickToDisk(ExtendedOctree &tree, uint8_t* pData, size_t index, BrickStatVec* pBrickStatVec, COMPORESSION_TYPE eCompression) {
  if (pBrickStatVec) {
    const size_t cc = size_t(tree.m_iComponentCount);

    if (pBrickStatVec->size() < (index+1)*cc) {
      pBrickStatVec->resize((index+1)*cc);
    }

    BrickStatVec elem;

    // compute hierarchy
    switch (tree.m_eComponentType) {
      case ExtendedOctree::CT_UINT8:
        elem = ComputeBrickStats<uint8_t>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
      case ExtendedOctree::CT_UINT16:
        elem = ComputeBrickStats<uint16_t>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
      case ExtendedOctree::CT_UINT32:
        elem = ComputeBrickStats<uint32_t>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
      case ExtendedOctree::CT_UINT64:
        elem = ComputeBrickStats<uint64_t>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
      case ExtendedOctree::CT_INT8:
        elem = ComputeBrickStats<int8_t>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
      case ExtendedOctree::CT_INT16:
        elem = ComputeBrickStats<int16_t>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
      case ExtendedOctree::CT_INT32:
        elem = ComputeBrickStats<int32_t>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
      case ExtendedOctree::CT_INT64:
        elem = ComputeBrickStats<int64_t>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
      case ExtendedOctree::CT_FLOAT32:
        elem = ComputeBrickStats<float>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
      case ExtendedOctree::CT_FLOAT64:
        elem = ComputeBrickStats<double>(pData, tree.m_vTOC[index].m_iLength, cc);
        break;
    }

    for (size_t c = 0;c<cc;++c)
     (*pBrickStatVec)[index*cc+c] = elem[c];
  }

  // compress brick if requested and beneficial
  tree.m_pLargeRAWFile->SeekPos(tree.m_iOffset+tree.m_vTOC[index].m_iOffset);
  uint64_t uncompressedLength = tree.ComputeBrickSize(tree.IndexToBrickCoords(index)).volume() * tree.GetComponentTypeSize() * tree.GetComponentCount();
  if (eCompression != CT_NONE) {
    uint8_t* pCompressedData = new uint8_t[size_t(uncompressedLength)];
    uint64_t compressedLength = uncompressedLength;

    // TODO: compress into pCompressedData using m_eCompression method

    // did we gain anything from the compression?
    if (compressedLength < uncompressedLength) {
      tree.m_vTOC[index].m_iLength = compressedLength;
      tree.m_vTOC[index].m_eCompression = eCompression;
      tree.m_pLargeRAWFile->WriteRAW(pCompressedData, tree.m_vTOC[index].m_iLength);
    } else {
      tree.m_vTOC[index].m_iLength = uncompressedLength;
      tree.m_vTOC[index].m_eCompression = CT_NONE;
      tree.m_pLargeRAWFile->WriteRAW(pData, tree.m_vTOC[index].m_iLength);
    }

    delete [] pCompressedData;
  } else {
    tree.m_vTOC[index].m_iLength = uncompressedLength;
    tree.m_vTOC[index].m_eCompression = CT_NONE;
    tree.m_pLargeRAWFile->WriteRAW(pData, tree.m_vTOC[index].m_iLength);
  }
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

  Retrieves a brick from the tree. First we check if the cache is enabled, if not we simply request
  the brick from the tree. Otherwise we check the cache and, if we have a hit, return the cache copy
  otherwise we fetch the data from disk and put a copy into the cache, therefore we search for a suitable
  cache entry (the entry with the oldest access counter). In either case (hit or miss) we update the
  access counter, i.e. we use true LRU as caching strategy.
*/
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
      if (cacheEntry->m_bDirty) WriteBrickToDisk(tree, cacheEntry, m_pBrickStatVec, m_eCompression);
    }

    // put new entry into cache
    cacheEntry->m_bDirty = false;
    cacheEntry->m_index = size_t(index);
    cacheEntry->m_iAccess = ++m_iCacheAccessCounter;
    memcpy(cacheEntry->m_pData, pData, size_t(tree.m_vTOC[cacheEntry->m_index].m_iLength));
  } else {
    // cache hit
    memcpy(pData, cacheEntry->m_pData, size_t(tree.m_vTOC[size_t(index)].m_iLength));
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
    WriteBrickToDisk(tree, pData, size_t(index), m_pBrickStatVec, m_eCompression);
    return;
  }

  BrickCacheIter cacheEntry = std::find_if(m_vBrickCache.begin(), m_vBrickCache.end(), std::bind2nd(HasIndex(), index));

  if (cacheEntry == m_vBrickCache.end()) {
    // cache miss

    if (bForceWrite) {
      WriteBrickToDisk(tree, pData, size_t(index), m_pBrickStatVec, m_eCompression);
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
      if (cacheEntry->m_bDirty) WriteBrickToDisk(tree, cacheEntry, m_pBrickStatVec, m_eCompression);
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
    if (bForceWrite) WriteBrickToDisk(tree, cacheEntry, m_pBrickStatVec, m_eCompression);
  }

}


/*
  CopyBrickToBrick:

  Copies (parts) of one brick into another. This routine is used to fill the
  overlap regions. Index Magic explained in he function.
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
void ExtendedOctreeConverter::FillOverlap(ExtendedOctree &tree, uint64_t iLoD) {
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

        // now that brick is complete, write it back to the tree
        SetBrick(&vTargetData[0], tree, coords);
      }
    }
  }

}

/*
  PermuteInputData:

  This method reorders the large input raw file into smaller bricks
  of maximum size m_vBrickSize with an overlap of m_iOverlap i.e.
  it computes LoD level zero. Therefore, it iterates over all bricks
  to be created and grabs each from the source data. In the process it
  also fills the tree ToC.
*/
void ExtendedOctreeConverter::PermuteInputData(ExtendedOctree &tree, LargeRAWFile_ptr pLargeRAWFileIn, uint64_t iInOffset) {
  std::vector<uint8_t> vData;
  UINT64VECTOR3 baseBricks = tree.GetBrickCount(0);

  uint64_t iCurrentOutOffset = tree.ComputeHeaderSize();
  for (uint64_t z = 0;z<baseBricks.z;z++) {
    for (uint64_t y = 0;y<baseBricks.y;y++) {
      for (uint64_t x = 0;x<baseBricks.x;x++) {
        UINT64VECTOR4 coords(x,y,z,0);
        uint64_t iUncompressedBrickSize = tree.ComputeBrickSize(UINT64VECTOR4(x,y,z,0)).volume() * tree.GetComponentTypeSize() * tree.GetComponentCount();
        TOCEntry t = {iCurrentOutOffset, iUncompressedBrickSize, CT_NONE, iUncompressedBrickSize, UINTVECTOR2(0,0)};
        tree.m_vTOC.push_back(t);

        GetInputBrick(vData, tree, pLargeRAWFileIn, iInOffset, coords);
        SetBrick(&(vData[0]), tree, coords);

        iCurrentOutOffset += iUncompressedBrickSize;
      }
    }
  }
  // apply compression to the first LoD
  Compress(tree,0);
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
  const UINT64VECTOR3 maxBrickSize  = tree.GetMaxBricksize();
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
                                         uint64_t iOffset,
                                         COMPORESSION_TYPE eCompression) {
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


  // go throught all bricks and convert them to deatlantified format
  for (size_t iBrick = 0;iBrick<tree.m_vTOC.size();iBrick++) {
    // convert
    Atalasify(tree, iBrick, atlasSize, pData);
    
    // write updated data to disk
    const uint64_t iUncompressedBrickSize = tree.ComputeBrickSize(tree.IndexToBrickCoords(iBrick)).volume() * tree.GetComponentTypeSize() * tree.GetComponentCount();
    const TOCEntry t = {(e.m_vTOC.end()-1)->m_iLength+(e.m_vTOC.end()-1)->m_iOffset, iUncompressedBrickSize, CT_NONE, iUncompressedBrickSize, atlasSize};
    e.m_vTOC.push_back(t);

    WriteBrickToDisk(e, pData, iBrick, NULL, eCompression);
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
  const UINT64VECTOR3 maxBrickSize  = tree.GetMaxBricksize();
  const UINT64VECTOR3 currBrickSize = tree.ComputeBrickSize(tree.IndexToBrickCoords(index));
  tree.GetBrickData(pData, index);

  // bail out if brick is atlantified and the size
  // is correct
  if (metaData.m_iAtlasSize.area() == 0) return;

  VolumeTools::DeAtalasify(size_t(currBrickSize.volume()*tree.GetComponentTypeSize()*tree.GetComponentCount()), metaData.m_iAtlasSize, maxBrickSize, currBrickSize, pData, pData);
}


bool ExtendedOctreeConverter::DeAtalasify(const ExtendedOctree &tree,
                                           LargeRAWFile_ptr pLargeRAWFile,
                                           uint64_t iOffset,
                                           COMPORESSION_TYPE eCompression) {
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


  // go throught all bricks and convert them to deatlantified format
  for (size_t iBrick = 0;iBrick<tree.m_vTOC.size();iBrick++) {
    // convert
    DeAtalasify(tree, iBrick, pData);
    
    // write updated data to disk
    const uint64_t iUncompressedBrickSize = tree.ComputeBrickSize(tree.IndexToBrickCoords(iBrick)).volume() * tree.GetComponentTypeSize() * tree.GetComponentCount();
    const TOCEntry t = {(e.m_vTOC.end()-1)->m_iLength+(e.m_vTOC.end()-1)->m_iOffset, iUncompressedBrickSize, CT_NONE, iUncompressedBrickSize, UINTVECTOR2(0,0)};
    e.m_vTOC.push_back(t);

    WriteBrickToDisk(e, pData, iBrick, NULL, eCompression);
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

