#include "Histogram2DDataBlock.h"
#include "Basics/Vectors.h"
#include "RasterDataBlock.h"
#include "TOCBlock.h"

using namespace std;

Histogram2DDataBlock::Histogram2DDataBlock() : 
  DataBlock(),
  m_fMaxGradMagnitude(0)
{
  ulBlockSemantics = UVFTables::BS_2D_HISTOGRAM;
  strBlockID       = "2D Histogram";
}

Histogram2DDataBlock::Histogram2DDataBlock(const Histogram2DDataBlock &other) :
  DataBlock(other),
  m_vHistData(other.m_vHistData),
  m_fMaxGradMagnitude(other.m_fMaxGradMagnitude)
{
}

Histogram2DDataBlock& Histogram2DDataBlock::operator=(const Histogram2DDataBlock& other) {
  strBlockID = other.strBlockID;
  ulBlockSemantics = other.ulBlockSemantics;
  ulCompressionScheme = other.ulCompressionScheme;
  ulOffsetToNextDataBlock = other.ulOffsetToNextDataBlock;

  m_vHistData = other.m_vHistData;
  m_fMaxGradMagnitude = other.m_fMaxGradMagnitude;

  return *this;
}


Histogram2DDataBlock::Histogram2DDataBlock(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian) {
  GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
}

Histogram2DDataBlock::~Histogram2DDataBlock() 
{
}

DataBlock* Histogram2DDataBlock::Clone() const {
  return new Histogram2DDataBlock(*this);
}

uint64_t Histogram2DDataBlock::GetHeaderFromFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian) {
  uint64_t iStart = iOffset + DataBlock::GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
  pStreamFile->SeekPos(iStart);

  uint64_t ulElementCountX, ulElementCountY;
  pStreamFile->ReadData(m_fMaxGradMagnitude, bIsBigEndian);
  pStreamFile->ReadData(ulElementCountX, bIsBigEndian);
  pStreamFile->ReadData(ulElementCountY, bIsBigEndian);

  m_vHistData.resize(size_t(ulElementCountX));
  vector<uint64_t> tmp((size_t)ulElementCountY);
  for (size_t i = 0;i<size_t(ulElementCountX);i++) {
    pStreamFile->ReadRAW((unsigned char*)&tmp[0], ulElementCountY*sizeof(uint64_t));
    m_vHistData[i] = tmp;
  }

  return pStreamFile->GetPos() - iOffset;
}

bool Histogram2DDataBlock::Compute(const TOCBlock* source, 
                                   uint64_t iLevel,
                                   size_t iHistoBinCount,
                                   double fMaxNonZeroValue) {
  // do not try to compute a histogram for floating point data,
  // anything beyond 32 bit or more than 1 component data
  if (source->GetComponentType() == ExtendedOctree::CT_FLOAT32 ||
    source->GetComponentType() == ExtendedOctree::CT_FLOAT64 ||
    source->GetComponentTypeSize() > 4 ||
    source->GetComponentCount() != 1) return false;

  // resize histogram 
  m_vHistData.resize(iHistoBinCount);
  for (size_t i = 0;i<iHistoBinCount;i++) {
    m_vHistData[i].resize(256);
    for (size_t j = 0;j<256;j++) {
      m_vHistData[i][j] = 0;
    }
  }

  // compute histogram
  switch (source->GetComponentType()) {
  case ExtendedOctree::CT_UINT8:
    ComputeTemplate<uint8_t>(source, double(std::numeric_limits<uint8_t>::max()), iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  case ExtendedOctree::CT_UINT16:
    ComputeTemplate<uint16_t>(source, double(std::numeric_limits<uint16_t>::max()), iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  case ExtendedOctree::CT_UINT32:
    ComputeTemplate<uint32_t>(source, double(std::numeric_limits<uint32_t>::max()), iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  case ExtendedOctree::CT_UINT64:
    ComputeTemplate<uint64_t>(source, double(std::numeric_limits<uint64_t>::max()), iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  case ExtendedOctree::CT_INT8:
    ComputeTemplate<int8_t>(source, double(std::numeric_limits<int8_t>::max()), iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  case ExtendedOctree::CT_INT16:
    ComputeTemplate<int16_t>(source, double(std::numeric_limits<int16_t>::max()), iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  case ExtendedOctree::CT_INT32:
    ComputeTemplate<int32_t>(source, double(std::numeric_limits<int32_t>::max()), iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  case ExtendedOctree::CT_INT64:
    ComputeTemplate<int64_t>(source, double(std::numeric_limits<int64_t>::max()), iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  case ExtendedOctree::CT_FLOAT32:
    ComputeTemplate<float>(source, 1.0, iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  case ExtendedOctree::CT_FLOAT64:
    ComputeTemplate<double>(source, 1.0, iLevel, iHistoBinCount, fMaxNonZeroValue);
    break;
  }

  // set data block information
  strBlockID = "2D Histogram for datablock " + source->strBlockID;

  return true;
}


/// \todo right now compute Histogram assumes that the lowest LOD level
/// consists only of a single brick, this brick is used for the hist.
/// computation
//       this should be changed to a more general approach
bool Histogram2DDataBlock::Compute(const RasterDataBlock* source,
                                   size_t iHistoBinCount,
                                   double fMaxNonZeroValue) {
  /// \todo right now we can only compute Histograms of scalar data this should be changed to a more general approach
  if (source->ulElementDimension != 1 || source->ulElementDimensionSize.size() != 1) return false;

  /// \todo right now compute Histogram assumes that at least the lowest LOD level consists only 
  //       of a single brick, this brick is used for the hist.-computation
  //       this should be changed to a more general approach
  vector<uint64_t> vSmallestLOD = source->GetSmallestBrickIndex();
  const vector<uint64_t>& vBricks = source->GetBrickCount(vSmallestLOD);
  for (unsigned int i = 0;i<vBricks.size();i++) if (vBricks[i] != 1) return false;
  
  /// \todo right now we can only compute 2D Histograms of at least 3D data this should be changed to a more general approach
  //       also we require that the first three entries as X,Y,Z
  if (source->ulDomainSize.size() < 3 || source->ulDomainSemantics[0] != UVFTables::DS_X ||
      source->ulDomainSemantics[1] != UVFTables::DS_Y || source->ulDomainSemantics[2] != UVFTables::DS_Z) return false;

  m_vHistData.resize(iHistoBinCount);
  for (size_t i = 0;i<iHistoBinCount;i++) {
    m_vHistData[i].resize(256);
    for (size_t j = 0;j<256;j++) {
      m_vHistData[i][j] = 0;
    }
  }

  std::vector<unsigned char> vcSourceData;

  // LargestSingleBrickLODBrickIndex is well defined as we tested above if we have a single brick LOD
  vector<uint64_t> vLOD = source->LargestSingleBrickLODBrickIndex();
  vector<uint64_t> vOneAndOnly;
  for (size_t i = 0;i<vBricks.size();i++) vOneAndOnly.push_back(0);
  if (!source->GetData(vcSourceData, vLOD, vOneAndOnly)) return false;

  vector<uint64_t> vSize = source->LargestSingleBrickLODBrickSize();

  uint64_t iDataSize = 1;
  for (size_t i = 0;i<vSize.size();i++) iDataSize*=vSize[i];

  /// \todo right now only 8 and 16 bit integer data is supported this should be changed to a more general approach
  m_fMaxGradMagnitude = 0.0f;
  if (source->ulElementBitSize[0][0] == 8) {
    for (size_t z = 0;z<size_t(vSize[2]);z++) {
      for (size_t y = 0;y<size_t(vSize[1]);y++) {
        for (size_t x = 0;x<size_t(vSize[0]);x++) {

          size_t iCenter = x+size_t(vSize[0])*y+size_t(vSize[0])*size_t(vSize[1])*z;
          size_t iLeft   = iCenter;
          size_t iRight  = iCenter;
          size_t iTop    = iCenter;
          size_t iBottom = iCenter;
          size_t iFront  = iCenter;
          size_t iBack   = iCenter;

          FLOATVECTOR3 vScale(0,0,0);

          if (x > 0)          {iLeft   = iCenter-1; vScale.x++;}
          if (x < vSize[0]-1) {iRight  = iCenter+1; vScale.x++;}
          if (y > 0)          {iTop    = iCenter-size_t(vSize[0]);vScale.y++;}
          if (y < vSize[1]-1) {iBottom = iCenter+size_t(vSize[0]);vScale.y++;}
          if (z > 0)          {iFront  = iCenter-size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}
          if (z < vSize[2]-1) {iBack   = iCenter+size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}

          FLOATVECTOR3 vGradient((float(vcSourceData[iLeft])-float(vcSourceData[iRight]))/(255*vScale.x),
                                 (float(vcSourceData[iTop])-float(vcSourceData[iBottom]))/(255*vScale.y),
                                 (float(vcSourceData[iFront])-float(vcSourceData[iBack]))/(255*vScale.z));

          if (vGradient.length() > m_fMaxGradMagnitude) m_fMaxGradMagnitude = vGradient.length();
        }
      }
    }
    for (size_t z = 0;z<size_t(vSize[2]);z++) {
      for (size_t y = 0;y<size_t(vSize[1]);y++) {
        for (size_t x = 0;x<size_t(vSize[0]);x++) {

          size_t iCenter = x+size_t(vSize[0])*y+size_t(vSize[0])*size_t(vSize[1])*z;
          size_t iLeft   = iCenter;
          size_t iRight  = iCenter;
          size_t iTop    = iCenter;
          size_t iBottom = iCenter;
          size_t iFront  = iCenter;
          size_t iBack   = iCenter;

          FLOATVECTOR3 vScale(0,0,0);

          if (x > 0)          {iLeft   = iCenter-1; vScale.x++;}
          if (x < vSize[0]-1) {iRight  = iCenter+1; vScale.x++;}
          if (y > 0)          {iTop    = iCenter-size_t(vSize[0]);vScale.y++;}
          if (y < vSize[1]-1) {iBottom = iCenter+size_t(vSize[0]);vScale.y++;}
          if (z > 0)          {iFront  = iCenter-size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}
          if (z < vSize[2]-1) {iBack   = iCenter+size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}

          FLOATVECTOR3 vGradient((float(vcSourceData[iLeft])-float(vcSourceData[iRight]))/(255*vScale.x),
                                 (float(vcSourceData[iTop])-float(vcSourceData[iBottom]))/(255*vScale.y),
                                 (float(vcSourceData[iFront])-float(vcSourceData[iBack]))/(255*vScale.z));

          unsigned char iGardientMagnitudeIndex = (unsigned char)(min<int>(255,int(vGradient.length()/m_fMaxGradMagnitude*255.0f)));
          m_vHistData[vcSourceData[iCenter]][iGardientMagnitudeIndex]++;
        }
      }
    }
  } else {
    if (source->ulElementBitSize[0][0] == 16) {
      unsigned short *psSourceData = (unsigned short*)(&(vcSourceData.at(0)));
      
      for (size_t z = 0;z<size_t(vSize[2]);z++) {
        for (size_t y = 0;y<size_t(vSize[1]);y++) {
          for (size_t x = 0;x<size_t(vSize[0]);x++) {

            size_t iCenter = x+size_t(vSize[0])*y+size_t(vSize[0])*size_t(vSize[1])*z;
            size_t iLeft   = iCenter;
            size_t iRight  = iCenter;
            size_t iTop    = iCenter;
            size_t iBottom = iCenter;
            size_t iFront  = iCenter;
            size_t iBack   = iCenter;

            FLOATVECTOR3 vScale(0,0,0);

            if (x > 0)          {iLeft   = iCenter-1; vScale.x++;}
            if (x < vSize[0]-1) {iRight  = iCenter+1; vScale.x++;}
            if (y > 0)          {iTop    = iCenter-size_t(vSize[0]);vScale.y++;}
            if (y < vSize[1]-1) {iBottom = iCenter+size_t(vSize[0]);vScale.y++;}
            if (z > 0)          {iFront  = iCenter-size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}
            if (z < vSize[2]-1) {iBack   = iCenter+size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}

            FLOATVECTOR3   vGradient((float(psSourceData[iLeft])-float(psSourceData[iRight]))/(65535*vScale.x),
                                     (float(psSourceData[iTop])-float(psSourceData[iBottom]))/(65535*vScale.y),
                                     (float(psSourceData[iFront])-float(psSourceData[iBack]))/(65535*vScale.z));

            
            if (vGradient.length() > m_fMaxGradMagnitude) m_fMaxGradMagnitude = vGradient.length();
          }
        }
      }
      for (size_t z = 0;z<size_t(vSize[2]);z++) {
        for (size_t y = 0;y<size_t(vSize[1]);y++) {
          for (size_t x = 0;x<size_t(vSize[0]);x++) {

            size_t iCenter = x+size_t(vSize[0])*y+size_t(vSize[0])*size_t(vSize[1])*z;
            size_t iLeft   = iCenter;
            size_t iRight  = iCenter;
            size_t iTop    = iCenter;
            size_t iBottom = iCenter;
            size_t iFront  = iCenter;
            size_t iBack   = iCenter;

            FLOATVECTOR3 vScale(0,0,0);

            if (x > 0)          {iLeft   = iCenter-1; vScale.x++;}
            if (x < vSize[0]-1) {iRight  = iCenter+1; vScale.x++;}
            if (y > 0)          {iTop    = iCenter-size_t(vSize[0]);vScale.y++;}
            if (y < vSize[1]-1) {iBottom = iCenter+size_t(vSize[0]);vScale.y++;}
            if (z > 0)          {iFront  = iCenter-size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}
            if (z < vSize[2]-1) {iBack   = iCenter+size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}

            FLOATVECTOR3   vGradient((float(psSourceData[iLeft])-float(psSourceData[iRight]))/(65535*vScale.x),
                                     (float(psSourceData[iTop])-float(psSourceData[iBottom]))/(65535*vScale.y),
                                     (float(psSourceData[iFront])-float(psSourceData[iBack]))/(65535*vScale.z));

            unsigned char iGardientMagnitudeIndex = (unsigned char)(min<int>(255,int(vGradient.length()/m_fMaxGradMagnitude*255.0f)));
            int iValue = (fMaxNonZeroValue <= double(iHistoBinCount-1)) 
                                                                ? psSourceData[iCenter] 
                                                                : min<int>(int(iHistoBinCount-1),int(float(psSourceData[iCenter] * float(iHistoBinCount-1)/fMaxNonZeroValue)));
            m_vHistData[iValue][iGardientMagnitudeIndex]++;
          }
        }
      }
    } else {
      return false;
    }
  }

  // set data block information
  strBlockID = "2D Histogram for datablock " + source->strBlockID;

  return true;
}


void Histogram2DDataBlock::CopyHeaderToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  DataBlock::CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);

  uint64_t ulElementCountX = uint64_t(m_vHistData.size());
  uint64_t ulElementCountY = uint64_t(m_vHistData[0].size());

  pStreamFile->WriteData(m_fMaxGradMagnitude, bIsBigEndian);
  pStreamFile->WriteData(ulElementCountX, bIsBigEndian);
  pStreamFile->WriteData(ulElementCountY, bIsBigEndian);
}


uint64_t Histogram2DDataBlock::CopyToFile(LargeRAWFile_ptr pStreamFile, uint64_t iOffset, bool bIsBigEndian, bool bIsLastBlock) {
  CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);

  vector<uint64_t> tmp;
  for (size_t i = 0;i<m_vHistData.size();i++) {
    tmp = m_vHistData[i];
    pStreamFile->WriteRAW((unsigned char*)&tmp[0], tmp.size()*sizeof(uint64_t));
  }

  return pStreamFile->GetPos() - iOffset;
}


uint64_t Histogram2DDataBlock::GetOffsetToNextBlock() const {
  return DataBlock::GetOffsetToNextBlock() + ComputeDataSize();
}

uint64_t Histogram2DDataBlock::ComputeDataSize() const {

  uint64_t ulElementCountX = uint64_t(m_vHistData.size());
  uint64_t ulElementCountY = uint64_t((ulElementCountX == 0) ? 0 : m_vHistData[0].size());

  return 1*sizeof(float) +                                // the m_fMaxGradMagnitude value
         2*sizeof(uint64_t) +                                // length of the vectors
         ulElementCountX*ulElementCountY*sizeof(uint64_t);  // the vectors themselves
}
