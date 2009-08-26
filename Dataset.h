/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/
/**
  \file    Dataset.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once
#ifndef TUVOK_DATASET_H
#define TUVOK_DATASET_H

#include <StdTuvokDefines.h>

#include <cstdlib>
#include <utility>
#ifdef DETECTED_OS_WINDOWS
# include <functional>
# include <memory>
# include <unordered_map>
#else
# include <tr1/functional>
# include <tr1/memory>
# include <tr1/unordered_map>
#endif
#include "boost/noncopyable.hpp"
#include "Basics/Vectors.h"
#include "Brick.h"
#include "TransferFunction1D.h"
#include "TransferFunction2D.h"

// Apple ships a broken version of gcc's tr1 on 10.4; see gcc bug 23053.  Also
// note that the bug has been fixed for over 4 years.
#if __GNUC__ && (__GNUC__ == 4 && __GNUC_MINOR__ == 0)
# define TR1_NOT_CONST_CORRECT
#endif

class LargeRAWFile;

namespace tuvok {

class Metadata;

/// Abstract interface to a dataset.
/// noncopyable not because it wouldn't work, but because we might be holding a
/// lot of data -- copying would be prohibitively expensive.
class Dataset : public boost::noncopyable {

public:
  Dataset();
  virtual ~Dataset();

  const Histogram1D& Get1DHistogram() const { return *m_pHist1D; }
  const Histogram2D& Get2DHistogram() const { return *m_pHist2D; }
  virtual float MaxGradientMagnitude() const = 0;

  virtual void AddBrick(const BrickKey&, const BrickMD&) = 0;
  /// Gets the number of voxels, per dimension.
  // (temp note): was GetBrickSize, renaming to make it more obvious what
  // information it's retrieving, and to differentiate from 'effective' brick
  // size.
  virtual UINTVECTOR3 GetBrickVoxelCounts(const BrickKey&) const = 0;
  virtual FLOATVECTOR3 GetBrickExtents(const BrickKey &) const = 0;
  virtual bool GetBrick(const BrickKey&,
                        std::vector<unsigned char>&) const = 0;
  virtual BrickTable::const_iterator BricksBegin() const = 0;
  virtual BrickTable::const_iterator BricksEnd() const = 0;
  virtual BrickTable::size_type GetBrickCount(size_t lod) const = 0;

  virtual bool BrickIsFirstInDimension(size_t, const BrickKey&) const = 0;
  virtual bool BrickIsLastInDimension(size_t, const BrickKey&) const = 0;

  /// unimplemented!
  virtual bool Export(UINT64 iLODLevel, const std::string& targetFilename,
                      bool bAppend,
                      bool (*brickFunc)(LargeRAWFile* pSourceFile,
                                        const std::vector<UINT64> vBrickSize,
                                        const std::vector<UINT64> vBrickOffset,
                                        void* pUserContext) = NULL,
                      void *pUserContext = NULL,
                      UINT64 iOverlap=0) const;

  Metadata&       GetInfo() { return *m_pVolumeDatasetInfo; }
  const Metadata& GetInfo() const { return *m_pVolumeDatasetInfo; }

protected:
  Histogram1D*       m_pHist1D;
  Histogram2D*       m_pHist2D;
  Metadata*          m_pVolumeDatasetInfo;
};

};

#endif // TUVOK_DATASET_H
