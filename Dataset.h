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
#include "boost/cstdint.hpp"
#include "boost/noncopyable.hpp"
#include "Basics/Grids.h"
#include "Basics/Vectors.h"
#include "Brick.h"
#include "Basics/tr1.h"
#include "Basics/Mesh.h"

#define MAX_TRANSFERFUNCTION_SIZE 4096

typedef Grid1D<UINT32> Histogram1D;
typedef Grid2D<UINT32> Histogram2D;
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

  const std::vector<Mesh*>& GetMeshes() const { return m_vpMeshList; }
  const Histogram1D& Get1DHistogram() const { return *m_pHist1D; }
  const Histogram2D& Get2DHistogram() const { return *m_pHist2D; }
  virtual float MaxGradientMagnitude() const = 0;

  /// Remove all cached bricks / brick metadata.
  virtual void Clear() = 0;

  virtual void AddBrick(const BrickKey&, const BrickMD&) = 0;
  /// Gets the number of voxels, per dimension.
  virtual UINTVECTOR3 GetBrickVoxelCounts(const BrickKey&) const = 0;
  /// World space extents.
  virtual FLOATVECTOR3 GetBrickExtents(const BrickKey &) const = 0;
  /// Data access
  ///@{
  virtual bool GetBrick(const BrickKey&, std::vector<boost::uint8_t>&) const=0;
  virtual bool GetBrick(const BrickKey&, std::vector<boost::int8_t>&) const = 0;
  virtual bool GetBrick(const BrickKey&, std::vector<boost::uint16_t>&) const=0;
  virtual bool GetBrick(const BrickKey&, std::vector<boost::int16_t>&) const=0;
  virtual bool GetBrick(const BrickKey&, std::vector<boost::uint32_t>&) const=0;
  virtual bool GetBrick(const BrickKey&, std::vector<boost::int32_t>&) const=0;
  virtual bool GetBrick(const BrickKey&, std::vector<float>&) const=0;
  virtual bool GetBrick(const BrickKey&, std::vector<double>&) const=0;
  ///@}
  virtual BrickTable::const_iterator BricksBegin() const = 0;
  virtual BrickTable::const_iterator BricksEnd() const = 0;
  /// @return the number of bricks in a given LoD + timestep.
  virtual BrickTable::size_type GetBrickCount(size_t lod, size_t ts) const = 0;

  virtual bool BrickIsFirstInDimension(size_t, const BrickKey&) const = 0;
  virtual bool BrickIsLastInDimension(size_t, const BrickKey&) const = 0;

  /// User rescaling factors.
  ///@{
  void SetRescaleFactors(const DOUBLEVECTOR3&);
  DOUBLEVECTOR3 GetRescaleFactors() const;
  /// If the underlying file format supports it, save the current scaling
  /// factors to the file.  The format should implicitly load and apply the
  /// scaling factors when opening the file.
  virtual bool SaveRescaleFactors() {return false;}
  ///@}

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
  virtual std::pair<double,double> GetRange() const = 0;
  virtual const std::vector< std::pair < std::string, std::string > > GetMetadata() const {
    std::vector< std::pair < std::string, std::string > > v;
    return v;
  }

  /// Acceleration queries.
  virtual bool ContainsData(const BrickKey&, double /*isoval*/) const {return true;}
  virtual bool ContainsData(const BrickKey&, double /*fMin*/, double /*fMax*/) const {return true;}
  virtual bool ContainsData(const BrickKey&, double /*fMin*/, double /*fMax*/, double /*fMinGradient*/, double /*fMaxGradient*/) const {return true;}

  /// unimplemented!  Override this if you want tools built on this IO layer
  /// to be able to create data in your format.
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
  std::vector<Mesh*> m_vpMeshList;

  DOUBLEVECTOR3      m_UserScale;
  DOUBLEVECTOR3      m_DomainScale;

  void DeleteMeshes();
};

};

#endif // TUVOK_DATASET_H
