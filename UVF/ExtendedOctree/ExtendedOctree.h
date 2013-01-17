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

#pragma once

#ifndef EXTENDEDOCTREE_H
#define EXTENDEDOCTREE_H

#include "Basics/StdDefines.h"

#include <memory>

#include "Basics/LargeRAWFile.h"
// for the small fixed size vectors
#include "Basics/Vectors.h"

/*! \brief This structure holds information about a specific level in the tree
 *
 *  This structure holds information about a specific level of the tree,
 *  such as the overall size of the level in pixels and in bricks,
 *  and the aspect ratio (which may vary from level to level if we
 *  downsample anisotropically)
 */
struct LODInfo {
  /// the aspect ratio of all bricks in this LoD, does not
  /// take the overall aspect into account just the aspect
  /// ratio changes that happen during the downsampling
  DOUBLEVECTOR3 m_vAspect;
  /// size of the entire LoD in pixels, i.e. for LoD 0 this
  /// is equal to the size of the original dataset
  UINT64VECTOR3 m_iLODPixelSize;
  /// number of bricks in x, y, and z in this LoD
  UINT64VECTOR3 m_iLODBrickCount;
  /// sum of all m_iLODBrickCount.volume() of the all the
  /// lower LoD, e.g. for LoD with index 2 this would be
  /// lod[0].m_iLODBrickCount.volume() + lod[1].m_iLODBrickCount.volume()
  /// it does NOT take the current LoD into account and is
  /// just the offset
  uint64_t m_iLoDOffset;
};

/// This enum lists the different compression techniques
enum COMPRESSION_TYPE {
  CT_NONE = 0,    // brick is not compressed
  CT_ZLIB,        // brick is compressed using zlib
  CT_JPEG         // brick is (slice wise) compressed using JPG
};

/// This enum lists the different layouts how bricks are ordered on disk
enum LAYOUT_TYPE {
  LT_SCANLINE = 0,  // bricks are ordered in x, y, z scanline order where x is the fastest
  LT_MORTON,        // bricks are laid out according to Morton order (Z-order)
  LT_HILBERT,       // bricks are laid out according to Hilbert space-filling curve
  LT_RANDOM,        // bricks are laid out randomly, just to check the worst case
  LT_UNKNOWN
};

/*! \brief This structure holds information about a specific brick in the tree
 *
 *  This structure holds information about a specific brick in the tree,
 *  such the offset from the header, the length (or size) in bytes of the brick
 *  and the compression method
 */
struct TOCEntry {
  /// the offset from the (beginning) of the header to this brick
  /// i.e. the offset for the very first brick in the file is the
  /// size of the header
  uint64_t m_iOffset;

  /// the length or size of this brick in bytes
  uint64_t m_iLength;

  /// the compression scheme of this brick
  COMPRESSION_TYPE m_eCompression;

  /// valid bytes in this brick (used for streaming files)
  /// for a complete brick m_iLength is equal to m_iValidLength
  uint64_t m_iValidLength;

  /// if this block is stored in "atlantified" format
  /// i.e. packed for 2D texture atlas representation
  /// then this variable holds the size of a 2D texture
  /// to store the atlas, otherwise at least one component
  /// is equal to zero
  UINTVECTOR2 m_iAtlasSize;

  // Returns the size of this struct it is basically the
  // the sum of sizeof calls to all members as that may
  // be different from sizeof(TOCEntry) due to compilers
  // padding this struct
  static size_t SizeInFile(uint64_t iVersion) {
    return (iVersion > 0 ? sizeof(uint64_t /*m_iOffset*/) : 0) +
           sizeof(uint64_t/*m_iLength*/) +
           sizeof(uint32_t /*m_eCompression*/) +
           sizeof(uint64_t /*m_iValidLength*/) +
           sizeof(UINTVECTOR2 /*m_iAtlasSize*/);
  }
};

// forward to the raw to brick converter, required for the
// friend declaration down below
class ExtendedOctreeConverter;

/*! \brief This class holds the actual octree data
 *
 *  This class holds the actual octree data
 */
class ExtendedOctree {
public:
  /// This enum lists the different data types
  enum COMPONENT_TYPE {
    CT_UINT8,
    CT_UINT16,
    CT_UINT32,
    CT_UINT64,
    CT_INT8,
    CT_INT16,
    CT_INT32,
    CT_INT64,
    CT_FLOAT32,
    CT_FLOAT64
  };

  /** does nothing more than set the internal state to fail safe values */
  ExtendedOctree();

  /**
    Reads the header information from an already open large raw file skipping iOffset bytes at the beginning
    @param  pLargeRAWFile the file the header is read from, file must be open already
    @param  iOffset the bytes to be skipped from the beginning of the file to get to the octree header
    @param  iUVFFileVersion UVF file version
    @return returns false if something went wrong trying to read from the file
  */
  bool Open(LargeRAWFile_ptr pLargeRAWFile, uint64_t iOffset, uint64_t iUVFFileVersion);

  /**
    Reads the header information from an file skipping iOffset bytes at the beginning
    @param  filename the name of the input file
    @param  iOffset the bytes to be skipped from the beginning of the file to get to the octree header
    @param  iUVFFileVersion UVF file version
    @return returns false if something went wrong trying to read from the file
  */
  bool Open(std::string filename, uint64_t iOffset, uint64_t iUVFFileVersion);


  /**
    Closes the underlying large raw file, after this call the Extended octree
    should not be used unless another open call is performed
  */
  void Close();

  /**
    Returns the component type enumerator of this octree
    @return returns the component type enumerator of this octree
  */
  COMPONENT_TYPE GetComponentType() const {return m_eComponentType;}

  /**
    Returns the value domain dimension
    @return the value domain dimension
  */
  uint64_t GetComponentCount() const {return m_iComponentCount;}

  /**
    Returns the true iff the dataset contains pre-computed normals
    @return true iff the dataset contains pre-computed normals
  */
  bool GetContainsPrecomputedNormals() const {return m_bPrecomputedNormals;}

  /**
    Returns the level of detail count, i.e. the depth of the tree
    @return the level of detail count, i.e. the depth of the tree
  */
  uint64_t GetLODCount() const {return m_vLODTable.size();}

  /**
    Returns the single sided overhead
    @return  the single sided overhead
  */
  uint32_t GetOverlap() const {return m_iOverlap;}

  /**
    Returns the brick size limit
    @return the brick size limit
  */
  UINTVECTOR3 GetMaxBrickSize() const { return UINTVECTOR3(m_iBrickSize); }

  /**
    Returns the number of bricks in a given LoD level. The result of this
    call is a vector that contains the brick count for each dimension
    to get the (scalar) number of bricks in  this level call .volume() on this
    @param iLoD index of the level to query (0 is highest resolution)
    @return the number of bricks in a given LoD level
  */
  UINT64VECTOR3 GetBrickCount(uint64_t iLOD) const;

  /**
    Returns the size in voxels of a given LoD level. The result of this
    call is a vector that contains the size for each dimension
    @param iLoD index of the level to query (0 is highest resolution)
    @return the number of voxels in a given LoD level
  */
  UINT64VECTOR3 GetLoDSize(uint64_t iLOD) const;

  /**
    Returns the size of a specific brick
    @param vBrickCoords coordinates of a brick: x,y,z are the spacial coordinates, w is the LoD level
    @return the size of a specific brick
  */
  UINT64VECTOR3 ComputeBrickSize(const UINT64VECTOR4& vBrickCoords) const;


  /**
    Returns the ToC Entry of a brick
    @param vBrickCoords coordinates of a brick: x,y,z are the spacial coordinates, w is the LoD level
    @return the ToC Entry of a brick
  */
  const TOCEntry& GetBrickToCData(const UINT64VECTOR4& vBrickCoords) const;

  /**
    Returns the ToC Entry of a brick
    @param index 1D index of the brick
    @return the ToC Entry of a brick
  */
  const TOCEntry& GetBrickToCData(size_t index) const;

  /**
    Returns the aspect ration of a specific brick, this does not include the global aspect ratio
    @param vBrickCoords coordinates of a brick: x,y,z are the spacial coordinates, w is the LoD level
    @return the aspect ration of a specific brick
  */
  DOUBLEVECTOR3 GetBrickAspect(const UINT64VECTOR4& vBrickCoords) const;

  /**
    use to get the raw (uncompressed) data of a specific brick
    @param pData the raw (uncompressed) data of a specific brick, the user has to make sure pData is big enough to hold the data
    @param vBrickCoords coordinates of a brick: x,y,z are the spacial coordinates, w is the LoD level
  */
  void GetBrickData(uint8_t* pData, const UINT64VECTOR4& vBrickCoords) const;


  /**
    Returns the global aspect ratio of the volume
    @return the global aspect ratio of the volume
  */
  DOUBLEVECTOR3 GetGlobalAspect() const {return m_vVolumeAspect;}

  /**
    Use to set the global aspect ratio of the volume, will write this into the header
    @param vVolumeAspect the new aspect ratio of the volume
  */
  bool SetGlobalAspect(const DOUBLEVECTOR3& vVolumeAspect);

  /**
    Returns the size (in bytes) of the component type e.g. 4 for CT_INT64
    @return the size (in bytes) of the component type
  */
  size_t GetComponentTypeSize() const;

  /**
    Returns the size of entire octree in bytes (including the header)
    @return the size of entire octree in bytes (including the header)
  */
  uint64_t GetSize() const {
    if (m_vTOC.empty())
      return ComputeHeaderSize(); // header only including ToC
    else
      return m_iSize; // header + brick data
  }

  /**
    Converts a 4-D brick coordinates into a 1D index, as used by the TOC
    @param vBrickCoords coordinates of a brick: x,y,z are the spacial coordinates, w is the LoD level
    @return the 1D index to be used for the ToC
  */
  uint64_t BrickCoordsToIndex(const UINT64VECTOR4& vBrickCoords) const;

  /**
    Converts a 1D index into 4-D brick coordinates
    @param  the 1D index as used for the ToC
    @return vBrickCoords coordinates of a brick: x,y,z are the spacial coordinates, w is the LoD level
  */
  UINT64VECTOR4 IndexToBrickCoords(uint64_t index) const;

private:
  /// type of the volume components (e.g. byte, int, float) stored as a COMPONENT_TYPE enum
  COMPONENT_TYPE m_eComponentType;

  /// stores how may components we have per voxel e.g. 4 for color data
  uint64_t m_iComponentCount;

  /// true iff the dataset contains pre-computed normals in the YZW components in addition
  /// to the actual data in the X channel, consequently this value is only true if
  /// the datasets component count is 4
  bool m_bPrecomputedNormals;

  /// the size (in voxels) of the volume represented by the hierarchy (= the size of LoD level 0)
  UINT64VECTOR3 m_vVolumeSize;

  /// aspect ratio of the volume
  DOUBLEVECTOR3 m_vVolumeAspect;

  /// maximum brick size i.e. the brick size of a non-boundary brick
  UINT64VECTOR3 m_iBrickSize;

  /// brick overlap
  uint32_t m_iOverlap;

  /// extended octree file version
  uint32_t m_iVersion;

  /// total octree size including header, necessary to allow "random" brick locations
  uint64_t m_iSize;

  /// offset of the header in the data file
  uint64_t m_iOffset;

  /// pointer to the data file
  LargeRAWFile_ptr m_pLargeRAWFile;

  /// the table of contents of the file, it holds the metadata for all bricks
  std::vector<TOCEntry> m_vTOC;

  /// table of LoD metadata
  std::vector<LODInfo> m_vLODTable;

  /**
    Computes whether a brick is the last brick in a row, column, or slice.

    @param vBrickCoords coordinates of a brick: x,y,z are the spacial coordinates, w is the LoD level
    @return whether a brick is the last brick in a row, column, or slice
  */
  VECTOR3<bool> IsLastBrick(const UINT64VECTOR4& vBrickCoords) const;

  /**
    Computes the LoD Table from the basic volume information (e.g. size, brick-size, overlap)
  */
  void ComputeMetadata();

  /**
    Computes the size of the header, as the header contains the brick ToC it varies from dataset to dataset
    @return the size of the header
  */
  uint64_t ComputeHeaderSize() const;

  /**
    Computes the number of bricks of all levels together WITHOUT using the brick ToC. This function does not use
    the brick ToC as it used to construct that very ToC. Once, the ToC exists it#s result
    is equal to m_vTOC.size()
    @return the number of bricks of all levels together
  */
  uint64_t ComputeBrickCount() const;

  /**
    Writes the Octree header into a file and stores the two parameters pLargeRAWFile and iOffset
    @param  pLargeRAWFile the file the header is written into, the file must be open already
    @param  iOffset the bytes to be skipped from the beginning of the file to get to the octree header
  */
  void WriteHeader(LargeRAWFile_ptr pLargeRAWFile, uint64_t iOffset);


  /**
    Static method that returns the size of a type specified as a COMPONENT_TYPE enum
    @param t the into type for which the size is to be computed
    @return the size of a type specified as a COMPONENT_TYPE enum
  */
  static uint32_t GetComponentTypeSize(COMPONENT_TYPE t);

  /**
    use to get the raw (uncompressed) data of a specific brick
    @param pData the raw (uncompressed) data of a specific brick, the user has to make sure pData is big enough to hold the data
    @param index the index of the brick in the LoD table
  */
  void GetBrickData(uint8_t* pData, uint64_t index) const;

  /** 
    returns true iff the large raw file holding this tree's
    data is is currently in RW mode
  */
  bool IsInRWMode() const  {return m_pLargeRAWFile->IsWritable();}

  /**
     close the file and reopen in RW mode
     does nothing if the file is already open in rw-mode
     @return if the file could be re-openend in rw-mode
  */
  bool ReOpenRW();

  /** 
    close the file if it is in rw mode and reopen in read-only mode
    does nothing if the file is already open in read-only mode
    @return if the file could be re-openend in read-only mode
  */
  bool ReOpenR();

  /// give the converter access to the internal data
  friend class ExtendedOctreeConverter;

};

#endif //  EXTENDEDOCTREE_H
