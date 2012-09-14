#pragma once

#ifndef GLVOLUMEPOOL_H
#define GLVOLUMEPOOL_H

#include "StdTuvokDefines.h"
#include <list>

#include "GLInclude.h"
#include "GLTexture2D.h"
#include "GLTexture3D.h"

//#define PROFILE_GLVOLUMEPOOL

#ifdef PROFILE_GLVOLUMEPOOL
#include "Basics/Timer.h"
#include "Basics/AvgMinMaxTracker.h"
#endif

namespace tuvok {
  class GLSLProgram;
  class UVFDataset;
  class AsyncVisibilityUpdater;
  class VisibilityState;

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
      GLVolumePool(const UINTVECTOR3& poolSize, UVFDataset* dataset, GLenum filter, bool bUseGLCore=true); // throws tuvok::Exception on init error
      virtual ~GLVolumePool();

      void RecomputeVisibility(const VisibilityState& visibility, size_t iTimestep);
      void UploadBricks(const std::vector<UINTVECTOR4>& vBrickIDs, std::vector<unsigned char>& vUploadMem);

      bool UploadBrick(const BrickElemInfo& metaData, void* pData);
      void UploadFirstBrick(const UINTVECTOR3& m_vVoxelSize, void* pData);
      bool IsBrickResident(const UINTVECTOR4& vBrickID) const;
      void Enable(float fLoDFactor, const FLOATVECTOR3& vExtend,
                  const FLOATVECTOR3& vAspect,
                  GLSLProgram* pShaderProgram) const;

      std::string GetShaderFragment(uint32_t iMetaTextureUnit, uint32_t iDataTextureUnit);
      
      void SetFilterMode(GLenum filter);

      virtual uint64_t GetCPUSize() const;
      virtual uint64_t GetGPUSize() const;

      inline uint32_t GetIntegerBrickID(const UINTVECTOR4& vBrickID) const; // x, y , z, lod (w) to iBrickID
      inline UINTVECTOR4 GetVectorBrickID(uint32_t iBrickID) const;
      inline UINTVECTOR3 const& GetPoolCapacity() const;
      inline UINTVECTOR3 const& GetVolumeSize() const;
      inline UINTVECTOR3 const& GetMaxInnerBrickSize() const;

    protected:
      GLTexture2D* m_PoolMetadataTexture;
      GLTexture3D* m_PoolDataTexture;
      UINTVECTOR3 m_vPoolCapacity;
      UINTVECTOR3 m_poolSize;
      UINTVECTOR3 m_maxInnerBrickSize;
      UINTVECTOR3 m_maxTotalBrickSize;
      UINTVECTOR3 m_volumeSize;
      GLenum m_filter;
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
      UVFDataset* m_pDataset;

      friend class AsyncVisibilityUpdater;
      AsyncVisibilityUpdater* m_pUpdater;
      bool m_bVisibilityUpdated;

#ifdef PROFILE_GLVOLUMEPOOL
      Timer m_Timer;
      AvgMinMaxTracker<float> m_TimesMetaTextureUpload;
      AvgMinMaxTracker<float> m_TimesRecomputeVisibility;
#endif

      std::vector<uint32_t>      m_brickMetaData; // ref by iBrickID, size of total brick count + some unused 2d texture padding
      std::vector<PoolSlotData*> m_brickToPoolMapping; // ref by iBrickID, size of total brick count + some unused 2d texture padding
      std::vector<PoolSlotData*> m_PoolSlotData; // size of available pool slots
      std::vector<uint32_t>      m_vLoDOffsetTable; // size of LoDs, stores index sums, level 0 is finest

      void CreateGLResources();
      void FreeGLResources();

      void UploadMetaData();
      void PrepareForPaging();

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
