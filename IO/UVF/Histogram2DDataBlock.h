#pragma once

#ifndef UVF_HISTOGRAM2DDATABLOCK_H
#define UVF_HISTOGRAM2DDATABLOCK_H

#include "DataBlock.h"
#include "TOCBlock.h"
#include "../../Basics/Vectors.h"

class RasterDataBlock;

class Histogram2DDataBlock : public DataBlock
{
public:
  Histogram2DDataBlock();
  ~Histogram2DDataBlock();
  Histogram2DDataBlock(const Histogram2DDataBlock &other);
  Histogram2DDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                       bool bIsBigEndian);
  virtual Histogram2DDataBlock& operator=(const Histogram2DDataBlock& other);
  virtual uint64_t ComputeDataSize() const;

  bool Compute(const TOCBlock* source, uint64_t iLevel, size_t iHistoBinCount,
               double fMaxNonZeroValue);
  bool Compute(const RasterDataBlock* source,
               size_t iHistoBinCount, double fMaxNonZeroValue);

  const std::vector<std::vector<uint64_t>>& GetHistogram() const {
    return m_vHistData;
  }
  void SetHistogram(std::vector<std::vector<uint64_t>>& vHistData,
                    float fMaxGradMagnitude) {
    m_vHistData = vHistData;
    m_fMaxGradMagnitude=fMaxGradMagnitude;
  }

  float GetMaxGradMagnitude() const {return m_fMaxGradMagnitude;}

protected:
  std::vector<std::vector<uint64_t>> m_vHistData;
  float                              m_fMaxGradMagnitude;

  virtual void CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                                bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile,
                                     uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                              bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;

  virtual DataBlock* Clone() const;


  template <class T>
  FUNC_PURE
  DOUBLEVECTOR3 ComputeGradient(const T* pTempBrickData,
                                double normalizationFactor, size_t iCompcount,
                                const UINTVECTOR3& size,
                                const UINTVECTOR3& coords);

  template <class T>
  void ComputeTemplate(const TOCBlock* source, double normalizationFactor,
                       uint64_t iLevel, size_t iHistoBinCount,
                       double fMaxNonZeroValue);
};
#endif // UVF_HISTOGRAM2DDATABLOCK_H
