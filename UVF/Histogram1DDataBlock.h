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
                                          uint64_t iLevel) {
    // compute histogram by iterating over all bricks of the given level
    UINT64VECTOR3 bricksInSourceLevel = source->GetBrickCount(iLevel);

    size_t iCompcount = size_t(source->GetComponentCount());
    T* pTempBrickData = new T[size_t(source->GetMaxBricksize().volume())*iCompcount];

    uint32_t iOverlap =source->GetOverlap();

    for (uint64_t bz = 0;bz<bricksInSourceLevel.z;bz++) {
      for (uint64_t by = 0;by<bricksInSourceLevel.y;by++) {
        for (uint64_t bx = 0;bx<bricksInSourceLevel.x;bx++) {
          UINT64VECTOR4 brickCoords(bx,by,bz,iLevel);
          source->GetData((uint8_t*)pTempBrickData, brickCoords);
          UINTVECTOR3 bricksize = source->GetBrickSize(brickCoords);

          for (uint32_t z = iOverlap;z<bricksize.z-iOverlap;z++) {
            for (uint32_t y = iOverlap;y<bricksize.y-iOverlap;y++) {
              for (uint32_t x = iOverlap;x<bricksize.x-iOverlap;x++) {
                // TODO: think about what todo with multi component data
                //       right now we only pick the first component
                size_t val = size_t(pTempBrickData[iCompcount*(x+y*bricksize.x+z*bricksize.x*bricksize.y)]);
                m_vHistData[val]++;
              }
            }
          }
        }
      }
    }
    delete [] pTempBrickData;
  }
};
#endif // UVF_HISTOGRAM1DDATABLOCK_H
