#pragma once
#ifndef UVF_H
#define UVF_H

#include <memory>
#include "UVFBasic.h"

#include "UVFTables.h"
#include "GlobalHeader.h"
class DataBlock;

class DataBlockListElem {
  public:
    DataBlockListElem() :
      m_bIsDirty(false),
      m_bHeaderIsDirty(false),
      m_iOffsetInFile(0),
      m_iBlockSize(0)
    {}
    
    DataBlockListElem(std::shared_ptr<DataBlock> block,
                      bool bIsDirty, uint64_t iOffsetInFile,
                      uint64_t iBlockSize) :
      m_block(block),
      m_bIsDirty(bIsDirty),
      m_bHeaderIsDirty(bIsDirty),
      m_iOffsetInFile(iOffsetInFile),
      m_iBlockSize(iBlockSize)
    {}

  std::shared_ptr<DataBlock> m_block;
  bool m_bIsDirty;
  bool m_bHeaderIsDirty;
  uint64_t m_iOffsetInFile;

  uint64_t GetBlockSize() {return m_iBlockSize;}

  protected:
    uint64_t m_iBlockSize;

    friend class UVF;
};

class UVF
{
public:
  static uint64_t ms_ulReaderVersion;

  UVF(std::wstring wstrFilename);
  virtual ~UVF(void);

  bool Open(bool bMustBeSameVersion=true, bool bVerify=true,
            bool bReadWrite=false, std::string* pstrProblem = NULL);
  void Close();

  const GlobalHeader& GetGlobalHeader() const {return m_GlobalHeader;}
  uint64_t GetDataBlockCount() const {return uint64_t(m_DataBlocks.size());}
  const std::shared_ptr<DataBlock> GetDataBlock(uint64_t index) const;
  DataBlock* GetDataBlockRW(uint64_t index, bool bOnlyChangeHeader);

  // file creation routines
  bool SetGlobalHeader(const GlobalHeader& GlobalHeader);
  bool AddConstDataBlock(std::shared_ptr<const DataBlock> dataBlock);
  bool AddDataBlock(std::shared_ptr<DataBlock> dataBlock);
  bool Create();

  // RW access routines
  bool AppendBlockToFile(std::shared_ptr<DataBlock> dataBlock);
  bool DropBlockFromFile(size_t iBlockIndex);

  static bool IsUVFFile(const std::wstring& wstrFilename);
  static bool IsUVFFile(const std::wstring& wstrFilename, bool& bChecksumFail);

protected:
  bool              m_bFileIsLoaded;
  bool              m_bFileIsReadWrite;
  LargeRAWFile_ptr  m_streamFile;
  uint64_t          m_iAccumOffsets;

  GlobalHeader m_GlobalHeader;
  std::vector<std::shared_ptr<DataBlockListElem>> m_DataBlocks;

  bool ParseGlobalHeader(bool bVerify, std::string* pstrProblem = NULL);
  void ParseDataBlocks();
  static bool VerifyChecksum(LargeRAWFile_ptr streamFile,
                             GlobalHeader& globalHeader,
                             std::string* pstrProblem = NULL);
  static std::vector<unsigned char> ComputeChecksum(
    LargeRAWFile_ptr streamFile,
    UVFTables::ChecksumSemanticTable eChecksumSemanticsEntry
  );

  static bool CheckMagic(LargeRAWFile_ptr streamFile);

  // file creation routines
  uint64_t ComputeNewFileSize();
  void UpdateChecksum();
};
#endif // UVF_H
