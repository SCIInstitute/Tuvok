#pragma once

#ifndef UVF_TOCBLOCK_H
#define UVF_TOCBLOCK_H

#include "DataBlock.h"
#include "ExtendedOctree/ExtendedOctree.h"

class AbstrDebugOut;
class MaxMinDataBlock;

class TOCBlock : public DataBlock
{
public:
  TOCBlock(uint64_t iUVFFileVersion);
  virtual ~TOCBlock();
  TOCBlock(const TOCBlock &other);
  TOCBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
           bool bIsBigEndian, uint64_t iUVFFileVersion);
  virtual uint64_t ComputeDataSize() const;

  uint32_t GetOverlap() const {return m_ExtendedOctree.GetOverlap();}
  UINTVECTOR3 GetMaxBrickSize() const {
    return m_ExtendedOctree.GetMaxBrickSize();
  }

  bool FlatDataToBrickedLOD(const std::string& strSourceFile,
                            const std::string& strTempFile,
                            ExtendedOctree::COMPONENT_TYPE eType,
                            uint64_t iComponentCount,
                            const UINT64VECTOR3& vVolumeSize,
                            const DOUBLEVECTOR3& vScale,
                            const UINT64VECTOR3& vMaxBrickSize,
                            uint32_t iOverlap,
                            bool bUseMedian,
                            bool bClampToEdge,
                            size_t iCacheSize,
                            std::shared_ptr<MaxMinDataBlock>
                              pMaxMinDatBlock =
                                std::shared_ptr<MaxMinDataBlock>(),
                            AbstrDebugOut* pDebugOut=NULL,
                            COMPRESSION_TYPE ct=CT_ZLIB,
                            LAYOUT_TYPE lt=LT_SCANLINE);
  bool FlatDataToBrickedLOD(LargeRAWFile_ptr pSourceData,
                            const std::string& strTempFile,
                            ExtendedOctree::COMPONENT_TYPE eType,
                            uint64_t iComponentCount,
                            const UINT64VECTOR3& vVolumeSize,
                            const DOUBLEVECTOR3& vScale,
                            const UINT64VECTOR3& vMaxBrickSize,
                            uint32_t iOverlap,
                            bool bUseMedian,
                            bool bClampToEdge,
                            size_t iCacheSize,
                            std::shared_ptr<MaxMinDataBlock>
                              pMaxMinDatBlock =
                                std::shared_ptr<MaxMinDataBlock>(),
                            AbstrDebugOut* pDebugOut=NULL,
                            COMPRESSION_TYPE ct=CT_ZLIB,
                            LAYOUT_TYPE lt=LT_SCANLINE);

  bool BrickedLODToFlatData(uint64_t iLoD,
                            const std::string& strTargetFile,
                            bool bAppend = false, AbstrDebugOut* pDebugOut=NULL) const;

  bool BrickedLODToFlatData(uint64_t iLoD,
                            LargeRAWFile_ptr pTargetFile,
                            bool bAppend = false, AbstrDebugOut* pDebugOut=NULL) const;


  bool ApplyFunction(uint64_t iLoD,
                     bool (*brickFunc)(void* pData,
                                    const UINT64VECTOR3& vBrickSize,
                                    const UINT64VECTOR3& vBrickOffset,
                                    void* pUserContext) = NULL,
                     void* pUserContext = NULL,
                     uint32_t iOverlap=0,
                     AbstrDebugOut* pDebugOut=NULL) const;

  void GetData(uint8_t* pData, UINT64VECTOR4 coordinates) const;

  uint64_t GetLoDCount() const;
  UINT64VECTOR3 GetBrickCount(uint64_t iLoD) const;
  UINT64VECTOR3 GetBrickSize(UINT64VECTOR4 coordinates) const;
  DOUBLEVECTOR3 GetBrickAspect(UINT64VECTOR4 coordinates) const;
  UINT64VECTOR3 GetLODDomainSize(uint64_t iLoD) const;
  const TOCEntry& GetBrickInfo(UINT64VECTOR4 coordinates) const;

  uint64_t GetLinearBrickIndex(UINT64VECTOR4 coordinates) const;

  uint64_t GetComponentCount() const {
    return m_ExtendedOctree.GetComponentCount();
  }
  size_t GetComponentTypeSize() const {
    return m_ExtendedOctree.GetComponentTypeSize();
  }
  ExtendedOctree::COMPONENT_TYPE GetComponentType() const {
    return m_ExtendedOctree.GetComponentType();
  }
  UINTVECTOR2 GetAtlasSize(UINT64VECTOR4 coordinates) const {
    return m_ExtendedOctree.GetBrickToCData(coordinates).m_iAtlasSize;
  }

  bool GetIsSigned() const;
  bool GetIsFloat() const;

  DOUBLEVECTOR3 GetScale() const;
  void SetScale(const DOUBLEVECTOR3& scale);

protected:
  uint64_t m_iOffsetToOctree;
  ExtendedOctree m_ExtendedOctree;
  bool m_bIsBigEndian;
  uint32_t m_iOverlap;
  UINT64VECTOR3 m_vMaxBrickSize;
  std::string m_strDeleteTempFile;
  uint64_t m_iUVFFileVersion;

  uint64_t ComputeHeaderSize() const;
  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile,
                                     uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                              bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;
  virtual DataBlock* Clone() const;

  friend class UVF;
};
#endif // UVF_TOCBLOCK_H
