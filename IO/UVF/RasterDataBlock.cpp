#include <cmath>
#include <numeric>
#include <sstream>

#include "RasterDataBlock.h"
#include "MaxMinDataBlock.h"
#include "DebugOut/AbstrDebugOut.h"
#include <Basics/MathTools.h>
#include <Basics/SysTools.h>
#include "Controller/Controller.h"
#include "Basics/nonstd.h"

using namespace boost;
using namespace std;
using namespace UVFTables;

//*************** Raster Data Block **********************

RasterDataBlock::RasterDataBlock() :
  DataBlock(),
  ulElementDimension(0),
  ulOffsetToDataBlock(0),

  m_pTempFile(LargeRAWFile_ptr()),
  m_pSourceFile(LargeRAWFile_ptr()),
  m_iSourcePos(0)
{
  ulBlockSemantics = BS_REG_NDIM_GRID;
}

RasterDataBlock::RasterDataBlock(const RasterDataBlock &other) :
  DataBlock(other),
  ulDomainSemantics(other.ulDomainSemantics),
  dDomainTransformation(other.dDomainTransformation),
  ulDomainSize(other.ulDomainSize),
  ulBrickSize(other.ulBrickSize),
  ulBrickOverlap(other.ulBrickOverlap),
  ulLODDecFactor(other.ulLODDecFactor),
  ulLODGroups(other.ulLODGroups),
  ulLODLevelCount(other.ulLODLevelCount),
  ulElementDimension(other.ulElementDimension),
  ulElementDimensionSize(other.ulElementDimensionSize),
  ulElementSemantic(other.ulElementSemantic),
  ulElementBitSize(other.ulElementBitSize),
  ulElementMantissa(other.ulElementMantissa),
  bSignedElement(other.bSignedElement),
  ulOffsetToDataBlock(other.ulOffsetToDataBlock),

  m_pTempFile(LargeRAWFile_ptr()),
  m_pSourceFile(LargeRAWFile_ptr()),
  m_iSourcePos(0)
{
  if (other.m_pTempFile != LargeRAWFile_ptr()) 
    m_pTempFile = LargeRAWFile_ptr(new LargeRAWFile(*(other.m_pTempFile)));
  else {
    m_pSourceFile = other.m_pStreamFile;
    m_iSourcePos = other.m_iOffset +
                   DataBlock::GetOffsetToNextBlock() +
                   other.ComputeHeaderSize();
  }

  m_vLODOffsets = other.m_vLODOffsets;
  m_vBrickCount = other.m_vBrickCount;
  m_vBrickOffsets = other.m_vBrickOffsets;
  m_vBrickSizes = other.m_vBrickSizes;
}


RasterDataBlock& RasterDataBlock::operator=(const RasterDataBlock& other)  {
  strBlockID = other.strBlockID;
  ulBlockSemantics = other.ulBlockSemantics;
  ulCompressionScheme = other.ulCompressionScheme;
  ulOffsetToNextDataBlock = other.ulOffsetToNextDataBlock;

  ulDomainSemantics = other.ulDomainSemantics;
  dDomainTransformation = other.dDomainTransformation;
  ulDomainSize = other.ulDomainSize;
  ulBrickSize = other.ulBrickSize;
  ulBrickOverlap = other.ulBrickOverlap;
  ulLODDecFactor = other.ulLODDecFactor;
  ulLODGroups = other.ulLODGroups;
  ulLODLevelCount = other.ulLODLevelCount;
  ulElementDimension = other.ulElementDimension;
  ulElementDimensionSize = other.ulElementDimensionSize;
  ulElementSemantic = other.ulElementSemantic;
  ulElementBitSize = other.ulElementBitSize;
  ulElementMantissa = other.ulElementMantissa;
  bSignedElement = other.bSignedElement;
  ulOffsetToDataBlock = other.ulOffsetToDataBlock;

  m_vLODOffsets = other.m_vLODOffsets;
  m_vBrickCount = other.m_vBrickCount;
  m_vBrickOffsets = other.m_vBrickOffsets;
  m_vBrickSizes = other.m_vBrickSizes;

  m_pTempFile = LargeRAWFile_ptr();
  m_pSourceFile = other.m_pSourceFile;
  m_iSourcePos = other.m_iSourcePos;

  return *this;
}

RasterDataBlock::~RasterDataBlock() {
  CleanupTemp();
}

RasterDataBlock::RasterDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian) :
  DataBlock(),
  ulElementDimension(0),
  ulOffsetToDataBlock(0),

  m_pTempFile(LargeRAWFile_ptr()),
  m_pSourceFile(LargeRAWFile_ptr())
{
  GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
}

DataBlock* RasterDataBlock::Clone() const {
  return new RasterDataBlock(*this);
}

uint64_t RasterDataBlock::GetOffsetToNextBlock() const {
  return DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize() + ComputeDataSize();
}

uint64_t RasterDataBlock::ComputeHeaderSize() const {
  uint64_t ulDomainDimension = ulDomainSemantics.size();
  uint64_t ulOverallElementSize=0;

  for (size_t i = 0;i<ulElementDimensionSize.size();i++) ulOverallElementSize += ulElementDimensionSize[i];

  return 1            * sizeof(uint64_t) +    // ulDomainDimension
       ulDomainDimension    * sizeof(uint64_t) +    // ulDomainSemantics
       (ulDomainDimension+1)*(ulDomainDimension+1)  * sizeof(double) +    // dDomainTransformation
       ulDomainDimension    * sizeof(uint64_t) +    // ulDomainSize
       ulDomainDimension    * sizeof(uint64_t) +    // ulBrickSize
       ulDomainDimension    * sizeof(uint64_t) +    // ulBrickOverlap
       ulDomainDimension    * sizeof(uint64_t) +    // ulLODDecFactor
       ulDomainDimension    * sizeof(uint64_t) +    // ulLODGroups
       ulLODLevelCount.size()  * sizeof(uint64_t) +    // ulLODLevelCount

       1            * sizeof(uint64_t) +    // ulElementDimension
       ulElementDimension    * sizeof(uint64_t) +    // ulElementDimensionSize

       ulOverallElementSize    * sizeof(uint64_t) +    // ulElementSemantic
       ulOverallElementSize    * sizeof(uint64_t) +    // ulElementBitSize
       ulOverallElementSize    * sizeof(uint64_t) +    // ulElementMantissa
       ulOverallElementSize    * sizeof(bool) +    // bSignedElement

       1            * sizeof(uint64_t);    // ulOffsetToDataBlock

}

uint64_t RasterDataBlock::GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian) {
  uint64_t iStart = iOffset + DataBlock::GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
  pStreamFile->SeekPos(iStart);

  uint64_t ulDomainDimension;
  pStreamFile->ReadData(ulDomainDimension, bIsBigEndian);

  if (ulDomainDimension > 0) {
    vector<uint64_t> uintVect;
    pStreamFile->ReadData(uintVect, ulDomainDimension, bIsBigEndian);
    ulDomainSemantics.resize(size_t(ulDomainDimension));
    for (size_t i = 0;i<uintVect.size();i++) ulDomainSemantics[i] = (DomainSemanticTable)uintVect[i];

    pStreamFile->ReadData(dDomainTransformation, (ulDomainDimension+1)*(ulDomainDimension+1), bIsBigEndian);
    pStreamFile->ReadData(ulDomainSize, ulDomainDimension, bIsBigEndian);
    pStreamFile->ReadData(ulBrickSize, ulDomainDimension, bIsBigEndian);
    pStreamFile->ReadData(ulBrickOverlap, ulDomainDimension, bIsBigEndian);
    pStreamFile->ReadData(ulLODDecFactor, ulDomainDimension, bIsBigEndian);
    pStreamFile->ReadData(ulLODGroups, ulDomainDimension, bIsBigEndian);

    RecompLODIndexCount();
  }

  uint64_t ulLODIndexCount = RecompLODIndexCount();
  pStreamFile->ReadData(ulLODLevelCount, ulLODIndexCount, bIsBigEndian);
  pStreamFile->ReadData(ulElementDimension, bIsBigEndian);
  pStreamFile->ReadData(ulElementDimensionSize, ulElementDimension, bIsBigEndian);

  ulElementSemantic.resize(size_t(ulElementDimension));
  ulElementBitSize.resize(size_t(ulElementDimension));
  ulElementMantissa.resize(size_t(ulElementDimension));
  bSignedElement.resize(size_t(ulElementDimension));
  for (size_t i = 0;i<size_t(ulElementDimension);i++) {

    vector<uint64_t> uintVect;
    pStreamFile->ReadData(uintVect, ulElementDimensionSize[i], bIsBigEndian);
    ulElementSemantic[i].resize(size_t(ulElementDimensionSize[i]));
    for (size_t j = 0;j<uintVect.size();j++) ulElementSemantic[i][j] = (ElementSemanticTable)uintVect[j];

    pStreamFile->ReadData(ulElementBitSize[i], ulElementDimensionSize[i], bIsBigEndian);
    pStreamFile->ReadData(ulElementMantissa[i], ulElementDimensionSize[i], bIsBigEndian);

    // reading bools failed on windows so we are reading chars
    vector<char> charVect;
    pStreamFile->ReadData(charVect, ulElementDimensionSize[i], bIsBigEndian);
    bSignedElement[i].resize(size_t(ulElementDimensionSize[i]));
    for (size_t j = 0;j<charVect.size();j++)  bSignedElement[i][j] = charVect[j] != 0;
  }

  pStreamFile->ReadData(ulOffsetToDataBlock, bIsBigEndian);

  ComputeDataSizeAndOffsetTables();  // build the offset table

  return pStreamFile->GetPos() - iOffset;
}

void RasterDataBlock::CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  DataBlock::CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);

  // write header
  uint64_t ulDomainDimension = ulDomainSemantics.size();
  pStreamFile->WriteData(ulDomainDimension, bIsBigEndian);

  if (ulDomainDimension > 0) {
    vector<uint64_t> uintVect; uintVect.resize(size_t(ulDomainDimension));
    for (size_t i = 0;i<uintVect.size();i++) uintVect[i] = (uint64_t)ulDomainSemantics[i];
    pStreamFile->WriteData(uintVect, bIsBigEndian);

    pStreamFile->WriteData(dDomainTransformation, bIsBigEndian);
    pStreamFile->WriteData(ulDomainSize, bIsBigEndian);
    pStreamFile->WriteData(ulBrickSize, bIsBigEndian);
    pStreamFile->WriteData(ulBrickOverlap, bIsBigEndian);
    pStreamFile->WriteData(ulLODDecFactor, bIsBigEndian);
    pStreamFile->WriteData(ulLODGroups, bIsBigEndian);
  }

  pStreamFile->WriteData(ulLODLevelCount, bIsBigEndian);
  pStreamFile->WriteData(ulElementDimension, bIsBigEndian);
  pStreamFile->WriteData(ulElementDimensionSize, bIsBigEndian);

  for (size_t i = 0;i<size_t(ulElementDimension);i++) {

    vector<uint64_t> uintVect; uintVect.resize(size_t(ulElementDimensionSize[i]));
    for (size_t j = 0;j<uintVect.size();j++)  uintVect[j] = (uint64_t)ulElementSemantic[i][j];
    pStreamFile->WriteData(uintVect, bIsBigEndian);

    pStreamFile->WriteData(ulElementBitSize[i], bIsBigEndian);
    pStreamFile->WriteData(ulElementMantissa[i], bIsBigEndian);

    // writing bools failed on windows so we are writing chars
    vector<char> charVect; charVect.resize(size_t(ulElementDimensionSize[i]));
    for (size_t j = 0;j<charVect.size();j++)  charVect[j] = bSignedElement[i][j] ? 1 : 0;
    pStreamFile->WriteData(charVect, bIsBigEndian);
  }

  pStreamFile->WriteData(ulOffsetToDataBlock, bIsBigEndian);
}

uint64_t RasterDataBlock::CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);

  uint64_t iDataSize = ComputeDataSize();

  LargeRAWFile_ptr pSourceFile;

  if (m_pTempFile) {
    m_pTempFile->SeekStart();
    pSourceFile = m_pTempFile;
  } else {
    m_pSourceFile->SeekPos(m_iSourcePos);
    pSourceFile = m_pSourceFile;
  }

  pStreamFile->SeekPos( pStreamFile->GetPos() + ulOffsetToDataBlock);

  unsigned char* pData = new unsigned char[size_t(min(iDataSize, BLOCK_COPY_SIZE))];
  for (uint64_t i = 0;i<iDataSize;i+=BLOCK_COPY_SIZE) {
    uint64_t iCopySize = min(BLOCK_COPY_SIZE, iDataSize-i);

    pSourceFile->ReadRAW(pData, iCopySize);
    pStreamFile->WriteRAW(pData, iCopySize);
  }
  delete [] pData;

  return pStreamFile->GetPos() - iOffset;
}

/**
 * Dumps the input data into a temp file and calls FlatDataToBrickedLOD
 * \param vElements - the input vectors of vector
 * \param iIndex - counter used internally to control the recursion, defaults
 *                 to 0 and should not be set
 * \return - the cartesian product of the ordered elements in the input vectors
 *           as a vector of vectors
 */
vector<vector<uint64_t>> RasterDataBlock::GenerateCartesianProduct(const vector<vector<uint64_t>>& vElements, uint64_t iIndex)  const {
  vector<vector<uint64_t>> vResult;
  if (iIndex == vElements.size()-1) {
    for (size_t i = 0;i<vElements[vElements.size()-1].size();i++) {
      vector<uint64_t> v;
      v.push_back(vElements[vElements.size()-1][i]);
      vResult.push_back(v);
    }
  } else {
    vector<vector<uint64_t>> vTmpResult = GenerateCartesianProduct(vElements,iIndex+1);
    for (size_t j = 0;j<vTmpResult.size();j++) {
      for (size_t i = 0;i<vElements[size_t(iIndex)].size();i++) {
        vector<uint64_t> v;
        v.push_back(vElements[size_t(iIndex)][i]);

        for (size_t k = 0;k<vTmpResult[j].size();k++) v.push_back(vTmpResult[j][k]);

        vResult.push_back(v);
      }
    }
  }

  return vResult;
}

/**
 * Computes a vector of vectors, where each vector holds a list of brick sizes
 * in one dimension
 * \param vDomainSize - the size of the domain
 * \return - a vector of vectors, where each vector holds a list of bricksizes
 *           in one dimension
 */
vector<vector<uint64_t>> RasterDataBlock::ComputeBricks(const vector<uint64_t>& vDomainSize) const {
  vector<vector<uint64_t>> vBrickLayout;

  for (size_t iDomainDimension = 0;iDomainDimension<vDomainSize.size();iDomainDimension++) {
    uint64_t iSize         = vDomainSize[iDomainDimension];
    uint64_t iBrickSize     = ulBrickSize[iDomainDimension];
    uint64_t iBrickOverlap = ulBrickOverlap[iDomainDimension];

    assert(iBrickSize>iBrickOverlap); // sanity check

    vector<uint64_t> vBricks;

    if (iSize <= iBrickSize) {
      vBricks.push_back(iSize);
    } else {
      do {
        if (iSize+iBrickOverlap <= iBrickSize) {
          vBricks.push_back(iSize);
          break;
        } else {
          vBricks.push_back(iBrickSize);
          iSize = iSize+iBrickOverlap-iBrickSize;
        }
      } while (iSize > iBrickOverlap);
    }

    vBrickLayout.push_back(vBricks);
  }

  return vBrickLayout;
}

/**
 * \return - the size of a single elment in the data IN BITS
 */
uint64_t RasterDataBlock::ComputeElementSize() const {
  // compute the size of a single data element
  uint64_t uiBitsPerElement = 0;
  for (size_t i = 0;i<ulElementDimension;i++)
    for (size_t j = 0;j<ulElementDimensionSize[i];j++)
      uiBitsPerElement += ulElementBitSize[i][j];
  return uiBitsPerElement;
}

uint64_t RasterDataBlock::ComputeLODLevelSizeAndOffsetTables(const vector<uint64_t>& vReducedDomainSize, uint64_t iLOD) {
  uint64_t ulSize = 0;

  // compute the size of a single data element
  uint64_t uiBitsPerElement = ComputeElementSize();

  // compute brick layout
  vector<vector<uint64_t>> vBricks = ComputeBricks(vReducedDomainSize);
  vector<vector<uint64_t>> vBrickPermutation = GenerateCartesianProduct(vBricks);

  for (size_t i=0; i < vBricks.size(); i++) {
    m_vBrickCount[size_t(iLOD)].push_back(vBricks[i].size());
  }
  m_vBrickOffsets[size_t(iLOD)].push_back(0);
  m_vBrickSizes[size_t(iLOD)] = vBrickPermutation;

  ulSize = 0;
  for (size_t i = 0;i<vBrickPermutation.size();i++) {
    uint64_t ulBrickSize = vBrickPermutation[i][0];
    for (size_t j = 1;j<vBrickPermutation[i].size();j++) {
      ulBrickSize *= vBrickPermutation[i][j];
    }
    ulSize += ulBrickSize;

    if (i<vBrickPermutation.size()-1) m_vBrickOffsets[size_t(iLOD)].push_back(ulSize*uiBitsPerElement);
  }

  return  ulSize * uiBitsPerElement;
}


uint64_t RasterDataBlock::ComputeLODLevelSize(const vector<uint64_t>& vReducedDomainSize) const {
  uint64_t ulSize = 0;

  // compute the size of a single data element
  uint64_t uiBitsPerElement = ComputeElementSize();

  // compute brick layout
  vector<vector<uint64_t>> vBricks = ComputeBricks(vReducedDomainSize);
  vector<vector<uint64_t>> vBrickPermutation = GenerateCartesianProduct(vBricks);

  ulSize = 0;
  for (size_t i = 0;i<vBrickPermutation.size();i++) {
    uint64_t ulBrickSize = vBrickPermutation[i][0];
    for (size_t j = 1;j<vBrickPermutation[i].size();j++) {
      ulBrickSize *= vBrickPermutation[i][j];
    }
    ulSize += ulBrickSize;
  }

  return  ulSize * uiBitsPerElement;
}

uint64_t RasterDataBlock::GetLODSize(vector<uint64_t>& vLODIndices) const {

  uint64_t ulSize = 0;

  vector<uint64_t> vReducedDomainSize;
  vReducedDomainSize.resize(ulDomainSemantics.size());

  // compute size of the domain
  for (size_t i=0;i<vReducedDomainSize.size();i++) {
    if(ulLODDecFactor[i] < 2)
      vReducedDomainSize[i]  = ulDomainSize[i];
    else
      vReducedDomainSize[i]  = max<uint64_t>(1,uint64_t(floor(double(ulDomainSize[i]) / double(MathTools::Pow(ulLODDecFactor[i],vLODIndices[size_t(ulLODGroups[i])])))));
  }

  ulSize = ComputeLODLevelSize(vReducedDomainSize);

  return ulSize;
}

uint64_t RasterDataBlock::GetLODSizeAndOffsetTables(vector<uint64_t>& vLODIndices, uint64_t iLOD) {

  uint64_t ulSize = 0;

  vector<uint64_t> vReducedDomainSize;
  vReducedDomainSize.resize(ulDomainSemantics.size());

  // compute size of the domain
  for (size_t i=0;i<vReducedDomainSize.size();i++) {
    if(ulLODDecFactor[i] < 2)
      vReducedDomainSize[i]  = ulDomainSize[i];
    else
      vReducedDomainSize[i]  = max<uint64_t>(1,uint64_t(floor(double(ulDomainSize[i]) / double(MathTools::Pow(ulLODDecFactor[i],vLODIndices[size_t(ulLODGroups[i])])))));
  }

  ulSize = ComputeLODLevelSizeAndOffsetTables(vReducedDomainSize, iLOD);

  return ulSize;
}

vector<vector<uint64_t>> RasterDataBlock::CountToVectors(vector<uint64_t> vCountVector) const {
  vector<vector<uint64_t>> vResult;

  vResult.resize(vCountVector.size());
  for (size_t i=0;i<vCountVector.size();i++) {
    for (size_t j=0;j<vCountVector[i];j++) {
      vResult[i].push_back(j);
    }
  }

  return vResult;
}


uint64_t RasterDataBlock::ComputeDataSize(string* pstrProblem) const {
  if (!Verify(pstrProblem)) return UVF_INVALID;

  uint64_t iDataSize = 0;

  // iterate over all LOD-Group Combinations
  vector<vector<uint64_t>> vLODCombis = GenerateCartesianProduct(CountToVectors(ulLODLevelCount));

  for (size_t i = 0;i<vLODCombis.size();i++) {
    uint64_t iLODLevelSize = GetLODSize(vLODCombis[i]);
    iDataSize += iLODLevelSize;
  }

  return iDataSize/8;
}

uint64_t RasterDataBlock::ComputeDataSizeAndOffsetTables() {
  if (!Verify()) return UVF_INVALID;

  uint64_t iDataSize = 0;

  // iterate over all LOD-Group Combinations
  vector<vector<uint64_t>> vLODCombis = GenerateCartesianProduct(CountToVectors(ulLODLevelCount));

  m_vLODOffsets.resize(vLODCombis.size());
  m_vBrickCount.resize(vLODCombis.size());
  m_vBrickOffsets.resize(vLODCombis.size());
  m_vBrickSizes.resize(vLODCombis.size());
  m_vLODOffsets[0] = 0;

  for (size_t i = 0;i<vLODCombis.size();i++) {
    uint64_t iLODLevelSize = GetLODSizeAndOffsetTables(vLODCombis[i],i);
    iDataSize += iLODLevelSize;

    if (i<vLODCombis.size()-1) m_vLODOffsets[i+1] = m_vLODOffsets[i] + iLODLevelSize;
  }

  return iDataSize/8;
}

uint64_t RasterDataBlock::RecompLODIndexCount() const {
  uint64_t ulLODIndexCount = 1;
  for (size_t i = 0;i<ulLODGroups.size();i++) if (ulLODGroups[i] >= ulLODIndexCount) ulLODIndexCount = ulLODGroups[i]+1;
  return ulLODIndexCount;
}


bool RasterDataBlock::Verify(string* pstrProblem) const {
  uint64_t ulDomainDimension = ulDomainSemantics.size();

  uint64_t ulLODIndexCount = RecompLODIndexCount();

  if ( dDomainTransformation.size() != (ulDomainDimension+1)*(ulDomainDimension+1) ||
     ulDomainSize.size() != ulDomainDimension ||
     ulBrickSize.size() != ulDomainDimension ||
     ulBrickOverlap.size() != ulDomainDimension ||
     ulLODDecFactor.size() != ulDomainDimension ||
     ulLODGroups.size() != ulDomainDimension ||
     ulLODDecFactor.size() != ulDomainDimension ||
     ulLODLevelCount.size() != ulLODIndexCount ||
     ulElementDimensionSize.size() != ulElementDimension) {

      if (pstrProblem != NULL) *pstrProblem = "RasterDataBlock::Verify ulDomainDimension mismatch";
      return false;
  }

  for (size_t i = 0;i<size_t(ulDomainDimension);i++) {
    if (ulBrickSize[i] <= ulBrickOverlap[i]) {
      if (pstrProblem != NULL) {
        stringstream s;
        s << "RasterDataBlock::Verify ulBrickSize[" << i << "] > ulBrickOverlap[" << i << "]";
        *pstrProblem = s.str();
      }
      return false;
    }
  }

  for (size_t i = 0;i<size_t(ulElementDimension);i++) {

    if (ulElementSemantic[i].size() != ulElementDimensionSize[i] ||
      ulElementBitSize[i].size() != ulElementDimensionSize[i] ||
      ulElementMantissa[i].size() != ulElementDimensionSize[i] ||
      bSignedElement[i].size() != ulElementDimensionSize[i]) {
        if (pstrProblem != NULL) {
          stringstream s;
          s << "RasterDataBlock::Verify ulElementDimensionSize[" << i << "] mismatch";
          *pstrProblem = s.str();
        }
        return false;
    }

  }

  return true;
}

bool RasterDataBlock::Verify(uint64_t iSizeofData, string* pstrProblem) const {
  if (pstrProblem != NULL && iSizeofData != UVF_INVALID) *pstrProblem = "RasterDataBlock::Verify iSizeofData != UVF_INVALID";

  // ComputeDataSize calls Verify() which does all the other checks
  return (iSizeofData != UVF_INVALID) && (ComputeDataSize(pstrProblem) == iSizeofData);
}


bool RasterDataBlock::SetBlockSemantic(BlockSemanticTable bs) {
  if (bs != BS_REG_NDIM_GRID &&
      bs != BS_NDIM_TRANSFER_FUNC &&
      bs != BS_PREVIEW_IMAGE) return false;

  ulBlockSemantics = bs;
  return true;
}


// **************** CONVENIENCE FUNCTIONS *************************

void RasterDataBlock::SetScaleOnlyTransformation(const vector<double>& vScale) {
  uint64_t ulDomainDimension = ulDomainSemantics.size();

  dDomainTransformation.resize(size_t((ulDomainDimension+1)*(ulDomainDimension+1)));

  for (size_t y = 0;y < size_t(ulDomainDimension+1);y++)
    for (size_t x = 0;x < size_t(ulDomainDimension+1);x++)
      dDomainTransformation[x+y*size_t(ulDomainDimension+1)] = (x == y) ? (x < vScale.size() ? vScale[x] : 1.0) : 0.0;

}

void RasterDataBlock::SetIdentityTransformation() {
  uint64_t ulDomainDimension = ulDomainSemantics.size();

  dDomainTransformation.resize(size_t((ulDomainDimension+1)*(ulDomainDimension+1)));

  for (size_t y = 0;y < size_t(ulDomainDimension+1);y++)
    for (size_t x = 0;x < size_t(ulDomainDimension+1);x++)
      dDomainTransformation[x+y*size_t(ulDomainDimension+1)] = (x == y) ? 1.0 : 0.0;

}

void RasterDataBlock::SetTypeToScalar(uint64_t iBitWith, uint64_t iMantissa, bool bSigned, ElementSemanticTable semantic) {
  vector<ElementSemanticTable> vSemantic;
  vSemantic.push_back(semantic);
  SetTypeToVector(iBitWith, iMantissa, bSigned, vSemantic);
}

void RasterDataBlock::SetTypeToVector(uint64_t iBitWith, uint64_t iMantissa, bool bSigned, vector<ElementSemanticTable> semantic) {

  vector<uint64_t> vecB;
  vector<uint64_t> vecM;
  vector<bool> vecSi;

  for (uint64_t i = 0;i<semantic.size();i++) {
    vecB.push_back(iBitWith);
    vecM.push_back(iMantissa);
    vecSi.push_back(bSigned);
  }

  ulElementDimension = 1;

  ulElementDimensionSize.push_back(semantic.size());
  ulElementSemantic.push_back(semantic);
  ulElementMantissa.push_back(vecM);
  bSignedElement.push_back(vecSi);
  ulElementBitSize.push_back(vecB);
}

void RasterDataBlock::SetTypeToUByte(ElementSemanticTable semantic) {
  SetTypeToScalar(8,8,false,semantic);
}

void RasterDataBlock::SetTypeToUShort(ElementSemanticTable semantic) {
  SetTypeToScalar(16,16,false,semantic);
}

void RasterDataBlock::SetTypeToInt32(ElementSemanticTable semantic) {
  SetTypeToScalar(32,31,true,semantic);
}

void RasterDataBlock::SetTypeToInt64(ElementSemanticTable semantic) {
  SetTypeToScalar(64,63,true,semantic);
}

void RasterDataBlock::SetTypeToUInt32(ElementSemanticTable semantic) {
  SetTypeToScalar(32,32,false,semantic);
}

void RasterDataBlock::SetTypeToUInt64(ElementSemanticTable semantic) {
  SetTypeToScalar(64,64,false,semantic);
}

void RasterDataBlock::SetTypeToFloat(ElementSemanticTable semantic) {
  SetTypeToScalar(32,23,true,semantic);
}

void RasterDataBlock::SetTypeToDouble(ElementSemanticTable semantic) {
  SetTypeToScalar(64,52,true,semantic);
}


uint64_t Product(const std::vector<uint64_t>& v) {
  uint64_t s = 1;
  for (size_t i = 0;i<v.size();i++) {
    s *= v[i];
  }
  return s;
}

const std::vector<uint64_t> RasterDataBlock::LargestSingleBrickLODBrickIndex() const {
  std::vector<uint64_t> vLargestSingleBrickLODIndex = GetSmallestBrickIndex();

  // for this to work we require the smalest level to contain only a single brick
  assert(Product(GetBrickCount(vLargestSingleBrickLODIndex)) == 1);

  for (size_t iLODGroups = 0;iLODGroups<ulLODLevelCount.size();iLODGroups++) {
    for (size_t iLOD = size_t(ulLODLevelCount[iLODGroups]); iLOD>0;  iLOD--) {  // being very carefull here as we are decrementing an unsigned value
      vLargestSingleBrickLODIndex[iLODGroups] = iLOD-1;
      if (Product(GetBrickCount(vLargestSingleBrickLODIndex)) > 1) {
        vLargestSingleBrickLODIndex[iLODGroups] = iLOD;
        break;
      }
    }
  }

  return vLargestSingleBrickLODIndex;
}

const std::vector<uint64_t>& RasterDataBlock::LargestSingleBrickLODBrickSize() const {
  std::vector<uint64_t> vLargestSingleBrickLOD = LargestSingleBrickLODBrickIndex();
  std::vector<uint64_t> vFirstBrick(GetBrickCount(vLargestSingleBrickLOD).size());
  for (size_t i = 0;i<vFirstBrick.size();i++) vFirstBrick[i] = 0; // get the size of the first brick
  return GetBrickSize(vLargestSingleBrickLOD, vFirstBrick);
}


const std::vector<uint64_t> RasterDataBlock::GetSmallestBrickIndex() const {
  std::vector<uint64_t> vSmallestLOD = ulLODLevelCount;
  for (size_t i = 0;i<vSmallestLOD.size();i++) vSmallestLOD[i] -= 1; // convert "size" to "maxindex"
  return vSmallestLOD;
}

const std::vector<uint64_t>& RasterDataBlock::GetSmallestBrickSize() const {
  std::vector<uint64_t> vSmallestLOD = GetSmallestBrickIndex();
  std::vector<uint64_t> vFirstBrick(GetBrickCount(vSmallestLOD).size());
  for (size_t i = 0;i<vFirstBrick.size();i++) vFirstBrick[i] = 0; // get the size of the first brick
  return GetBrickSize(vSmallestLOD, vFirstBrick);
}

const std::vector<uint64_t> RasterDataBlock::GetLargestBrickSizes() const {
  std::vector<uint64_t> vMax(m_vBrickSizes[0][0]);

  for (size_t i = 0;i<m_vBrickSizes.size();i++) {
    for (size_t j = 0;j<m_vBrickSizes[i].size();j++) {      
      for (size_t k = 0;k<m_vBrickSizes[i][j].size();k++) {
        vMax[k] = std::max(m_vBrickSizes[i][j][k],vMax[k]);
      }
    }
  }
  
  return vMax;
}

uint64_t RasterDataBlock::Serialize(const vector<uint64_t>& vec,
                                  const vector<uint64_t>& vSizes) const {
  uint64_t index = 0;
  uint64_t iPrefixProd = 1;
  for (size_t i = 0;i<vSizes.size();i++) {
    index += vec[i] * iPrefixProd;
    iPrefixProd *= vSizes[i];
  }

  return index;
}

const vector<uint64_t>&
RasterDataBlock::GetBrickCount(const vector<uint64_t>& vLOD) const {
  return m_vBrickCount[size_t(Serialize(vLOD, ulLODLevelCount))];
}

const vector<uint64_t>& RasterDataBlock::GetBrickSize(const vector<uint64_t>& vLOD,
                                                    const vector<uint64_t>& vBrick) const
{
  const uint64_t iLODIndex = Serialize(vLOD, ulLODLevelCount);
  const size_t brick_index = static_cast<size_t>(
    Serialize(vBrick, m_vBrickCount[size_t(iLODIndex)])
  );
  return m_vBrickSizes[size_t(iLODIndex)][brick_index];
}

uint64_t
RasterDataBlock::GetLocalDataPointerOffset(const vector<uint64_t>& vLOD,
                                           const vector<uint64_t>& vBrick) const
{
  assert(!vLOD.empty() && !vBrick.empty());
  if (vLOD.size() != ulLODLevelCount.size()) return 0;
  uint64_t iLODIndex = Serialize(vLOD, ulLODLevelCount);
  if (iLODIndex > m_vLODOffsets.size()) return 0;

  if (vBrick.size() != ulBrickSize.size()) return 0;
  uint64_t iBrickIndex = Serialize(vBrick, m_vBrickCount[size_t(iLODIndex)]);

  return GetLocalDataPointerOffset(iLODIndex,iBrickIndex);
}

void RasterDataBlock::SubSample(LargeRAWFile_ptr pSourceFile,
                                LargeRAWFile_ptr pTargetFile,
                                std::vector<uint64_t> sourceSize,
                                std::vector<uint64_t> targetSize,
                                void (*combineFunc)(
                                  const std::vector<uint64_t> &vSource,
                                  uint64_t iTarget,
                                  const void* pIn, void* pOut
                                ),
                                AbstrDebugOut* pDebugOut, uint64_t iLODLevel,
                                uint64_t iMaxLODLevel)
{
  pSourceFile->SeekStart();
  pTargetFile->SeekStart();

  uint64_t iTargetElementCount = 1;
  uint64_t iReduction = 1;
  vector<uint64_t> vReduction;
  for (size_t i = 0;i<targetSize.size();i++) {
    iTargetElementCount*= targetSize[i];
    vReduction.push_back(sourceSize[i] / targetSize[i]);
    iReduction *= vReduction[i];
  }

  // generate offset vector
  vector<vector<uint64_t>> vOffsetVectors = GenerateCartesianProduct(CountToVectors(vReduction));

  // generate 1D offset coords into serialized source data
  vector<uint64_t> vPrefixProd;
  vPrefixProd.push_back(1);
  for (size_t i = 1;i<sourceSize.size();i++) {
    vPrefixProd.push_back(*(vPrefixProd.end()-1) *  sourceSize[i-1]);
  }

  vector<uint64_t> vOffsetVector;
  vOffsetVector.resize(vOffsetVectors.size());
  for (size_t i = 0;i<vOffsetVector.size();i++) {
    vOffsetVector[i]  = vOffsetVectors[i][0];
    for (size_t j = 1;j<vPrefixProd.size();j++) vOffsetVector[i] += vOffsetVectors[i][j] * vPrefixProd[j];
  }

  vector<uint64_t> sourceElementsSerialized;
  sourceElementsSerialized.resize(vOffsetVector.size());

  unsigned char* pSourceData = NULL;
  unsigned char* pTargetData = NULL;

  uint64_t iSourceMinWindowSize = vOffsetVector[vOffsetVector.size()-1]+1;
  uint64_t iSourceWindowSize = iSourceMinWindowSize+(sourceSize[0]-vReduction[0]);
  uint64_t iTargetWindowSize = targetSize[0];

  vector<uint64_t> vSourcePos;
  for (size_t i = 0;i<sourceSize.size();i++) vSourcePos.push_back(0);

  uint64_t iElementSize = ComputeElementSize()/8;
  uint64_t iSourcePos = 0;
  uint64_t iWindowSourcePos = 0;
  uint64_t iWindowTargetPos = 0;
  
  static const size_t uItersPerUpdate = 100;
  size_t uCount = uItersPerUpdate;

  for (uint64_t i=0; i<iTargetElementCount; i++) {
    if (i==0 || iWindowTargetPos >= iTargetWindowSize) {
      if (i==0) {
        pSourceData = new unsigned char[size_t(iSourceWindowSize*iElementSize)];
        pTargetData = new unsigned char[size_t(iTargetWindowSize*iElementSize)];
      } else {
        const uint64_t bytes = iTargetWindowSize * iElementSize;
        if(pTargetFile->WriteRAW(pTargetData, bytes) != bytes) {
          pDebugOut->Error(_func_, "short write while subsampling!");
          assert(1 == 0);
        }
      }

      if(pDebugOut && (--uCount == 0)) {
        uCount = uItersPerUpdate;
        float fCurrentOutput = (100.0f*i)/float(iTargetElementCount);
        pDebugOut->Message(_func_, "Generating data for lod level %i of %i:"
                           "%6.2f%% completed", int(iLODLevel+1),
                           int(iMaxLODLevel), fCurrentOutput);
      }

      const uint64_t bytes = iSourceWindowSize*iElementSize;
      if (pSourceFile == pTargetFile) {
        // save and later restore position for in place subsampling
        uint64_t iFilePos = pSourceFile->GetPos();
        pSourceFile->SeekPos(iSourcePos*iElementSize);
        if(pSourceFile->ReadRAW(pSourceData, bytes) != bytes) {
          pDebugOut->Error(_func_, "short read from '%s'",
                           pSourceFile->GetFilename().c_str());
        }
        pSourceFile->SeekPos(iFilePos);
      } else {
        pSourceFile->SeekPos(iSourcePos*iElementSize);
        if(pSourceFile->ReadRAW(pSourceData, bytes) != bytes) {
          pDebugOut->Error(_func_, "short read from data");
        }
      }

      iWindowSourcePos = 0;
      iWindowTargetPos = 0;
    }

    // gather data in source array and combine into target array
    for (size_t j = 0;j<vOffsetVector.size();j++) sourceElementsSerialized[j] = vOffsetVector[j] + iWindowSourcePos;
    combineFunc(sourceElementsSerialized, iWindowTargetPos, pSourceData, pTargetData);

    // advance to next position in source array
    iWindowSourcePos += vReduction[0];
    iWindowTargetPos++;

    iSourcePos = 0;
    vSourcePos[0]+=vReduction[0];
    for (size_t j = 1;j<sourceSize.size();j++) {
      if (vSourcePos[j-1]+vReduction[j-1] > sourceSize[j-1]) {
        vSourcePos[j-1] = 0;
        vSourcePos[j] += vReduction[j-1];
      }
      iSourcePos += vPrefixProd[j-1] * vSourcePos[j-1];
    }
    iSourcePos += vPrefixProd[sourceSize.size()-1] * vSourcePos[sourceSize.size()-1];
  }

  const uint64_t bytes = iTargetWindowSize*iElementSize;
  if(pTargetFile->WriteRAW(pTargetData, bytes) != bytes) {
    pDebugOut->Error(_func_, "short write to '%s'",
                     pTargetFile->GetFilename().c_str());
  }
  delete[] pSourceData;
  delete[] pTargetData;
}


void RasterDataBlock::AllocateTemp(const string& strTempFile, bool bBuildOffsetTables) {
  CleanupTemp();

  uint64_t iDataSize = (bBuildOffsetTables) ? ComputeDataSizeAndOffsetTables(): ComputeDataSize();

  m_pTempFile = LargeRAWFile_ptr(new LargeRAWFile(strTempFile));
  if (!m_pTempFile->Create(iDataSize)) {
    m_pTempFile.reset();
    throw "Unable To create Temp File";
  }
}

/**
 * Dumps the input data into a temp file and calls FlatDataToBrickedLOD
 * \param pSourceData - void pointer to the flat input data
 * \param strTempFile - filename of a temp files during the conversion
 * \param combineFunc - the function used to compute the LOD, this is mostly an
 *                      average function
 * \return void
 * \see FlatDataToBrickedLOD
 */
bool RasterDataBlock::FlatDataToBrickedLOD(
  const void* pSourceData, const string& strTempFile,
  void (*combineFunc)(const vector<uint64_t>& vSource, uint64_t iTarget,
                      const void* pIn, void* pOut),
  void (*maxminFunc)(const void* pIn, size_t iStart, size_t iCount,
                     std::vector<DOUBLEVECTOR4>& fMinMax),
  std::shared_ptr<MaxMinDataBlock> pMaxMinDatBlock,
  AbstrDebugOut* pDebugOut)
{
  // size of input data
  uint64_t iInPointerSize = ComputeElementSize()/8;
  for (size_t i = 0;i<ulDomainSize.size();i++) iInPointerSize *= ulDomainSize[i];

  // create temp file and dump data into it
  LargeRAWFile pSourceFile(SysTools::AppendFilename(strTempFile,"0"));

  if (!pSourceFile.Create(  iInPointerSize )) {
    throw "Unable To create Temp File ";
  }

  pSourceFile.WriteRAW((unsigned char*)pSourceData, iInPointerSize);

  // convert the flat file to our bricked LOD representation
  bool bResult = FlatDataToBrickedLOD(
    std::shared_ptr<LargeRAWFile>(&pSourceFile, nonstd::null_deleter()),
    strTempFile, combineFunc, maxminFunc, pMaxMinDatBlock, pDebugOut
  );

  // delete tempfile
  pSourceFile.Delete();

  return bResult;
}


vector<uint64_t> RasterDataBlock::GetLODDomainSize(const vector<uint64_t>& vLOD) const {

  vector<uint64_t> vReducedDomainSize;
  vReducedDomainSize.resize(ulDomainSemantics.size());

  for (size_t j=0;j<vReducedDomainSize.size();j++)
    vReducedDomainSize[j] = (ulLODDecFactor[j] < 2)
                             ? ulDomainSize[j]
                             : max<uint64_t>(1,uint64_t(floor(double(ulDomainSize[j]) / double((MathTools::Pow(ulLODDecFactor[j],vLOD[size_t(ulLODGroups[j])]))))));

  return vReducedDomainSize;
}

/**
 * Converts data stored in a file to a bricked LODed format
 * \param pSourceData - pointer to the source data file
 * \param strTempFile - filename of a temp files during the conversion
 * \param combineFunc - the function used to compute the LOD, this is mostly an
 *                      average function
 * \return void
 * \see FlatDataToBrickedLOD
 */
bool
RasterDataBlock::FlatDataToBrickedLOD(
  LargeRAWFile_ptr pSourceData, const string& strTempFile,
  void (*combineFunc)(const vector<uint64_t>& vSource, uint64_t iTarget,
                      const void* pIn, void* pOut),
  void (*maxminFunc)(const void* pIn, size_t iStart, size_t iCount,
                     std::vector<DOUBLEVECTOR4>& fMinMax),
  std::shared_ptr<MaxMinDataBlock> pMaxMinDatBlock,
  AbstrDebugOut* pDebugOut)
{

  // parameter sanity checks
  for (size_t i = 0;i<ulBrickSize.size();i++)  {
    if (ulBrickSize[i] < ulBrickOverlap[i]) {
      if (pDebugOut) pDebugOut->Error(_func_, "Invalid parameters: Bricksze is smaler than brick overlap");
      return false;
    }
  }

  uint64_t uiBytesPerElement = ComputeElementSize()/8;

  if (!m_pTempFile) {
    AllocateTemp(SysTools::AppendFilename(strTempFile,"1"),
                 m_vLODOffsets.empty());
  }

  LargeRAWFile_ptr tempFile;

  // iterate over all LOD-Group Combinations
  vector<vector<uint64_t>> vLODCombis = GenerateCartesianProduct(CountToVectors(ulLODLevelCount));

  vector<uint64_t> vLastReducedDomainSize;
  vLastReducedDomainSize.resize(ulDomainSemantics.size());

  vector<uint64_t> vReducedDomainSize;

  for (size_t i = 0;i<vLODCombis.size();i++) {
    if (pDebugOut) {
      pDebugOut->Message(_func_,
                         "Generating data for lod level %i of %i", int(i+1),
                         int(vLODCombis.size()));
    }

    // compute size of the domain
    vReducedDomainSize = GetLODDomainSize(vLODCombis[i]);

    LargeRAWFile_ptr pBrickSource;
    // in the first iteration 0 (outer else case) do not subsample at all but
    // brick the input data at fullres
    // in the second iteration 1 (inner else case) use the input data as source
    // for the subsampling
    // in all other cases use the previously subsampled data to generate the
    // next subsample level
    if (i > 0) {
      if (i > 1) {
        SubSample(tempFile, tempFile, vLastReducedDomainSize,
                  vReducedDomainSize, combineFunc, pDebugOut, i,
                  vLODCombis.size());
      } else {
        tempFile = LargeRAWFile_ptr(new LargeRAWFile(SysTools::AppendFilename(strTempFile,"2")));
        if (!tempFile->Create(ComputeDataSize())) {
          if (pDebugOut) pDebugOut->Error(_func_, "Unable To create Temp File");
          return false;
        }
        SubSample(pSourceData, tempFile, ulDomainSize, vReducedDomainSize,
                  combineFunc, pDebugOut, i, vLODCombis.size());
      }
      pBrickSource = tempFile;
      vLastReducedDomainSize = vReducedDomainSize;
    } else {
      pBrickSource = pSourceData;
    }

    //Console::printf("\n");
    //uint64_t j = 0;
    //for (uint64_t t = 0;t<vReducedDomainSize[2];t++) {
    //  for (uint64_t y = 0;y<vReducedDomainSize[1];y++) {
    //    for (uint64_t x = 0;x<vReducedDomainSize[0];x++) {
    //      Console::printf("%g ", ((double*)pBrickSource)[j++]);
    //    }
    //    Console::printf("\n");
    //  }
    //  Console::printf("\n");
    //  Console::printf("\n");
    //}
    //Console::printf("\n    Generating bricks:\n");

    // compute brick layout
    vector< vector<uint64_t>> vBricks = ComputeBricks(vReducedDomainSize);
    vector< vector<uint64_t>> vBrickPermutation = GenerateCartesianProduct(vBricks);

    // compute positions of bricks in source data
    vector<uint64_t> vBrickLayout;
    for (size_t j=0;j<vBricks.size();j++) vBrickLayout.push_back(vBricks[j].size());
    vector< vector<uint64_t>> vBrickIndices = GenerateCartesianProduct(CountToVectors(vBrickLayout));

    vector<uint64_t> vPrefixProd;
    vPrefixProd.push_back(1);
    for (size_t j = 1;j<vReducedDomainSize.size();j++) vPrefixProd.push_back(*(vPrefixProd.end()-1) *  vReducedDomainSize[j-1]);

    vector<uint64_t> vBrickOffset;
    vBrickOffset.resize(vBrickPermutation.size());
    vBrickOffset[0] = 0;
    for (size_t j=1;j<vBrickPermutation.size();j++) {
      vBrickOffset[j] = 0;
      for (size_t k=0;k<vBrickIndices[j].size();k++)
        vBrickOffset[j] += vBrickIndices[j][k] * (ulBrickSize[k] - ulBrickOverlap[k]) * vPrefixProd[k] * uiBytesPerElement;
    }

    // ********** fill bricks with data
    unsigned char* pData = new unsigned char[BLOCK_COPY_SIZE];
    for (size_t j=0;j<vBrickPermutation.size();j++) {

       if (pDebugOut) pDebugOut->Message(_func_, "Processing brick %i of %i in lod level %i of %i",int(j+1),int(vBrickPermutation.size()),int(i+1), int(vLODCombis.size()));
      //Console::printf("      Brick %i (",j);
      //for (uint64_t k=0;k<vBrickPermutation[j].size();k++) Console::printf("%i ",vBrickPermutation[j][k]);

      uint64_t iBrickSize = vBrickPermutation[j][0];
      vector<uint64_t> vBrickPrefixProd;
      vBrickPrefixProd.push_back(1);
      for (size_t k=1;k<vBrickPermutation[j].size();k++) {
        iBrickSize *= vBrickPermutation[j][k];
        vBrickPrefixProd.push_back(*(vBrickPrefixProd.end()-1) * vBrickPermutation[j][k-1]);
      }

      uint64_t iTargetOffset = GetLocalDataPointerOffset(i,j)/8;
      uint64_t iSourceOffset = vBrickOffset[j];
      uint64_t iPosTargetArray = 0;

      if (pMaxMinDatBlock) pMaxMinDatBlock->StartNewValue();

      assert(ulElementDimension <= std::numeric_limits<size_t>::max());
      vector<DOUBLEVECTOR4> fMinMax(static_cast<size_t>(ulElementDimension));
      for (uint64_t k=0;k<iBrickSize/vBrickPermutation[j][0];k++) {

        m_pTempFile->SeekPos(iTargetOffset);
        pBrickSource->SeekPos(iSourceOffset);

        uint64_t iDataSize = vBrickPermutation[j][0] * uiBytesPerElement;
        for (uint64_t l = 0;l<iDataSize;l+=BLOCK_COPY_SIZE) {
          uint64_t iCopySize = min(BLOCK_COPY_SIZE, iDataSize-l);

          size_t bytes = pBrickSource->ReadRAW(pData, iCopySize);
          if(bytes != iCopySize) {
            T_ERROR("Error reading data from %s!",
                    pBrickSource->GetFilename().c_str());
            return false;
          }
          m_pTempFile->WriteRAW(pData, iCopySize);

          if (pMaxMinDatBlock) {
            maxminFunc(pData, 0, size_t(iCopySize/uiBytesPerElement), fMinMax);
            pMaxMinDatBlock->MergeData(fMinMax);
          }
        }

        iTargetOffset += vBrickPermutation[j][0] * uiBytesPerElement;

        iPosTargetArray += vBrickPermutation[j][0];
        if (iPosTargetArray % vBrickPrefixProd[1] == 0) iSourceOffset += vReducedDomainSize[0] * uiBytesPerElement;

        for (size_t l = 2;l<vReducedDomainSize.size();l++)
          if (iPosTargetArray % vBrickPrefixProd[l] == 0) {
            iSourceOffset -=  (vBrickPermutation[j][l-1] * vPrefixProd[l-1]) * uiBytesPerElement;
            iSourceOffset +=   vPrefixProd[l] * uiBytesPerElement;
          }
      }


      //Console::printf(")\n");

      //Console::printf("\n");
      //uint64_t xxx = 0;
      //uint64_t yyy = GetLocalDataPointerOffset(i,j)/64;
      //for (uint64_t t = 0;t<vBrickPermutation[j][2];t++) {
      //  for (uint64_t y = 0;y<vBrickPermutation[j][1];y++) {
      //    for (uint64_t x = 0;x<vBrickPermutation[j][0];x++) {
      //      Console::printf("%g ", ((double*)pData+yyy)[xxx++]);
      //    }
      //    Console::printf("\n");
      //  }
      //  Console::printf("\n");
      //}

    }
    delete[] pData;

//    Console::printf("\n");
  }

  if (tempFile != LargeRAWFile_ptr())tempFile->Delete();

  return true;
}

void RasterDataBlock::CleanupTemp() {
  if (m_pTempFile != LargeRAWFile_ptr()) m_pTempFile->Delete();
}

namespace {
  template<typename T>
  void size_vector_for_io(std::vector<T>& v, size_t sz)
  {
    v.resize(sz / sizeof(T));
  }
}

size_t
RasterDataBlock::GetBrickByteSize(const std::vector<uint64_t>& vLOD,
                                  const std::vector<uint64_t>& vBrick) const
{
  vector<uint64_t> vSize = GetBrickSize(vLOD,vBrick);
  uint64_t iSize = ComputeElementSize()/8;
  for (size_t i = 0;i<vSize.size();i++) iSize *= vSize[i];
  return static_cast<size_t>(iSize);
}

LargeRAWFile_ptr
RasterDataBlock::SeekToBrick(const std::vector<uint64_t>& vLOD,
                             const std::vector<uint64_t>& vBrick) const
{
  if (m_pTempFile == LargeRAWFile_ptr() && 
      m_pStreamFile == LargeRAWFile_ptr()) return LargeRAWFile_ptr();
  if (m_vLODOffsets.empty()) { return LargeRAWFile_ptr(); }

  LargeRAWFile_ptr  pStreamFile;
  uint64_t iOffset = GetLocalDataPointerOffset(vLOD, vBrick)/8;

  if (m_pStreamFile) {
    // add global offset
    iOffset += m_iOffset;
    // add size of header
    iOffset += DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize();
    pStreamFile = m_pStreamFile;
  } else {
    pStreamFile = m_pTempFile;
  }
  pStreamFile->SeekPos(iOffset);
  return pStreamFile;
}

bool RasterDataBlock::GetData(uint8_t* vData, size_t bytes,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick) const
{
  LargeRAWFile_ptr pStreamFile = SeekToBrick(vLOD, vBrick);
  if(!pStreamFile) { return false; }

  pStreamFile->ReadRAW(vData, bytes);
  return true;
}

bool RasterDataBlock::ValidLOD(const std::vector<uint64_t>& vLOD) const
{
  const uint64_t lod = Serialize(vLOD, ulLODLevelCount);
  const size_t lod_s = static_cast<size_t>(lod);
  if(lod_s >= m_vBrickSizes.size()) { return false; }

  return true;
}

bool RasterDataBlock::ValidBrickIndex(const std::vector<uint64_t>& vLOD,
                                      const std::vector<uint64_t>& vBrick) const
{
  const uint64_t lod = Serialize(vLOD, ulLODLevelCount);
  const size_t lod_s = static_cast<size_t>(lod);
  if(lod_s >= m_vBrickSizes.size()) { return false; }

  const uint64_t b_idx = Serialize(vBrick, m_vBrickCount[lod_s]);
  const size_t b_idx_s = static_cast<size_t>(b_idx);
  const uint64_t count = std::accumulate(m_vBrickCount[lod_s].begin(),
                                       m_vBrickCount[lod_s].end(),
                                       static_cast<uint64_t>(1),
                                       std::multiplies<uint64_t>());
  if(b_idx_s >= count) { return false; }

  return true;
}

bool RasterDataBlock::GetData(std::vector<uint8_t>& vData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick) const
{
  if(!ValidBrickIndex(vLOD, vBrick)) { return false; }
  size_t iSize = GetBrickByteSize(vLOD, vBrick);
  size_vector_for_io(vData, iSize);
  return GetData(&vData[0], iSize, vLOD, vBrick);
}
bool RasterDataBlock::GetData(std::vector<int8_t>& vData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick) const
{
  if(!ValidBrickIndex(vLOD, vBrick)) { return false; }
  const size_t bytes = GetBrickByteSize(vLOD, vBrick);
  size_vector_for_io(vData, bytes);
  return GetData(reinterpret_cast<uint8_t*>(&vData[0]), bytes, vLOD, vBrick);
}

bool RasterDataBlock::GetData(std::vector<uint16_t>& vData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick) const
{
  if(!ValidBrickIndex(vLOD, vBrick)) { return false; }
  const size_t bytes = GetBrickByteSize(vLOD, vBrick);
  size_vector_for_io(vData, bytes);
  return GetData(reinterpret_cast<uint8_t*>(&vData[0]), bytes, vLOD, vBrick);
}
bool RasterDataBlock::GetData(std::vector<int16_t>& vData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick) const
{
  if(!ValidBrickIndex(vLOD, vBrick)) { return false; }
  const size_t bytes = GetBrickByteSize(vLOD, vBrick);
  size_vector_for_io(vData, bytes);
  return GetData(reinterpret_cast<uint8_t*>(&vData[0]), bytes, vLOD, vBrick);
}

bool RasterDataBlock::GetData(std::vector<uint32_t>& vData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick) const
{
  if(!ValidBrickIndex(vLOD, vBrick)) { return false; }
  const size_t bytes = GetBrickByteSize(vLOD, vBrick);
  size_vector_for_io(vData, bytes);
  return GetData(reinterpret_cast<uint8_t*>(&vData[0]), bytes, vLOD, vBrick);
}
bool RasterDataBlock::GetData(std::vector<int32_t>& vData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick) const
{
  if(!ValidBrickIndex(vLOD, vBrick)) { return false; }
  const size_t bytes = GetBrickByteSize(vLOD, vBrick);
  size_vector_for_io(vData, bytes);
  return GetData(reinterpret_cast<uint8_t*>(&vData[0]), bytes, vLOD, vBrick);
}

bool RasterDataBlock::GetData(std::vector<float>& vData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick) const
{
  if(!ValidBrickIndex(vLOD, vBrick)) { return false; }
  const size_t bytes = GetBrickByteSize(vLOD, vBrick);
  size_vector_for_io(vData, bytes);
  return GetData(reinterpret_cast<uint8_t*>(&vData[0]), bytes, vLOD, vBrick);
}
bool RasterDataBlock::GetData(std::vector<double>& vData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick) const
{
  if(!ValidBrickIndex(vLOD, vBrick)) { return false; }
  const size_t bytes = GetBrickByteSize(vLOD, vBrick);
  size_vector_for_io(vData, bytes);
  return GetData(reinterpret_cast<uint8_t*>(&vData[0]), bytes, vLOD, vBrick);
}

bool RasterDataBlock::Settable() const {
  return m_pStreamFile &&
         m_pStreamFile->IsWritable() &&
         !m_vLODOffsets.empty();
}

bool RasterDataBlock::SetData( int8_t* pData, const vector<uint64_t>& vLOD,
                              const vector<uint64_t>& vBrick) {
  if(!Settable()) { return false; }

  SeekToBrick(vLOD, vBrick);
  uint64_t sz = GetBrickByteSize(vLOD, vBrick);
  return m_pStreamFile->WriteRAW(reinterpret_cast<uint8_t*>(pData), sz) == sz;
}
bool RasterDataBlock::SetData(uint8_t* pData, const vector<uint64_t>& vLOD,
                              const vector<uint64_t>& vBrick) {
  if(!Settable()) { return false; }

  SeekToBrick(vLOD, vBrick);
  uint64_t sz = GetBrickByteSize(vLOD, vBrick);
  return m_pStreamFile->WriteRAW(pData, sz) == sz;;
}

bool RasterDataBlock::SetData(int16_t* pData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick)
{
  if(!Settable()) { return false; }

  SeekToBrick(vLOD, vBrick);
  uint64_t sz = GetBrickByteSize(vLOD, vBrick);
  return m_pStreamFile->WriteRAW(reinterpret_cast<uint8_t*>(pData), sz) == sz;
}
bool RasterDataBlock::SetData(uint16_t* pData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick)
{
  if(!Settable()) { return false; }

  SeekToBrick(vLOD, vBrick);
  uint64_t sz = GetBrickByteSize(vLOD, vBrick);
  return m_pStreamFile->WriteRAW(reinterpret_cast<uint8_t*>(pData), sz) == sz;
}

bool RasterDataBlock::SetData(int32_t* pData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick)
{
  if(!Settable()) { return false; }

  SeekToBrick(vLOD, vBrick);
  uint64_t sz = GetBrickByteSize(vLOD, vBrick);
  return m_pStreamFile->WriteRAW(reinterpret_cast<uint8_t*>(pData), sz) == sz;
}
bool RasterDataBlock::SetData(uint32_t* pData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick)
{
  if(!Settable()) { return false; }

  SeekToBrick(vLOD, vBrick);
  uint64_t sz = GetBrickByteSize(vLOD, vBrick);
  return m_pStreamFile->WriteRAW(reinterpret_cast<uint8_t*>(pData), sz) == sz;
}

bool RasterDataBlock::SetData(float* pData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick)
{
  if(!Settable()) { return false; }

  SeekToBrick(vLOD, vBrick);
  uint64_t sz = GetBrickByteSize(vLOD, vBrick);
  return m_pStreamFile->WriteRAW(reinterpret_cast<uint8_t*>(pData), sz) == sz;
}
bool RasterDataBlock::SetData(double* pData,
                              const std::vector<uint64_t>& vLOD,
                              const std::vector<uint64_t>& vBrick)
{
  if(!Settable()) { return false; }

  SeekToBrick(vLOD, vBrick);
  uint64_t sz = GetBrickByteSize(vLOD, vBrick);
  return m_pStreamFile->WriteRAW(reinterpret_cast<uint8_t*>(pData), sz) == sz;
}

void RasterDataBlock::ResetFile(LargeRAWFile_ptr raw)
{
  CleanupTemp();
  m_pStreamFile = m_pSourceFile = raw;
}

 bool RasterDataBlock::ApplyFunction(const std::vector<uint64_t>& vLOD,
                                     bool (*brickFunc)(void* pData, 
                                                    const UINT64VECTOR3& vBrickSize,
                                                    const UINT64VECTOR3& vBrickOffset,
                                                    void* pUserContext),
                                     void* pUserContext,
                                     uint64_t iOverlap,
                                     AbstrDebugOut* pDebugOut) const {

#ifdef _DEBUG
  for (size_t i = 0;i<ulBrickSize.size();i++) {
    assert(iOverlap <= ulBrickOverlap[i]/2);  // we cannot output more overlap
                                            // than we have stored
  }
#endif

  const vector<uint64_t> vBrickCount = GetBrickCount(vLOD);
  vector<uint64_t> vCoords(vBrickCount.size());

  std::vector<unsigned char> pData;

  // generate 1D offset coords into serialized target data
  vector<uint64_t> vPrefixProd;
  vPrefixProd.push_back(1);
  uint64_t iBrickCount = vBrickCount[0];
  vector<uint64_t> vLODDomSize = GetLODDomainSize(vLOD);
  for (size_t i = 1;i<vLODDomSize.size();i++) {
    vPrefixProd.push_back(*(vPrefixProd.end()-1) * vLODDomSize[i-1]);
    iBrickCount *= vBrickCount[i];
  }
  uint64_t iElementSize = ComputeElementSize();
  uint64_t iBrickCounter = 0;

  TraverseBricksToApplyFunction(iBrickCounter, iBrickCount, vLOD, vBrickCount, vCoords,
                                vBrickCount.size()-1, pData,
                                iElementSize/8, vPrefixProd, pDebugOut, brickFunc,
                                pUserContext, iOverlap );

  return true;
}

bool RasterDataBlock::BrickedLODToFlatData(const vector<uint64_t>& vLOD, const std::string& strTargetFile,
                                           bool bAppend, AbstrDebugOut* pDebugOut) const {



  LargeRAWFile_ptr pTargetFile = LargeRAWFile_ptr(new LargeRAWFile(strTargetFile));

  if (bAppend)
    pTargetFile->Append();
  else
    pTargetFile->Create();


  if (!pTargetFile->IsOpen()) {
    if (pDebugOut) pDebugOut->Error(_func_,"Unable to write to target file %s.", strTargetFile.c_str());
    return false;
  }

  const vector<uint64_t> vBrickCount = GetBrickCount(vLOD);
  vector<uint64_t> vCoords(vBrickCount.size());

  std::vector<unsigned char> pData;

  // generate 1D offset coords into serialized target data
  vector<uint64_t> vPrefixProd;
  vPrefixProd.push_back(1);
  uint64_t iBrickCount = vBrickCount[0];
  vector<uint64_t> vLODDomSize = GetLODDomainSize(vLOD);
  for (size_t i = 1;i<vLODDomSize.size();i++) {
    vPrefixProd.push_back(*(vPrefixProd.end()-1) * vLODDomSize[i-1]);
    iBrickCount *= vBrickCount[i];
  }

  uint64_t iElementSize = ComputeElementSize();
  uint64_t iTargetOffset = pTargetFile->GetCurrentSize();
  uint64_t iBrickCounter = 0;

  TraverseBricksToWriteBrickToFile(iBrickCounter, iBrickCount, vLOD, vBrickCount, vCoords,
                                    vBrickCount.size()-1, iTargetOffset, pData, pTargetFile,
                                    iElementSize/8, vPrefixProd, pDebugOut);

  pTargetFile->Close();
  return true;
}


bool
RasterDataBlock::TraverseBricksToApplyFunction(
  uint64_t& iBrickCounter, uint64_t iBrickCount,
  const std::vector<uint64_t>& vLOD,
  const std::vector<uint64_t>& vBrickCount,
  std::vector<uint64_t> vCoords,
  size_t iCurrentDim,
  std::vector<unsigned char>& vData,
  uint64_t iElementSize,
  const std::vector<uint64_t>& vPrefixProd,
  AbstrDebugOut* pDebugOut,
  bool (*brickFunc)(void* pData, 
    const UINT64VECTOR3& vBrickSize,
    const UINT64VECTOR3& vBrickOffset,
    void* pUserContext),
  void* pUserContext, uint64_t iOverlap
) const {
  if (iCurrentDim>0) {
    for (size_t i = 0;i<vBrickCount[iCurrentDim];i++) {
      vCoords[iCurrentDim] = i;
      if (!TraverseBricksToApplyFunction(iBrickCounter, iBrickCount, vLOD, vBrickCount,
                                         vCoords, iCurrentDim-1,vData ,
                                         iElementSize, vPrefixProd, pDebugOut,
                                         brickFunc, pUserContext, iOverlap)) return false;
    }
  } else {
    for (size_t i = 0;i<vBrickCount[0];i++) {

      vCoords[0] = i;
      const std::vector<uint64_t> vBrickSize = GetBrickSize(vLOD, vCoords);
      std::vector<uint64_t> vEffectiveBrickSize = vBrickSize;

      for (size_t j = 0;j<vEffectiveBrickSize.size();j++) {
        if (vCoords[j] < vBrickCount[j]-1) {
          vEffectiveBrickSize[j] -= (ulBrickOverlap[j]-iOverlap);
        }
      }

      GetData(vData, vLOD, vCoords);
      std::vector<unsigned char> vDataOverlapFixed(vData.size());

      std::vector<uint64_t> vBrickPrefixProduct(vCoords);
      vBrickPrefixProduct[0] = 1;
      for (size_t j = 1;j<vBrickSize.size();j++) {
        vBrickPrefixProduct[j] = vBrickSize[j-1]*vBrickPrefixProduct[j-1];
      }

      uint64_t iSourceOffset = 0, iTargetOffset = 0;
      if (pDebugOut) pDebugOut->Message(_func_, "Extracting volume data\nProcessing brick %i of %i",int(++iBrickCounter),int(iBrickCount));


      WriteBrickToArray(vBrickCount.size()-1, iSourceOffset, iTargetOffset, vBrickSize,
                       vEffectiveBrickSize, vData, vDataOverlapFixed, iElementSize, vPrefixProd,
                       vBrickPrefixProduct);

      std::vector<uint64_t> vAbsCoords(vCoords);
      for (size_t j = 0;j<ulBrickSize.size();j++)
        vAbsCoords[j] *= (ulBrickSize[j]-ulBrickOverlap[j]);

      if (pDebugOut) pDebugOut->Message(_func_, "Processing volume data\nProcessing brick %i of %i",int(iBrickCounter),int(iBrickCount));

      if (!brickFunc(&vDataOverlapFixed[0], UINT64VECTOR3(vEffectiveBrickSize), UINT64VECTOR3(vAbsCoords), pUserContext )) return false;

    }
  }
  return true;
}

bool
RasterDataBlock::TraverseBricksToWriteBrickToFile(
  uint64_t& iBrickCounter, uint64_t iBrickCount,
  const std::vector<uint64_t>& vLOD,
  const std::vector<uint64_t>& vBrickCount,
  std::vector<uint64_t> vCoords,
  size_t iCurrentDim, uint64_t iTargetOffset,
  std::vector<unsigned char>& vData,
  LargeRAWFile_ptr pTargetFile,
  uint64_t iElementSize,
  const std::vector<uint64_t>& vPrefixProd,
  AbstrDebugOut* pDebugOut
) const {
  if (iCurrentDim>0) {
    for (size_t i = 0;i<vBrickCount[iCurrentDim];i++) {
      vCoords[iCurrentDim] = i;
      if (!TraverseBricksToWriteBrickToFile(iBrickCounter, iBrickCount, vLOD, vBrickCount,
                                            vCoords, iCurrentDim-1, iTargetOffset, vData,
                                            pTargetFile, iElementSize, vPrefixProd, 
                                            pDebugOut)) return false;
    }
  } else {

    for (size_t i = 1;i<ulDomainSize.size();i++) {
      iTargetOffset += vPrefixProd[i] * vCoords[i] * (ulBrickSize[i]-ulBrickOverlap[i]);
    }

    for (size_t i = 0;i<vBrickCount[0];i++) {

      vCoords[0] = i;
      const std::vector<uint64_t> vBrickSize = GetBrickSize(vLOD, vCoords);
      std::vector<uint64_t> vEffectiveBrickSize = vBrickSize;

      for (size_t j = 0;j<vEffectiveBrickSize.size();j++) {
        if (vCoords[j] < vBrickCount[j]-1) {
          vEffectiveBrickSize[j] -= ulBrickOverlap[j];
        }
      }

      GetData(vData, vLOD, vCoords);

      std::vector<uint64_t> vBrickPrefixProduct(vCoords);
      vBrickPrefixProduct[0] = 1;
      for (size_t j = 1;j<vBrickSize.size();j++) {
        vBrickPrefixProduct[j] = vBrickSize[j-1]*vBrickPrefixProduct[j-1];
      }

      uint64_t iSourceOffset = 0;
      
      if (pDebugOut) pDebugOut->Message(_func_, "Processing brick %i of %i",int(++iBrickCounter),int(iBrickCount));
      WriteBrickToFile(vBrickCount.size()-1, iSourceOffset, iTargetOffset, vBrickSize,
                       vEffectiveBrickSize, vData, pTargetFile, iElementSize, vPrefixProd,
                       vBrickPrefixProduct, true);
      iTargetOffset += vEffectiveBrickSize[0];
    }
  }
  return true;
}

void
RasterDataBlock::WriteBrickToFile(size_t iCurrentDim,
                                  uint64_t& iSourceOffset, uint64_t& iTargetOffset,
                                  const std::vector<uint64_t>& vBrickSize,
                                  const std::vector<uint64_t>& vEffectiveBrickSize,
                                  std::vector<unsigned char>& vData,
                                  LargeRAWFile_ptr pTargetFile,
                                  uint64_t iElementSize,
                                  const std::vector<uint64_t>& vPrefixProd,
                                  const std::vector<uint64_t>& vBrickPrefixProduct,
                                  bool bDoSeek) const {
  if (iCurrentDim>0) {
    for (size_t i = 0;i<vEffectiveBrickSize[iCurrentDim];i++) {
      uint64_t iBrickTargetOffset = iTargetOffset + vPrefixProd[iCurrentDim] * i;
      WriteBrickToFile(iCurrentDim-1, iSourceOffset, iBrickTargetOffset, vBrickSize,
                       vEffectiveBrickSize, vData, pTargetFile, iElementSize, vPrefixProd,
                       vBrickPrefixProduct, bDoSeek);
    }
    iSourceOffset += (vBrickSize[iCurrentDim]-vEffectiveBrickSize[iCurrentDim])*vBrickPrefixProduct[iCurrentDim];
  } else {
    if (bDoSeek) pTargetFile->SeekPos(iTargetOffset*iElementSize);
    pTargetFile->WriteRAW(&(vData.at(size_t(iSourceOffset*iElementSize))),
                          vEffectiveBrickSize[0]*iElementSize);
    iSourceOffset += vBrickSize[0];
  }
}

void
RasterDataBlock::WriteBrickToArray(size_t iCurrentDim,
                                  uint64_t& iSourceOffset, uint64_t& iTargetOffset,
                                  const std::vector<uint64_t>& vBrickSize,
                                  const std::vector<uint64_t>& vEffectiveBrickSize,
                                  std::vector<unsigned char>& vData,
                                  std::vector<unsigned char>& vTarget,
                                  uint64_t iElementSize,
                                  const std::vector<uint64_t>& vPrefixProd,
                                  const std::vector<uint64_t>& vBrickPrefixProduct) const {
  if (iCurrentDim>0) {
    for (size_t i = 0;i<vEffectiveBrickSize[iCurrentDim];i++) {
      WriteBrickToArray(iCurrentDim-1, iSourceOffset, iTargetOffset, vBrickSize,
                       vEffectiveBrickSize, vData, vTarget, iElementSize, vPrefixProd,
                       vBrickPrefixProduct);
    }
    iSourceOffset += (vBrickSize[iCurrentDim]-vEffectiveBrickSize[iCurrentDim])*vBrickPrefixProduct[iCurrentDim];
  } else {
    memcpy(&(vTarget.at(size_t(iTargetOffset*iElementSize))), 
           &(vData.at(size_t(iSourceOffset*iElementSize))),
           size_t(vEffectiveBrickSize[0]*iElementSize));
    iSourceOffset += vBrickSize[0];
    iTargetOffset += vEffectiveBrickSize[0];
  }
}
