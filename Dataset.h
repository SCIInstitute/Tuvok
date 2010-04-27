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
#include "Basics/tr1.h"

#define MAX_TFSIZE 4096

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

  /// Remove all cached bricks / brick metadata.
  virtual void Clear() = 0;

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
  virtual BrickTable::size_type GetBrickCount(size_t lod, size_t ts) const = 0;

  virtual bool BrickIsFirstInDimension(size_t, const BrickKey&) const = 0;
  virtual bool BrickIsLastInDimension(size_t, const BrickKey&) const = 0;

  void SetRescaleFactors(const DOUBLEVECTOR3&);
  DOUBLEVECTOR3 GetRescaleFactors() const;
  virtual bool SaveRescaleFactors() {return false;}

  virtual UINT64 GetLODLevelCount() const = 0;
  /// @todo FIXME, should be pure virtual && overridden in derived
  virtual UINT64 GetNumberOfTimesteps() const { return 1; }
  virtual UINT64VECTOR3 GetDomainSize(const size_t lod=0,
                                      const size_t ts=0) const = 0;
  DOUBLEVECTOR3 GetScale() const {return m_DomainScale * m_UserScale;}
  virtual UINTVECTOR3 GetBrickOverlapSize() const = 0;
  /// @return the number of voxels for the given brick, per dimension, taking
  ///         into account any brick overlaps.
  virtual UINT64VECTOR3 GetEffectiveBrickSize(const BrickKey &) const = 0;

  virtual UINT64 GetBitWidth() const = 0;
  virtual UINT64 GetComponentCount() const = 0;
  virtual bool GetIsSigned() const = 0;
  virtual bool GetIsFloat() const = 0;
  virtual bool IsSameEndianness() const = 0;
  virtual std::pair<double,double> GetRange() = 0;
  virtual const std::vector< std::pair < std::string, std::string > > GetMetadata() const {
    std::vector< std::pair < std::string, std::string > > v;
    return v;
  }

  /// Acceleration queries.
  virtual bool ContainsData(const BrickKey&, double /*isoval*/) const {return true;}
  virtual bool ContainsData(const BrickKey&, double /*fMin*/, double /*fMax*/) const {return true;}
  virtual bool ContainsData(const BrickKey&, double /*fMin*/, double /*fMax*/, double /*fMinGradient*/, double /*fMaxGradient*/) const {return true;}

  /// unimplemented!
  virtual bool Export(UINT64 iLODLevel, const std::string& targetFilename,
                      bool bAppend,
                      bool (*brickFunc)(LargeRAWFile* pSourceFile,
                                        const std::vector<UINT64> vBrickSize,
                                        const std::vector<UINT64> vBrickOffset,
                                        void* pUserContext) = NULL,
                      void *pUserContext = NULL,
                      UINT64 iOverlap=0) const;

  /// A user-visible name for your format.  This might get displayed in UI
  /// elements; e.g. the GUI might ask if the user wants to use the "Name()
  /// reader" to open a particular file.
  virtual const char* Name() const { return "Generic"; }
  /// Virtual constructor.
  virtual Dataset* Create(const std::string&, UINT64, bool) const=0;

protected:
  Histogram1D*       m_pHist1D;
  Histogram2D*       m_pHist2D;

  DOUBLEVECTOR3      m_UserScale;
  DOUBLEVECTOR3      m_DomainScale;
};

};

#endif // TUVOK_DATASET_H
