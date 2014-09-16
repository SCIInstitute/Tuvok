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

#include <stdexcept>
#include "ExtendedOctree.h"
#include "Basics/nonstd.h"
#include "Basics/Timer.h"
#include "Controller/Controller.h"
#include "Controller/StackTimer.h"
#include "ZlibCompression.h"
#include "LzmaCompression.h"
#include "Lz4Compression.h"
#include "BzlibCompression.h"

ExtendedOctree::ExtendedOctree() :
  m_eComponentType(CT_UINT8), 
  m_iComponentCount(0), 
  m_bPrecomputedNormals(false),
  m_vVolumeSize(0,0,0), 
  m_vVolumeAspect(0,0,0), 
  m_iBrickSize(0,0,0), 
  m_iOverlap(0), 
  m_iVersion(2), // increment version number here if something changes...
  m_iSize(0),
  m_iCompressionLevel(4), // our default level for LZMA, it's fast and still compresses well
  m_iOffset(0), 
  m_pLargeRAWFile()
{}

void ExtendedOctree::InitLzmaCompression()
{
  try {
    lzmaProperties(m_lzmaProps, m_iCompressionLevel);
  } catch (std::exception const&) {
    assert("could not init LZMA compression properties" && false);
    throw;
  }
}

/*
 Open (string):
 
 Convenience function that calls the open below with a large raw file 
 constructed from the given string
 */
bool ExtendedOctree::Open(std::string filename, uint64_t iOffset,
                          uint64_t iUVFFileVersion) {
  LargeRAWFile_ptr inFile(new LargeRAWFile(filename));
  if (!inFile->Open()) {
    return false;
  }
  return Open(inFile, iOffset, iUVFFileVersion);
}

/*
 Open (largeRawFile):
 
 Reads the header from and computes derived metadata. The header
 can be split into two sections. First, the basic header with with 
 global such as the size of the original volume, the aspect ratio, the
 maximum brick size, and overlap. Second, the table of contents (ToC) 
 which contains per brick information about their sizes, compression 
 methods and offsets in the file. After reading the global information
 about the level of detail, it can be computed.
*/
bool ExtendedOctree::Open(LargeRAWFile_ptr pLargeRAWFile, uint64_t iOffset,
                          uint64_t iUVFFileVersion) {
  if (!pLargeRAWFile->IsOpen()) return false;
  m_pLargeRAWFile = pLargeRAWFile;
  m_iOffset = iOffset;

  const bool isBE = EndianConvert::IsBigEndian();

  // load global header
  m_pLargeRAWFile->SeekPos(m_iOffset);
  uint32_t comp;
  m_pLargeRAWFile->ReadData(comp, isBE);
  m_eComponentType = static_cast<COMPONENT_TYPE>(comp);
  m_pLargeRAWFile->ReadData(m_iComponentCount, isBE);
  m_pLargeRAWFile->ReadData(m_bPrecomputedNormals, isBE);
  m_pLargeRAWFile->ReadData(m_vVolumeSize.x, isBE);
  m_pLargeRAWFile->ReadData(m_vVolumeSize.y, isBE);
  m_pLargeRAWFile->ReadData(m_vVolumeSize.z, isBE);
  m_pLargeRAWFile->ReadData(m_vVolumeAspect.x, isBE);
  m_pLargeRAWFile->ReadData(m_vVolumeAspect.y, isBE);
  m_pLargeRAWFile->ReadData(m_vVolumeAspect.z, isBE);
  m_pLargeRAWFile->ReadData(m_iBrickSize.x, isBE);
  m_pLargeRAWFile->ReadData(m_iBrickSize.y, isBE);
  m_pLargeRAWFile->ReadData(m_iBrickSize.z, isBE);
  m_pLargeRAWFile->ReadData(m_iOverlap, isBE);

  // UVF file version 5 introduced the version flag inside ExtendedOctree data
  if (iUVFFileVersion > 4) {
    m_pLargeRAWFile->ReadData(m_iVersion, isBE);
    assert(m_iVersion != 0); // doesn't make sense, probably means corrupt file
    if (m_iVersion == 0) return false;
  } else
    m_iVersion = 0; // version is not stored

  if (m_iVersion > 0)
    m_pLargeRAWFile->ReadData(m_iSize, isBE);
  if (m_iVersion > 1)
    m_pLargeRAWFile->ReadData(m_iCompressionLevel, isBE);

  // if any of the above numbers (except for the overlap) 
  // is zero than there must have been an issue reading the file
  if (m_iComponentCount * m_vVolumeSize.volume() * 
      m_vVolumeAspect.volume() * m_iBrickSize.volume() == 0) return false;

  // if the dataset is supposed to contain precomputed normals
  // it must have four components (data + 3D normal)
  if (m_bPrecomputedNormals && m_iComponentCount != 4) return false;

  // compute metadata
  ComputeMetadata();
  uint64_t iOverallBrickCount = ComputeBrickCount();

  // read brick TOC
  m_vTOC.resize(size_t(iOverallBrickCount));
  if (m_iVersion > 0) {
    for (size_t i = 0;i<iOverallBrickCount;i++) {
      m_pLargeRAWFile->ReadData(m_vTOC[i].m_iOffset, isBE);
      m_pLargeRAWFile->ReadData(m_vTOC[i].m_iLength, isBE);
      uint32_t comp;
      m_pLargeRAWFile->ReadData(comp, isBE);
      m_vTOC[i].m_eCompression = static_cast<COMPRESSION_TYPE>(comp);
      m_pLargeRAWFile->ReadData(m_vTOC[i].m_iValidLength, isBE);
      m_pLargeRAWFile->ReadData(m_vTOC[i].m_iAtlasSize.x, isBE);
      m_pLargeRAWFile->ReadData(m_vTOC[i].m_iAtlasSize.y, isBE);
    }
  } else {
    uint64_t iLoDOffset = ComputeHeaderSize();
    for (size_t i = 0;i<iOverallBrickCount;i++) {
      m_vTOC[i].m_iOffset = iLoDOffset;
      m_pLargeRAWFile->ReadData(m_vTOC[i].m_iLength, isBE);
      uint32_t comp;
      m_pLargeRAWFile->ReadData(comp, isBE);
      m_vTOC[i].m_eCompression = static_cast<COMPRESSION_TYPE>(comp);
      iLoDOffset += m_vTOC[i].m_iLength;
    }
  }

  return true;
}

/*
 Close:
 
 closes the underlying large raw file, after this call the Extended octree
 should not be used unless another open call is performed
*/
void ExtendedOctree::Close() {
  if ( m_pLargeRAWFile != LargeRAWFile_ptr()) 
    m_pLargeRAWFile->Close();
}

/*
 ComputeMetadata:
 
 This method computes all the metadata that is not directly stored in
 the file but can be derived from the global header. This is basically
 the size of each LoD level (from which we can compute the brick dimensions),
 the aspect ratio changes of each level, the brick count, and the brick offset.
 This offset (in bricks, not in bytes) describes were in the bricklist (ToC) 
 the bricks for an LoD level are stored.
*/
void ExtendedOctree::ComputeMetadata() {
  // compute LOD metadata (except for the m_iLoDOffsets
  // which are computed afterwards)
  UINT64VECTOR3 vVolumeSize = m_vVolumeSize;
  DOUBLEVECTOR3 vAspect(1.0,1.0,1.0);

  assert(this->GetMaxBrickSize()[0] > (2*m_iOverlap));
  assert(this->GetMaxBrickSize()[1] > (2*m_iOverlap));
  assert(this->GetMaxBrickSize()[2] > (2*m_iOverlap));
  const UINTVECTOR3 vUsableBrickSize = this->GetMaxBrickSize() - 2*m_iOverlap;
  do { 
    LODInfo l;
    l.m_iLODPixelSize = vVolumeSize;

    // downsample the volume (except for the first LoD)
    if (!m_vLODTable.empty())  {
      if (vVolumeSize.x > 1) {
        l.m_iLODPixelSize.x = uint64_t(ceil(vVolumeSize.x/2.0));
        vAspect.x *= (vVolumeSize.x%2) ? float(vVolumeSize.x)/float(l.m_iLODPixelSize.x) : 2;
      } 

      if (vVolumeSize.y > 1) {
        l.m_iLODPixelSize.y = uint64_t(ceil(vVolumeSize.y/2.0));
        vAspect.y *= (vVolumeSize.y%2) ? float(vVolumeSize.y)/float(l.m_iLODPixelSize.y) : 2;
      }
      if (vVolumeSize.z > 1) {
        l.m_iLODPixelSize.z = uint64_t(ceil(vVolumeSize.z/2.0));
        vAspect.z *= (vVolumeSize.z%2) ? float(vVolumeSize.z)/float(l.m_iLODPixelSize.z) : 2;
      }
      vAspect /= vAspect.maxVal();

      vVolumeSize = l.m_iLODPixelSize;
    }

    l.m_vAspect = vAspect;
    l.m_iLODBrickCount.x = uint64_t(ceil(vVolumeSize.x /
                                         double(vUsableBrickSize.x)));
    l.m_iLODBrickCount.y = uint64_t(ceil(vVolumeSize.y /
                                         double(vUsableBrickSize.y)));
    l.m_iLODBrickCount.z = uint64_t(ceil(vVolumeSize.z /
                                         double(vUsableBrickSize.z)));

    l.m_iLoDOffset = 0; // don't know it yet, but for now quiet a warning.
    m_vLODTable.push_back(l);
  } while (vVolumeSize.x > 1 ||
           vVolumeSize.y > 1 ||
           vVolumeSize.z > 1);

  // fill m_iLoDOffsets
  m_vLODTable[0].m_iLoDOffset = 0;
  for (size_t i = 1;i<m_vLODTable.size();i++) {
    m_vLODTable[i].m_iLoDOffset = m_vLODTable[i-1].m_iLoDOffset + m_vLODTable[i-1].m_iLODBrickCount.volume();
  }

  InitLzmaCompression();
}

/*
 GetBrickCount:
 
 Accessor for the m_iLODBrickCount variable of the LoD Table
*/
UINT64VECTOR3 ExtendedOctree::GetBrickCount(uint64_t iLOD) const {
  return m_vLODTable[size_t(iLOD)].m_iLODBrickCount;
}

/*
 GetLoDSize:
 
 Accessor for the m_iLODPixelSize variable of the LoD Table
 */
UINT64VECTOR3 ExtendedOctree::GetLoDSize(uint64_t iLOD) const {
  return m_vLODTable[size_t(iLOD)].m_iLODPixelSize;
}

/*
 ComputeBrickSize:
 
 computes the size of a given brick. The idea is as follows: Any inner brick
 has the maximum m_iBrickSize as size, if it's the last brick it may
 be smaller, so first we determine of the brick is a boundary brick in x, y, and
 z. If yes then we compute it's size, which is the remainder of what we get when
 we divide the size of the LoD by the effective size of a brick, i.e. the size
 minus the two overlaps. To this required size we add the two overlaps back.
 One special case needs to observed that is if the boundary brick has to have
 the maximum size in this case we also return the maximum size (just like with
 inner bricks).
*/
UINT64VECTOR3 ExtendedOctree::ComputeBrickSize(const UINT64VECTOR4& vBrickCoords) const {
  const VECTOR3<bool> bIsLast = IsLastBrick(vBrickCoords);
  const UINT64VECTOR3 iPixelSize = m_vLODTable[size_t(vBrickCoords.w)].m_iLODPixelSize;
  const uint32_t i2Overlap = 2*m_iOverlap;
  const UINTVECTOR3 iBrickCore = this->GetMaxBrickSize() - i2Overlap;

  return UINT64VECTOR3(bIsLast.x && (iPixelSize.x % iBrickCore.x) ? (i2Overlap + (iPixelSize.x % iBrickCore.x)) : m_iBrickSize.x,
                       bIsLast.y && (iPixelSize.y % iBrickCore.y) ? (i2Overlap + (iPixelSize.y % iBrickCore.y)) : m_iBrickSize.y,
                       bIsLast.z && (iPixelSize.z % iBrickCore.z) ? (i2Overlap + (iPixelSize.z % iBrickCore.z)) : m_iBrickSize.z);
}

/*
 GetBrickAspect:
 
 the aspect ratio of the LoD (can be different from 1:1:1 due to anisotropic downsampling)
*/
DOUBLEVECTOR3 ExtendedOctree::GetBrickAspect(const UINT64VECTOR4& vBrickCoords) const {  
  return m_vLODTable[size_t(vBrickCoords.w)].m_vAspect;
}


const TOCEntry& ExtendedOctree::GetBrickToCData(const UINT64VECTOR4& vBrickCoords) const {
  return m_vTOC[size_t(BrickCoordsToIndex(vBrickCoords))];
}

const TOCEntry& ExtendedOctree::GetBrickToCData(size_t index) const {
  return m_vTOC[index];
}

/*
 GetBrickData (scalar):
 
 Reads a brick from file and decompresses it if necessary. No magic here it 
 simply seeks to the position in the file, which is the header offset + the 
 brick-offset from the header, and then reads the data. Finally, checks if
 decompression is required.
*/ 
void ExtendedOctree::GetBrickData(uint8_t* pData, uint64_t index) const {

  tuvok::Controller::Instance().IncrementPerfCounter(PERF_EO_BRICKS, 1.0);

  if(m_vTOC[size_t(index)].m_eCompression == CT_NONE) {
    // not compressed, just read it directly into the buffer.
    tuvok::StackTimer t(PERF_EO_DISK_READ);
    m_pLargeRAWFile->SeekPos(m_iOffset+m_vTOC[size_t(index)].m_iOffset);
    m_pLargeRAWFile->ReadRAW(pData, m_vTOC[size_t(index)].m_iLength);
    return;
  }

  // the data are compressed; read them into a temporary buffer and then expand
  // that buffer into 'pData'.
  const size_t uncompressedSize =
    this->ComputeBrickSize(this->IndexToBrickCoords(index)).volume() *
    this->GetComponentCount() *
    this->GetComponentTypeSize();

  std::shared_ptr<uint8_t> buf(new uint8_t[uncompressedSize],
                               nonstd::DeleteArray<uint8_t>());
  std::shared_ptr<uint8_t> out(pData, nonstd::null_deleter());
  TimedStatement(PERF_EO_DISK_READ,
    m_pLargeRAWFile->SeekPos(m_iOffset+m_vTOC[size_t(index)].m_iOffset);
    m_pLargeRAWFile->ReadRAW(buf.get(), m_vTOC[size_t(index)].m_iLength);
  );
  tuvok::StackTimer decompress(PERF_EO_DECOMPRESSION);
  switch (m_vTOC[size_t(index)].m_eCompression) {
  case CT_ZLIB:
    zDecompress(buf, out, uncompressedSize);
    break;
  case CT_LZMA:
    lzmaDecompress(buf, out, uncompressedSize, m_lzmaProps);
    break;
  case CT_LZ4:
    lz4Decompress(buf, out, uncompressedSize);
    break;
  case CT_BZLIB:
    bzDecompress(buf, size_t(m_vTOC[size_t(index)].m_iLength),
                 out, uncompressedSize);
    break;
  case CT_LZHAM:
    throw std::runtime_error("lzham compression format is not supported anymore by Tuvok");
    break;
  default:
    throw std::runtime_error("unknown compression format");
  }
}

/*
 GetBrickData (vector):

 Convenience function that calls the function above after computing the 1D
 index from the brick coordinates
*/ 
void ExtendedOctree::GetBrickData(uint8_t* pData, const UINT64VECTOR4& vBrickCoords) const {
  GetBrickData(pData, BrickCoordsToIndex(vBrickCoords));
}

/*
 IsLastBrick:
 
 Computes whether a brick is the last brick in a row, column, or slice. 
 To do this we simply fetch the brick count of the brick's LoD and
 then check if it's index is equal to the max index. 
*/ 
VECTOR3<bool> ExtendedOctree::IsLastBrick(const UINT64VECTOR4& vBrickCoords) const {
  const UINT64VECTOR3 vLODSize = m_vLODTable[size_t(vBrickCoords.w)].m_iLODBrickCount;

  const VECTOR3<bool> res( vBrickCoords.x >= vLODSize.x-1,
                           vBrickCoords.y >= vLODSize.y-1,
                           vBrickCoords.z >= vLODSize.z-1);
  return res;
}

/*
 BrickCoordsToIndex:
 
 Computes the 1D index from a coordinate vector this is the offset introduced
 by the LoD level + the index within that LoD 
*/ 
uint64_t ExtendedOctree::BrickCoordsToIndex(const UINT64VECTOR4& vBrickCoords) const {
  const UINT64VECTOR3 vLODSize = m_vLODTable[size_t(vBrickCoords.w)].m_iLODBrickCount;

  return m_vLODTable[size_t(vBrickCoords.w)].m_iLoDOffset + 
         vBrickCoords.x + 
         vBrickCoords.y * vLODSize.x + 
         vBrickCoords.z * vLODSize.x * vLODSize.y;
}


/*
 IndexToBrickCoords:
 
 Computes the 4D coordinates for the 1D ToC index 
*/ 
UINT64VECTOR4 ExtendedOctree::IndexToBrickCoords(uint64_t index) const {
  UINT64VECTOR4 vBrickCoords(0,0,0,0);

  for (size_t i = 0;i<m_vLODTable.size();++i) {
    if (m_vLODTable[i].m_iLoDOffset <= index) {
      vBrickCoords.w = i;
    } else {
      break;
    }
  }
  index -= m_vLODTable[size_t(vBrickCoords.w)].m_iLoDOffset;

  UINT64VECTOR3 brickCount = m_vLODTable[size_t(vBrickCoords.w)].m_iLODBrickCount;
  vBrickCoords.x = index % brickCount.x;
  vBrickCoords.y = (index / brickCount.x) % brickCount.y;
  vBrickCoords.z = index / (brickCount.x*brickCount.y);
  return vBrickCoords;
}

/*
 SetGlobalAspect:
 
 Changes the "global" aspect ratio in the header, therefore reopen the file
 in read write, change the few bytes in the header and then reopen the in
 read only again
*/ 
bool ExtendedOctree::SetGlobalAspect(const DOUBLEVECTOR3& vVolumeAspect) {
  bool bTreeWasInRWModeAlready = IsInRWMode();
  if (!bTreeWasInRWModeAlready && !ReOpenRW()) return false;

  const bool isBE = EndianConvert::IsBigEndian();

  // change aspect ratio entries
  m_vVolumeAspect = vVolumeAspect;
  m_pLargeRAWFile->SeekPos(m_iOffset +
                           sizeof(uint32_t /*m_eComponentType*/) +
                           sizeof(uint64_t /*m_iComponentCount*/) +
                           sizeof(bool /*m_bPrecomputedNormals*/) +
                           3 * sizeof(uint64_t /*m_vVolumeSize*/));
  m_pLargeRAWFile->WriteData(m_vVolumeAspect.x, isBE);
  m_pLargeRAWFile->WriteData(m_vVolumeAspect.y, isBE);
  m_pLargeRAWFile->WriteData(m_vVolumeAspect.z, isBE);

  if (!bTreeWasInRWModeAlready && !ReOpenR()) return false;

  return true;
}

/*
 ComputeBrickCount:
 
 "Computes" the total number of bricks in the file, by adding the number of
 bricks of the last LoD to it's brick offset. This function can be used before
 a valid brick ToC exists, once that has been constructed the overall brick 
 count can also be computer as m_vTOC.size()
*/
uint64_t ExtendedOctree::ComputeBrickCount() const {
  return (m_vLODTable.end()-1)->m_iLoDOffset + 
         (m_vLODTable.end()-1)->m_iLODBrickCount.volume();
}

/*
 ComputeHeaderSize:
 
 The size of the header is simply a bunch of sizeof calls of the contents
 the global header + brickcount times what we store per brick (i.e. it's
 length in bytes and the compression method)
*/
uint64_t ExtendedOctree::ComputeHeaderSize() const {
  return
    sizeof(uint32_t /*m_eComponentType*/) +
    sizeof(uint64_t /*m_iComponentCount*/) +
    (m_iVersion > 0 ? sizeof(bool /*m_bPrecomputedNormals*/) : 0) +
    3 * sizeof(uint64_t /*m_vVolumeSize*/) +
    3 * sizeof(double /*m_vVolumeAspect*/) +
    3 * (m_iVersion > 0 ? sizeof(uint64_t /*m_iBrickSize*/) : sizeof(uint32_t)) +
    sizeof(uint32_t /*m_iOverlap*/) +
    (m_iVersion > 0 ? sizeof(uint32_t /*m_iVersion*/) : 0) +
    (m_iVersion > 0 ? sizeof(uint64_t /*m_iSize*/) : 0) +
    (m_iVersion > 1 ? sizeof(uint32_t /*m_iCompressionLevel*/) : 0) +
    ComputeBrickCount() * TOCEntry::SizeInFile(m_iVersion);
}

/*
 WriteHeader:
 
 Nothing special here, simply stores global header and then the ToC. As the
 size of the ToC can be computed from the global header info alone (using the
 ComputeBrickCount method) we don't need to store the length of the ToC
*/
void ExtendedOctree::WriteHeader(LargeRAWFile_ptr pLargeRAWFile,
                                 uint64_t iOffset) {
  m_pLargeRAWFile = pLargeRAWFile;
  m_iOffset = iOffset;

  assert(m_iComponentCount);
  assert(m_vVolumeSize.volume() > 0);
  assert(m_vVolumeAspect.volume() > 0);
  assert(m_iBrickSize.volume() > 0);

  // write global header
  const bool isBE = EndianConvert::IsBigEndian();
  m_pLargeRAWFile->SeekPos(m_iOffset);
  m_pLargeRAWFile->WriteData(uint32_t(m_eComponentType), isBE);
  m_pLargeRAWFile->WriteData(m_iComponentCount, isBE);
  m_pLargeRAWFile->WriteData(m_bPrecomputedNormals, isBE);
  m_pLargeRAWFile->WriteData(m_vVolumeSize.x, isBE);
  m_pLargeRAWFile->WriteData(m_vVolumeSize.y, isBE);
  m_pLargeRAWFile->WriteData(m_vVolumeSize.z, isBE);
  m_pLargeRAWFile->WriteData(m_vVolumeAspect.x, isBE);
  m_pLargeRAWFile->WriteData(m_vVolumeAspect.y, isBE);
  m_pLargeRAWFile->WriteData(m_vVolumeAspect.z, isBE);
  m_pLargeRAWFile->WriteData(m_iBrickSize.x, isBE);
  m_pLargeRAWFile->WriteData(m_iBrickSize.y, isBE);
  m_pLargeRAWFile->WriteData(m_iBrickSize.z, isBE);
  m_pLargeRAWFile->WriteData(m_iOverlap, isBE);
  if (m_iVersion > 0) {
    m_pLargeRAWFile->WriteData(m_iVersion, isBE);
    m_pLargeRAWFile->WriteData(m_iSize, isBE);
  }
  if (m_iVersion > 1) {
    m_pLargeRAWFile->WriteData(m_iCompressionLevel, isBE);
  }

  // write ToC
  if (m_iVersion > 0) {
    for (size_t i = 0;i<m_vTOC.size();i++) {
      m_pLargeRAWFile->WriteData(m_vTOC[i].m_iOffset, isBE);
      m_pLargeRAWFile->WriteData(m_vTOC[i].m_iLength, isBE);
      m_pLargeRAWFile->WriteData(uint32_t(m_vTOC[i].m_eCompression), isBE);
      m_pLargeRAWFile->WriteData(m_vTOC[i].m_iValidLength, isBE);
      m_pLargeRAWFile->WriteData(m_vTOC[i].m_iAtlasSize.x, isBE);
      m_pLargeRAWFile->WriteData(m_vTOC[i].m_iAtlasSize.y, isBE);
    }
  } else {
    for (size_t i = 0;i<m_vTOC.size();i++) {
      m_pLargeRAWFile->WriteData(m_vTOC[i].m_iLength, isBE);
      m_pLargeRAWFile->WriteData(uint32_t(m_vTOC[i].m_eCompression), isBE);
    }
  }
}

/*
 GetComponentTypeSize:
 
 This is simply a big switch that turns our type enum into it's size semantic.
*/
uint32_t ExtendedOctree::GetComponentTypeSize(COMPONENT_TYPE t) {
  switch (t) {
    case CT_INT8:
    case CT_UINT8:
      return 1;
    case CT_INT16:
    case CT_UINT16:
      return 2;
    case CT_FLOAT32:
    case CT_INT32:
    case CT_UINT32:
      return 4;
    case CT_FLOAT64:
    case CT_INT64:
    case CT_UINT64:
      return 8;      
    default:
      return 0;
  }
}

/*
 GetComponentTypeSize:
 
 Convenience function that calls the above method with this' tree's data type
*/
size_t ExtendedOctree::GetComponentTypeSize() const {
  return GetComponentTypeSize(m_eComponentType);
}


bool ExtendedOctree::ReOpenRW() {
  if (IsInRWMode()) return true;

  // close read-only file
  m_pLargeRAWFile->Close();

  // re-open in read/write mode
  if (m_pLargeRAWFile->Open(true)) {
    
    // if opening in rw failed, return to read only mode
    m_pLargeRAWFile->Open(false);
    return false;
  }
  return true;
}

bool ExtendedOctree::ReOpenR() {
  if (!IsInRWMode()) return true;

  m_pLargeRAWFile->Close();
  return m_pLargeRAWFile->Open(false);
}
