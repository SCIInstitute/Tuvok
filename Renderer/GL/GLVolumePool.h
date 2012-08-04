#pragma once

#ifndef GLVOLUMEPOOL_H
#define GLVOLUMEPOOL_H

#include "StdTuvokDefines.h"
#include <list>

#include "GLInclude.h"
#include "GLTexture2D.h"
#include "GLTexture3D.h"

namespace tuvok {
  class PoolSlotData {
  public:
    PoolSlotData(const UINTVECTOR3& vPositionInPool) :
      m_vPositionInPool(vPositionInPool),
      m_iBrickID(-1),
      m_iTimeOfCreation(0)
    {}

    const UINTVECTOR3& PositionInPool() const {return m_vPositionInPool;}

    int32_t     m_iBrickID;
    uint64_t    m_iTimeOfCreation;
  private:
    UINTVECTOR3 m_vPositionInPool;
    PoolSlotData();
  };

  struct BrickElemInfo {
    BrickElemInfo(const UINTVECTOR4& vBrickID, const UINTVECTOR3& vVoxelSize) :
      m_vBrickID(vBrickID),
      m_vVoxelSize(vVoxelSize)
    {}

    UINTVECTOR4    m_vBrickID;
    UINTVECTOR3    m_vVoxelSize;
  };

  struct VolumePoolElemInfo : public BrickElemInfo {
    VolumePoolElemInfo(const BrickElemInfo& bei,
                       const UINTVECTOR3& vPoolCoordinates) :
        BrickElemInfo(bei),
        m_vPoolCoordinates(vPoolCoordinates),
        m_iPriority(0)
    {}

    bool operator<(const VolumePoolElemInfo& vei) const {
      return this->m_iPriority < vei.m_iPriority;
    }

    void Age() {
      m_iPriority++;
    }

    void Renew() {
      m_iPriority = 0;
    }

    UINTVECTOR3 m_vPoolCoordinates;
    uint64_t m_iPriority;
  };

  class GLVolumePool : public GLObject {
    public:
      GLVolumePool(const UINTVECTOR3& poolSize, const UINTVECTOR3& maxBrickSize,
                   const UINTVECTOR3& overlap, const UINTVECTOR3& volumeSize,
                   GLint internalformat, GLenum format, GLenum type, 
                   bool bUseGLCore=true);
      virtual ~GLVolumePool();

      bool UploadBrick(const BrickElemInfo& metaData, void* pData);
      void UploadMetaData();
      bool IsBrickResident(const UINTVECTOR4& vBrickID) const;
      void BindTexures() const;

      std::string GetShaderFragment(uint32_t iMetaTextureUnit, uint32_t iDataTextureUnit);

      virtual uint64_t GetCPUSize() const;
      virtual uint64_t GetGPUSize() const;

    protected:
      
      std::list<VolumePoolElemInfo> m_BricksInPool;
      GLTexture2D* m_PoolMetadataTexture;
      GLTexture3D* m_PoolDataTexture;
      UINTVECTOR3 m_poolSize;
      UINTVECTOR3 m_maxBrickSize;
      UINTVECTOR3 m_overlap;
      UINTVECTOR3 m_volumeSize;
      GLint m_internalformat;
      GLenum m_format;
      GLenum m_type;

      uint32_t m_iMetaTextureUnit;
      uint32_t m_iDataTextureUnit;
      bool m_bUseGLCore;

      UINTVECTOR2 m_metaTexSize;


      std::vector<FLOATVECTOR4> m_brickMetaData;
      std::vector<PoolSlotData> m_PoolSlotData;
      std::vector<uint32_t> m_vLoDOffsetTable;

/*
      Hash<UINTVECTOR4, BrickElemInfo>::Table m_BrickHash;
      UINTVECTOR3 m_allocPos;

*/

      void CreateGLResources();
      void FreeGLResources();

      UINTVECTOR3 FindNextPoolPosition() const;
      void UpdateMetadata();
      uint32_t GetIntegerBrickID(const UINTVECTOR4& vBrickID) const;
  };
}
#endif // GLVOLUMEPOOL_H

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2011 Interactive Visualization and Data Analysis Group.


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
