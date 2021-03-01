#pragma once

#ifndef MAXMINDATABLOCK_H
#define MAXMINDATABLOCK_H

#include <algorithm>
#include "DataBlock.h"
#include "Basics/MinMaxBlock.h"
#include "Basics/Vectors.h"
#include "ExtendedOctree/ExtendedOctreeConverter.h"

typedef std::vector<tuvok::MinMaxBlock> MinMaxComponent;
typedef std::vector<MinMaxComponent> MaxMinVec;

class MaxMinDataBlock : public DataBlock
{
public:
  MaxMinDataBlock(size_t iComponentCount);
  ~MaxMinDataBlock();
  MaxMinDataBlock(const MaxMinDataBlock &other);
  MaxMinDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);

  virtual MaxMinDataBlock& operator=(const MaxMinDataBlock& other);
  virtual uint64_t ComputeDataSize() const;

  const tuvok::MinMaxBlock& GetValue(size_t iIndex, size_t iComponent=0) const;
  void StartNewValue();
  void MergeData(const std::vector<DOUBLEVECTOR4>& fMaxMinData);
  void SetDataFromFlatVector(BrickStatVec& source, uint64_t iComponentCount);

  const tuvok::MinMaxBlock& GetGlobalValue(size_t iComponent=0) const {
    return m_GlobalMaxMin[iComponent];
  }

  size_t GetComponentCount() const {
    return m_iComponentCount;
  }

protected:
  std::vector<tuvok::MinMaxBlock> m_GlobalMaxMin;
  MaxMinVec   m_vfMaxMinData;
  size_t  m_iComponentCount;

  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                                   bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                            bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;

  virtual DataBlock* Clone() const;
  void MergeData(const tuvok::MinMaxBlock& data, const size_t iComponent);
  void ResetGlobal();
  void SetComponentCount(size_t iComponentCount);
};

#endif // MAXMINDATABLOCK_H
