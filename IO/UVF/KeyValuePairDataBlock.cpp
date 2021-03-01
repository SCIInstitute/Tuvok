#include "KeyValuePairDataBlock.h"

using namespace std;
using namespace UVFTables;

KeyValuePairDataBlock::KeyValuePairDataBlock() {
  ulBlockSemantics = BS_KEY_VALUE_PAIRS;
  strBlockID       = "KeyValue Pair Block";
}

KeyValuePairDataBlock::KeyValuePairDataBlock(const KeyValuePairDataBlock &other) : 
  DataBlock(other), m_KeyValuePairs(other.m_KeyValuePairs)
{
}

KeyValuePairDataBlock::KeyValuePairDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian) {
  GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
}

KeyValuePairDataBlock& KeyValuePairDataBlock::operator=(const KeyValuePairDataBlock& other) {
  strBlockID = other.strBlockID;
  ulBlockSemantics = other.ulBlockSemantics;
  ulCompressionScheme = other.ulCompressionScheme;
  ulOffsetToNextDataBlock = other.ulOffsetToNextDataBlock;

  m_KeyValuePairs = other.m_KeyValuePairs;

  return *this;
}

uint64_t KeyValuePairDataBlock::GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian) {
  uint64_t iStart = iOffset + DataBlock::GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
  pStreamFile->SeekPos(iStart);

  uint64_t ulElementCount;
  pStreamFile->ReadData(ulElementCount, bIsBigEndian);

  for (uint64_t i = 0;i<ulElementCount;i++) {
    string key = "", value = "";
    uint64_t iStrLength;

    pStreamFile->ReadData(iStrLength, bIsBigEndian);
    // Use a damn RasterDataBlock if it doesn't, weirdo.  This isn't meant for
    // storing gigabytes of data.
    assert(iStrLength <= 4294967296ULL &&
           "value must fit in 32bit address space.");
    pStreamFile->ReadData(key, iStrLength);

    pStreamFile->ReadData(iStrLength, bIsBigEndian);
    assert(iStrLength <= 4294967296ULL &&
           "value must fit in 32bit address space.");
    pStreamFile->ReadData(value, iStrLength);

    KeyValuePair p(key, value);

    m_KeyValuePairs.push_back(p);
  }

  return pStreamFile->GetPos() - iOffset;
}

void KeyValuePairDataBlock::CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  DataBlock::CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);
  pStreamFile->WriteData(uint64_t(m_KeyValuePairs.size()), bIsBigEndian);
}

uint64_t KeyValuePairDataBlock::CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);

  for (size_t i = 0;i<m_KeyValuePairs.size();i++) {
    string key(m_KeyValuePairs[i].strKey), value(m_KeyValuePairs[i].strValue);

    pStreamFile->WriteData(uint64_t(key.length()), bIsBigEndian);
    pStreamFile->WriteData(key);

    pStreamFile->WriteData(uint64_t(value.length()), bIsBigEndian);
    pStreamFile->WriteData(value);
  }

  return pStreamFile->GetPos() - iOffset;
}

DataBlock* KeyValuePairDataBlock::Clone() const {
  return new KeyValuePairDataBlock(*this);
}

uint64_t KeyValuePairDataBlock::GetOffsetToNextBlock() const {
  return DataBlock::GetOffsetToNextBlock() + ComputeDataSize();
}


uint64_t KeyValuePairDataBlock::ComputeDataSize() const {
  uint64_t iCharCount = 0;

  for (size_t i = 0;i<m_KeyValuePairs.size();i++) {
    iCharCount += m_KeyValuePairs[i].strKey.length() + m_KeyValuePairs[i].strValue.length();
  }

  return sizeof(uint64_t) +                 // size of the vector
       iCharCount +                   // the strings themselves
       m_KeyValuePairs.size() * 2 * sizeof(uint64_t);  // UINT64s indicating the stringlength
}

bool KeyValuePairDataBlock::AddPair(std::wstring key, std::wstring value) {
  for (size_t i = 0;i<m_KeyValuePairs.size();i++) {
    if (key == m_KeyValuePairs[i].wstrKey()) return false;
  }

  KeyValuePair p(key,value);
  m_KeyValuePairs.push_back(p);

  return true;
}
