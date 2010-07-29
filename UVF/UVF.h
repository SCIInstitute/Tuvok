#pragma once
#ifndef UVF_H
#define UVF_H

#include "UVFBasic.h"

#include "UVFTables.h"
#include "GlobalHeader.h"
class DataBlock;

class DataBlockListElem {
  public:
    DataBlockListElem() :
      m_block(NULL),
      m_bSelfCreatedPointer(false),
      m_bIsDirty(false),
      m_bHeaderIsDirty(false),
      m_iOffsetInFile(0),
      m_iLastSize(0)
    {}
    
    DataBlockListElem(DataBlock* block, bool bSelfCreatedPointer, bool bIsDirty, UINT64 iOffsetInFile, UINT64 iLastSize) :
      m_block(block),
      m_bSelfCreatedPointer(bSelfCreatedPointer),
      m_bIsDirty(bIsDirty),
      m_bHeaderIsDirty(bIsDirty),
      m_iOffsetInFile(iOffsetInFile),
      m_iLastSize(iLastSize)
    {}

  DataBlock* m_block;
  bool m_bSelfCreatedPointer;
  bool m_bIsDirty;
  bool m_bHeaderIsDirty;
  UINT64 m_iOffsetInFile;
  UINT64 m_iLastSize;
};

class UVF
{
public:
  static UINT64 ms_ulReaderVersion;

  UVF(std::wstring wstrFilename);
  virtual ~UVF(void);

  bool Open(bool bMustBeSameVersion=true, bool bVerify=true, bool bReadWrite=false, std::string* pstrProblem = NULL);
  void Close();

  const GlobalHeader& GetGlobalHeader() const {return m_GlobalHeader;}
  UINT64 GetDataBlockCount() const {return UINT64(m_DataBlocks.size());}
  const DataBlock* GetDataBlock(UINT64 index) const {return m_DataBlocks[size_t(index)]->m_block;}
  DataBlock* GetDataBlockRW(UINT64 index, bool bOnlyChangeHeader);

  // file creation routines
  bool SetGlobalHeader(const GlobalHeader& GlobalHeader);
  bool AddConstDataBlock(const DataBlock* dataBlock, UINT64 iSizeofData);
  bool AddDataBlock(DataBlock* dataBlock, UINT64 iSizeofData, bool bUseSourcePointer=false);
  bool Create();
  bool AppendBlockToFile(DataBlock* dataBlock);

  static bool IsUVFFile(const std::wstring& wstrFilename);
  static bool IsUVFFile(const std::wstring& wstrFilename, bool& bChecksumFail);

protected:
  bool            m_bFileIsLoaded;
  bool            m_bFileIsReadWrite;
  LargeRAWFile    m_streamFile;
  UINT64          m_iAccumOffsets;

  GlobalHeader m_GlobalHeader;
  std::vector< DataBlockListElem* > m_DataBlocks;

  bool ParseGlobalHeader(bool bVerify, std::string* pstrProblem = NULL);
  void ParseDataBlocks();
  static bool VerifyChecksum(LargeRAWFile& streamFile, GlobalHeader& globalHeader, std::string* pstrProblem = NULL);
  static std::vector<unsigned char> ComputeChecksum(LargeRAWFile& streamFile, UVFTables::ChecksumSemanticTable eChecksumSemanticsEntry);

  static bool CheckMagic(LargeRAWFile& streamFile);

  // file creation routines
  UINT64 ComputeNewFileSize();
  void UpdateChecksum();
};
#endif // UVF_H
