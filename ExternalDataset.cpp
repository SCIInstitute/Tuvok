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
#include <Controller/Controller.h>

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
#ifdef TR1_NOT_CONST_CORRECT
  ExternalDataset* cthis = const_cast<ExternalDataset*>(this);
  BrickTable::const_iterator iter = cthis->bricks.find(bk);
#else
  BrickTable::const_iterator iter = this->bricks.find(bk);
#endif
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
  MESSAGE("voxels: %ux%ux%u", iter->second.n_voxels[0],
          iter->second.n_voxels[1], iter->second.n_voxels[2]);
  return iter->second.n_voxels;
}

bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<unsigned char>& brick) const
{
  size_t bytes=0;

#ifdef TR1_NOT_CONST_CORRECT
  ExternalDataset* cthis = const_cast<ExternalDataset*>(this);
  DataTable::const_iterator brick_data = cthis->m_Data.find(bk);
#else
  DataTable::const_iterator brick_data = this->m_Data.find(bk);
#endif
  assert(brick_data != this->m_Data.end());
  const VariantArray &varray = brick_data->second;

  switch(brick_data->second.type()) {
    case VariantArray::DT_FLOAT:
      bytes = brick_data->second.size() * sizeof(float);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), varray.getf(), bytes);
      break;
    case VariantArray::DT_UBYTE:
      bytes = brick_data->second.size() * sizeof(unsigned char);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), varray.getub(), bytes);
      break;
    case VariantArray::DT_SHORT:
      bytes = brick_data->second.size() * sizeof(short);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), varray.gets(), bytes);
      break;
    case VariantArray::DT_USHORT:
      bytes = brick_data->second.size() * sizeof(unsigned short);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), varray.getus(), bytes);
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

template<typename T> void
update_metadata(ExternalDataset &ds, T brick_min, T brick_max)
{
  { // data range
    std::pair<double,double> minmax = ds.GetRange();
    // We only store ranges as doubles, even if the underlying dataset is
    // fixed point.
    minmax.first = std::min(minmax.first, static_cast<double>(brick_min));
    minmax.second = std::max(minmax.second, static_cast<double>(brick_max));
    ds.SetRange(minmax);
    MESSAGE("Range: [%g - %g]", minmax.first, minmax.second);
  }
}

template<typename T> void
add_brick(ExternalDataset &ds, const BrickKey& bk,
          const std::tr1::shared_ptr<T>& data, size_t len,
          ExternalDataset::DataTable& table, T brick_min, T brick_max)
{
  VariantArray varr;
  varr.set(data, len);
  table.insert(std::pair<BrickKey, VariantArray>(bk, varr));
  MESSAGE("added %u-elem brick with key: (%u, %u)", static_cast<unsigned>(len),
          static_cast<unsigned>(bk.first), static_cast<unsigned>(bk.second));
  update_metadata(ds, brick_min, brick_max);
}

}; // anonymous namespace.


void ExternalDataset::AddBrick(const BrickKey& bk, const BrickMD& md,
                               const std::tr1::shared_ptr<float> data,
                               size_t len, float fMin, float fMax)
{
  BrickedDataset::AddBrick(bk, md);
  add_brick(*this, bk, data, len, this->m_Data, fMin, fMax);
  Recalculate1DHistogram();
}

void ExternalDataset::AddBrick(const BrickKey& bk, const BrickMD& md,
                               const std::tr1::shared_ptr<unsigned char> data,
                               size_t len,
                               unsigned char ubmin, unsigned char ubmax)
{
  BrickedDataset::AddBrick(bk, md);
  add_brick(*this, bk, data, len, this->m_Data, ubmin, ubmax);
  Recalculate1DHistogram();
}

void ExternalDataset::AddBrick(const BrickKey& bk, const BrickMD& md,
                               const std::tr1::shared_ptr<short> data,
                               size_t len, short sMin, short sMax)
{
  BrickedDataset::AddBrick(bk, md);
  add_brick(*this, bk, data, len, this->m_Data, sMin, sMax);
  Recalculate1DHistogram();
}

void ExternalDataset::AddBrick(const BrickKey& bk, const BrickMD& md,
                               const std::tr1::shared_ptr<unsigned short> data,
                               size_t len,
                               unsigned short usMin, unsigned short usMax)
{
  BrickedDataset::AddBrick(bk, md);
  add_brick(*this, bk, data, len, this->m_Data, usMin, usMax);
  Recalculate1DHistogram();
}


namespace {
  template<typename T> void
  update_data(ExternalDataset::DataTable& table, const BrickKey& bk,
              const std::tr1::shared_ptr<T> data, size_t len)
  {
    ExternalDataset::DataTable::iterator iter = table.find(bk);
    if(iter == table.end()) {
      throw ExternalDataset::BrickNotFound("You must add the brick first!");
    }
    iter->second.set(data, len);
  }
}; // anonymous namespace.

void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::tr1::shared_ptr<float> data,
                                 size_t len)
{
  update_data(this->m_Data, bk, data, len);
}
void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::tr1::shared_ptr<unsigned char> data,
                                 size_t len)
{
  update_data(this->m_Data, bk, data, len);
}
void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::tr1::shared_ptr<short> data,
                                 size_t len)
{
  update_data(this->m_Data, bk, data, len);
}
void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::tr1::shared_ptr<unsigned short> data,
                                 size_t len)
{
  update_data(this->m_Data, bk, data, len);
}

void ExternalDataset::Clear() {
    MESSAGE("Clearing brick data");
    this->m_Data.clear();
    BrickedDataset::Clear();
}

void ExternalDataset::SetGradientMagnitudeRange(float, float high)
{
  // Minimum value is currently ignored; might be needed later.
  m_fMaxMagnitude = high;
}

void ExternalDataset::SetRange(const std::pair<double, double>& range)
{
  m_DataRange = range;
}

std::pair<double,double> ExternalDataset::GetRange()
{
  return m_DataRange;
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

  switch(this->GetDataType()) {
    case VariantArray::DT_FLOAT: {
      const float *data = m_Data.getf();
      for(size_t i=0; i < m_DataSize; ++i) {
        m_pHist1D->Set(i, data[i]);
      }
      break;
    }
    case VariantArray::DT_UBYTE: {
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
