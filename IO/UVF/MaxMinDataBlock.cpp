#include "MaxMinDataBlock.h"

#include <memory.h>
using namespace std;
using namespace UVFTables;

#ifdef min
  #undef min
#endif

#ifdef max
  #undef max
#endif


MaxMinDataBlock::MaxMinDataBlock(UINT64 iComponentCount) : 
  DataBlock()
{
  ulBlockSemantics = BS_MAXMIN_VALUES;
  strBlockID       = "Brick Max/Min Values";
  m_vfMaxMinData.clear();
  m_iComponentCount = iComponentCount;
}

MaxMinDataBlock::MaxMinDataBlock(const MaxMinDataBlock &other) :
  DataBlock(other),
  m_vfMaxMinData(other.m_vfMaxMinData),
  m_iComponentCount(other.m_iComponentCount)
{
}

MaxMinDataBlock& MaxMinDataBlock::operator=(const MaxMinDataBlock& other) {
  strBlockID = other.strBlockID;
  ulBlockSemantics = other.ulBlockSemantics;
  ulCompressionScheme = other.ulCompressionScheme;
  ulOffsetToNextDataBlock = other.ulOffsetToNextDataBlock;

  m_iComponentCount = other.m_iComponentCount;
  m_vfMaxMinData = other.m_vfMaxMinData;

  return *this;
}


MaxMinDataBlock::MaxMinDataBlock(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian) {
  GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
}

MaxMinDataBlock::~MaxMinDataBlock() 
{
}

DataBlock* MaxMinDataBlock::Clone() {
  return new MaxMinDataBlock(*this);
}

UINT64 MaxMinDataBlock::GetHeaderFromFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian) {
  UINT64 iStart = iOffset + DataBlock::GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
  pStreamFile->SeekPos(iStart);

  UINT64 ulBrickCount;
  pStreamFile->ReadData(ulBrickCount, bIsBigEndian);
  pStreamFile->ReadData(m_iComponentCount, bIsBigEndian);

  m_vfMaxMinData.resize(size_t(ulBrickCount));

  for (UINT64 i = 0;i<ulBrickCount;i++) {
    m_vfMaxMinData[i].resize(m_iComponentCount);
    for (UINT64 j = 0;j<m_iComponentCount;j++) {
      pStreamFile->ReadData(m_vfMaxMinData[i][j].minScalar, bIsBigEndian);
      pStreamFile->ReadData(m_vfMaxMinData[i][j].maxScalar, bIsBigEndian);
      pStreamFile->ReadData(m_vfMaxMinData[i][j].minGradient, bIsBigEndian);
      pStreamFile->ReadData(m_vfMaxMinData[i][j].maxGradient, bIsBigEndian);
    }
  }

  return pStreamFile->GetPos() - iOffset;
}

UINT64 MaxMinDataBlock::CopyToFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  UINT64 iStart = iOffset + DataBlock::CopyToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);
  pStreamFile->SeekPos(iStart);
  
  // for some strange reason throwing in the raw expression into WriteData causes random values to written into the file on windows
  UINT64 ulBrickCount = UINT64(m_vfMaxMinData.size());
  pStreamFile->WriteData(ulBrickCount, bIsBigEndian);
  pStreamFile->WriteData(m_iComponentCount, bIsBigEndian);
  
  for (UINT64 i = 0;i<ulBrickCount;i++) {
    for (UINT64 j = 0;j<m_iComponentCount;j++) {
      pStreamFile->WriteData(m_vfMaxMinData[i][j].minScalar, bIsBigEndian);
      pStreamFile->WriteData(m_vfMaxMinData[i][j].maxScalar, bIsBigEndian);
      pStreamFile->WriteData(m_vfMaxMinData[i][j].minGradient, bIsBigEndian);
      pStreamFile->WriteData(m_vfMaxMinData[i][j].maxGradient, bIsBigEndian);
    }
  }

  return pStreamFile->GetPos() - iOffset;
}


UINT64 MaxMinDataBlock::GetOffsetToNextBlock() const {
  return DataBlock::GetOffsetToNextBlock() + ComputeDataSize();
}

UINT64 MaxMinDataBlock::ComputeDataSize() const {
  return sizeof(UINT64) +                                          // length of the vector
         sizeof(InternalMaxMinElement) * m_vfMaxMinData.size();    // the vector itself
}

const InternalMaxMinElement& MaxMinDataBlock::GetValue(size_t iIndex, UINT64 iComponent) {
  return m_vfMaxMinData[iIndex][iComponent];
}

void MaxMinDataBlock::StartNewValue() {
  vector<InternalMaxMinElement> elems;
  InternalMaxMinElement elem(std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), -std::numeric_limits<double>::max());  
  for (UINT64 i = 0;i<m_iComponentCount;i++) elems.push_back(elem);  
  m_vfMaxMinData.push_back(elems);
}

void MaxMinDataBlock::MergeData(const std::vector<DOUBLEVECTOR4>& fMaxMinData) {
  for (UINT64 i = 0;i<m_iComponentCount;i++) 
    MergeData(InternalMaxMinElement(fMaxMinData[i].x, fMaxMinData[i].y, fMaxMinData[i].z, fMaxMinData[i].w), i);
}

void MaxMinDataBlock::MergeData(const InternalMaxMinElement& data, const UINT64 iComponent) {
  m_vfMaxMinData[m_vfMaxMinData.size()-1][iComponent].Merge(data);
}
