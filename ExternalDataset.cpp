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
#include "ExternalDataset.h"
#include "ExternalMetadata.h"

namespace tuvok {

typedef std::vector<std::vector<UINT32> > hist2d;

ExternalDataset::ExternalDataset()
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
ExternalDataset::~ExternalDataset() {}

UINTVECTOR3 ExternalDataset::GetBrickVoxelCounts(const BrickKey& bk) const
{
  MESSAGE("looking up brick with key: (%u, %u)",
          static_cast<unsigned>(bk.first),
          static_cast<unsigned>(bk.second));
  BrickTable::const_iterator iter = this->bricks.find(bk);
  if(iter == this->bricks.end()) {
    // HACK!
    char *k = static_cast<char*>(malloc(1024)); // leaked, oh well.
#ifdef DETECTED_OS_WINDOWS
  _snprintf_s(k, 1024,1024, "GetBrickSize: no brick w/ key (%u, %u)",
             static_cast<unsigned>(bk.first),
             static_cast<unsigned>(bk.second));
#else
    snprintf(k, 1024, "GetBrickSize: no brick w/ key (%u, %u)",
             static_cast<unsigned>(bk.first),
             static_cast<unsigned>(bk.second));
#endif
    throw BrickNotFound(k);
  }
  return UINTVECTOR3(iter->second.n_voxels[0],
                     iter->second.n_voxels[1],
                     iter->second.n_voxels[2]);
}

bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<unsigned char>& brick) const
{
  size_t bytes;

  DataTable::const_iterator brick_data = this->m_Data.find(bk);
  assert(brick_data != this->m_Data.end());
  const VariantArray &varray = brick_data->second;

  const ExternalMetadata &metadata =
    dynamic_cast<const ExternalMetadata&>(this->GetInfo());
  switch(metadata.GetDataType()) {
    case ExternalMetadata::MDT_FLOAT:
      bytes = this->m_Data.size() * sizeof(float);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), varray.getf(), bytes);
      break;
    case ExternalMetadata::MDT_BYTE:
      bytes = this->m_Data.size() * sizeof(unsigned char);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), varray.getub(), bytes);
      break;
    default:
      T_ERROR("Unhandled data type.");
      bytes = 0;
      break;
  }
  UINTVECTOR3 sz = this->GetBrickVoxelCounts(bk);
  MESSAGE("Copied brick of size %u, dimensions %u %u %u", bytes,
          sz[0], sz[1], sz[2]);
  return true;
}

float ExternalDataset::MaxGradientMagnitude() const
{
  return m_fMaxMagnitude;
}

void ExternalDataset::SetHistogram(const std::vector<UINT32>& hist)
{
  if(m_pHist1D) { delete m_pHist1D; }
  m_pHist1D = new Histogram1D(hist.size());
  std::memcpy(m_pHist1D->GetDataPointer(), &(hist.at(0)),
              sizeof(UINT32)*hist.size());
}

void ExternalDataset::SetHistogram(const std::vector<std::vector<UINT32> >& hist)
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
  enum { value = ExternalMetadata::MDT_FLOAT };
};
template<> struct type2enum<const float> {
  enum { value = ExternalMetadata::MDT_FLOAT };
};
template<> struct type2enum<unsigned char> {
  enum { value = ExternalMetadata::MDT_BYTE };
};
template<> struct type2enum<const unsigned char> {
  enum { value = ExternalMetadata::MDT_BYTE };
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
             ExternalMetadata &metadata)
{
  int dtype = type2enum<T>::value;
  metadata.SetDataType(static_cast<ExternalMetadata::MD_Data_Type>(dtype));

  std::pair<double,double> mmax;
  if(range_has_not_been_set(metadata)) {
    std::pair<T*,T*> curmm = boost::minmax_element(data.get(), data.get()+len);
    mmax = std::make_pair(static_cast<double>(*curmm.first),
                          static_cast<double>(*curmm.second));
  }
  return mmax;
}

template<typename T> void
update_metadata(ExternalMetadata &md, const BrickMD& b,
                T brick_min, T brick_max)
{
  { // type
    int dtype = type2enum<T>::value;
    md.SetDataType(static_cast<ExternalMetadata::MD_Data_Type>(dtype));
  }
  { // data range
    std::pair<T,T> minmax = md.GetRange();
    minmax.first = std::min(minmax.first, brick_min);
    minmax.second = std::max(minmax.second, brick_max);
    md.SetRange(minmax);
  }
  { // max brick size
    UINTVECTOR3 brick_sz = md.GetMaxBrickSize();
    brick_sz[0] = std::max(static_cast<UINT>(brick_sz[0]), b.n_voxels[0]);
    brick_sz[1] = std::max(static_cast<UINT>(brick_sz[1]), b.n_voxels[1]);
    brick_sz[2] = std::max(static_cast<UINT>(brick_sz[2]), b.n_voxels[2]);
    md.SetMaxBrickSize(brick_sz);
  }
}

}; // anonymous namespace.

void ExternalDataset::AddBrick(const BrickKey& bk, const BrickMD& md,
                               const std::tr1::shared_ptr<float> data,
                               size_t len, float fMin, float fMax)
{
  BrickedDataset::AddBrick(bk, md);
  VariantArray varr;
  varr.set(data, len);
  this->m_Data.insert(std::pair<BrickKey, VariantArray>(bk, varr));
  MESSAGE("added brick with key: (%u, %u)",
          static_cast<unsigned>(bk.first),
          static_cast<unsigned>(bk.second));

  ExternalMetadata& metadata = dynamic_cast<ExternalMetadata&>(this->GetInfo());
  update_metadata<float>(metadata, md, fMin, fMax);

  Recalculate1DHistogram();
}
void ExternalDataset::AddBrick(const BrickKey& bk, const BrickMD& md,
                               const std::tr1::shared_ptr<unsigned char> data,
                               size_t len,
                               unsigned char ubmin, unsigned char ubmax)
{
  BrickedDataset::AddBrick(bk, md);
  VariantArray varr;
  varr.set(data, len);
  this->m_Data.insert(std::pair<BrickKey, VariantArray>(bk, varr));
  MESSAGE("added brick with key: (%u, %u)",
          static_cast<unsigned>(bk.first),
          static_cast<unsigned>(bk.second));

  ExternalMetadata& metadata = dynamic_cast<ExternalMetadata&>(this->GetInfo());
  update_metadata<unsigned char>(metadata, md, ubmin, ubmax);

  Recalculate1DHistogram();
}

void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::tr1::shared_ptr<float> data,
                                 size_t len)
{
  DataTable::iterator iter = this->m_Data.find(bk);
  if(iter == this->m_Data.end()) {
    throw BrickNotFound("You must add the brick first!");
  }
  iter->second.set(data, len);
}
void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::tr1::shared_ptr<unsigned char> data,
                                 size_t len)
{
  DataTable::iterator iter = this->m_Data.find(bk);
  if(iter == this->m_Data.end()) {
    throw BrickNotFound("You must add the brick first!");
  }
  iter->second.set(data, len);
}

void ExternalDataset::SetGradientMagnitudeRange(float, float high)
{
  // Minimum value is currently ignored; might be needed later.
  m_fMaxMagnitude = high;
}

void ExternalDataset::Recalculate1DHistogram()
{
  // We call this way too much; we can't reasonably iterate over every brick
  // every time this gets called just to calculate a histogram.  See if we can
  // get by without the histogram for now; otherwise I guess add a setter so
  // that client apps can pass this information to us, or perhaps compute it in
  // pieces, every AddBrick call.
#if 0
  if(m_pHist1D) { delete m_pHist1D; }
  m_pHist1D = new Histogram1D(m_DataSize);
  std::fill(m_pHist1D->GetDataPointer(),
            m_pHist1D->GetDataPointer()+m_pHist1D->GetSize(), 0);

  const ExternalMetadata &metadata =
    dynamic_cast<const ExternalMetadata&>(this->GetInfo());
  switch(metadata.GetDataType()) {
    case ExternalMetadata::MDT_FLOAT: {
      const float *data = m_Data.getf();
      for(size_t i=0; i < m_DataSize; ++i) {
        m_pHist1D->Set(i, data[i]);
      }
      break;
    }
    case ExternalMetadata::MDT_BYTE: {
      const unsigned char *data = m_Data.getub();
      for(size_t i=0; i < m_DataSize; ++i) {
        m_pHist1D->Set(i, data[i]);
      }
      break;
    }
    default:
      T_ERROR("Unhandled data type");
      break;
  }
#else
  T_ERROR("histogram calculation is bogus.");
  //assert(1 == 0);
#endif
}

} //namespace tuvok
