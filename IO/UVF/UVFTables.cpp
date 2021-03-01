#include "UVFTables.h"

using namespace std;

#include "DataBlock.h"
#include "RasterDataBlock.h"
#include "Histogram1DDataBlock.h"
#include "Histogram2DDataBlock.h"
#include "KeyValuePairDataBlock.h"
#include "MaxMinDataBlock.h"
#include "GeometryDataBlock.h"
#include "TOCBlock.h"

string UVFTables::ChecksumSemanticToCharString(ChecksumSemanticTable uiTable) {
  switch (uiTable) {
    case (CS_NONE)  : return "none";
    case (CS_CRC32) : return "CRC32";
    case (CS_MD5)   : return "MD5";
    default         : return "Unknown";
  }
}

wstring UVFTables::ChecksumSemanticToString(ChecksumSemanticTable uiTable) {
  string charString = ChecksumSemanticToCharString(uiTable);
  wstring result(charString.begin(), charString.end());
  return result;
}

uint64_t UVFTables::ChecksumElemLength(ChecksumSemanticTable uiTable) {
  switch (uiTable) {
    case (CS_NONE)  : return 0;
    case (CS_CRC32) : return 32/8;
    case (CS_MD5)   : return 128/8;
    default          : throw "ChecksumElemLength: Unknown Checksum type";
  }
}

string UVFTables::CompressionSemanticToCharString(CompressionSemanticTable uiTable) {
  switch (uiTable) {
    case (COS_NONE)  : return "none";
    default    : return "Unknown";
  }
}

wstring UVFTables::CompressionSemanticToString(CompressionSemanticTable uiTable) {
  string charString = CompressionSemanticToCharString(uiTable);
  wstring result(charString.begin(), charString.end());
  return result;
}


string UVFTables::BlockSemanticTableToCharString(BlockSemanticTable uiTable) {
  switch (uiTable) {
    case (BS_EMPTY)              : return "Empty";
    case (BS_REG_NDIM_GRID)      : return "Regular N-Dimensional Grid";
    case (BS_NDIM_TRANSFER_FUNC) : return "N-Dimensional Transfer function";
    case (BS_PREVIEW_IMAGE)      : return "Preview Image";
    case (BS_KEY_VALUE_PAIRS)    : return "8bit String Key/Value Pairs";
    case (BS_1D_HISTOGRAM)       : return "Histogram (1D)";
    case (BS_2D_HISTOGRAM)       : return "Histogram (2D)";
    case (BS_MAXMIN_VALUES)      : return "Brick Max/Min Values";
    case (BS_GEOMETRY)           : return "Geometry";
    case (BS_TOC_BLOCK)          : return "Table Of Contents (TOC)";
    default                      : return "Unknown";
  }
}

wstring UVFTables::BlockSemanticTableToString(BlockSemanticTable uiTable) {
  string charString = BlockSemanticTableToCharString(uiTable);
  wstring result(charString.begin(), charString.end());
  return result;
}

std::shared_ptr<DataBlock>
UVFTables::CreateBlockFromSemanticEntry(BlockSemanticTable uiTable,
                                        LargeRAWFile_ptr pStreamFile,
                                        uint64_t iOffset, bool bIsBigEndian,
                                        uint64_t iUVFFileVersion) {
  DataBlock* d;
  switch (uiTable) {
    case BS_EMPTY:
      d = new DataBlock(pStreamFile, iOffset, bIsBigEndian);
      break;
    case BS_REG_NDIM_GRID:       /* fall through */
    case BS_NDIM_TRANSFER_FUNC:  /* fall through */
    case BS_PREVIEW_IMAGE:
      d = new RasterDataBlock(pStreamFile, iOffset, bIsBigEndian);
      break;
    case BS_1D_HISTOGRAM:
      d = new Histogram1DDataBlock(pStreamFile, iOffset, bIsBigEndian);
      break;
    case BS_2D_HISTOGRAM:
      d = new Histogram2DDataBlock(pStreamFile, iOffset, bIsBigEndian);
      break;
    case BS_KEY_VALUE_PAIRS: 
      d = new KeyValuePairDataBlock(pStreamFile, iOffset, bIsBigEndian);
      break;
    case BS_MAXMIN_VALUES: 
      d = new MaxMinDataBlock(pStreamFile, iOffset, bIsBigEndian);
      break;
    case BS_GEOMETRY:
      d = new GeometryDataBlock(pStreamFile, iOffset, bIsBigEndian);
      break;
    case BS_TOC_BLOCK:
      d = new TOCBlock(pStreamFile, iOffset, bIsBigEndian, iUVFFileVersion);
      break;
    default: throw "CreateBlockFromSemanticEntry: Unknown block semantic";
  }
  return std::shared_ptr<DataBlock>(d);
}

string UVFTables::DomainSemanticToCharString(DomainSemanticTable uiTable) {
  switch (uiTable) {
    case (DS_NONE)  : return "Empty";
    case (DS_X)      : return "X";
    case (DS_Y)      : return "Y";
    case (DS_Z)     : return "Z";
    case (DS_TIME)  : return "Time";
    default          : return "Unknown";
  }
}

wstring UVFTables::DomainSemanticToString(DomainSemanticTable uiTable) {
  string charString = DomainSemanticToCharString(uiTable);
  wstring result(charString.begin(), charString.end());
  return result;
}


string UVFTables::ElementSemanticTableToCharString(ElementSemanticTable uiTable) {
  switch (uiTable) {
    case (ES_UNDEFINED)    : return "Undefined";
    case (ES_VECTOR)      : return "General Vector Value";
    case (ES_TENSOR)      : return "General Tensor Value";
    case (ES_SYM_TENSOR)  : return "Symmetric Tensor Value";
    case (ES_RED)          : return "Color Component Red";
    case (ES_GREEN)        : return "Color Component Green";
    case (ES_BLUE)        : return "Color Component Blue";
    case (ES_ALPHA)        : return "Color Component Alpha";
    case (ES_MR)          : return "MR";
    case (ES_CT)          : return "CT";
    case (ES_TIME)        : return "Time (Second)";
    case (ES_MASS)        : return "Mass (Kilogram)";
    case (ES_ELECTRIC_CURRENT)          : return "Electric Current (Ampere)";
    case (ES_THERMODYNAMIC_TEMPERATURE) : return "Thermodynamic Temperature (Kelvin)";
    case (ES_AMOUNT_OF_SUBSTANCE)        : return "Amount of substance (Mole)";
    case (ES_LUMINOUS_INTENSITY)        : return "Luminous Intensity (Candela)";
    default                              : return "Unknown";
  }
}

wstring UVFTables::ElementSemanticTableToString(ElementSemanticTable uiTable) {
  string charString = ElementSemanticTableToCharString(uiTable);
  wstring result(charString.begin(), charString.end());
  return result;
}
