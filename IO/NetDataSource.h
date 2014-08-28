/* forwards all requests to the socket given. */
#ifndef TUVOK_NET_DATA_SOURCE_H
#define TUVOK_NET_DATA_SOURCE_H

#include <array>
#include <memory>
#include <vector>
#include "BrickCache.h"
#include "FileBackedDataset.h"
#include "LinearIndexDataset.h"
#include "netds.h"
using NETDS::DSMetaData;
using NETDS::BatchInfo;

namespace tuvok {

class NetDataSource : public LinearIndexDataset, public FileBackedDataset {
public:
  NetDataSource(const struct DSMetaData& dsm);

  virtual ~NetDataSource();
  void SetCache(std::shared_ptr<BrickCache> cache);

  /// Data access
  ///@{
  virtual bool GetBrick(const BrickKey&, std::vector<uint8_t>&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<uint16_t>&) const;
  virtual bool GetBrick(const BrickKey&, std::vector<uint32_t>&) const;
  ///@}

  virtual unsigned GetLODLevelCount() const;
  virtual UINT64VECTOR3 GetDomainSize(const size_t lod=0,
                                      const size_t ts=0) const;
  UINT64VECTOR3 GetEffectiveBrickSize(const BrickKey&) const;
  virtual UINTVECTOR3 GetBrickLayout(size_t lod, size_t ts) const;
  virtual UINTVECTOR3 GetBrickOverlapSize() const;

  virtual unsigned GetBitWidth() const;
  virtual uint64_t GetComponentCount() const;
  virtual bool GetIsSigned() const;
  virtual bool GetIsFloat() const;
  virtual bool IsSameEndianness() const;
  virtual std::pair<double,double> GetRange() const;
  void GetHistograms(size_t ts);

  /// Acceleration queries.
  virtual bool ContainsData(const BrickKey&, double /*isoval*/) const;
  virtual bool ContainsData(const BrickKey&, double /*fMin*/,
                            double /*fMax*/) const;
  virtual bool ContainsData(const BrickKey&, double /*fMin*/, double /*fMax*/,
                            double /*fMinGradient*/,
                            double /*fMaxGradient*/) const;
  virtual tuvok::MinMaxBlock MaxMinForKey(const BrickKey& k) const;

  /// Virtual constructor.
  virtual NetDataSource* Create(const std::string&, uint64_t,bool) const;

  /// functions for FileBackedDataset interface
  ///@{
  virtual std::string Filename() const;
  virtual const char* Name() const;

  virtual bool CanRead(const std::string&, const std::vector<int8_t>&) const;
  virtual bool Verify(const std::string&) const;
  virtual std::list<std::string> Extensions() const;
  ///@}

  virtual float MaxGradientMagnitude() const;
  virtual bool Export(uint64_t iLODLevel, const std::string& targetFilename,
    bool bAppend) const;

  virtual bool ApplyFunction(uint64_t iLODLevel,
                        bool (*brickFunc)(void* pData,
                                          const UINT64VECTOR3& vBrickSize,
                                          const UINT64VECTOR3& vBrickOffset,
                                          void* pUserContext),
                        void *pUserContext,
                        uint64_t iOverlap) const;

private:
  struct DSMetaData dsm;
  std::shared_ptr<BrickCache> cache;
};

}
#endif
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
