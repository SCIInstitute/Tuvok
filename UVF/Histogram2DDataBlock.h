#pragma once

#ifndef HISTOGRAM2DDATABLOCK_H
#define HISTOGRAM2DDATABLOCK_H

#include "DataBlock.h"
#include "../../Basics/Vectors.h"

class RasterDataBlock;
class TOCBlock;

class Histogram2DDataBlock : public DataBlock
{
public:
  Histogram2DDataBlock();
  ~Histogram2DDataBlock();
  Histogram2DDataBlock(const Histogram2DDataBlock &other);
  Histogram2DDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);
  virtual Histogram2DDataBlock& operator=(const Histogram2DDataBlock& other);
  virtual uint64_t ComputeDataSize() const;

  bool Compute(const TOCBlock* source, uint64_t iLevel, size_t iHistoBinCount, double fMaxNonZeroValue);
  bool Compute(const RasterDataBlock* source, size_t iHistoBinCount, double fMaxNonZeroValue);

  const std::vector< std::vector<uint64_t> >& GetHistogram() const {
    return m_vHistData;
  }
  void SetHistogram(std::vector< std::vector<uint64_t> >& vHistData, float fMaxGradMagnitude) {m_vHistData = vHistData;m_fMaxGradMagnitude=fMaxGradMagnitude;}

  float GetMaxGradMagnitude() const {return m_fMaxGradMagnitude;}

protected:
  std::vector< std::vector<uint64_t> > m_vHistData;
  float                              m_fMaxGradMagnitude;

  virtual void CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;

  virtual DataBlock* Clone() const;


  template < class T > DOUBLEVECTOR3 ComputeGradient(T* pTempBrickData, double normalizationFactor, size_t iCompcount, 
                                                     const UINTVECTOR3& size, const UINTVECTOR3& coords) {
    // TODO: think about what todo with multi component data
    //       right now we only pick the first component
    size_t iCenter = size_t(coords.x+size.x*coords.y+size.x*size.y*coords.z);
    size_t iLeft   = iCenter-1;
    size_t iRight  = iCenter+1;
    size_t iTop    = iCenter-size_t(size.x);
    size_t iBottom = iCenter+size_t(size.x);
    size_t iFront  = iCenter-size_t(size.x)*size_t(size.y);
    size_t iBack   = iCenter+size_t(size.x)*size_t(size.y);

    DOUBLEVECTOR3 vGradient((double(pTempBrickData[iCompcount*iLeft]) -double(pTempBrickData[iCompcount*iRight])) /(normalizationFactor*2),
                            (double(pTempBrickData[iCompcount*iTop])  -double(pTempBrickData[iCompcount*iBottom]))/(normalizationFactor*2),
                            (double(pTempBrickData[iCompcount*iFront])-double(pTempBrickData[iCompcount*iBack]))  /(normalizationFactor*2));
    return vGradient;
  }

  template < class T > void ComputeTemplate(const TOCBlock* source, double normalizationFactor,
                                            uint64_t iLevel, size_t iHistoBinCount, double fMaxNonZeroValue) {
    // compute histogram by iterating over all bricks of the given level
    UINT64VECTOR3 bricksInSourceLevel = source->GetBrickCount(iLevel);

    size_t iCompcount = size_t(source->GetComponentCount());
    T* pTempBrickData = new T[size_t(source->GetMaxBricksize().volume())*iCompcount];

    uint32_t iOverlap =source->GetOverlap();
    double fMaxGradMagnitude = 0;

    // find the maximum gradient magnitude
    for (uint64_t bz = 0;bz<bricksInSourceLevel.z;bz++) {
      for (uint64_t by = 0;by<bricksInSourceLevel.y;by++) {
        for (uint64_t bx = 0;bx<bricksInSourceLevel.x;bx++) {

          UINT64VECTOR4 brickCoords(bx,by,bz,iLevel);
          source->GetData((uint8_t*)pTempBrickData, brickCoords);
          UINTVECTOR3 bricksize = source->GetBrickSize(brickCoords);

          for (uint32_t z = iOverlap;z<bricksize.z-iOverlap;z++) {
            for (uint32_t y = iOverlap;y<bricksize.y-iOverlap;y++) {
              for (uint32_t x = iOverlap;x<bricksize.x-iOverlap;x++) {

                DOUBLEVECTOR3 vGradient = ComputeGradient(pTempBrickData, normalizationFactor,
                                                          iCompcount, bricksize, UINTVECTOR3(x,y,z));
                if (vGradient.length() > fMaxGradMagnitude) fMaxGradMagnitude = vGradient.length();
              }
            }
          }
        }
      }
    }

    // fill the histogram the maximum gradient magnitude
    for (uint64_t bz = 0;bz<bricksInSourceLevel.z;bz++) {
      for (uint64_t by = 0;by<bricksInSourceLevel.y;by++) {
        for (uint64_t bx = 0;bx<bricksInSourceLevel.x;bx++) {

          UINT64VECTOR4 brickCoords(bx,by,bz,iLevel);
          source->GetData((uint8_t*)pTempBrickData, brickCoords);
          UINTVECTOR3 bricksize = source->GetBrickSize(brickCoords);

          for (uint32_t z = iOverlap;z<bricksize.z-iOverlap;z++) {
            for (uint32_t y = iOverlap;y<bricksize.y-iOverlap;y++) {
              for (uint32_t x = iOverlap;x<bricksize.x-iOverlap;x++) {

                DOUBLEVECTOR3 vGradient = ComputeGradient(pTempBrickData, normalizationFactor,
                                                          iCompcount, bricksize, UINTVECTOR3(x,y,z));
                size_t iCenter = size_t(x+bricksize.x*y+bricksize.x*bricksize.y*z);

                size_t iGardientMagnitudeIndex = std::min<size_t>(255,size_t(vGradient.length()/fMaxGradMagnitude*255.0f));
                size_t iValue = (fMaxNonZeroValue <= double(iHistoBinCount-1)) 
                                   ? size_t(pTempBrickData[iCenter]) 
                                   : size_t(double(pTempBrickData[iCenter]) * double(iHistoBinCount-1)/fMaxNonZeroValue);

                // make sure round errors don't cause index to go out of bounds
                if (iGardientMagnitudeIndex > 255) iGardientMagnitudeIndex = 255;
                if (iValue > iHistoBinCount-1) iValue = iHistoBinCount-1; 
                m_vHistData[iValue][iGardientMagnitudeIndex]++;
              }
            }
          }
        }
      }
    }

    m_fMaxGradMagnitude = float(fMaxGradMagnitude);

    delete [] pTempBrickData;
  }

};

#endif // HISTOGRAM2DDATABLOCK_H
