#include <iterator>
#pragma once

#ifndef UVF_RASTERDATABLOCK_H
#define UVF_RASTERDATABLOCK_H

#include "boost/cstdint.hpp"

#include <algorithm>
#include <string>
#include "boost/cstdint.hpp"
#include "DataBlock.h"
#include "Basics/Vectors.h"

class AbstrDebugOut;

template<class T, size_t iVecLength>
void SimpleMaxMin(const void* pIn, size_t iStart, size_t iCount,
                  std::vector<DOUBLEVECTOR4>& fMinMax) {
  const T *pDataIn = (T*)pIn;

  fMinMax.resize(iVecLength);

  for (size_t i = 0;i<iVecLength;i++) {
    fMinMax[i].x = pDataIn[iStart+i]; // .x will be the minimum
    fMinMax[i].y = pDataIn[iStart+i]; // .y will be the max

    /// \todo remove this if the gradient computations is implemented below
    fMinMax[i].z = -std::numeric_limits<double>::max(); // min gradient
    fMinMax[i].w = std::numeric_limits<double>::max();  // max gradient
  }

  for (size_t i = iStart+iVecLength;i<iStart+iCount;i++) {
    for (size_t iComponent = 0;iComponent<iVecLength;iComponent++) {
        fMinMax[iComponent].x = std::min(fMinMax[iComponent].x,
                                         static_cast<double>
                                         (pDataIn[i*iVecLength+iComponent]));
        fMinMax[iComponent].y = std::max(fMinMax[iComponent].y,
                                         static_cast<double>
                                         (pDataIn[i*iVecLength+iComponent]));
      /// \todo compute gradients
    }
  }
}

template<class T> void CombineAverage(const std::vector<UINT64>& vSource, UINT64 iTarget, const void* pIn, const void* pOut) {
  const T *pDataIn = (T*)pIn;
  T *pDataOut = (T*)pOut;

  double temp = 0;
  for (size_t i = 0;i<vSource.size();i++) {
    temp += double(pDataIn[size_t(vSource[i])]);
  }
  // make sure not to touch pDataOut before we are finished with reading pDataIn, this allows for inplace combine calls
  pDataOut[iTarget] = T(temp / double(vSource.size()));
}

template<class T, size_t iVecLength> void CombineAverage(const std::vector<UINT64>& vSource, UINT64 iTarget, const void* pIn, const void* pOut) {
  const T *pDataIn = (T*)pIn;
  T *pDataOut = (T*)pOut;

  double temp[iVecLength];  
  for (size_t i = 0;i<size_t(iVecLength);i++) temp[i] = 0.0;

  for (size_t i = 0;i<vSource.size();i++) {
    for (size_t v = 0;v<size_t(iVecLength);v++)
      temp[v] += double(pDataIn[size_t(vSource[i])*iVecLength+v]) / double(vSource.size());
  }
  // make sure not to touch pDataOut before we are finished with reading pDataIn, this allows for inplace combine calls
  for (UINT64 v = 0;v<iVecLength;v++)
    pDataOut[v+iTarget*iVecLength] = T(temp[v]);
}

class Histogram1DDataBlock;
class Histogram2DDataBlock;
class MaxMinDataBlock;

class RasterDataBlock : public DataBlock {
public:
  RasterDataBlock();
  RasterDataBlock(const RasterDataBlock &other);
  RasterDataBlock(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian);
  virtual RasterDataBlock& operator=(const RasterDataBlock& other);
  virtual ~RasterDataBlock();

  virtual bool Verify(UINT64 iSizeofData, std::string* pstrProblem = NULL) const;
  virtual bool Verify(std::string* pstrProblem = NULL) const;

  std::vector<UVFTables::DomainSemanticTable> ulDomainSemantics;
  std::vector<double> dDomainTransformation;
  std::vector<UINT64> ulDomainSize;
  std::vector<UINT64> ulBrickSize;
  std::vector<UINT64> ulBrickOverlap;
  std::vector<UINT64> ulLODDecFactor;
  std::vector<UINT64> ulLODGroups;
  std::vector<UINT64> ulLODLevelCount;
  UINT64 ulElementDimension;
  std::vector<UINT64> ulElementDimensionSize;
  std::vector<std::vector<UVFTables::ElementSemanticTable> > ulElementSemantic;
  std::vector<std::vector<UINT64> > ulElementBitSize;
  std::vector<std::vector<UINT64> > ulElementMantissa;
  std::vector<std::vector<bool> > bSignedElement;
  UINT64 ulOffsetToDataBlock;

  virtual UINT64 ComputeDataSize() const {return ComputeDataSize(NULL);}
  virtual UINT64 ComputeDataSize(std::string* pstrProblem) const;
  virtual UINT64 ComputeHeaderSize() const;

  virtual bool SetBlockSemantic(UVFTables::BlockSemanticTable bs);

  // CONVENIENCE FUNCTIONS
  void SetScaleOnlyTransformation(const std::vector<double>& vScale);
  void SetIdentityTransformation();
  void SetTypeToScalar(UINT64 iBitWith, UINT64 iMantissa, bool bSigned,
                       UVFTables::ElementSemanticTable semantic);
  void SetTypeToVector(UINT64 iBitWith, UINT64 iMantissa, bool bSigned,
                       std::vector<UVFTables::ElementSemanticTable> semantic);
  void SetTypeToUByte(UVFTables::ElementSemanticTable semantic);
  void SetTypeToUShort(UVFTables::ElementSemanticTable semantic);
  void SetTypeToFloat(UVFTables::ElementSemanticTable semantic);
  void SetTypeToDouble(UVFTables::ElementSemanticTable semantic);
  void SetTypeToInt32(UVFTables::ElementSemanticTable semantic);
  void SetTypeToInt64(UVFTables::ElementSemanticTable semantic);
  void SetTypeToUInt32(UVFTables::ElementSemanticTable semantic);
  void SetTypeToUInt64(UVFTables::ElementSemanticTable semantic);

  bool GetData(std::vector<boost::uint8_t>& vData,
               const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick) const;
  bool GetData(std::vector<boost::int8_t>& vData,
               const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick) const;
  bool GetData(std::vector<boost::uint16_t>& vData,
               const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick) const;
  bool GetData(std::vector<boost::int16_t>& vData,
               const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick) const;
  bool GetData(std::vector<boost::uint32_t>& vData,
               const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick) const;
  bool GetData(std::vector<boost::int32_t>& vData,
               const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick) const;
  bool GetData(std::vector<float>& vData,
               const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick) const;
  bool GetData(std::vector<double>& vData,
               const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick) const;
  bool SetData(boost::int8_t* pData, const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick);
  bool SetData(boost::uint8_t* pData, const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick);
  bool SetData(boost::int16_t*, const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick);
  bool SetData(boost::uint16_t*, const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick);
  bool SetData(boost::int32_t*, const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick);
  bool SetData(boost::uint32_t*, const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick);
  bool SetData( float*, const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick);
  bool SetData(double*, const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick);

  /// Change the file we're reading/writing to.  Closes any open
  /// temporary file.  Maintains file position information.
  /// Useful for writing a raster data block into a new file.
  /// NOTE: steals pointer; LargeRAWFile must live as long as the RDB.
  void ResetFile(LargeRAWFile*);

  const std::vector<UINT64>& GetBrickCount(const std::vector<UINT64>& vLOD) const;
  const std::vector<UINT64>& GetBrickSize(const std::vector<UINT64>& vLOD, const std::vector<UINT64>& vBrick) const;
  std::vector<UINT64> GetLODDomainSize(const std::vector<UINT64>& vLOD) const;

  bool BrickedLODToFlatData(const std::vector<UINT64>& vLOD, const std::string& strTargetFile,
                            bool bAppend = false, AbstrDebugOut* pDebugOut=NULL,
                            bool (*brickFunc)(LargeRAWFile* pSourceFile,
                                const std::vector<UINT64> vBrickSize,
                                const std::vector<UINT64> vBrickOffset,
                                void* pUserContext ) = NULL,
                            void* pUserContext = NULL,
                            UINT64 iOverlap=0) const;

  const std::vector<UINT64> LargestSingleBrickLODBrickIndex() const;
  const std::vector<UINT64>& LargestSingleBrickLODBrickSize() const;

  const std::vector<UINT64> GetSmallestBrickIndex() const;
  const std::vector<UINT64>& GetSmallestBrickSize() const;
  const std::vector<UINT64> GetLargestBrickSizes() const;

  bool FlatDataToBrickedLOD(const void* pSourceData, const std::string& strTempFile,
                            void (*combineFunc)(const std::vector<UINT64> &vSource, UINT64 iTarget, const void* pIn, const void* pOut),
                            void (*maxminFunc)(const void* pIn, size_t iStart,
                                               size_t iCount,
                                               std::vector<DOUBLEVECTOR4>& fMinMax),
                            MaxMinDataBlock* pMaxMinDatBlock = NULL, AbstrDebugOut* pDebugOut=NULL);
  bool FlatDataToBrickedLOD(LargeRAWFile* pSourceData, const std::string& strTempFile,
                            void (*combineFunc)(const std::vector<UINT64> &vSource, UINT64 iTarget, const void* pIn, const void* pOut),
                            void (*maxminFunc)(const void* pIn, size_t iStart,
                                               size_t iCount,
                                               std::vector<DOUBLEVECTOR4>& fMinMax),
                            MaxMinDataBlock* pMaxMinDatBlock = NULL, AbstrDebugOut* pDebugOut=NULL);
  void AllocateTemp(const std::string& strTempFile, bool bBuildOffsetTables=false);
  bool ValidLOD(const std::vector<UINT64>& vLOD) const;
  bool ValidBrickIndex(const std::vector<UINT64>& vLOD,
                       const std::vector<UINT64>& vBrick) const;


protected:
  LargeRAWFile* m_pTempFile;
  LargeRAWFile* m_pSourceFile;
  UINT64        m_iSourcePos;

  virtual void CopyHeaderToFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual UINT64 GetHeaderFromFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian);
  virtual UINT64 CopyToFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual DataBlock* Clone() const;
  virtual UINT64 GetOffsetToNextBlock() const;

  std::vector<std::vector<UINT64> > CountToVectors(std::vector<UINT64> vCountVector) const ;
  UINT64 ComputeElementSize() const;
  virtual UINT64 GetLODSize(std::vector<UINT64>& vLODIndices) const;
  virtual UINT64 ComputeLODLevelSize(const std::vector<UINT64>& vReducedDomainSize) const;
  virtual std::vector<std::vector<UINT64> > ComputeBricks(const std::vector<UINT64>& vDomainSize) const;
  virtual std::vector<std::vector<UINT64> > GenerateCartesianProduct(const std::vector<std::vector<UINT64> >& vElements, UINT64 iIndex=0) const;
  UINT64 RecompLODIndexCount() const;
  void CleanupTemp();


  friend class UVF;
  friend class Histogram1DDataBlock;
  friend class Histogram2DDataBlock;

  // CONVENIENCE FUNCTION HELPERS
  std::vector<UINT64> m_vLODOffsets;
  std::vector<std::vector<UINT64> > m_vBrickCount;
  std::vector<std::vector<UINT64> > m_vBrickOffsets;
  std::vector<std::vector<std::vector<UINT64> > > m_vBrickSizes;

  UINT64 Serialize(const std::vector<UINT64>& vec, const std::vector<UINT64>& vSizes) const;
  UINT64 GetLocalDataPointerOffset(const std::vector<UINT64>& vLOD, const std::vector<UINT64>& vBrick) const;
  UINT64 GetLocalDataPointerOffset(const UINT64 iLODIndex, const UINT64 iBrickIndex) const {return m_vLODOffsets[size_t(iLODIndex)] + m_vBrickOffsets[size_t(iLODIndex)][size_t(iBrickIndex)];}
  void SubSample(LargeRAWFile* pSourceFile, LargeRAWFile* pTargetFile, std::vector<UINT64> sourceSize, std::vector<UINT64> targetSize, void (*combineFunc)(const std::vector<UINT64> &vSource, UINT64 iTarget, const void* pIn, const void* pOut), AbstrDebugOut* pDebugOut=NULL, UINT64 iLODLevel=0, UINT64 iMaxLODLevel=0);
  UINT64 ComputeDataSizeAndOffsetTables();
  UINT64 GetLODSizeAndOffsetTables(std::vector<UINT64>& vLODIndices, UINT64 iLOD);
  UINT64 ComputeLODLevelSizeAndOffsetTables(const std::vector<UINT64>& vReducedDomainSize, UINT64 iLOD);

  bool TraverseBricksToWriteBrickToFile(
      UINT64& iBrickCounter, UINT64 iBrickCount,
      const std::vector<UINT64>& vLOD,
      const std::vector<UINT64>& vBrickCount,
      std::vector<UINT64> vCoords,
      size_t iCurrentDim, UINT64 iTargetOffset,
      std::vector<unsigned char> &vData,
      LargeRAWFile* pTargetFile, UINT64 iElementSize,
      const std::vector<UINT64>& vPrefixProd,
      AbstrDebugOut* pDebugOut,
      bool (*brickFunc)(LargeRAWFile* pSourceFile,
                        const std::vector<UINT64> vBrickSize,
                        const std::vector<UINT64> vBrickOffset,
                        void* pUserContext),
      void* pUserContext,
      UINT64 iOverlap
  ) const;

  void WriteBrickToFile(size_t iCurrentDim,
                        UINT64& iSourceOffset, UINT64& iTargetOffset,
                        const std::vector<UINT64>& vBrickSize,
                        const std::vector<UINT64>& vEffectiveBrickSize,
                        std::vector<unsigned char>& vData,
                        LargeRAWFile* pTargetFile, UINT64 iElementSize,
                        const std::vector<UINT64>& vPrefixProd,
                        const std::vector<UINT64>& vPrefixProdBrick,
                        bool bDoSeek) const;
private:
  LargeRAWFile* SeekToBrick(const std::vector<UINT64>& vLOD,
                            const std::vector<UINT64>& vBrick) const;
  bool GetData(unsigned char*, size_t bytes,
               const std::vector<UINT64>& vLOD,
               const std::vector<UINT64>& vBrick) const;

  size_t GetBrickByteSize(const std::vector<UINT64>& vLOD,
                          const std::vector<UINT64>& vBrick) const;
  /// @return true if 'SetData' can work given current state.
  bool Settable() const;
};

enum RDBResolution {
  FINEST_RESOLUTION=-2,
  COARSEST_RESOLUTION=-1,
};

/// An input iterator which iterates over all data in a given LoD.  The
/// order of bricks is unspecified.  Access is done in an out-of-core manner;
/// only one brick will ever be loaded at any given time.
/// The LoD argument gives the level of detail; higher numbers are coarser
/// representations.
template <typename T, int LoD=FINEST_RESOLUTION>
class LODBrickIterator : public std::iterator<std::input_iterator_tag, T> {
  public:
    /// Constructs an end-of-stream iterator.
    LODBrickIterator(): rdb(NULL), brick(0), iter(0), eos(true) {}

    /// Constructs an iterator which is at the start of the DS.
    LODBrickIterator(const RasterDataBlock* b) : rdb(b), brick(0), iter(0),
                                                 eos(false) {}

    T& operator*() {
      assert(!this->eos); // can't deref end-of-stream iterator.
      if(this->buffer.empty()) {
        NextBrick();
      }
      return this->buffer[this->iter];
    }

    void operator++() {
      ++iter;
      if(this->buffer.empty() || this->iter >= this->buffer.size()) {
        NextBrick();
      }
    }
    void operator++(int) { LODBrickIterator<T, LoD>::operator++(); }

    /// Two end-of-stream iterators are always equal.
    /// An end-of-stream iterator is not equal to a non-end-of-stream iterator.
    /// Two non-EOS iterators are equal when they are constructed from the same
    /// stream.
    ///@{
    bool operator==(const LODBrickIterator<T, LoD> bi) const {
      return (this->eos && bi.eos) ||
             (this->rdb == bi.rdb && !this->eos && !bi.eos);
    }
    bool operator!=(const LODBrickIterator<T, LoD> bi) const {
      return !(*this == bi);
    }
    ///@}

  private:
    void NextBrick() {
      this->iter = 0;
      std::vector<UINT64> vl(1);
      std::vector<UINT64> b = this->NDBrickIndex(brick);
      vl[0] = this->LODIndex();
      /// FIXME -- this getdata fails when we hit the 10th brick in a 9-brick
      /// dataset.  either fix GetData or test to make sure we're not beyond
      /// the end of our bricks first!
      if(!this->rdb->GetData(buffer, vl, b)) {
        this->eos = true;
      }
      this->brick++;
    }

    /// RDB doesn't expose a linear counter for bricks.  Instead it
    /// keeps separate counts for each dimension.  Thus we need to be
    /// able to take our linear counter and convert it to RDB indices.
    std::vector<UINT64> NDBrickIndex(size_t b) {
      UINT64 brick = static_cast<UINT64>(b);
      std::vector<UINT64> lod(1);
      lod[0] = LODIndex();
      const std::vector<UINT64>& counts = rdb->GetBrickCount(lod);

      UINT64 z = static_cast<UINT64>(brick / (counts[0] * counts[1]));
      brick = brick % (counts[0] * counts[1]);
      UINT64 y = static_cast<UINT64>(brick / counts[0]);
      brick = brick % counts[0];
      UINT64 x = brick;

      std::vector<UINT64> vec(3);
      vec[0] = x;
      vec[1] = y;
      vec[2] = z;
      return vec;
    }

    size_t LODIndex() const {
      switch(LoD) {
        case FINEST_RESOLUTION: return 0;
        case COARSEST_RESOLUTION: return rdb->ulLODLevelCount[0];
        default: return LoD;
      }
    }

  private:
    const RasterDataBlock* rdb;
    std::vector<T> buffer;
    size_t brick;
    size_t iter;
    bool eos; // end-of-stream.
};

#endif // UVF_RASTERDATABLOCK_H
