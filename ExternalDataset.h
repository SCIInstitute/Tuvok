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
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#pragma once

#ifndef TUVOK_EXTERNAL_DATASET_H
#define TUVOK_EXTERNAL_DATASET_H

#include <StdTuvokDefines.h>

#ifdef DETECTED_OS_WINDOWS
# include <functional>
# include <memory>
# include <unordered_map>
#else
# include <tr1/functional>
# include <tr1/memory>
# include <tr1/unordered_map>
#endif
#include "BrickedDataset.h"
#include "VariantArray.h"

namespace tuvok {

/** An external dataset is a bricked dataset which comes from a source
 * which is not under Tuvok's control.  Many existing applications generate
 * and store data their own way, and simply want Tuvok to render it.  This
 * provides a mechanism for Tuvok to share ownership with the client
 * applications' data. */
class ExternalDataset : public BrickedDataset {
public:
  ExternalDataset();
  virtual ~ExternalDataset();

  virtual float MaxGradientMagnitude() const;

  virtual UINTVECTOR3 GetBrickVoxelCounts(const BrickKey&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<unsigned char>&) const;

  void SetMetadata(Metadata * const md) {
    this->m_pVolumeDatasetInfo = md;
  }

  /// Uploads external histograms.  One will be calculated implicitly if not
  /// given.  To prevent this (potentially extra) work, upload the histogram
  /// *before* the data.
  ///@{
  void SetHistogram(const std::vector<UINT32>&);
  void SetHistogram(const std::vector<std::vector<UINT32> >&);
  ///@}

  /// Adds a brick to the dataset.  The first two parameters give the data
  /// itself.  The next three give the dimension of the brick: the number of
  /// data points in each dimension.  Data are assumed to be centered on the
  /// nodes of a cartesian grid.  Finally, the last three parameters give the
  /// index of this brick in the overall dataset, with (0,0,0) being the
  /// leftmost, bottom, closest brick, and (NX, NY, NZ) being the rightmost,
  /// topmost, farthest brick.
  ///@{
  void AddBrick(const BrickKey&, const BrickMD&,
                const std::tr1::shared_ptr<float>, size_t len,
                float fMin, float fMax);
  void AddBrick(const BrickKey&, const BrickMD&,
                const std::tr1::shared_ptr<unsigned char>, size_t len,
                unsigned char ubMin, unsigned char ubMax);
  ///@}
  /// Updates the data within an existing brick.
  ///@{
  void UpdateData(const BrickKey&, const std::tr1::shared_ptr<float>,
                  size_t len);
  void UpdateData(const BrickKey&, const std::tr1::shared_ptr<unsigned char>,
                  size_t len);
  ///@}

  /// Thrown when a brick lookup fails.
  struct BrickNotFound : public std::exception {
    BrickNotFound(const char *m) : msg(m) {}
    virtual const char *what() const throw() { return msg; }
    private: const char *msg;
  };
                 
  /// Important for correct 2D transfer function rendering.
  void SetGradientMagnitudeRange(float, float);

  /// Hacks for BrickedExternalMetadata.
  ///@{
  UINT64VECTOR3 GetEffectiveBrickSize(const BrickKey &) const;
  ///@}

protected:
  /// Should the data change and the client isn't going to supply a histogram,
  /// we should supply one ourself.
  void Recalculate1DHistogram();

private:
  typedef std::tr1::unordered_map<BrickKey, VariantArray, BKeyHash> DataTable;
  DataTable                           m_Data;
  float                               m_fMaxMagnitude;
};

}; // namespace tuvok

#endif // TUVOK_EXTERNAL_DATASET_H
