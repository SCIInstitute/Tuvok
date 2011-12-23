#pragma once
#ifndef GLOBALHEADER_H
#define GLOBALHEADER_H

#include <vector>
#include "UVFTables.h"

class GlobalHeader {
public:
  GlobalHeader();
  GlobalHeader(const GlobalHeader &other);
  GlobalHeader& operator=(const GlobalHeader& other);

  bool                              bIsBigEndian;
  uint64_t                            ulFileVersion;
  UVFTables::ChecksumSemanticTable  ulChecksumSemanticsEntry;
  std::vector<unsigned char>        vcChecksum;
  uint64_t                            ulAdditionalHeaderSize;

protected:
  uint64_t GetDataPos();
  void GetHeaderFromFile(LargeRAWFile_ptr pStreamFile);
  void CopyHeaderToFile(LargeRAWFile_ptr pStreamFile);
  uint64_t GetSize();
  static uint64_t GetMinSize();
  uint64_t ulOffsetToFirstDataBlock;
  void UpdateChecksum(std::vector<unsigned char> checksum, LargeRAWFile_ptr pStreamFile);

  friend class UVF;
};

#endif // GLOBALHEADER_H
