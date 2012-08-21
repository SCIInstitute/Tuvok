#pragma once

#ifndef GLVOLUMEPOOL_H
#define GLVOLUMEPOOL_H

#include "StdTuvokDefines.h"
#include <list>

#include "GLInclude.h"
#include "GLTexture2D.h"
#include "GLTexture3D.h"

namespace tuvok {
  class GLSLProgram;

  class PoolSlotData {
  public:
    PoolSlotData(const UINTVECTOR3& vPositionInPool) :
      m_iBrickID(-1),
      m_iTimeOfCreation(0),
      m_iOrigTimeOfCreation(0),
      m_vPositionInPool(vPositionInPool)
    {}

    void FlagEmpty() {
      m_iOrigTimeOfCreation = m_iTimeOfCreation;
      m_iTimeOfCreation = 1;
    }

    void Restore() {
      m_iTimeOfCreation = m_iOrigTimeOfCreation;
    }

    const UINTVECTOR3& PositionInPool() const {return m_vPositionInPool;}

    int32_t     m_iBrickID;
    uint64_t    m_iTimeOfCreation;
    uint64_t    m_iOrigTimeOfCreation;
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

  class GLVolumePool : public GLObject {
    public:
      GLVolumePool(const UINTVECTOR3& poolSize, const UINTVECTOR3& maxBrickSize,
                   const UINTVECTOR3& overlap, const UINTVECTOR3& volumeSize,
                   GLint internalformat, GLenum format, GLenum type, 
                   bool bUseGLCore=true);
      virtual ~GLVolumePool();

      bool UploadBrick(const BrickElemInfo& metaData, void* pData);
      void UploadFirstBrick(const UINTVECTOR3& m_vVoxelSize, void* pData);
      void UploadMetaData();
      void BrickIsVisible(uint32_t iLoD, uint32_t iIndexInLoD,
                             bool bVisible, bool bChildrenVisible);
      bool IsBrickResident(const UINTVECTOR4& vBrickID) const;
      void Enable(float fLoDFactor, const FLOATVECTOR3& vExtend,
                  const FLOATVECTOR3& vAspect,
                  GLSLProgram* pShaderProgram) const;

      std::string GetShaderFragment(uint32_t iMetaTextureUnit, uint32_t iDataTextureUnit);

      virtual uint64_t GetCPUSize() const;
      virtual uint64_t GetGPUSize() const;

    protected:
      GLTexture2D* m_PoolMetadataTexture;
      GLTexture3D* m_PoolDataTexture;
      UINTVECTOR3 m_vPoolCapacity;
      UINTVECTOR3 m_poolSize;
      UINTVECTOR3 m_maxInnerBrickSize;
      UINTVECTOR3 m_maxTotalBrickSize;
      UINTVECTOR3 m_volumeSize;
      GLint m_internalformat;
      GLenum m_format;
      GLenum m_type;
      uint64_t m_iTimeOfCreation;

      uint32_t m_iMetaTextureUnit;
      uint32_t m_iDataTextureUnit;
      bool m_bUseGLCore;

      size_t m_iInsertPos;

      UINTVECTOR2 m_metaTexSize;
      uint32_t m_iTotalBrickCount;

      std::vector<uint32_t>      m_brickMetaData;
      std::vector<PoolSlotData*> m_brickToPoolMapping;
      std::vector<PoolSlotData*> m_PoolSlotData;
      std::vector<uint32_t>      m_vLoDOffsetTable;

      void CreateGLResources();
      void FreeGLResources();

      void UpdateMetadata();
      uint32_t GetIntegerBrickID(const UINTVECTOR4& vBrickID) const;
      void UploadBrick(uint32_t iBrickID, const UINTVECTOR3& vVoxelSize, void* pData, 
                       size_t iInsertPos, uint64_t iTimeOfCreation);
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
