#include "TOCBlock.h"

#include "MaxMinDataBlock.h"
#include "DebugOut/AbstrDebugOut.h"
#include "ExtendedOctree/ExtendedOctreeConverter.h"

using namespace std;

TOCBlock::TOCBlock() :
  m_iOffsetToOctree(0),
  m_bIsBigEndian(false),
  m_iOverlap(2),
  m_vMaxBrickSize(128,128,128),
  m_strDeleteTempFile("")
{
  ulBlockSemantics = UVFTables::BS_TOC_BLOCK;
  strBlockID       = "Table of Contents Raster Data Block";
}

TOCBlock::TOCBlock(const TOCBlock &other) :
  DataBlock(other),
  m_bIsBigEndian(other.m_bIsBigEndian)
{
  if (!m_pStreamFile->IsOpen()) m_pStreamFile->Open();
  GetHeaderFromFile(m_pStreamFile, m_iOffset, m_bIsBigEndian);
}

TOCBlock::~TOCBlock(){
  if (!m_strDeleteTempFile.empty()) {
    m_ExtendedOctree.Close();
    remove(m_strDeleteTempFile.c_str());
  }
}

TOCBlock::TOCBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                   bool bIsBigEndian) {
  GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
}

uint64_t TOCBlock::GetHeaderFromFile(LargeRAWFile_ptr pStreamFile,
                                     uint64_t iOffset, bool bIsBigEndian) {
  assert(pStreamFile->IsOpen());
  m_bIsBigEndian = bIsBigEndian;
  m_iOffsetToOctree = iOffset +
                      DataBlock::GetHeaderFromFile(pStreamFile, iOffset,
                                                   bIsBigEndian);
  m_ExtendedOctree.Open(pStreamFile, m_iOffsetToOctree);
  return pStreamFile->GetPos() - iOffset;
}

uint64_t TOCBlock::CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset,
                              bool bIsBigEndian, bool bIsLastBlock) {
  if (!m_pStreamFile->IsOpen()) m_pStreamFile->Open(); // source data

  assert(pStreamFile->IsOpen()); // destination
  CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);

  uint64_t iDataSize = ComputeDataSize();
  m_pStreamFile->SeekPos(m_iOffsetToOctree);
  unsigned char* pData = new unsigned char[size_t(min(iDataSize, BLOCK_COPY_SIZE))];
  for (uint64_t i = 0;i<iDataSize;i+=BLOCK_COPY_SIZE) {
    uint64_t iCopySize = min(BLOCK_COPY_SIZE, iDataSize-i);
    uint64_t bytes = m_pStreamFile->ReadRAW(pData, iCopySize);
#ifdef NDEBUG
    (void)bytes;
#endif
    assert(bytes == iCopySize && "we know the exact file size; a short read "
           "makes no sense.");
    bytes = pStreamFile->WriteRAW(pData, iCopySize);
    assert(bytes == iCopySize);
  }
  delete [] pData;

  return pStreamFile->GetPos() - iOffset;
}

DataBlock* TOCBlock::Clone() const {
  return new TOCBlock(*this);
}

uint64_t TOCBlock::ComputeHeaderSize() const {
  // currently TOC Block contains no header in addition to the
  // header info stored in the ExtendedOctree
  return 0;
}


uint64_t TOCBlock::GetOffsetToNextBlock() const {
  return DataBlock::GetOffsetToNextBlock() + ComputeHeaderSize() + ComputeDataSize();
}

uint64_t TOCBlock::ComputeDataSize() const {
  return m_ExtendedOctree.GetSize();
}

bool TOCBlock::FlatDataToBrickedLOD(
  const std::string& strSourceFile, const std::string& strTempFile,
  ExtendedOctree::COMPONENT_TYPE eType, uint64_t iComponentCount,
  const UINT64VECTOR3& vVolumeSize,
  const DOUBLEVECTOR3& vScale,
  const UINT64VECTOR3& vMaxBrickSize,
  uint32_t iOverlap,
  size_t iCacheSize,
  std::tr1::shared_ptr<MaxMinDataBlock> pMaxMinDatBlock,
  AbstrDebugOut* debugOut
) {

  LargeRAWFile_ptr inFile(new LargeRAWFile(strSourceFile));
  if (!inFile->Open()) {
    return false;
  }

  return FlatDataToBrickedLOD(inFile, strTempFile, eType, iComponentCount,
                              vVolumeSize, vScale, vMaxBrickSize,
                              iOverlap, iCacheSize,
                              pMaxMinDatBlock, debugOut);
}

bool TOCBlock::FlatDataToBrickedLOD(
  LargeRAWFile_ptr pSourceData, const std::string& strTempFile,
  ExtendedOctree::COMPONENT_TYPE eType, uint64_t iComponentCount,
  const UINT64VECTOR3& vVolumeSize,
  const DOUBLEVECTOR3& vScale,
  const UINT64VECTOR3& vMaxBrickSize,
  uint32_t iOverlap,
  size_t iCacheSize,
  std::tr1::shared_ptr<MaxMinDataBlock> pMaxMinDatBlock,
  AbstrDebugOut*
) {
  m_vMaxBrickSize = vMaxBrickSize;
  m_iOverlap = iOverlap;

  LargeRAWFile_ptr outFile(new LargeRAWFile(strTempFile));
  if (!outFile->Create()) {
    return false;
  }
  m_pStreamFile = outFile;
  m_strDeleteTempFile = strTempFile;
  ExtendedOctreeConverter c(m_vMaxBrickSize, m_iOverlap, iCacheSize);
  BrickStatVec statsVec;

  if (!pSourceData->IsOpen()) pSourceData->Open();

  bool bResult = c.Convert(pSourceData, 0, eType, iComponentCount, vVolumeSize,
                           vScale, outFile, 0, &statsVec, CT_ZIP);
  outFile->Close();

  if (bResult) {
    pMaxMinDatBlock->SetDataFromFlatVector(statsVec, iComponentCount);
    return m_ExtendedOctree.Open(strTempFile,0);
  } else {
    return false;
  }
}

bool TOCBlock::BrickedLODToFlatData(
  uint64_t iLoD,
  const std::string& strTargetFile,
  bool bAppend,
  AbstrDebugOut* pDebugOut
) const {
  LargeRAWFile_ptr outFile;
  outFile = LargeRAWFile_ptr(new LargeRAWFile(strTargetFile));
  if (bAppend) {
    if (!outFile->Append()) {
      return false;
    }
  } else {
    if (!outFile->Create()) {
      return false;
    }
  }

  return BrickedLODToFlatData(iLoD, outFile, bAppend, pDebugOut);
}


bool TOCBlock::BrickedLODToFlatData(uint64_t iLoD,
                                    LargeRAWFile_ptr pTargetFile,
                                    bool bAppend, AbstrDebugOut*) const {
  uint64_t iOffset = bAppend ? pTargetFile->GetCurrentSize() : 0;
  return ExtendedOctreeConverter::ExportToRAW(m_ExtendedOctree, pTargetFile,
                                              iLoD, iOffset);
}

bool TOCBlock::ApplyFunction(uint64_t iLoD,
                             bool (*brickFunc)(
                               void* pData,
                               const UINT64VECTOR3& vBrickSize,
                               const UINT64VECTOR3& vBrickOffset,
                               void* pUserContext
                             ),
                             void* pUserContext,
                             uint32_t iOverlap,
                             AbstrDebugOut*) const {
  return ExtendedOctreeConverter::ApplyFunction(m_ExtendedOctree, iLoD,
                                                brickFunc, pUserContext,
                                                iOverlap);
}

void TOCBlock::GetData(uint8_t* pData, UINT64VECTOR4 coordinates) const {
  m_ExtendedOctree.GetBrickData(pData, coordinates);
}

UINT64VECTOR3 TOCBlock::GetBrickCount(uint64_t iLoD) const {
  return m_ExtendedOctree.GetBrickCount(iLoD);
}

UINT64VECTOR3 TOCBlock::GetBrickSize(UINT64VECTOR4 coordinates) const {
  return m_ExtendedOctree.ComputeBrickSize(coordinates);
}

DOUBLEVECTOR3 TOCBlock::GetBrickAspect(UINT64VECTOR4 coordinates) const {
  return m_ExtendedOctree.GetBrickAspect(coordinates);
}

UINT64VECTOR3 TOCBlock::GetLODDomainSize(uint64_t iLoD) const {
  return m_ExtendedOctree.GetLoDSize(iLoD);
}

uint64_t TOCBlock::GetLoDCount() const {
  return m_ExtendedOctree.GetLODCount();
}

bool TOCBlock::GetIsSigned() const {
  const ExtendedOctree::COMPONENT_TYPE t = GetComponentType();
  switch (t) {
    case ExtendedOctree::CT_INT8:
    case ExtendedOctree::CT_INT16:
    case ExtendedOctree::CT_INT32:
    case ExtendedOctree::CT_INT64:
    case ExtendedOctree::CT_FLOAT32:
    case ExtendedOctree::CT_FLOAT64: return true;

    case ExtendedOctree::CT_UINT8:
    case ExtendedOctree::CT_UINT16:
    case ExtendedOctree::CT_UINT32:
    case ExtendedOctree::CT_UINT64:
    default: return false;
  }
}

bool TOCBlock::GetIsFloat() const {
  const ExtendedOctree::COMPONENT_TYPE t = GetComponentType();
  switch (t) {
    case ExtendedOctree::CT_FLOAT32:
    case ExtendedOctree::CT_FLOAT64: return true;

    case ExtendedOctree::CT_INT8:
    case ExtendedOctree::CT_INT16:
    case ExtendedOctree::CT_INT32:
    case ExtendedOctree::CT_INT64:
    case ExtendedOctree::CT_UINT8:
    case ExtendedOctree::CT_UINT16:
    case ExtendedOctree::CT_UINT32:
    case ExtendedOctree::CT_UINT64:
    default: return false;
  }
}

DOUBLEVECTOR3 TOCBlock::GetScale() const {
  return m_ExtendedOctree.GetGlobalAspect();
}

void TOCBlock::SetScale(const DOUBLEVECTOR3& scale) {
  m_ExtendedOctree.SetGlobalAspect(scale);
}

uint64_t TOCBlock::GetLinearBrickIndex(UINT64VECTOR4 coordinates) const {
  return m_ExtendedOctree.BrickCoordsToIndex(coordinates);
}
