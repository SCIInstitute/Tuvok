/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
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
  \file    GLSBVR2D.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    August 2008
*/
#pragma once

#ifndef GLSBVR2D_H
#define GLSBVR2D_H

#include "../../StdTuvokDefines.h"
#include "GLRenderer.h"
#include "../SBVRGeogen2D.h"

namespace tuvok {
  /** \class GLSBVR2D
   * Slice-based GPU volume renderer.
   *
   * GLSBVR2D is a slice based volume renderer which uses GLSL
   * and emulates 3D volumes with three stacks of 2D textures. */
  class GLSBVR2D : public GLRenderer {
    public:
      /** Constructs a VRer with immediate redraw, and
       * wireframe mode off.
       * \param pMasterController message routing object 
       * \param bUseOnlyPowerOfTwo force power of two textures (compatibility)
       * \param bDownSampleTo8Bits force 8bit textures (compatibility) */
      GLSBVR2D(MasterController* pMasterController, 
               bool bUseOnlyPowerOfTwo,
               bool bDownSampleTo8Bits, 
               bool bDisableBorder);
      virtual ~GLSBVR2D();

      /// registers a data set with this renderer
      virtual bool RegisterDataset(tuvok::Dataset*);

      virtual bool SupportsClearView() {
        return m_pDataset->GetComponentCount() == 1;
      }

      virtual std::string ClearViewDisableReason() const {
        if (m_pDataset->GetComponentCount() != 1) 
          return "this dataset has more than one component";
        return "";
      }

      virtual void EnableClipPlane(RenderRegion *renderRegion);
      virtual void DisableClipPlane(RenderRegion *renderRegion);

      virtual ERendererType GetRendererType() const {return RT_SBVR;}

      bool GetUse3DTexture() const {return m_bUse3DTexture;}
      void SetUse3DTexture(bool bUse3DTexture);

      virtual void SetInterpolant(Interpolant eInterpolant);

    protected:
      SBVRGeogen2D  m_SBVRGeogen;
      bool          m_bUse3DTexture;

      void SetBrickDepShaderVars(const RenderRegion3D& region,
                                 const Brick& currentBrick);

      virtual void Render3DPreLoop(const RenderRegion3D& region);
      virtual void Render3DInLoop(const RenderRegion3D& renderRegion,
                                  size_t iCurrentBrick, EStereoID eStereoID);

      virtual void RenderHQMIPPreLoop(RenderRegion2D& region);
      virtual void RenderHQMIPInLoop(const RenderRegion2D& region, const Brick& b);

      void RenderProxyGeometry() const;
      void RenderProxyGeometry2D() const;
      void RenderProxyGeometry3D() const;
      virtual void CleanupShaders();

      virtual void ComposeSurfaceImage(const RenderRegion& renderRegion,
                                       EStereoID eStereoID);
      virtual void UpdateLightParamsInShaders();
  
      virtual bool BindVolumeTex(const BrickKey& bkey, 
                                 const uint64_t iIntraFrameCounter);
      virtual bool IsVolumeResident(const BrickKey& key) const;
      virtual void RenderSlice(const RenderRegion2D& region, 
                       double fSliceIndex,
                       FLOATVECTOR3 vMinCoords, FLOATVECTOR3 vMaxCoords,
                       DOUBLEVECTOR3 vAspectRatio, 
                       DOUBLEVECTOR2 vWinAspectRatio);
      /** Loads GLSL vertex and fragment shaders. */
      virtual bool LoadShaders();

      virtual void SetDataDepShaderVars();

  private:
      void BindVolumeStringsToTexUnit(GLSLProgram* program, bool bGradients=true);

  };
} // tuvok namespace
#endif // GLSBVR2D_H
