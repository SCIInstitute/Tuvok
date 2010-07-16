#include <sstream>

#include "TriangleSoupBlock.h"
#include "UVF.h"

using namespace std;
using namespace UVFTables;

TriangleSoupBlock::TriangleSoupBlock() : DataBlock() {}

TriangleSoupBlock::TriangleSoupBlock(const TriangleSoupBlock& other) :
  DataBlock(other) {}

TriangleSoupBlock::TriangleSoupBlock(LargeRAWFile* pStreamFile, UINT64 iOffset,
                                     bool bIsBigEndian) :
  DataBlock(pStreamFile, iOffset, bIsBigEndian)
{
  GetHeaderFromFile(pStreamFile, iOffset, bIsBigEndian);
}

TriangleSoupBlock::~TriangleSoupBlock() { triangles.clear(); }

TriangleSoupBlock& TriangleSoupBlock::operator=(const TriangleSoupBlock& other)
{
  strBlockID = other.strBlockID;
  ulBlockSemantics = other.ulBlockSemantics;
  triangles = other.triangles;

  return *this;
}

bool TriangleSoupBlock::Verify(UINT64 iSizeofData, std::string* pstrProblem) const
{
  UINT64 iCorrectSize = ComputeDataSize();
  bool bResult = iCorrectSize == iSizeofData;

  if (!bResult && pstrProblem != NULL)  {
    stringstream s;
    s << "TriangleSoupBlock::Verify: size mismatch. Should be " << iCorrectSize << " but parameter was " << iSizeofData << ".";
    *pstrProblem = s.str();
  }

  return bResult;
}

UINT64 TriangleSoupBlock::ComputeDataSize() const
{
  return sizeof(UINT64) +  // n_triangles
         sizeof(float) * 3 * this->triangles.size(); // triangles.
}

DataBlock* TriangleSoupBlock::Clone()
{
  return new TriangleSoupBlock(*this);
}

UINT64 TriangleSoupBlock::GetHeaderFromFile(LargeRAWFile* stream,
                                            UINT64 offset, bool big_endian)
{
  UINT64 start = offset + DataBlock::GetHeaderFromFile(stream, offset,
                                                       big_endian);
  stream->SeekPos(start);

  UINT64 n_triangles;
  stream->ReadData(n_triangles, big_endian);
  this->triangles.resize(n_triangles);

  // .. and now we read the data, too, despite the method name.

  // This is actually UB.  But a standard implementor would have to
  // really hate us to break it (it relies on the fact that '&x ==
  // &x[0]' for an 'array<float, 3> x').  That is, an array consists of
  // nothing more than its data.
  // We should probably change the representation to just a ptr if we
  // ever decide to play nicely; the corresponding loops would be slow.
  stream->ReadData(this->triangles, n_triangles, big_endian);

  return stream->GetPos() - offset;
}

UINT64 TriangleSoupBlock::CopyToFile(LargeRAWFile* pStreamFile, UINT64 iOffset,
                                     bool bIsBigEndian, bool bIsLastBlock)
{
  CopyHeaderToFile(pStreamFile, iOffset, bIsBigEndian, bIsLastBlock);
  UINT64 sz = static_cast<UINT64>(this->triangles.size());
  pStreamFile->WriteData(sz, bIsBigEndian);

  // see GetHeader comment.
  pStreamFile->WriteData(this->triangles, bIsBigEndian);
  return pStreamFile->GetPos() - iOffset;
}

UINT64 TriangleSoupBlock::GetOffsetToNextBlock() const
{
  return DataBlock::GetOffsetToNextBlock() + ComputeDataSize();
}
