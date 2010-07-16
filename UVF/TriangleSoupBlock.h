#pragma once

#ifndef UVF_TRIANGLE_SOUP_BLOCK_H
#define UVF_TRIANGLE_SOUP_BLOCK_H

#ifdef _MSC_VER
# include <array>
#else
# include <tr1/array>
#endif
#include <string>
#include <vector>
#include "DataBlock.h"

class AbstrDebugOut;

class TriangleSoupBlock : public DataBlock {
public:
  TriangleSoupBlock();
  TriangleSoupBlock(const TriangleSoupBlock &other);
  TriangleSoupBlock(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian);
  virtual TriangleSoupBlock& operator=(const TriangleSoupBlock& other);
  virtual ~TriangleSoupBlock();

  virtual bool Verify(UINT64 iSizeofData, std::string* pstrProblem = NULL) const;
  virtual UINT64 ComputeDataSize() const;

  // triangles:   3 coords      each coord is 3 floats
  std::vector<std::tr1::array<std::tr1::array<float, 3>, 3> > triangles;

protected:
  virtual UINT64 GetHeaderFromFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian);
  virtual UINT64 CopyToFile(LargeRAWFile* pStreamFile, UINT64 iOffset, bool bIsBigEndian, bool bIsLastBlock);
  virtual UINT64 GetOffsetToNextBlock() const;

  virtual DataBlock* Clone();

  friend class UVF;
};
#endif // UVF_TRIANGLE_SOUP_BLOCK_H
