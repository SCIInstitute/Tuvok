/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2009 Scientific Computing and Imaging Institute,
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
  \file    BrickedDataset.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once
#ifndef TUVOK_BRICKED_DATASET_H
#define TUVOK_BRICKED_DATASET_H

#include "Basics/MinMaxBlock.h"
#include "Dataset.h"

namespace tuvok {

/// Base for data sets which split their data into blocks.  All bricks are kept
/// into an internal table; derived classes should add to it via AddBrick.
/// This class then handles the query of much meta data.
class BrickedDataset : public Dataset {
public:
  BrickedDataset();
  virtual ~BrickedDataset();

  /// Looks up the spatial range of a brick.
  virtual FLOATVECTOR3 GetBrickExtents(const BrickKey &) const;
  /// number of voxels in the given brick.
  virtual UINTVECTOR3 GetBrickVoxelCounts(const BrickKey&) const;

  /// @return an iterator that can be used to visit every brick in the dataset.
  virtual BrickTable::const_iterator BricksBegin() const;
  virtual BrickTable::const_iterator BricksEnd() const;
  /// @return the number of bricks at the given LOD + timestep
  virtual BrickTable::size_type GetBrickCount(size_t lod, size_t ts) const;
  virtual size_t GetLargestSingleBrickLOD(size_t ts) const;
  virtual uint64_t GetTotalBrickCount() const;

  virtual const BrickMD& GetBrickMetadata(const BrickKey&) const;

  /// @returns the bricking size used for this decomposition
  virtual UINTVECTOR3 GetMaxBrickSize() const;
  /// @returns the largest brick which *actually* got used when splitting up
  /// the data set.  This can be smaller than GetMaxBrickSize, when, for
  /// example, the domain is smaller than the brick size requested.
  virtual UINTVECTOR3 GetMaxUsedBrickSizes() const;
  /// @returns the min/max scalar and gradient values for the given brick.
  virtual tuvok::MinMaxBlock MaxMinForKey(const BrickKey&) const=0;

  virtual void Clear();

  /// It can be important to know whether the given brick is the first or last
  /// along any particular axis.  As an example, there's 0 brick overlap for a
  /// border brick.
  ///@{
  /// @return true if the brick is the minimum brick in the given dimension.
  virtual bool BrickIsFirstInDimension(size_t, const BrickKey&) const;
  /// @return true if the brick is the maximum brick in the given dimension.
  virtual bool BrickIsLastInDimension(size_t, const BrickKey&) const;
  ///@}

protected:
  /// gives a hint to this object that we'll have 'n' bricks in the end.
  virtual void NBricksHint(size_t n);

  /// Adds a brick to the dataset.
  virtual void AddBrick(const BrickKey&, const BrickMD&);

protected:
  BrickTable bricks;
};

} // namespace tuvok

#endif // TUVOK_BRICKED_DATASET_H
