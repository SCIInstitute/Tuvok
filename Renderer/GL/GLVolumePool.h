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

//!    File   : GLVolumePool.h
//!    Author : Jens Krueger
//!             IVDA, MMCI, DFKI Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : November 2011
//
//!    Copyright (C) 2011 IVDA, MMCI, DFKI, SCI Institute

#pragma once

#ifndef GLVOLUMEPOOL_H
#define GLVOLUMEPOOL_H

#include "../../StdTuvokDefines.h"
#include <map>
#include <deque>

#include "GLInclude.h"
#include "GLTexture1D.h"
#include "GLTexture2D.h"
#include "GLTexture3D.h"

namespace tuvok {
  class ExtendedOctreeInfo {
    public:
      static UINTVECTOR3 GetLODCount(UINTVECTOR3 inputSize,
                                     UINTVECTOR3 innerBricksize,
                                     uint32_t overlap);
      static UINTVECTOR3 GetLODSize(uint32_t iLOD, UINTVECTOR3 inputSize,
                                    UINTVECTOR3 innerBricksize,
                                    uint32_t overlap);
      static UINTVECTOR3 GetBrickSize(UINTVECTOR3 iBrickIndex, uint32_t iLOD,
                                      UINTVECTOR3 inputSize,
                                      UINTVECTOR3 innerBricksize,
                                      uint32_t overlap);
  };

  class BrickElemInfo {
  public:
    BrickElemInfo(UINTVECTOR4 vBrickID, UINTVECTOR3 vVoxelSize,
                  FLOATVECTOR3 vTopLeft, FLOATVECTOR3 vBottomRight) :
      m_vBrickID(vBrickID),
      m_vVoxelSize(vVoxelSize),
      m_vTopLeft(vTopLeft),
      m_vBottomRight(vBottomRight)
    {}

    UINTVECTOR4 m_vBrickID;
    UINTVECTOR3 m_vVoxelSize;
    FLOATVECTOR3 m_vTopLeft;
    FLOATVECTOR3 m_vBottomRight;
  };

  class VolumePoolElemInfo : public BrickElemInfo {
  public:
    VolumePoolElemInfo(const BrickElemInfo& bei,
                       const UINTVECTOR3& vPoolCoordiantes, int iPriority) :
        BrickElemInfo(bei),
        m_vPoolCoordiantes(vPoolCoordiantes),
        m_iPriority(iPriority)
    {}

    UINTVECTOR3 m_vPoolCoordiantes;
    int m_iPriority;
  };

  class GLVolumePool : public GLObject {
    public:
      GLVolumePool(UINTVECTOR3 inputSize, UINTVECTOR3 innerBricksize,
                   uint32_t overlap, const UINTVECTOR3& poolTexSize,
                   const UINTVECTOR3& brickSize,
                   GLint internalformat, GLenum format, GLenum type,
                   uint32_t iSizePerElement);

      bool UploadBrick(const BrickElemInfo& metaData, void* pData);
      bool IsBrickResident(const UINTVECTOR4& vBrickID) const;
      void BindTexures(unsigned int iMetaTextureUnit,
                       unsigned int iDataTextureUnit) const;

      virtual ~GLVolumePool();
      virtual uint64_t GetCPUSize() const;
      virtual uint64_t GetGPUSize() const;

    protected:
      std::map<UINTVECTOR4, BrickElemInfo> m_BrickHash;
      /// @todo replace this by a priority queue
      std::deque<VolumePoolElemInfo> m_BricksInPool;
      GLTexture1D* m_StaticLODLUT;
      GLTexture2D* m_PoolMetadataTexture;
      GLTexture3D* m_PoolDataTexture;

      UINTVECTOR3 m_poolTexSize;
      UINTVECTOR2 m_metaTexSize;
      UINTVECTOR3 m_brickSize;
      GLint m_internalformat;
      GLenum m_format;
      GLenum m_type;
      uint32_t m_iSizePerElement;

      void CreateGLResources();
      void FreeGLResources();

      UINTVECTOR3 FindNextPoolPosition();
      void UpdateMetadataTexture();
  };
}
#endif // GLVOLUMEPOOL_H
