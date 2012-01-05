#include "DataBlock.h"

#include "UVF.h"
#include <sstream>

using namespace std;
using namespace UVFTables;

DataBlock::DataBlock() : 
  strBlockID(""),
  ulCompressionScheme(COS_NONE),
  m_pStreamFile(),
  m_iOffset(0),
  ulBlockSemantics(BS_EMPTY),
  ulOffsetToNextDataBlock(0)
{}

DataBlock::DataBlock(const DataBlock &other) : 
  strBlockID(other.strBlockID),
  ulCompressionScheme(other.ulCompressionScheme),
  m_pStreamFile(other.m_pStreamFile),
  m_iOffset(other.m_iOffset),
  ulBlockSemantics(other.ulBlockSemantics),
  ulOffsetToNextDataBlock(other.ulOffsetToNextDataBlock)
{}

DataBlock::DataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian)  {
  GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
}

DataBlock::~DataBlock() {
  // nothing to do here yet
}


DataBlock& DataBlock::operator=(const DataBlock& other)  { 
  strBlockID = other.strBlockID;
  ulCompressionScheme = other.ulCompressionScheme;
  m_pStreamFile = other.m_pStreamFile;
  m_iOffset = other.m_iOffset;
  ulBlockSemantics = other.ulBlockSemantics;
  ulOffsetToNextDataBlock = other.ulOffsetToNextDataBlock;

  return *this;
}

DataBlock* DataBlock::Clone() const {
  return new DataBlock(*this);
}

uint64_t DataBlock::GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian) {
  m_pStreamFile = pStreamFile;
  m_iOffset = iOffset;

  assert(pStreamFile->IsOpen());

  m_pStreamFile->SeekPos(iOffset);

  uint64_t ulStringLengthBlockID;
  m_pStreamFile->ReadData(ulStringLengthBlockID, bIsBigEndian);
  m_pStreamFile->ReadData(strBlockID, ulStringLengthBlockID);

  uint64_t uintSem;
  m_pStreamFile->ReadData(uintSem, bIsBigEndian);
  ulBlockSemantics = (BlockSemanticTable)uintSem;
  m_pStreamFile->ReadData(uintSem, bIsBigEndian);
  ulCompressionScheme = (CompressionSemanticTable)uintSem;

  m_pStreamFile->ReadData(ulOffsetToNextDataBlock, bIsBigEndian);

  return m_pStreamFile->GetPos() - iOffset;
}

void DataBlock::CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  assert(pStreamFile->IsOpen());
  pStreamFile->SeekPos(iOffset);

  pStreamFile->WriteData(uint64_t(strBlockID.size()), bIsBigEndian);
  pStreamFile->WriteData(strBlockID);
  pStreamFile->WriteData(uint64_t(ulBlockSemantics), bIsBigEndian);
  pStreamFile->WriteData(uint64_t(ulCompressionScheme), bIsBigEndian);

  if (bIsLastBlock)
    pStreamFile->WriteData(uint64_t(0), bIsBigEndian);
  else
    pStreamFile->WriteData(GetOffsetToNextBlock(), bIsBigEndian);
}

uint64_t DataBlock::CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);
  return pStreamFile->GetPos() - iOffset;
}

uint64_t DataBlock::GetOffsetToNextBlock() const {
  return strBlockID.size() + 4 * sizeof(uint64_t);
}

bool DataBlock::Verify(uint64_t iSizeofData, std::string* pstrProblem) const {
  uint64_t iCorrectSize = ComputeDataSize();
  bool bResult = iCorrectSize == iSizeofData;

  if (pstrProblem != NULL)  {
    stringstream s;
    s << "DataBlock::Verify: size mismatch. Should be " << iCorrectSize << " but parameter was " << iSizeofData << ".";
    *pstrProblem = s.str();
  }

  return bResult;
}
