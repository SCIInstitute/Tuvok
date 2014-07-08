#include "DynamicBrickingDS.h"
#include "NetDynamicBrickingDS.h"
#include "uvfDataset.h"
#include "netds.h"
#include "DebugOut/debug.h"

DECLARE_CHANNEL(netsrc);

namespace tuvok {

  /// @param ds the source data set to break up
  /// @param maxBrickSize the brick size to use in the new data set
  /// @param cacheBytes how many bytes to use for the brick cache
  /// @param mm how should we handle brick min maxes?
NetDynDS::NetDynDS(const char* fname,
                   std::array<size_t, 3> maxBrickSize,
                   size_t cacheBytes)
{
  struct NETDS::DSMetaData ignored;
  NETDS::openFile(fname, ignored, DynamicBrickingDS::MM_DYNAMIC, maxBrickSize, 1920, 1080);
  std::shared_ptr<UVFDataset> uvf(new UVFDataset(fname, 512, false));
  DynamicBrickingDS* dyn = new DynamicBrickingDS(uvf, maxBrickSize, cacheBytes,
                                                 DynamicBrickingDS::MM_DYNAMIC);
  this->ds = std::unique_ptr<DynamicBrickingDS>(dyn);
}

NetDynDS::~NetDynDS() {
  NETDS::closeFile(this->Filename().c_str());
  this->ds.reset();
}

std::shared_ptr<const Histogram1D> NetDynDS::Get1DHistogram() const {
  return this->ds->Get1DHistogram();
}
std::shared_ptr<const Histogram2D> NetDynDS::Get2DHistogram() const {
  return this->ds->Get2DHistogram();
}

/// modifies the cache size used for holding large bricks.
void NetDynDS::SetCacheSize(size_t megabytes) {
  this->ds->SetCacheSize(megabytes);
}
/// get the cache size used for holding large bricks in MB
size_t NetDynDS::GetCacheSize() const { return this->ds->GetCacheSize(); }

float NetDynDS::MaxGradientMagnitude() const {
  return this->ds->MaxGradientMagnitude();
}
void NetDynDS::Clear() { this->ds->Clear(); }

/// Data access
///@{
bool NetDynDS::GetBrick(const BrickKey& k, std::vector<uint8_t>& data) const {
    FIXME(netsrc, "Actually check if it's in the cash");
    bool cashHasBrick = true;
    if(cashHasBrick)
        return this->ds->GetBrick(k, data);

    if(!NETDS::getBrick(std::get<1>(k), std::get<2>(k), data))
        return false;

    FIXME(netsrc, "Actually write the data to the cash");
    return true;
}
bool NetDynDS::GetBrick(const BrickKey& k, std::vector<uint16_t>& data) const {
    FIXME(netsrc, "Actually check if it's in the cash");
    bool cashHasBrick = true;
    if(cashHasBrick)
        return this->ds->GetBrick(k, data);

    if(!NETDS::getBrick(std::get<1>(k), std::get<2>(k), data))
        return false;

    FIXME(netsrc, "Actually write the data to the cash");
    return true;
}
bool NetDynDS::GetBrick(const BrickKey& k, std::vector<uint32_t>& data) const {
    FIXME(netsrc, "Actually check if it's in the cash");
    bool cashHasBrick = true;
    if(cashHasBrick)
        return this->ds->GetBrick(k, data);

    if(!NETDS::getBrick(std::get<1>(k), std::get<2>(k), data))
        return false;

    FIXME(netsrc, "Actually write the data to the cash");
    return true;
}
///@}

/// User rescaling factors.
///@{
void NetDynDS::SetRescaleFactors(const DOUBLEVECTOR3& factors) {
  this->ds->SetRescaleFactors(factors);
}
DOUBLEVECTOR3 NetDynDS::GetRescaleFactors() const {
  return this->ds->GetRescaleFactors();
}
/// If the underlying file format supports it, save the current scaling
/// factors to the file.  The format should implicitly load and apply the
/// scaling factors when opening the data set.
bool NetDynDS::SaveRescaleFactors() { return this->ds->SaveRescaleFactors(); }
DOUBLEVECTOR3 NetDynDS::GetScale() const { return this->ds->GetScale(); }
///@}

unsigned NetDynDS::GetLODLevelCount() const {
  return this->ds->GetLODLevelCount();
}
uint64_t NetDynDS::GetNumberOfTimesteps() const {
  return this->ds->GetNumberOfTimesteps();
}
UINT64VECTOR3 NetDynDS::GetDomainSize(const size_t lod,
                                      const size_t ts) const {
  return this->ds->GetDomainSize(lod, ts);
}
UINTVECTOR3 NetDynDS::GetBrickOverlapSize() const {
  return this->ds->GetBrickOverlapSize();
}
UINT64VECTOR3 NetDynDS::GetEffectiveBrickSize(const BrickKey& k) const {
  return this->ds->GetEffectiveBrickSize(k);
}

UINTVECTOR3 NetDynDS::GetMaxBrickSize() const {
  return this->ds->GetMaxBrickSize();
}
UINTVECTOR3 NetDynDS::GetBrickLayout(size_t lod, size_t ts) const {
  return this->ds->GetBrickLayout(lod, ts);
}

unsigned NetDynDS::GetBitWidth() const { return this->ds->GetBitWidth(); }
uint64_t NetDynDS::GetComponentCount() const {
  return this->ds->GetComponentCount();
}
bool NetDynDS::GetIsSigned() const { return this->ds->GetIsSigned(); }
bool NetDynDS::GetIsFloat() const { return this->ds->GetIsFloat(); }
bool NetDynDS::IsSameEndianness() const {
  return this->ds->IsSameEndianness();
}
std::pair<double,double>
NetDynDS::GetRange() const { return this->ds->GetRange(); }

  /// Acceleration queries.
bool
NetDynDS::ContainsData(const BrickKey& k, double isoval) const {
  return this->ds->ContainsData(k, isoval);
}
bool
NetDynDS::ContainsData(const BrickKey& k, double fMin, double fMax) const {
  return this->ds->ContainsData(k, fMin, fMax);
}
bool
NetDynDS::ContainsData(const BrickKey& k, double fMin, double fMax,
                       double fMinGradient, double fMaxGradient) const {
  return this->ds->ContainsData(k, fMin,fMax, fMinGradient,fMaxGradient);
}
tuvok::MinMaxBlock
NetDynDS::MaxMinForKey(const BrickKey& k) const {
  return this->ds->MaxMinForKey(k);
}

bool NetDynDS::Export(uint64_t lod, const std::string& targetFN,
                      bool append) const {
  return this->ds->Export(lod, targetFN, append);
}

bool NetDynDS::ApplyFunction(uint64_t lod, 
                   bool (*brickFunc)(void* pData, 
                                     const UINT64VECTOR3& vBrickSize,
                                     const UINT64VECTOR3& vBrickOffset,
                                     void* pUserContext),
                   void *pUserContext,
                   uint64_t iOverlap) const {
  return this->ds->ApplyFunction(lod, brickFunc, pUserContext, iOverlap);
}

/// Virtual constructor.
NetDynDS*
NetDynDS::Create(const std::string& fn, uint64_t bsize, bool) const {
  std::array<size_t,3> mbsize = {{bsize, bsize, bsize}};
  return new NetDynDS(fn.c_str(), mbsize, this->ds->GetCacheSize());
}

/// functions for FileBackedDataset interface
///@{
std::string NetDynDS::Filename() const { return this->ds->Filename(); }
const char* NetDynDS::Name() const { return this->ds->Name(); }

bool
NetDynDS::CanRead(const std::string& fn, const std::vector<int8_t>& hdr) const {
  return this->ds->CanRead(fn, hdr);
}
bool
NetDynDS::Verify(const std::string& fn) const { return this->ds->Verify(fn); }
std::list<std::string>
NetDynDS::Extensions() const { return this->ds->Extensions(); }
///@}

}
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2014 The ImageVis3D Developers


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
