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
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include "UnbrickedDataset.h"
#include "UnbrickedDSMetadata.h"

namespace tuvok {

typedef std::vector<std::vector<UINT32> > hist2d;

UnbrickedDataset::UnbrickedDataset()
{
  // setup some default histograms.
  // default value is 1, since the `FilledSize' ignores 0-valued elements, so
  // other code would think a histogram filled with 0's is empty.
  std::vector<UINT32> h1d(8,1);
  hist2d h2d;
  h2d.resize(256);
  for(hist2d::iterator iter = h2d.begin(); iter < h2d.end(); ++iter) {
    iter->resize(256,1);
  }
  SetHistogram(h1d);
  SetHistogram(h2d);
}
UnbrickedDataset::~UnbrickedDataset() {}

// There's only one brick!  Ignore the key they gave us, just return the domain
// size.
UINTVECTOR3 UnbrickedDataset::GetBrickSize(const BrickKey&) const
{
  // arguments to GetBrickSize are garbage, only to satisfy interface
  /// \todo FIXME Datasets and Metadata use different BrickKeys (uint,uint
  /// versus uint,uint3vec)!
  UINT64VECTOR3 sz = GetInfo().GetBrickSize(
                       UnbrickedDSMetadata::BrickKey(0, UINT64VECTOR3(0,0,0))
                     );
  return UINTVECTOR3(sz[0],sz[1],sz[2]);
}

bool UnbrickedDataset::GetBrick(const BrickKey&, unsigned char **brick) const
{
  if(*brick == NULL) {
    *brick = new unsigned char[m_vScalar.size()];
  }
  UINTVECTOR3 sz = this->GetBrickSize();

  MESSAGE("Copying brick of size %zu, dimensions %lu %lu %lu...",
          m_vScalar.size(), sz[0], sz[1], sz[2]);
  std::memcpy(*brick, &m_vScalar.at(0), m_vScalar.size());
  return true;
}

float UnbrickedDataset::MaxGradientMagnitude() const
{
  return *std::max_element(m_vGradientMagnitude.begin(),
                           m_vGradientMagnitude.end());
}

void UnbrickedDataset::SetHistogram(const std::vector<UINT32>& hist)
{
  if(m_pHist1D) { delete m_pHist1D; }
  m_pHist1D = new Histogram1D(hist.size());
  std::memcpy(m_pHist1D->GetDataPointer(), &(hist.at(0)),
              sizeof(UINT32)*hist.size());
}

void UnbrickedDataset::SetHistogram(const std::vector<std::vector<UINT32> >& hist)
{
  if(m_pHist2D) { delete m_pHist2D; }
  // assume the 2D histogram is square: hist[0].size() == hist[1].size() == ...
  const VECTOR2<size_t> sz(hist.size(), hist[0].size());
  m_pHist2D = new Histogram2D(sz);

  UINT32 *data = m_pHist2D->GetDataPointer();
  for(hist2d::const_iterator iter = hist.begin(); iter < hist.end(); ++iter) {
    std::memcpy(data, &(iter->at(0)), sizeof(UINT32)*iter->size());
    data += iter->size();
  }
}

void UnbrickedDataset::SetData(float *data, size_t len)
{
  m_vScalar.resize(len*sizeof(float));
  std::memcpy(&m_vScalar.at(0), data, sizeof(float) * len);

  Recalculate1DHistogram();
  dynamic_cast<UnbrickedDSMetadata&>(this->GetInfo()).SetDataType(
    UnbrickedDSMetadata::MDT_FLOAT
  );
}

void UnbrickedDataset::SetData(unsigned char *data, size_t len)
{
  m_vScalar.resize(len);
  std::memcpy(&m_vScalar.at(0), data, len);

  Recalculate1DHistogram();
  dynamic_cast<UnbrickedDSMetadata&>(this->GetInfo()).SetDataType(
    UnbrickedDSMetadata::MDT_BYTE
  );
}

void UnbrickedDataset::SetGradientMagnitude(float *gmn, size_t len)
{
  m_vGradientMagnitude.resize(len);
  std::memcpy(&m_vGradientMagnitude.at(0), gmn, sizeof(float) * len);
}

void UnbrickedDataset::Recalculate1DHistogram()
{
  if(m_pHist1D) { delete m_pHist1D; }
  m_pHist1D = new Histogram1D(m_vScalar.size());
  std::fill(m_pHist1D->GetDataPointer(),
            m_pHist1D->GetDataPointer()+m_pHist1D->GetSize(), 0);
  for(size_t i=0; i < m_vScalar.size(); ++i) {
    m_pHist1D->Set(i, m_vScalar[i]);
  }
}

}; //namespace tuvok
