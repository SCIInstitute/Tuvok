#pragma once

#ifndef UVF_HISTOGRAM1DDATABLOCK_H
#define UVF_HISTOGRAM1DDATABLOCK_H

#include "DataBlock.h"
#include "TOCBlock.h"

class RasterDataBlock;

class Histogram1DDataBlock : public DataBlock
{
public:
  Histogram1DDataBlock();
  ~Histogram1DDataBlock();
  Histogram1DDataBlock(const Histogram1DDataBlock &other);
  Histogram1DDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                       bool bIsBigEndian);
  virtual Histogram1DDataBlock& operator=(const Histogram1DDataBlock& other);
  virtual uint64_t ComputeDataSize() const;

  bool Compute(const TOCBlock* source, uint64_t iLevel);
  bool Compute(const RasterDataBlock* source);
  const std::vector<uint64_t>& GetHistogram() const {return m_vHistData;}
  void SetHistogram(std::vector<uint64_t>& vHistData) {m_vHistData = vHistData;}
  size_t Compress(size_t maxTargetSize);

protected:
  std::vector<uint64_t> m_vHistData;

  virtual void CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                                bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile,
                                     uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                              bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;

  virtual DataBlock* Clone() const;

  template <class T> void ComputeTemplate(const TOCBlock* source,
                                          uint64_t iLevel);
};
#endif // UVF_HISTOGRAM1DDATABLOCK_H
