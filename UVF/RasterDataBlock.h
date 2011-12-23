#include <iterator>
#pragma once

#ifndef UVF_RASTERDATABLOCK_H
#define UVF_RASTERDATABLOCK_H

#include <algorithm>
#include <string>
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

template<class T> void CombineAverage(const std::vector<uint64_t>& vSource, uint64_t iTarget, const void* pIn, const void* pOut) {
  const T *pDataIn = (T*)pIn;
  T *pDataOut = (T*)pOut;

  double temp = 0;
  for (size_t i = 0;i<vSource.size();i++) {
    temp += double(pDataIn[size_t(vSource[i])]);
  }
  // make sure not to touch pDataOut before we are finished with reading pDataIn, this allows for inplace combine calls
  pDataOut[iTarget] = T(temp / double(vSource.size()));
}

template<class T, size_t iVecLength> void CombineAverage(const std::vector<uint64_t>& vSource, uint64_t iTarget, const void* pIn, const void* pOut) {
  const T *pDataIn = (T*)pIn;
  T *pDataOut = (T*)pOut;

  double temp[iVecLength];  
  for (size_t i = 0;i<size_t(iVecLength);i++) temp[i] = 0.0;

  for (size_t i = 0;i<vSource.size();i++) {
    for (size_t v = 0;v<size_t(iVecLength);v++)
      temp[v] += double(pDataIn[size_t(vSource[i])*iVecLength+v]) / double(vSource.size());
  }
  // make sure not to touch pDataOut before we are finished with reading pDataIn, this allows for inplace combine calls
  for (uint64_t v = 0;v<iVecLength;v++)
    pDataOut[v+iTarget*iVecLength] = T(temp[v]);
}

class Histogram1DDataBlock;
class Histogram2DDataBlock;
class MaxMinDataBlock;

class RasterDataBlock : public DataBlock {
public:
  RasterDataBlock();
  RasterDataBlock(const RasterDataBlock &other);
  RasterDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);
  virtual RasterDataBlock& operator=(const RasterDataBlock& other);
  virtual ~RasterDataBlock();

  virtual bool Verify(uint64_t iSizeofData, std::string* pstrProblem = NULL) const;
  virtual bool Verify(std::string* pstrProblem = NULL) const;

  std::vector<UVFTables::DomainSemanticTable> ulDomainSemantics;
  std::vector<double> dDomainTransformation;
  std::vector<uint64_t> ulDomainSize;
  std::vector<uint64_t> ulBrickSize;
  std::vector<uint64_t> ulBrickOverlap;
  std::vector<uint64_t> ulLODDecFactor;
  std::vector<uint64_t> ulLODGroups;
  std::vector<uint64_t> ulLODLevelCount;
  uint64_t ulElementDimension;
  std::vector<uint64_t> ulElementDimensionSize;
  std::vector<std::vector<UVFTables::ElementSemanticTable> > ulElementSemantic;
  std::vector<std::vector<uint64_t> > ulElementBitSize;
  std::vector<std::vector<uint64_t> > ulElementMantissa;
  std::vector<std::vector<bool> > bSignedElement;
  uint64_t ulOffsetToDataBlock;

  virtual uint64_t ComputeDataSize() const {return ComputeDataSize(NULL);}
  virtual uint64_t ComputeDataSize(std::string* pstrProblem) const;
  virtual uint64_t ComputeHeaderSize() const;

  virtual bool SetBlockSemantic(UVFTables::BlockSemanticTable bs);

  // CONVENIENCE FUNCTIONS
  void SetScaleOnlyTransformation(const std::vector<double>& vScale);
  void SetIdentityTransformation();
  void SetTypeToScalar(uint64_t iBitWith, uint64_t iMantissa, bool bSigned,
                       UVFTables::ElementSemanticTable semantic);
  void SetTypeToVector(uint64_t iBitWith, uint64_t iMantissa, bool bSigned,
                       std::vector<UVFTables::ElementSemanticTable> semantic);
  void SetTypeToUByte(UVFTables::ElementSemanticTable semantic);
  void SetTypeToUShort(UVFTables::ElementSemanticTable semantic);
  void SetTypeToFloat(UVFTables::ElementSemanticTable semantic);
  void SetTypeToDouble(UVFTables::ElementSemanticTable semantic);
  void SetTypeToInt32(UVFTables::ElementSemanticTable semantic);
  void SetTypeToInt64(UVFTables::ElementSemanticTable semantic);
  void SetTypeToUInt32(UVFTables::ElementSemanticTable semantic);
  void SetTypeToUInt64(UVFTables::ElementSemanticTable semantic);

  bool GetData(std::vector<uint8_t>& vData,
               const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick) const;
  bool GetData(std::vector<int8_t>& vData,
               const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick) const;
  bool GetData(std::vector<uint16_t>& vData,
               const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick) const;
  bool GetData(std::vector<int16_t>& vData,
               const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick) const;
  bool GetData(std::vector<uint32_t>& vData,
               const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick) const;
  bool GetData(std::vector<int32_t>& vData,
               const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick) const;
  bool GetData(std::vector<float>& vData,
               const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick) const;
  bool GetData(std::vector<double>& vData,
               const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick) const;
  bool SetData(int8_t* pData, const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick);
  bool SetData(uint8_t* pData, const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick);
  bool SetData(int16_t*, const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick);
  bool SetData(uint16_t*, const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick);
  bool SetData(int32_t*, const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick);
  bool SetData(uint32_t*, const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick);
  bool SetData( float*, const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick);
  bool SetData(double*, const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick);

  /// Change the file we're reading/writing to.  Closes any open
  /// temporary file.  Maintains file position information.
  /// Useful for writing a raster data block into a new file.
  /// NOTE: steals pointer; LargeRAWFile must live as long as the RDB.
  void ResetFile(LargeRAWFile_ptr);

  const std::vector<uint64_t>& GetBrickCount(const std::vector<uint64_t>& vLOD) const;
  const std::vector<uint64_t>& GetBrickSize(const std::vector<uint64_t>& vLOD, const std::vector<uint64_t>& vBrick) const;
  std::vector<uint64_t> GetLODDomainSize(const std::vector<uint64_t>& vLOD) const;

  bool BrickedLODToFlatData(const std::vector<uint64_t>& vLOD, const std::string& strTargetFile,
                            bool bAppend = false, AbstrDebugOut* pDebugOut=NULL,
                            bool (*brickFunc)(LargeRAWFile_ptr pSourceFile,
                                const std::vector<uint64_t> vBrickSize,
                                const std::vector<uint64_t> vBrickOffset,
                                void* pUserContext ) = NULL,
                            void* pUserContext = NULL,
                            uint64_t iOverlap=0) const;

  const std::vector<uint64_t> LargestSingleBrickLODBrickIndex() const;
  const std::vector<uint64_t>& LargestSingleBrickLODBrickSize() const;

  const std::vector<uint64_t> GetSmallestBrickIndex() const;
  const std::vector<uint64_t>& GetSmallestBrickSize() const;
  const std::vector<uint64_t> GetLargestBrickSizes() const;

  bool FlatDataToBrickedLOD(const void* pSourceData, const std::string& strTempFile,
                            void (*combineFunc)(const std::vector<uint64_t> &vSource, uint64_t iTarget, const void* pIn, const void* pOut),
                            void (*maxminFunc)(const void* pIn, size_t iStart,
                                               size_t iCount,
                                               std::vector<DOUBLEVECTOR4>& fMinMax),
                            MaxMinDataBlock* pMaxMinDatBlock = NULL, AbstrDebugOut* pDebugOut=NULL);
  bool FlatDataToBrickedLOD(LargeRAWFile_ptr pSourceData, const std::string& strTempFile,
                            void (*combineFunc)(const std::vector<uint64_t> &vSource, uint64_t iTarget, const void* pIn, const void* pOut),
                            void (*maxminFunc)(const void* pIn, size_t iStart,
                                               size_t iCount,
                                               std::vector<DOUBLEVECTOR4>& fMinMax),
                            MaxMinDataBlock* pMaxMinDatBlock = NULL, AbstrDebugOut* pDebugOut=NULL);
  void AllocateTemp(const std::string& strTempFile, bool bBuildOffsetTables=false);
  bool ValidLOD(const std::vector<uint64_t>& vLOD) const;
  bool ValidBrickIndex(const std::vector<uint64_t>& vLOD,
                       const std::vector<uint64_t>& vBrick) const;


protected:
  LargeRAWFile_ptr m_pTempFile;
  LargeRAWFile_ptr m_pSourceFile;
  uint64_t        m_iSourcePos;

  virtual void CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual DataBlock* Clone() const;
  virtual uint64_t GetOffsetToNextBlock() const;

  std::vector<std::vector<uint64_t> > CountToVectors(std::vector<uint64_t> vCountVector) const ;
  uint64_t ComputeElementSize() const;
  virtual uint64_t GetLODSize(std::vector<uint64_t>& vLODIndices) const;
  virtual uint64_t ComputeLODLevelSize(const std::vector<uint64_t>& vReducedDomainSize) const;
  virtual std::vector<std::vector<uint64_t> > ComputeBricks(const std::vector<uint64_t>& vDomainSize) const;
  virtual std::vector<std::vector<uint64_t> > GenerateCartesianProduct(const std::vector<std::vector<uint64_t> >& vElements, uint64_t iIndex=0) const;
  uint64_t RecompLODIndexCount() const;
  void CleanupTemp();


  friend class UVF;
  friend class Histogram1DDataBlock;
  friend class Histogram2DDataBlock;

  // CONVENIENCE FUNCTION HELPERS
  std::vector<uint64_t> m_vLODOffsets;
  std::vector<std::vector<uint64_t> > m_vBrickCount;
  std::vector<std::vector<uint64_t> > m_vBrickOffsets;
  std::vector<std::vector<std::vector<uint64_t> > > m_vBrickSizes;

  uint64_t Serialize(const std::vector<uint64_t>& vec, const std::vector<uint64_t>& vSizes) const;
  uint64_t GetLocalDataPointerOffset(const std::vector<uint64_t>& vLOD, const std::vector<uint64_t>& vBrick) const;
  uint64_t GetLocalDataPointerOffset(const uint64_t iLODIndex, const uint64_t iBrickIndex) const {return m_vLODOffsets[size_t(iLODIndex)] + m_vBrickOffsets[size_t(iLODIndex)][size_t(iBrickIndex)];}
  void SubSample(LargeRAWFile_ptr pSourceFile, LargeRAWFile_ptr pTargetFile, std::vector<uint64_t> sourceSize, std::vector<uint64_t> targetSize, void (*combineFunc)(const std::vector<uint64_t> &vSource, uint64_t iTarget, const void* pIn, const void* pOut), AbstrDebugOut* pDebugOut=NULL, uint64_t iLODLevel=0, uint64_t iMaxLODLevel=0);
  uint64_t ComputeDataSizeAndOffsetTables();
  uint64_t GetLODSizeAndOffsetTables(std::vector<uint64_t>& vLODIndices, uint64_t iLOD);
  uint64_t ComputeLODLevelSizeAndOffsetTables(const std::vector<uint64_t>& vReducedDomainSize, uint64_t iLOD);

  bool TraverseBricksToWriteBrickToFile(
      uint64_t& iBrickCounter, uint64_t iBrickCount,
      const std::vector<uint64_t>& vLOD,
      const std::vector<uint64_t>& vBrickCount,
      std::vector<uint64_t> vCoords,
      size_t iCurrentDim, uint64_t iTargetOffset,
      std::vector<unsigned char> &vData,
      LargeRAWFile_ptr pTargetFile, uint64_t iElementSize,
      const std::vector<uint64_t>& vPrefixProd,
      AbstrDebugOut* pDebugOut,
      bool (*brickFunc)(LargeRAWFile_ptr pSourceFile,
                        const std::vector<uint64_t> vBrickSize,
                        const std::vector<uint64_t> vBrickOffset,
                        void* pUserContext),
      void* pUserContext,
      uint64_t iOverlap
  ) const;

  void WriteBrickToFile(size_t iCurrentDim,
                        uint64_t& iSourceOffset, uint64_t& iTargetOffset,
                        const std::vector<uint64_t>& vBrickSize,
                        const std::vector<uint64_t>& vEffectiveBrickSize,
                        std::vector<unsigned char>& vData,
                        LargeRAWFile_ptr pTargetFile, uint64_t iElementSize,
                        const std::vector<uint64_t>& vPrefixProd,
                        const std::vector<uint64_t>& vPrefixProdBrick,
                        bool bDoSeek) const;
private:
  LargeRAWFile_ptr SeekToBrick(const std::vector<uint64_t>& vLOD,
                            const std::vector<uint64_t>& vBrick) const;
  bool GetData(unsigned char*, size_t bytes,
               const std::vector<uint64_t>& vLOD,
               const std::vector<uint64_t>& vBrick) const;

  size_t GetBrickByteSize(const std::vector<uint64_t>& vLOD,
                          const std::vector<uint64_t>& vBrick) const;
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
      std::vector<uint64_t> vl(1);
      std::vector<uint64_t> b = this->NDBrickIndex(brick);
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
    std::vector<uint64_t> NDBrickIndex(size_t b) {
      uint64_t brick = static_cast<uint64_t>(b);
      std::vector<uint64_t> lod(1);
      lod[0] = LODIndex();
      const std::vector<uint64_t>& counts = rdb->GetBrickCount(lod);

      uint64_t z = static_cast<uint64_t>(brick / (counts[0] * counts[1]));
      brick = brick % (counts[0] * counts[1]);
      uint64_t y = static_cast<uint64_t>(brick / counts[0]);
      brick = brick % counts[0];
      uint64_t x = brick;

      std::vector<uint64_t> vec(3);
      vec[0] = x;
      vec[1] = y;
      vec[2] = z;
      return vec;
    }

    size_t LODIndex() const {
      switch(LoD) {
        case FINEST_RESOLUTION: return 0;
        case COARSEST_RESOLUTION:
          return static_cast<size_t>(rdb->ulLODLevelCount[0]);
        default: return static_cast<size_t>(LoD);
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
