#pragma once

#ifndef GLVOLUMEPOOL_H
#define GLVOLUMEPOOL_H

#include "StdTuvokDefines.h"
#include <list>

#include "Basics/Timer.h"
#include "GLInclude.h"
#include "GLTexture2D.h"
#include "GLTexture3D.h"

//#define GLVOLUMEPOOL_PROFILE // define to measure some timings

#ifdef GLVOLUMEPOOL_PROFILE
#include "Basics/AvgMinMaxTracker.h"
#endif

namespace tuvok {
  class GLSLProgram;
  class LinearIndexDataset;
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

    bool WasEverUsed() const {
      return m_iBrickID != -1;
    }

    bool ContainsVisibleBrick() const {
      return m_iTimeOfCreation > 1;
    }

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
      enum DebugMode {
        DM_NONE = 0, // no debugging
        DM_BUSY,     // prevent the async worker from doing anything useful forcing the situation when the async updater has not yet updated the metadata but should
        DM_SYNC,     // disables the async worker and forces synchronous metadata updates all the time
        DM_NOEMPTYSPACELEAPING // prevent all visibility computations and thus never ever skip/leap empty space
      };

      /// @throws tuvok::Exception on init error
      GLVolumePool(const UINTVECTOR3& poolSize, LinearIndexDataset* pDataset,
                   GLenum filter, bool bUseGLCore=true, DebugMode dm=DM_NONE);
      virtual ~GLVolumePool();

      // signals if meta texture is up-to-date including child emptiness for
      // the whole hierarchy
      bool IsVisibilityUpdated() const { return m_bVisibilityUpdated; }
      // @return (totalProcessedBrickCount, emptyBrickCount, childEmptyBrickCount, emptyLeafBrickCount)
      UINTVECTOR4 RecomputeVisibility(const VisibilityState& visibility, size_t iTimestep, bool bForceSynchronousUpdate = false);
      // returns number of bricks paged in that must not be equal to given
      // number of brick IDs especially if async updater is busy we'll get
      // requests for empty bricks too that won't be paged in
      /// @param brickDebug write out md5sums of bricks as we read them.
      uint32_t UploadBricks(const std::vector<UINTVECTOR4>& vBrickIDs,
                            bool brickDebug);

      void UploadFirstBrick(const BrickKey& bkey);

      // returns false if we need to render first before we can continue to upload further bricks
      bool UploadBrick(const BrickElemInfo& metaData, void* pData); // TODO: we could use the 1D-index here too
      void UploadFirstBrick(const UINTVECTOR3& m_vVoxelSize, void* pData);
      void UploadMetadataTexture();
      void UploadMetadataTexel(uint32_t iBrickID);
      bool IsBrickResident(const UINTVECTOR4& vBrickID) const;
      void Enable(float fLoDFactor, const FLOATVECTOR3& vExtend,
                  const FLOATVECTOR3& vAspect,
                  GLSLProgram* pShaderProgram) const;
      void Disable() const;

      // resets the pool and metadata and preserves the single largest brick
      void PH_Reset(const VisibilityState& visibility, size_t iTimestep);

      enum MissingBrickStrategy {
        OnlyNeeded,
        RequestAll,
        SkipOneLevel,
        SkipTwoLevels
      };
      std::string GetShaderFragment(uint32_t iMetaTextureUnit,
                                    uint32_t iDataTextureUnit,
                                    enum MissingBrickStrategy,
                                    const std::string& = "");
      
      void SetFilterMode(GLenum filter);

      virtual uint64_t GetCPUSize() const;
      virtual uint64_t GetGPUSize() const;

      uint32_t GetLoDCount() const;
      uint32_t GetIntegerBrickID(const UINTVECTOR4& vBrickID) const; // x, y , z, lod (w) to iBrickID
      UINTVECTOR4 GetVectorBrickID(uint32_t iBrickID) const;
      UINTVECTOR3 const& GetPoolCapacity() const;
      UINTVECTOR3 const& GetVolumeSize() const;
      UINTVECTOR3 const& GetMaxInnerBrickSize() const;

      struct MinMax {
        double min;
        double max;
      };

      uint64_t GetMaxUsedBrickBytes() const { return m_iMaxUsedBrickBytes; }

    protected:
      GLTexture3D* m_pPoolMetadataTexture;
      GLTexture3D* m_pPoolDataTexture;
      UINTVECTOR3 m_vPoolCapacity;
      UINTVECTOR3 m_poolSize;
      UINTVECTOR3 m_maxInnerBrickSize;
      UINTVECTOR3 m_maxTotalBrickSize;
      UINTVECTOR3 m_volumeSize;
      uint32_t m_iLoDCount;
      GLenum m_filter;
      GLint m_internalformat;
      GLenum m_format;
      GLenum m_type;
      uint64_t m_iTimeOfCreation;

      uint32_t m_iMetaTextureUnit;
      uint32_t m_iDataTextureUnit;
      bool m_bUseGLCore;

      size_t m_iInsertPos;

      uint32_t m_iTotalBrickCount;
      LinearIndexDataset* m_pDataset;

      friend class AsyncVisibilityUpdater;
      AsyncVisibilityUpdater* m_pUpdater;
      bool m_bVisibilityUpdated;

#ifdef GLVOLUMEPOOL_PROFILE
      Timer m_Timer;
      AvgMinMaxTracker<float> m_TimesRecomputeVisibilityForBrickPool;
      AvgMinMaxTracker<float> m_TimesMetaTextureUpload;
      AvgMinMaxTracker<float> m_TimesRecomputeVisibility;
#endif

      std::vector<uint32_t>     m_vBrickMetadata;  // ref by iBrickID, size of total brick count + some unused 2d texture padding
      std::vector<PoolSlotData> m_vPoolSlotData;   // size of available pool slots
      std::vector<uint32_t>     m_vLoDOffsetTable; // size of LoDs, stores index sums, level 0 is finest

      size_t m_iMinMaxScalarTimestep;        // current timestep of scalar acceleration structure below
      size_t m_iMinMaxGradientTimestep;      // current timestep of gradient acceleration structure below
      std::vector<MinMax> m_vMinMaxScalar;   // accelerates access to minmax scalar information, gets constructed in c'tor
      std::vector<MinMax> m_vMinMaxGradient; // accelerates access to minmax gradient information, gets constructed on first access to safe some mem
      double m_BrickIOTime;
      uint64_t m_BrickIOBytes;

      // time savers, derived from Dataset::GetMaxUsedBrickSize()
      uint64_t m_iMaxUsedBrickVoxelCount;
      uint64_t m_iMaxUsedBrickBytes;

      void CreateGLResources();
      void FreeGLResources();

      void PrepareForPaging();

      void UploadBrick(uint32_t iBrickID, const UINTVECTOR3& vVoxelSize, void* pData, 
                       size_t iInsertPos, uint64_t iTimeOfCreation);

      DebugMode const m_eDebugMode;
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
