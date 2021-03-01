#include "GlobalHeader.h"

using namespace std;
using namespace UVFTables;

GlobalHeader::GlobalHeader() : 
  bIsBigEndian(false),
  ulFileVersion(0),
  ulChecksumSemanticsEntry(CS_NONE),
  ulAdditionalHeaderSize(0),
  ulOffsetToFirstDataBlock(0)
{}

GlobalHeader::GlobalHeader(const GlobalHeader &other) : 
  bIsBigEndian(other.bIsBigEndian),
  ulFileVersion(other.ulFileVersion),
  ulChecksumSemanticsEntry(other.ulChecksumSemanticsEntry),
  vcChecksum(other.vcChecksum),
  ulAdditionalHeaderSize(other.ulAdditionalHeaderSize),
  ulOffsetToFirstDataBlock(other.ulOffsetToFirstDataBlock)
{
}

GlobalHeader& GlobalHeader::operator=(const GlobalHeader& other)  { 
  bIsBigEndian = other.bIsBigEndian;
  ulFileVersion = other.ulFileVersion;
  ulChecksumSemanticsEntry = other.ulChecksumSemanticsEntry;
  ulAdditionalHeaderSize = other.ulAdditionalHeaderSize;
  ulOffsetToFirstDataBlock = other.ulOffsetToFirstDataBlock;
  vcChecksum = other.vcChecksum;
  return *this;
}

uint64_t GlobalHeader::GetDataPos() {
  return 8 + GetSize();
}

void GlobalHeader::GetHeaderFromFile(LargeRAWFile_ptr pStreamFile) {
  pStreamFile->ReadData(bIsBigEndian, false);
  pStreamFile->ReadData(ulFileVersion, bIsBigEndian);
  uint64_t uintSem;
  pStreamFile->ReadData(uintSem, bIsBigEndian);
  ulChecksumSemanticsEntry = (ChecksumSemanticTable)uintSem;
  uint64_t ulChecksumLength;
  pStreamFile->ReadData(ulChecksumLength, bIsBigEndian);
   pStreamFile->ReadData(vcChecksum, ulChecksumLength, bIsBigEndian);
  pStreamFile->ReadData(ulOffsetToFirstDataBlock, bIsBigEndian);
}

void GlobalHeader::CopyHeaderToFile(LargeRAWFile_ptr pStreamFile) {
  pStreamFile->WriteData(bIsBigEndian, false);
  pStreamFile->WriteData(ulFileVersion, bIsBigEndian);
  pStreamFile->WriteData(uint64_t(ulChecksumSemanticsEntry), bIsBigEndian);
  pStreamFile->WriteData(uint64_t(vcChecksum.size()), bIsBigEndian);
  pStreamFile->WriteData(vcChecksum, bIsBigEndian);
  pStreamFile->WriteData(ulOffsetToFirstDataBlock, bIsBigEndian);
}

uint64_t GlobalHeader::GetSize() {
  return GetMinSize() + vcChecksum.size() + ulOffsetToFirstDataBlock;
}

uint64_t GlobalHeader::GetMinSize() {
  return sizeof(bool) + 4 * sizeof(uint64_t);
}

void GlobalHeader::UpdateChecksum(vector<unsigned char> checksum, LargeRAWFile_ptr pStreamFile) {
  vcChecksum = checksum;
  uint64_t ulLastPos = pStreamFile->GetPos();
  pStreamFile->SeekPos(33);
  pStreamFile->WriteData(vcChecksum, bIsBigEndian);
  pStreamFile->SeekPos(ulLastPos);
}
