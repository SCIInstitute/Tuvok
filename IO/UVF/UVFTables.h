#pragma once

#ifndef UVFTABLES_H
#define UVFTABLES_H

#include "UVFBasic.h"

/************************************************
Tables
*************************************************/
namespace UVFTables {
  enum ChecksumSemanticTable {
    CS_NONE = 0,
    CS_CRC32,
    CS_MD5,
    CS_UNKNOWN
  };

  enum CompressionSemanticTable {
    COS_NONE = 0,
    COS_UNKNOWN
  };

  enum BlockSemanticTable {
    BS_EMPTY = 0,
    BS_REG_NDIM_GRID,
    BS_NDIM_TRANSFER_FUNC,
    BS_PREVIEW_IMAGE,
    BS_KEY_VALUE_PAIRS,
    BS_1D_HISTOGRAM,
    BS_2D_HISTOGRAM,
    BS_MAXMIN_VALUES,
    BS_GEOMETRY,
    BS_TOC_BLOCK,
    BS_UNKNOWN
  };

  enum DomainSemanticTable {
    DS_NONE = 0,
    DS_X,
    DS_Y,
    DS_Z,
    DS_TIME,
    DS_UNKNOWN
  };

  enum ElementSemanticTable {
    ES_UNDEFINED = 0,
    ES_VECTOR,
    ES_TENSOR,
    ES_SYM_TENSOR,
    ES_RED = 10000,
    ES_GREEN,
    ES_BLUE,
    ES_ALPHA,
    ES_MR = 20000,
    ES_CT,
    ES_TIME = 30000,
    ES_MASS,
    ES_ELECTRIC_CURRENT,
    ES_THERMODYNAMIC_TEMPERATURE,
    ES_AMOUNT_OF_SUBSTANCE,
    ES_LUMINOUS_INTENSITY,
    ES_UNKNOWN
  };


  /************************************************
  Calls
  *************************************************/

  std::string ChecksumSemanticToCharString(ChecksumSemanticTable uiTable);
  std::wstring ChecksumSemanticToString(ChecksumSemanticTable uiTable);
  uint64_t ChecksumElemLength(ChecksumSemanticTable uiTable);

  std::string CompressionSemanticToCharString(CompressionSemanticTable uiTable);
  std::wstring CompressionSemanticToString(CompressionSemanticTable uiTable);

  std::string BlockSemanticTableToCharString(BlockSemanticTable uiTable);
  std::wstring BlockSemanticTableToString(BlockSemanticTable uiTable);
  std::shared_ptr<DataBlock> CreateBlockFromSemanticEntry(
    BlockSemanticTable uiTable, LargeRAWFile_ptr pStreamFile,
    uint64_t iOffset, bool bIsBigEndian, uint64_t iUVFFileVersion
  );

  std::string DomainSemanticToCharString(DomainSemanticTable uiTable);
  std::wstring DomainSemanticToString(DomainSemanticTable uiTable);

  std::string ElementSemanticTableToCharString(ElementSemanticTable uiTable);
  std::wstring ElementSemanticTableToString(ElementSemanticTable uiTable);
}
#endif // UVFTABLES_H
