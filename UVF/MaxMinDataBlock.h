#pragma once

#ifndef MAXMINDATABLOCK_H
#define MAXMINDATABLOCK_H

#include <algorithm>
#include "DataBlock.h"
#include "Basics/Vectors.h"
#include "ExtendedOctree/ExtendedOctreeConverter.h"

template<class T, class S> class MaxMinElemen {
public:
  MaxMinElemen() :
   minScalar(0),
   maxScalar(0),
   minGradient(0),
   maxGradient(0)
  {}

  MaxMinElemen(T _minScalar, T _maxScalar, S _minGradient, S _maxGradient) :
   minScalar(_minScalar),
   maxScalar(_maxScalar),
   minGradient(_minGradient),
   maxGradient(_maxGradient)
  {}

  void Merge(const MaxMinElemen<T,S>& other) {
    minScalar = std::min(minScalar, other.minScalar);
    maxScalar = std::max(maxScalar, other.maxScalar);
    minGradient = std::min(minGradient, other.minGradient);
    maxGradient = std::max(maxGradient, other.maxGradient);
  }

  T minScalar;
  T maxScalar;
  S minGradient;
  S maxGradient;
};

typedef MaxMinElemen<double, double> InternalMaxMinComponent;
typedef std::vector< InternalMaxMinComponent > InternalMaxMinVoxel;
typedef std::vector< InternalMaxMinVoxel > MaxMinVec;

class MaxMinDataBlock : public DataBlock
{
public:
  MaxMinDataBlock(size_t iComponentCount);
  ~MaxMinDataBlock();
  MaxMinDataBlock(const MaxMinDataBlock &other);
  MaxMinDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);

  virtual MaxMinDataBlock& operator=(const MaxMinDataBlock& other);
  virtual uint64_t ComputeDataSize() const;

  const InternalMaxMinComponent& GetValue(size_t iIndex, size_t iComponent=0) const;
  void StartNewValue();
  void MergeData(const std::vector<DOUBLEVECTOR4>& fMaxMinData);
  void SetDataFromFlatVector(BrickStatVec& source, uint64_t iComponentCount);

  const InternalMaxMinComponent& GetGlobalValue(size_t iComponent=0) const {
    return m_GlobalMaxMin[iComponent];
  }

  size_t GetComponentCount() const {
    return m_iComponentCount;
  }

protected:
  std::vector<InternalMaxMinComponent> m_GlobalMaxMin;
  MaxMinVec   m_vfMaxMinData;
  size_t  m_iComponentCount;

  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                                   bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                            bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;

  virtual DataBlock* Clone() const;
  void MergeData(const InternalMaxMinComponent& data, const size_t iComponent);
  void ResetGlobal();
  void SetComponentCount(size_t iComponentCount);
};

#endif // MAXMINDATABLOCK_H
