#include <sstream>
#include "UVF.h"
#include "Basics/Checksums/crc32.h"
#include "Basics/Checksums/MD5.h"
#include "Basics/nonstd.h"
#include "DataBlock.h"

using namespace std;
using namespace UVFTables;

uint64_t UVF::ms_ulReaderVersion = UVFVERSION;

UVF::UVF(std::wstring wstrFilename) : 
  m_bFileIsLoaded(false),
  m_bFileIsReadWrite(false),
  m_streamFile(new LargeRAWFile(wstrFilename)),
  m_iAccumOffsets(0)
{

}

UVF::~UVF(void)
{
  Close();
}

bool UVF::CheckMagic(LargeRAWFile_ptr streamFile) {
  if (!streamFile->Open(false)) {
    return false;
  }

  if (streamFile->GetCurrentSize() < GlobalHeader::GetMinSize() + 8) {
    return false;
  }

  unsigned char pData[8];
  streamFile->ReadRAW(pData, 8);

  if (pData[0] != 'U' || pData[1] != 'V' || pData[2] != 'F' || pData[3] != '-' || pData[4] != 'D' || pData[5] != 'A' || pData[6] != 'T' || pData[7] != 'A') {
    return false;
  }

  return true;
}

bool UVF::IsUVFFile(const std::wstring& wstrFilename) {
  LargeRAWFile_ptr streamFile(new LargeRAWFile (wstrFilename));
  bool bResult = CheckMagic(streamFile);
  streamFile->Close();
  return bResult;
}

bool UVF::IsUVFFile(const std::wstring& wstrFilename, bool& bChecksumFail) {

  LargeRAWFile_ptr streamFile( new LargeRAWFile(wstrFilename));

  if (!CheckMagic(streamFile)) {
    bChecksumFail = false;
    streamFile->Close();
    return false;
  }


  GlobalHeader g;
  g.GetHeaderFromFile(streamFile);
  bChecksumFail = !VerifyChecksum(streamFile, g);
  streamFile->Close();
  return true;
}

bool UVF::Open(bool bMustBeSameVersion, bool bVerify, bool bReadWrite, std::string* pstrProblem) {
  if (m_bFileIsLoaded) return true;

  m_bFileIsLoaded = m_streamFile->Open(bReadWrite);

  if (!m_bFileIsLoaded) {
    if (pstrProblem) (*pstrProblem) = "file not found or access denied";
    return false;
  }
  m_bFileIsReadWrite = bReadWrite;

  if (ParseGlobalHeader(bVerify,pstrProblem)) {
    if (bMustBeSameVersion && ms_ulReaderVersion != m_GlobalHeader.ulFileVersion) {
      if (pstrProblem) (*pstrProblem) = "wrong UVF file version";
      return false;
    }
    ParseDataBlocks();    
    return true;
  } else {
    Close(); // file is not a UVF file or checksum is invalid
    return false;
  }
}

void UVF::Close() {
  if (m_bFileIsLoaded) {
    if (m_bFileIsReadWrite) {
      bool dirty = false;
      for (size_t i = 0;i<m_DataBlocks.size();i++) {
        if (m_DataBlocks[i]->m_bHeaderIsDirty) {
          m_DataBlocks[i]->m_block->CopyHeaderToFile(
            m_streamFile,
            m_DataBlocks[i]->m_iOffsetInFile+m_GlobalHeader.GetDataPos(),
            m_GlobalHeader.bIsBigEndian,
            i == m_DataBlocks.size()-1
          );
          dirty = true;
        } 
        if (m_DataBlocks[i]->m_bIsDirty) {
          // for now we only support changes in the datablock that do
          // not influence its size TODO: will need to extend this to
          // arbitrary changes once we add more features
          assert(m_DataBlocks[i]->m_block->GetOffsetToNextBlock() ==
                 m_DataBlocks[i]->GetBlockSize());

          m_DataBlocks[i]->m_block->CopyToFile(
            m_streamFile,
            m_DataBlocks[i]->m_iOffsetInFile+m_GlobalHeader.GetDataPos(),
            m_GlobalHeader.bIsBigEndian,
            i == m_DataBlocks.size()-1
          );
          dirty = true;
        }
      }
      if(dirty) {
        UpdateChecksum();
      }
    }
    m_streamFile->Close();
    m_bFileIsLoaded = false;
  }

  m_DataBlocks.clear();
}

bool UVF::ParseGlobalHeader(bool bVerify, std::string* pstrProblem) {
  if (m_streamFile->GetCurrentSize() < GlobalHeader::GetMinSize() + 8) {
    if (pstrProblem!=NULL) (*pstrProblem) = "file to small to be a UVF file";
    return false;
  }

  unsigned char pData[8];
  m_streamFile->ReadRAW(pData, 8);

  if (pData[0] != 'U' || pData[1] != 'V' || pData[2] != 'F' || pData[3] != '-' || pData[4] != 'D' || pData[5] != 'A' || pData[6] != 'T' || pData[7] != 'A') {
    if (pstrProblem!=NULL) (*pstrProblem) = "file magic not found";
    return false;
  }
  
  m_GlobalHeader.GetHeaderFromFile(m_streamFile);
  
  return !bVerify || VerifyChecksum(m_streamFile, m_GlobalHeader, pstrProblem);
}


vector<unsigned char> UVF::ComputeChecksum(LargeRAWFile_ptr streamFile, ChecksumSemanticTable eChecksumSemanticsEntry) {
  vector<unsigned char> checkSum;

  uint64_t iOffset    = 33+UVFTables::ChecksumElemLength(eChecksumSemanticsEntry);
  uint64_t iFileSize  = streamFile->GetCurrentSize();
  uint64_t iSize      = iFileSize-iOffset;

  streamFile->SeekPos(iOffset);

  unsigned char *ucBlock=new unsigned char[1<<20];
  switch (eChecksumSemanticsEntry) {
    case CS_CRC32 : {
              CRC32     crc;
              uint64_t iBlocks=iFileSize>>20;
              unsigned long dwCRC32=0xFFFFFFFF;
              for (uint64_t i=0; i<iBlocks; i++) {
                streamFile->ReadRAW(ucBlock,1<<20);
                crc.chunk(ucBlock,1<<20,dwCRC32);
              }

              size_t iLengthLastChunk=size_t(iFileSize-(iBlocks<<20));
              streamFile->ReadRAW(ucBlock,iLengthLastChunk);
              crc.chunk(ucBlock,iLengthLastChunk,dwCRC32);
              dwCRC32^=0xFFFFFFFF;

              for (uint64_t i = 0;i<4;i++) {
                unsigned char c = dwCRC32 & 255;
                checkSum.push_back(c);
                dwCRC32 = dwCRC32>>8;
              }
            } break;
    case CS_MD5 : {
              MD5    md5;
              int    iError=0;
              uint32_t iBlockSize;

              while (iSize > 0)
              {
                iBlockSize = uint32_t(min(iSize,uint64_t(1<<20)));

                streamFile->ReadRAW(ucBlock,iBlockSize);
                md5.Update(ucBlock, iBlockSize, iError);
                iSize   -= iBlockSize;
              }

              checkSum = md5.Final(iError);

            } break;
    case CS_NONE : 
    default     : break;
  }
  delete [] ucBlock;

  streamFile->SeekStart();
  return checkSum;
}


bool UVF::VerifyChecksum(LargeRAWFile_ptr streamFile, GlobalHeader& globalHeader, std::string* pstrProblem) {
  vector<unsigned char> vecActualCheckSum = ComputeChecksum(streamFile, globalHeader.ulChecksumSemanticsEntry);

  if (vecActualCheckSum.size() != globalHeader.vcChecksum.size()) {
    if (pstrProblem != NULL)  {
      stringstream s;
      string strActual = "", strFile = "";
      for (size_t i = 0;i<vecActualCheckSum.size();i++) strActual += vecActualCheckSum[i];
      for (size_t i = 0;i<globalHeader.vcChecksum.size();i++) strFile += globalHeader.vcChecksum[i];

      s << "UVF::VerifyChecksum: checksum mismatch (stage 1). Should be " << strActual << " but is " << strFile << ".";
      *pstrProblem = s.str();
    }
    return false;
  }

  for (size_t i = 0;i<vecActualCheckSum.size();i++) 
    if (vecActualCheckSum[i] != globalHeader.vcChecksum[i]) {      
      if (pstrProblem != NULL)  {
        stringstream s;
        string strActual = "", strFile = "";
        for (size_t j = 0;j<vecActualCheckSum.size();j++) strActual += vecActualCheckSum[j];
        for (size_t j = 0;j<globalHeader.vcChecksum.size();j++) strFile += globalHeader.vcChecksum[j];

        s << "UVF::VerifyChecksum: checksum mismatch (stage 2). Should be " << strActual << " but is " << strFile << ".";
        *pstrProblem = s.str();
      }
      return false;
    }

  return true;
}


void UVF::ParseDataBlocks() {
  uint64_t iOffset = m_GlobalHeader.GetDataPos();
  do  {
    std::tr1::shared_ptr<DataBlock> d(
      new DataBlock(m_streamFile, iOffset, m_GlobalHeader.bIsBigEndian)
    );

    // if we recognize the block -> read it completely
    if (d->ulBlockSemantics > BS_EMPTY && d->ulBlockSemantics < BS_UNKNOWN) {
      BlockSemanticTable eTableID = d->ulBlockSemantics;
      d = CreateBlockFromSemanticEntry(eTableID, m_streamFile, iOffset,
                                       m_GlobalHeader.bIsBigEndian);
    }

    m_DataBlocks.push_back(std::tr1::shared_ptr<DataBlockListElem>(
      new DataBlockListElem(d, false, iOffset-m_GlobalHeader.GetDataPos(),
                            d->ulOffsetToNextDataBlock)
    ));
    iOffset += d->ulOffsetToNextDataBlock;

  } while (m_DataBlocks[m_DataBlocks.size()-1]->m_block->ulOffsetToNextDataBlock != 0);
}

// ********************** file creation routines

void UVF::UpdateChecksum() {
  if (m_GlobalHeader.ulChecksumSemanticsEntry == CS_NONE) return;
  m_GlobalHeader.UpdateChecksum(ComputeChecksum(m_streamFile, m_GlobalHeader.ulChecksumSemanticsEntry), m_streamFile);
}

bool UVF::SetGlobalHeader(const GlobalHeader& globalHeader) {
  if (m_bFileIsLoaded) return false;

  m_GlobalHeader = globalHeader;
  
  // set the canonical data
  m_GlobalHeader.ulAdditionalHeaderSize = 0;
  m_GlobalHeader.ulOffsetToFirstDataBlock = 0;
  m_GlobalHeader.ulFileVersion = ms_ulReaderVersion;

  if (m_GlobalHeader.ulChecksumSemanticsEntry >= CS_UNKNOWN) m_GlobalHeader.ulChecksumSemanticsEntry = CS_NONE;

  if (m_GlobalHeader.ulChecksumSemanticsEntry > CS_NONE) {
    m_GlobalHeader.vcChecksum.resize(size_t(ChecksumElemLength(m_GlobalHeader.ulChecksumSemanticsEntry)));
  } else m_GlobalHeader.vcChecksum.resize(0);
  return true;
}

bool UVF::AddConstDataBlock(const DataBlock* dataBlock) {
  const uint64_t iSizeofData = dataBlock->ComputeDataSize();
  if (!dataBlock->Verify(iSizeofData)) return false;

  std::tr1::shared_ptr<DataBlock> d(dataBlock->Clone());
  
  m_DataBlocks.push_back(std::tr1::shared_ptr<DataBlockListElem>(
    new DataBlockListElem(d,true,m_iAccumOffsets,
                          d->GetOffsetToNextBlock())
  ));
  d->ulOffsetToNextDataBlock = d->GetOffsetToNextBlock();
  m_iAccumOffsets += d->ulOffsetToNextDataBlock;

  return true;
}

bool UVF::AddDataBlock(std::tr1::shared_ptr<DataBlock> dataBlock) {
  const uint64_t iSizeofData = dataBlock->ComputeDataSize();

  if (!dataBlock->Verify(iSizeofData)) return false;

  m_DataBlocks.push_back(std::tr1::shared_ptr<DataBlockListElem>(
    new DataBlockListElem(dataBlock, true, m_iAccumOffsets,
                          dataBlock->GetOffsetToNextBlock())
  ));
  dataBlock->ulOffsetToNextDataBlock = dataBlock->GetOffsetToNextBlock();
  m_iAccumOffsets += dataBlock->ulOffsetToNextDataBlock;

  return true;
}

uint64_t UVF::ComputeNewFileSize() {
  uint64_t iFileSize = m_GlobalHeader.GetDataPos();
  for (size_t i = 0;i<m_DataBlocks.size();i++) 
    iFileSize += m_DataBlocks[i]->m_block->GetOffsetToNextBlock();
  return iFileSize;
}


bool UVF::Create() {
  if (m_bFileIsLoaded) return false;

  m_bFileIsLoaded = m_streamFile->Create();

  if (m_bFileIsLoaded) {

    m_bFileIsReadWrite = true;

    unsigned char pData[8];
    pData[0] = 'U';
    pData[1] = 'V';
    pData[2] = 'F';
    pData[3] = '-';
    pData[4] = 'D';
    pData[5] = 'A';
    pData[6] = 'T';
    pData[7] = 'A';
    m_streamFile->WriteRAW(pData, 8);

    m_GlobalHeader.CopyHeaderToFile(m_streamFile);
    
    uint64_t iOffset = m_GlobalHeader.GetDataPos();
    for (size_t i = 0;i<m_DataBlocks.size();i++) {
        iOffset += m_DataBlocks[i]->m_block->CopyToFile(m_streamFile, iOffset,
                                                m_GlobalHeader.bIsBigEndian, 
                                                i == m_DataBlocks.size()-1);
        m_DataBlocks[i]->m_bIsDirty = false;
    }

    return true;
  }else {
    return false;
  }
}


DataBlock* UVF::GetDataBlockRW(uint64_t index, bool bOnlyChangeHeader) {
  if (bOnlyChangeHeader)
    m_DataBlocks[size_t(index)]->m_bHeaderIsDirty = true; 
  else {
    m_DataBlocks[size_t(index)]->m_bIsDirty = true; 
  }
  return m_DataBlocks[size_t(index)]->m_block.get();
}


bool UVF::AppendBlockToFile(std::tr1::shared_ptr<DataBlock> dataBlock) {
  if (!m_bFileIsReadWrite)  return false;
  
  // add new block to the datablock vector
  DataBlockListElem* dble = new DataBlockListElem(dataBlock, false,
                                           m_streamFile->GetCurrentSize(),
                                           dataBlock->GetOffsetToNextBlock());
  m_DataBlocks.push_back(std::tr1::shared_ptr<DataBlockListElem>(dble));

  // the block before the last needs to rewrite offset
  m_DataBlocks[m_DataBlocks.size()-2]->m_bHeaderIsDirty = true; 

  // and the last block needs to written to file
  dataBlock->CopyToFile(m_streamFile, m_streamFile->GetCurrentSize(),
                        m_GlobalHeader.bIsBigEndian, true);

  return true;
}


bool UVF::DropBlockFromFile(size_t iBlockIndex) {
  if (!m_bFileIsReadWrite)  return false;

  // create copy buffer
  std::tr1::shared_ptr<unsigned char> pBuffer(
    new unsigned char[BLOCK_COPY_SIZE], nonstd::DeleteArray<unsigned char>()
  );

  // remove data from file, by shifting all blocks after the
  // one to be removed towards the front of the file

  uint64_t iShiftSize = m_DataBlocks[iBlockIndex]->m_block->GetOffsetToNextBlock();
  for (size_t i = iBlockIndex+1;i<m_DataBlocks.size();i++) {
    uint64_t iSourcePos = m_DataBlocks[i]->m_iOffsetInFile +
                          m_GlobalHeader.GetDataPos();
    uint64_t iTargetPos = iSourcePos-iShiftSize;
    uint64_t iSize      = m_DataBlocks[i]->GetBlockSize();
    if (!m_streamFile->CopyRAW(iSize, iSourcePos, iTargetPos,
                               pBuffer.get(), BLOCK_COPY_SIZE)) {
      return false;
    }
  }

  // if block was last block in file, flag the previous block as last
  // i.e. set offset to 0
  if (iBlockIndex == m_DataBlocks.size()-1) {
    m_DataBlocks[m_DataBlocks.size()-2]->m_bHeaderIsDirty = true;
    m_streamFile->SeekPos(m_DataBlocks[iBlockIndex]->m_iOffsetInFile +
                          m_GlobalHeader.GetDataPos());
  }

  // truncate file
  m_streamFile->Truncate();

  // remove data from datablock vector
  m_DataBlocks.erase(m_DataBlocks.begin()+iBlockIndex);

  return true;
}
