#include "Histogram1DDataBlock.h"

#include "RasterDataBlock.h"
#include "../../Basics/MathTools.h"
#include "../../Controller/Controller.h"
#include "../../Basics/ProgressTimer.h"

// simple/generic progress update message

using namespace std;
using namespace UVFTables;

Histogram1DDataBlock::Histogram1DDataBlock() : DataBlock() {
  ulBlockSemantics = BS_1D_HISTOGRAM;
  strBlockID       = "1D Histogram";
}

Histogram1DDataBlock::Histogram1DDataBlock(const Histogram1DDataBlock &other) :
  DataBlock(other),
  m_vHistData(other.m_vHistData)
{
}

Histogram1DDataBlock& Histogram1DDataBlock::operator=(const Histogram1DDataBlock& other) {
  strBlockID = other.strBlockID;
  ulBlockSemantics = other.ulBlockSemantics;
  ulCompressionScheme = other.ulCompressionScheme;
  ulOffsetToNextDataBlock = other.ulOffsetToNextDataBlock;

  m_vHistData = other.m_vHistData;

  return *this;
}


Histogram1DDataBlock::Histogram1DDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian) {
  GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
}

Histogram1DDataBlock::~Histogram1DDataBlock() 
{
}

DataBlock* Histogram1DDataBlock::Clone() const {
  return new Histogram1DDataBlock(*this);
}

uint64_t Histogram1DDataBlock::GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian) {
  uint64_t iStart = iOffset + DataBlock::GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
  pStreamFile->SeekPos(iStart);

  uint64_t ulElementCount;
  pStreamFile->ReadData(ulElementCount, bIsBigEndian);

  m_vHistData.resize(size_t(ulElementCount));
  pStreamFile->ReadRAW((unsigned char*)&m_vHistData[0], ulElementCount*sizeof(uint64_t));
  return pStreamFile->GetPos() - iOffset;
}

bool Histogram1DDataBlock::Compute(const TOCBlock* source, uint64_t iLevel) {
  // do not try to compute a histogram for floating point data,
  // anything beyond 32 bit or more than 1 component data
  if (source->GetComponentType() == ExtendedOctree::CT_FLOAT32 ||
      source->GetComponentType() == ExtendedOctree::CT_FLOAT64 ||
      source->GetComponentTypeSize() > 4 ||
      source->GetComponentCount() != 1) return false;

  // resize histogram 
  size_t iValueRange = size_t(MathTools::Pow2(uint64_t(source->GetComponentTypeSize()*8)));
  m_vHistData.resize(iValueRange);
  if (m_vHistData.size() != iValueRange) return false;
  std::fill(m_vHistData.begin(), m_vHistData.end(), 0);

  // compute histogram
  switch (source->GetComponentType()) {
    case ExtendedOctree::CT_UINT8:
      ComputeTemplate<uint8_t>(source, iLevel);
      break;
    case ExtendedOctree::CT_UINT16:
      ComputeTemplate<uint16_t>(source, iLevel);
      break;
    case ExtendedOctree::CT_UINT32:
      ComputeTemplate<uint32_t>(source, iLevel);
      break;
    case ExtendedOctree::CT_UINT64:
      ComputeTemplate<uint64_t>(source, iLevel);
      break;
    case ExtendedOctree::CT_INT8:
      ComputeTemplate<int8_t>(source, iLevel);
      break;
    case ExtendedOctree::CT_INT16:
      ComputeTemplate<int16_t>(source, iLevel);
      break;
    case ExtendedOctree::CT_INT32:
      ComputeTemplate<int32_t>(source, iLevel);
      break;
    case ExtendedOctree::CT_INT64:
      ComputeTemplate<int64_t>(source, iLevel);
      break;
    case ExtendedOctree::CT_FLOAT32:
      ComputeTemplate<float>(source, iLevel);
      break;
    case ExtendedOctree::CT_FLOAT64:
      ComputeTemplate<double>(source, iLevel);
      break;
  }


  // find maximum-index non zero entry and clip histogram data
  size_t iSize = 0;
  for (size_t i = 0;i<iValueRange;i++) if (m_vHistData[i] != 0) iSize = i+1;
  m_vHistData.resize(iSize);

  // set data block information
  strBlockID = "1D Histogram for datablock " + source->strBlockID;

  return true;
}

template <class T> 
void Histogram1DDataBlock::ComputeTemplate(const TOCBlock* source,
                                           uint64_t iLevel) {
  // compute histogram by iterating over all bricks of the given level
  UINT64VECTOR3 bricksInSourceLevel = source->GetBrickCount(iLevel);

  size_t iCompcount = size_t(source->GetComponentCount());
  T* pTempBrickData = new T[size_t(source->GetMaxBrickSize().volume())
                            *iCompcount];

  uint32_t iOverlap =source->GetOverlap();

  ProgressTimer timer;
  timer.Start();

  for (uint64_t bz = 0;bz<bricksInSourceLevel.z;bz++) {
    for (uint64_t by = 0;by<bricksInSourceLevel.y;by++) {
      for (uint64_t bx = 0;bx<bricksInSourceLevel.x;bx++) {
        UINT64VECTOR4 brickCoords(bx,by,bz,iLevel);
        source->GetData((uint8_t*)pTempBrickData, brickCoords);
        UINTVECTOR3 bricksize = UINTVECTOR3(source->GetBrickSize(brickCoords));

        for (uint32_t z = iOverlap;z<bricksize.z-iOverlap;z++) {
          for (uint32_t y = iOverlap;y<bricksize.y-iOverlap;y++) {
            for (uint32_t x = iOverlap;x<bricksize.x-iOverlap;x++) {
              // TODO: think about what todo with multi component data
              //       right now we only pick the first component
              size_t val = size_t(pTempBrickData[iCompcount*(x+y*bricksize.x+z*bricksize.x*bricksize.y)]);
              m_vHistData[val]++;
            }
          }
        }
      }
    }

    float progress = float(bz)/float(bricksInSourceLevel.z);
    MESSAGE("Computing 1D Histogram %5.2f%% (%s)", 
            progress * 100.0f,
            timer.GetProgressMessage(progress).c_str());
  }
  delete [] pTempBrickData;
}


size_t Histogram1DDataBlock::Compress(size_t maxTargetSize) {
  if (m_vHistData.size() > maxTargetSize) {
    // compute the smallest integer that reduces m_vHistData.size 
    // under the maxTargetSize threshold, we want an integer to
    // avoid an uneven combination of histogram bins
    size_t reduction = size_t(ceil( double(m_vHistData.size())  / double(maxTargetSize)));
    size_t newSize = size_t(ceil(double(m_vHistData.size())/double(reduction)));
    std::vector<uint64_t> tempHist(newSize);

    for (size_t i = 0;i<newSize;++i) {
      tempHist[i] = 0;
      for (size_t j = 0;j<reduction;++j) {
        size_t coord = i*reduction+j;
        if (coord < m_vHistData.size()) 
          tempHist[i] += m_vHistData[coord];
      }
    }


    m_vHistData = tempHist;
  }
  return m_vHistData.size();
}

bool Histogram1DDataBlock::Compute(const RasterDataBlock* source) {

  // TODO: right now we can only compute Histograms of scalar data this
  // should be changed to a more general approach
  if (source->ulElementDimension != 1 ||
      source->ulElementDimensionSize.size() != 1)
    return false;

  /// \todo right now compute Histogram assumes that at least the
  // lowest LOD level consists only of a single brick, this brick is
  // used for the hist.-computation this should be changed to a more
  // general approach
  vector<uint64_t> vSmallestLOD = source->GetSmallestBrickIndex();
  const vector<uint64_t>& vBricks = source->GetBrickCount(vSmallestLOD);
  for (size_t i = 0;i<vBricks.size();i++) if (vBricks[i] != 1) return false;
  
  // create temp histogram 
  size_t iValueRange = size_t(1<<(source->ulElementBitSize[0][0]));
  m_vHistData.resize(iValueRange);
  if (m_vHistData.size() != iValueRange) return false;
  std::fill(m_vHistData.begin(), m_vHistData.end(), 0);

  // LargestSingleBrickLODBrickIndex is well defined as we tested above
  // if we have a single brick LOD
  std::vector<unsigned char> vcSourceData;
  vector<uint64_t> vLOD = source->LargestSingleBrickLODBrickIndex();
  vector<uint64_t> vOneAndOnly;
  for (size_t i = 0;i<vBricks.size();i++) vOneAndOnly.push_back(0);
  if (!source->GetData(vcSourceData, vLOD, vOneAndOnly)) return false;

  vector<uint64_t> vSize  = source->LargestSingleBrickLODBrickSize();

  uint64_t iDataSize = 1;
  for (size_t i = 0;i<vSize.size();i++) iDataSize*=vSize[i];

  /// @todo only 8 and 16 bit integer data are supported.  this should be
  ///       changed to use a more general approach.
  if (source->ulElementBitSize[0][0] == 8) {
    for (uint64_t i = 0;i<iDataSize;i++) {
       m_vHistData[vcSourceData[size_t(i)]]++;
    }
  } else {
    if (source->ulElementBitSize[0][0] == 16) {
      unsigned short *psSourceData = (unsigned short*)(&(vcSourceData.at(0)));
      for (uint64_t i = 0;i<iDataSize;i++) {
        m_vHistData[psSourceData[size_t(i)]]++;
      }
    } else {
      return false;
    }
  }

  // find maximum-index non zero entry
  size_t iSize = 0;
  for (size_t i = 0;i<iValueRange;i++) if (m_vHistData[i] != 0) iSize = i+1;
  m_vHistData.resize(iSize);
  
  // set data block information
  strBlockID = "1D Histogram for datablock " + source->strBlockID;

  return true;
}


void Histogram1DDataBlock::CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  DataBlock::CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);
  uint64_t ulElementCount = uint64_t(m_vHistData.size());
  pStreamFile->WriteData(ulElementCount, bIsBigEndian);
}

uint64_t Histogram1DDataBlock::CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);
  pStreamFile->WriteRAW((unsigned char*)&m_vHistData[0], m_vHistData.size()*sizeof(uint64_t));
  return pStreamFile->GetPos() - iOffset;
}


uint64_t Histogram1DDataBlock::GetOffsetToNextBlock() const {
  return DataBlock::GetOffsetToNextBlock() + ComputeDataSize();
}

uint64_t Histogram1DDataBlock::ComputeDataSize() const {
  return sizeof(uint64_t) +                  // length of the vector
       m_vHistData.size()*sizeof(uint64_t);  // the vector itself
}
