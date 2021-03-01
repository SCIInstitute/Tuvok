#pragma once

#ifndef UVF_DATABLOCK_H
#define UVF_DATABLOCK_H

#include <string>
#include "UVFTables.h"

class DataBlock {
public:
  DataBlock();
  DataBlock(const DataBlock &other);
  DataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);
  virtual ~DataBlock();
  virtual DataBlock& operator=(const DataBlock& other);
  virtual bool Verify(uint64_t iSizeofData,
                      std::string* pstrProblem = NULL) const;

  UVFTables::BlockSemanticTable GetBlockSemantic() const {
    return ulBlockSemantics;
  }

  std::string strBlockID;
  UVFTables::CompressionSemanticTable ulCompressionScheme;
  virtual uint64_t ComputeDataSize() const {return 0;}

protected:
  LargeRAWFile_ptr  m_pStreamFile;
  uint64_t    m_iOffset;

  UVFTables::BlockSemanticTable ulBlockSemantics;
  uint64_t ulOffsetToNextDataBlock;

  virtual void CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                                bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile,
                                     uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                              bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;

  virtual DataBlock* Clone() const;

  friend class UVF;
};
#endif // UVF_DATABLOCK_H
