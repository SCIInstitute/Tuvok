#pragma once

#ifndef UVF_KEYVALUEPAIRDATABLOCK_H
#define UVF_KEYVALUEPAIRDATABLOCK_H

#include "DataBlock.h"
#include "Basics/SysTools.h"

class KeyValuePair {
public:
  KeyValuePair() : strKey(""), strValue("")  {}

  KeyValuePair(std::wstring _strKey, std::wstring _strValue) {
      strKey = SysTools::toNarrow(_strKey);
      strValue = SysTools::toNarrow(_strValue);
  }

  KeyValuePair(std::string _strKey, std::string _strValue) :
    strKey(_strKey), strValue(_strValue)  {}

  std::wstring wstrKey() const {
      return SysTools::toWide(strKey);
  }
  std::wstring wstrValue() const {
      return SysTools::toWide(strValue);
  }

  std::string strKey;
  std::string strValue;

};

#ifdef max
  #undef max
#endif

class KeyValuePairDataBlock : public DataBlock
{
public:
  KeyValuePairDataBlock();
  KeyValuePairDataBlock(const KeyValuePairDataBlock &other);
  KeyValuePairDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                        bool bIsBigEndian);
  virtual KeyValuePairDataBlock& operator=(const KeyValuePairDataBlock& other);

  size_t GetKeyCount() const {return m_KeyValuePairs.size();}
  std::wstring GetKeyByIndex(size_t iIndex) const {
    return m_KeyValuePairs[size_t(iIndex)].wstrKey();
  }
  std::wstring GetValueByIndex(size_t iIndex) const {
    return m_KeyValuePairs[size_t(iIndex)].wstrValue();
  }
  uint64_t GetIndexByKey(std::wstring strKey) const {
    for (size_t i = 0;i<GetKeyCount();i++) {
      if (GetKeyByIndex(i) == strKey) {
        return uint64_t(i);
      }
    }
    return UVF_INVALID;
  }

  bool AddPair(std::wstring key, std::wstring value);
  virtual uint64_t ComputeDataSize() const;

protected:
  std::vector<KeyValuePair> m_KeyValuePairs;

  virtual void CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                                bool bIsBigEndian, bool bIsLastBlock);

  virtual uint64_t GetHeaderFromFile(LargeRAWFile_ptr pStreamFile,
                                     uint64_t iOffset, bool bIsBigEndian);
  virtual uint64_t CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                              bool bIsBigEndian, bool bIsLastBlock);
  virtual uint64_t GetOffsetToNextBlock() const;

  virtual DataBlock* Clone() const;

  friend class UVF;
};
#endif // UVF_KEYVALUEPAIRDATABLOCK_H
