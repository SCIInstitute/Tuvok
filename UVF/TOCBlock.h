#pragma once

#ifndef TOCBLOCK_H
#define TOCBLOCK_H

#include "DataBlock.h"
#include "ExtendedOctree/ExtendedOctree.h"

class AbstrDebugOut;
class MaxMinDataBlock;

class TOCBlock : public DataBlock
{
public:
  TOCBlock(); 
  virtual ~TOCBlock();
  TOCBlock(const TOCBlock &other);
  TOCBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t ComputeDataSize() const;

  void SetOverlap(uint32_t iOverlap) {m_iOverlap=iOverlap;}
  uint32_t GetOverlap() const {return m_iOverlap;}

  void SetMaxBricksize(const UINTVECTOR3& vMaxBrickSize) {m_vMaxBrickSize=vMaxBrickSize;}
  UINTVECTOR3 GetMaxBricksize() const {return m_vMaxBrickSize;}

  bool FlatDataToBrickedLOD(const std::string& strSourceFile, const std::string& strTempFile, 
                            ExtendedOctree::COMPONENT_TYPE eType, UINT64 iComponentCount, 
                            UINT64VECTOR3 vVolumeSize, DOUBLEVECTOR3 vScale, size_t iCacheSize, 
                            MaxMinDataBlock* pMaxMinDatBlock = NULL, AbstrDebugOut* pDebugOut=NULL);
  bool FlatDataToBrickedLOD(LargeRAWFile_ptr pSourceData, const std::string& strTempFile, 
                            ExtendedOctree::COMPONENT_TYPE eType, UINT64 iComponentCount, 
                            UINT64VECTOR3 vVolumeSize, DOUBLEVECTOR3 vScale, size_t iCacheSize, 
                            MaxMinDataBlock* pMaxMinDatBlock = NULL, AbstrDebugOut* pDebugOut=NULL);

  bool BrickedLODToFlatData(const std::string& strTargetFile, uint64_t iLoD, AbstrDebugOut* pDebugOut=NULL) const;
  bool BrickedLODToFlatData(LargeRAWFile_ptr pTargetFile, uint64_t iLoD, AbstrDebugOut* pDebugOut=NULL) const;

  void GetData(uint8_t* pData, UINT64VECTOR4 coordinates) const;

  uint64_t GetLoDCount() const;
  UINT64VECTOR3 GetBrickCount(uint64_t iLoD) const;
  UINTVECTOR3 GetBrickSize(UINT64VECTOR4 coordinates) const;
  UINT64VECTOR3 GetLODDomainSize(uint64_t iLoD) const;

  uint64_t GetComponentCount() const {return m_ExtendedOctree.GetComponentCount();}
  size_t GetComponentTypeSize() const {return m_ExtendedOctree.GetComponentTypeSize();}
  ExtendedOctree::COMPONENT_TYPE GetComponentType() const {return m_ExtendedOctree.GetComponentType();}

protected:
  uint64_t m_iOffsetToOctree;
  ExtendedOctree m_ExtendedOctree;
  bool m_bIsBigEndian;
  uint32_t m_iOverlap;
  UINTVECTOR3 m_vMaxBrickSize;
  std::string m_strDeleteTempFile;
  
  uint64_t ComputeHeaderSize() const;
  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;
  virtual DataBlock* Clone() const;

  friend class UVF;
};


#endif // TOCBLOCK_H
