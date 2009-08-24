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
#include <boost/algorithm/minmax_element.hpp>
#include "UnbrickedDataset.h"
#include "UnbrickedDSMetadata.h"

namespace tuvok {

typedef std::vector<std::vector<UINT32> > hist2d;

UnbrickedDataset::UnbrickedDataset() : m_DataSize(0)
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
UINT64VECTOR3 UnbrickedDataset::GetBrickSize(const BrickKey&) const
{
  // arguments to GetBrickSize are garbage, only to satisfy interface
  /// \todo FIXME Datasets and Metadata use different BrickKeys (uint,uint
  /// versus uint,uint3vec)!
  UINT64VECTOR3 sz = GetInfo().GetBrickSize(
                       UnbrickedDSMetadata::BrickKey(0, UINT64VECTOR3(0,0,0))
                     );
  return sz;
}

bool UnbrickedDataset::GetBrick(const BrickKey&,
                                std::vector<unsigned char>& brick) const
{
  size_t bytes;

  const UnbrickedDSMetadata &metadata =
    dynamic_cast<const UnbrickedDSMetadata&>(this->GetInfo());
  switch(metadata.GetDataType()) {
    case UnbrickedDSMetadata::MDT_FLOAT:
      bytes = this->m_DataSize * sizeof(float);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), m_Scalarf.get(), bytes);
      break;
    case UnbrickedDSMetadata::MDT_BYTE:
      bytes = this->m_DataSize * sizeof(unsigned char);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), m_Scalarub.get(), bytes);
      break;
    default:
      T_ERROR("Unhandled data type.");
      break;
  }
  UINT64VECTOR3 sz = this->GetBrickSize();
  MESSAGE("Copied brick of size %u, dimensions %u %u %u",
          static_cast<UINT32>(bytes), static_cast<UINT32>(sz[0]),
          static_cast<UINT32>(sz[1]), static_cast<UINT32>(sz[2]));
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

namespace {

template<typename T> struct type2enum {};
template<> struct type2enum<float> {
  enum { value = UnbrickedDSMetadata::MDT_FLOAT };
};
template<> struct type2enum<const float> {
  enum { value = UnbrickedDSMetadata::MDT_FLOAT };
};
template<> struct type2enum<unsigned char> {
  enum { value = UnbrickedDSMetadata::MDT_BYTE };
};
template<> struct type2enum<const unsigned char> {
  enum { value = UnbrickedDSMetadata::MDT_BYTE };
};

static bool range_has_not_been_set(const Metadata &md)
{
  return md.GetRange().first == md.GetRange().second;
}

// Returns the range of the data; we can't set it here directly because we're
// not a friend of Metadata.
template<typename T>
std::pair<double,double>
set_metadata(const std::tr1::shared_ptr<T> data, size_t len,
             UnbrickedDSMetadata &metadata)
{
  int dtype = type2enum<T>::value;
  metadata.SetDataType(static_cast<UnbrickedDSMetadata::MD_Data_Type>(dtype));

  std::pair<double,double> mmax;
  if(range_has_not_been_set(metadata)) {
    std::pair<T*,T*> curmm = boost::minmax_element(data.get(), data.get()+len);
    mmax = std::make_pair(static_cast<double>(*curmm.first),
                          static_cast<double>(*curmm.second));
  }
  return mmax;
}

}; // anonymous namespace.

void UnbrickedDataset::SetData(const std::tr1::shared_ptr<float> data,
                               size_t len)
{
  UnbrickedDSMetadata &metadata =
    dynamic_cast<UnbrickedDSMetadata&>(this->GetInfo());

  this->m_Scalarf = data;
  this->m_DataSize = len;
  std::pair<double,double> mmax = set_metadata(this->m_Scalarf, len, metadata);

  if(range_has_not_been_set(metadata)) {
    metadata.SetRange(mmax);
  }
  Recalculate1DHistogram();
}

void UnbrickedDataset::SetData(const std::tr1::shared_ptr<unsigned char> data,
                               size_t len)
{
  UnbrickedDSMetadata &metadata =
    dynamic_cast<UnbrickedDSMetadata&>(this->GetInfo());
  this->m_Scalarub = data;
  this->m_DataSize = len;
  std::pair<double,double> mmax = set_metadata(this->m_Scalarub, len, metadata);

  if(range_has_not_been_set(metadata)) {
    metadata.SetRange(mmax);
  }
  Recalculate1DHistogram();
}

void UnbrickedDataset::SetGradientMagnitude(float *gmn, size_t len)
{
  m_vGradientMagnitude.resize(len);
  std::memcpy(&m_vGradientMagnitude.at(0), gmn, sizeof(float) * len);
}

void UnbrickedDataset::Recalculate1DHistogram()
{
  if(m_pHist1D) { delete m_pHist1D; }
  m_pHist1D = new Histogram1D(m_DataSize);
  std::fill(m_pHist1D->GetDataPointer(),
            m_pHist1D->GetDataPointer()+m_pHist1D->GetSize(), 0);

  const UnbrickedDSMetadata &metadata =
    dynamic_cast<const UnbrickedDSMetadata&>(this->GetInfo());
  switch(metadata.GetDataType()) {
    case UnbrickedDSMetadata::MDT_FLOAT: {
      const float *data = m_Scalarf.get();
      for(size_t i=0; i < m_DataSize; ++i) {
        m_pHist1D->Set(i, data[i]);
      }
      break;
    }
    case UnbrickedDSMetadata::MDT_BYTE: {
      const unsigned char *data = m_Scalarub.get();
      for(size_t i=0; i < m_DataSize; ++i) {
        m_pHist1D->Set(i, data[i]);
      }
      break;
    }
    default:
      T_ERROR("Unhandled data type");
      break;
  }

}

}; //namespace tuvok
