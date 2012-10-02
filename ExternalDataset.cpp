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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ExternalDataset.h"
#include <Controller/Controller.h>

using namespace boost;

namespace tuvok {

typedef std::vector<std::vector<uint32_t>> hist2d;

ExternalDataset::ExternalDataset()
{
  // setup some default histograms.
  // default value is 1, since the `FilledSize' ignores 0-valued elements, so
  // other code would think a histogram filled with 0's is empty.
  std::vector<uint32_t> h1d(8,1);
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
  MESSAGE("looking up brick with key: (%u, %u, %u)",
          static_cast<unsigned>(std::get<0>(bk)),
          static_cast<unsigned>(std::get<1>(bk)),
          static_cast<unsigned>(std::get<2>(bk)));
#ifdef TR1_NOT_CONST_CORRECT
  ExternalDataset* cthis = const_cast<ExternalDataset*>(this);
  BrickTable::const_iterator iter = cthis->bricks.find(bk);
#else
  BrickTable::const_iterator iter = this->bricks.find(bk);
#endif
  if(iter == this->bricks.end()) {
    /// @todo FIXME HACK!
    char *k = static_cast<char*>(malloc(1024)); // leaked, oh well.
#ifdef DETECTED_OS_WINDOWS
  _snprintf_s(k, 1024,1024, "GetBrickSize: no brick w/ key (%u, %u, %u)",
              static_cast<unsigned>(std::get<0>(bk)),
              static_cast<unsigned>(std::get<1>(bk)),
              static_cast<unsigned>(std::get<2>(bk)));
#else
    snprintf(k, 1024, "GetBrickSize: no brick w/ key (%u, %u, %u)",
             static_cast<unsigned>(std::get<0>(bk)),
             static_cast<unsigned>(std::get<1>(bk)),
             static_cast<unsigned>(std::get<2>(bk)));
#endif
    throw BrickNotFound(k);
  }
  MESSAGE("voxels: %ux%ux%u", iter->second.n_voxels[0],
          iter->second.n_voxels[1], iter->second.n_voxels[2]);
  return iter->second.n_voxels;
}

ExternalDataset::DataTable::const_iterator
ExternalDataset::Lookup(const BrickKey& k) const
{
#ifdef TR1_NOT_CONST_CORRECT
  ExternalDataset* cthis = const_cast<ExternalDataset*>(this);
  return cthis->m_Data.find(k);
#else
  return this->m_Data.find(k);
#endif
}

bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<uint8_t>& brick) const
{
  size_t bytes=0;

  DataTable::const_iterator brick_data = this->Lookup(bk);
  assert(brick_data != this->m_Data.end());
  const VariantArray &varray = brick_data->second;

  switch(brick_data->second.type()) {
    case VariantArray::DT_DOUBLE:
      bytes = brick_data->second.size() * sizeof(double);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), varray.getd(), bytes);
      break;
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
    case VariantArray::DT_BYTE:
      bytes = brick_data->second.size() * sizeof(char);
      brick.resize(bytes);
      std::memcpy(&brick.at(0), varray.getb(), bytes);
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
  MESSAGE("Copied brick of size %u, dimensions %u %u %u", uint32_t(bytes),
          sz[0], sz[1], sz[2]);
  return true;
}

bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<int8_t>& brick) const
{
  DataTable::const_iterator brick_data = this->Lookup(bk);
  assert(this->m_Data.end() != brick_data);
  const VariantArray& varray = brick_data->second;
  brick.resize(varray.size());

  assert(varray.type() == VariantArray::DT_BYTE);
  std::memcpy(&brick[0], varray.getb(), varray.size()*sizeof(int8_t));
  return true;
}

bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<uint16_t>& brick) const
{
  DataTable::const_iterator brick_data = this->Lookup(bk);
  assert(this->m_Data.end() != brick_data);
  const VariantArray& varray = brick_data->second;
  brick.resize(varray.size());

  assert(varray.type() == VariantArray::DT_USHORT);
  std::memcpy(&brick[0], varray.getus(), varray.size()*sizeof(uint16_t));
  return true;
}
bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<int16_t>& brick) const
{
  DataTable::const_iterator brick_data = this->Lookup(bk);
  assert(this->m_Data.end() != brick_data);
  const VariantArray& varray = brick_data->second;
  brick.resize(varray.size());

  assert(varray.type() == VariantArray::DT_SHORT);
  std::memcpy(&brick[0], varray.gets(), varray.size()*sizeof(int16_t));
  return true;
}

bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<uint32_t>& brick) const
{
  DataTable::const_iterator brick_data = this->Lookup(bk);
  assert(this->m_Data.end() != brick_data);
  const VariantArray& varray = brick_data->second;
  brick.resize(varray.size());

  //assert(varray.type() == VariantArray::DT_UINT);
  //std::memcpy(&brick[0], varray.getui(), varray.size()*sizeof(uint32_t));
  return false;
}
bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<int32_t>& brick) const
{
  DataTable::const_iterator brick_data = this->Lookup(bk);
  assert(this->m_Data.end() != brick_data);
  const VariantArray& varray = brick_data->second;
  brick.resize(varray.size());

  //assert(varray.type() == VariantArray::DT_INT);
  //std::memcpy(&brick[0], varray.geti(), varray.size()*sizeof(int32_t));
  return false;
}

bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<float>& brick) const
{
  DataTable::const_iterator brick_data = this->Lookup(bk);
  assert(this->m_Data.end() != brick_data);
  const VariantArray& varray = brick_data->second;
  brick.resize(varray.size());

  assert(varray.type() == VariantArray::DT_FLOAT);
  std::memcpy(&brick[0], varray.getf(), varray.size()*sizeof(float));
  return true;
}
bool ExternalDataset::GetBrick(const BrickKey& bk,
                               std::vector<double>& brick) const
{
  DataTable::const_iterator brick_data = this->Lookup(bk);
  assert(this->m_Data.end() != brick_data);
  const VariantArray& varray = brick_data->second;
  brick.resize(varray.size());

  //assert(varray.type() == VariantArray::DT_DOUBLE);
  //std::memcpy(&brick[0], varray.getd(), varray.size()*sizeof(double));
  return false;
}

float ExternalDataset::MaxGradientMagnitude() const
{
  return m_fMaxMagnitude;
}

void ExternalDataset::SetHistogram(const std::vector<uint32_t>& hist)
{
  m_pHist1D.reset(new Histogram1D(hist.size()));
  std::memcpy(m_pHist1D->GetDataPointer(), &(hist.at(0)),
              sizeof(uint32_t)*hist.size());
}

void ExternalDataset::SetHistogram(const std::vector<std::vector<uint32_t>>& hist)
{
  // assume the 2D histogram is square: hist[0].size() == hist[1].size() == ...
  const VECTOR2<size_t> sz(hist.size(), hist[0].size());
  m_pHist2D.reset(new Histogram2D(sz));

  uint32_t *data = m_pHist2D->GetDataPointer();
  for(hist2d::const_iterator iter = hist.begin(); iter < hist.end(); ++iter) {
    std::memcpy(data, &(iter->at(0)), sizeof(uint32_t)*iter->size());
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
          const std::shared_ptr<T>& data, size_t len,
          ExternalDataset::DataTable& table, T brick_min, T brick_max)
{
  VariantArray varr;
  varr.set(data, len);
  table.insert(std::pair<BrickKey, VariantArray>(bk, varr));
  MESSAGE("added %u-elem brick with key: (%u, %u, %u)", static_cast<unsigned>(len),
          static_cast<unsigned>(std::get<0>(bk)),
          static_cast<unsigned>(std::get<1>(bk)),
          static_cast<unsigned>(std::get<2>(bk)));
  update_metadata(ds, brick_min, brick_max);
}

}; // anonymous namespace.


void ExternalDataset::AddBrickData(const BrickKey& bk, const BrickMD& md,
                               const std::shared_ptr<double> data,
                               size_t len, double dMin, double dMax)
{
  BrickedDataset::AddBrick(bk, md);
  add_brick(*this, bk, data, len, this->m_Data, dMin, dMax);
  Recalculate1DHistogram();
}

void ExternalDataset::AddBrickData(const BrickKey& bk, const BrickMD& md,
                               const std::shared_ptr<float> data,
                               size_t len, float fMin, float fMax)
{
  BrickedDataset::AddBrick(bk, md);
  add_brick(*this, bk, data, len, this->m_Data, fMin, fMax);
  Recalculate1DHistogram();
}

void ExternalDataset::AddBrickData(const BrickKey& bk, const BrickMD& md,
                               const std::shared_ptr<unsigned char> data,
                               size_t len,
                               unsigned char ubmin, unsigned char ubmax)
{
  BrickedDataset::AddBrick(bk, md);
  add_brick(*this, bk, data, len, this->m_Data, ubmin, ubmax);
  Recalculate1DHistogram();
}

void ExternalDataset::AddBrickData(const BrickKey& bk, const BrickMD& md,
                               const std::shared_ptr<short> data,
                               size_t len, short sMin, short sMax)
{
  BrickedDataset::AddBrick(bk, md);
  add_brick(*this, bk, data, len, this->m_Data, sMin, sMax);
  Recalculate1DHistogram();
}

void ExternalDataset::AddBrickData(const BrickKey& bk, const BrickMD& md,
                               const std::shared_ptr<unsigned short> data,
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
              const std::shared_ptr<T> data, size_t len)
  {
    ExternalDataset::DataTable::iterator iter = table.find(bk);
    if(iter == table.end()) {
      throw ExternalDataset::BrickNotFound("You must add the brick first!");
    }
    iter->second.set(data, len);
  }
}; // anonymous namespace.

void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::shared_ptr<double> data,
                                 size_t len)
{
  update_data(this->m_Data, bk, data, len);
}
void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::shared_ptr<float> data,
                                 size_t len)
{
  update_data(this->m_Data, bk, data, len);
}
void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::shared_ptr<unsigned char> data,
                                 size_t len)
{
  update_data(this->m_Data, bk, data, len);
}
void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::shared_ptr<short> data,
                                 size_t len)
{
  update_data(this->m_Data, bk, data, len);
}
void ExternalDataset::UpdateData(const BrickKey& bk,
                                 const std::shared_ptr<unsigned short> data,
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

std::pair<double,double> ExternalDataset::GetRange() const
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
  m_pHist1D.reset(new Histogram1D(m_DataSize));
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

// parameters don't really make much sense for this .. hrm.
ExternalDataset* ExternalDataset::Create(const std::string& /* filename */,
                                         uint64_t /* max_brick_size */,
                                         bool /* verify */) const
{
  return new ExternalDataset();
}

} //namespace tuvok
