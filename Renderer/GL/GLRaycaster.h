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
  \file    GLRaycaster.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    November 2008
*/
#pragma once

#ifndef GLRAYCASTER_H
#define GLRAYCASTER_H

#include "../../StdTuvokDefines.h"
#include "GLGPURayTraverser.h"

class ExtendedPlane;

namespace tuvok {
  class GLVBO;

/** \class GLRaycaster
 * GPU Rayster.
 *
 * GLRaycaster is a GLSL-based raycaster for volumetric data */
class GLRaycaster : public GLGPURayTraverser {
  public:
    /** Constructs a VRer with immediate redraw, and
     * wireframe mode off.
     * \param pMasterController message routing object
     * \param bUseOnlyPowerOfTwo force power of two textures (compatibility)
     * \param bDownSampleTo8Bits force 8bit textures (compatibility) */
    GLRaycaster(MasterController* pMasterController, 
                bool bUseOnlyPowerOfTwo, 
                bool bDownSampleTo8Bits, 
                bool bDisableBorder);
    virtual ~GLRaycaster();


    /// Can only use CV on scalar datasets.  There's nothing really preventing
    /// its application to RGBA datasets, but shaders would need updating (and
    /// they haven't been)
    virtual bool SupportsClearView() {
      return m_pDataset->GetComponentCount() == 1;
    }

    virtual std::string ClearViewDisableReason() const {
      if (m_pDataset->GetComponentCount() != 1) 
        return "this dataset has more than one component";
      return "";
    }

    virtual ERendererType GetRendererType() const {return RT_RC;}

  protected:
    GLFBOTex*       m_pFBORayEntry;
    GLSLProgram*    m_pProgramRenderFrontFaces;
    GLSLProgram*    m_pProgramRenderFrontFacesNT;
    GLSLProgram*    m_pProgramIso2;

    /** Sets variables related to bricks in the shader. */
    void SetBrickDepShaderVars(const RenderRegion3D& region,
                               const Brick& currentBrick,
                               size_t iCurrentBrick);

    virtual void CreateOffscreenBuffers();
    void RenderBox(const RenderRegion& renderRegion,
                   const FLOATVECTOR3& vCenter, const FLOATVECTOR3& vExtend,
                   const FLOATVECTOR3& vMinCoords, const FLOATVECTOR3& vMaxCoords,
                   bool bCullBack, EStereoID eStereoID) const;

    virtual void Render3DPreLoop(const RenderRegion3D& region);
    virtual void Render3DInLoop(const RenderRegion3D& renderRegion,
                                size_t iCurrentBrick, EStereoID eStereoID);

    virtual void RenderHQMIPPreLoop(RenderRegion2D &region);
    virtual void RenderHQMIPInLoop(const RenderRegion2D &renderRegion, const Brick& b);

    virtual void StartFrame();
    virtual void SetDataDepShaderVars();

    FLOATMATRIX4 ComputeEyeToTextureMatrix(const RenderRegion &renderRegion,
                                           FLOATVECTOR3 p1, FLOATVECTOR3 t1,
                                           FLOATVECTOR3 p2, FLOATVECTOR3 t2,
                                           EStereoID eStereoID) const;
    /** Loads GLSL vertex and fragment shaders. */
    virtual bool LoadShaders();

    /** Deallocates Shaders */
    virtual void CleanupShaders();
  
    /** Deallocates GPU memory allocated during the rendering process. */
    virtual void Cleanup();
};
} // tuvok namespace.
#endif // GLRAYCASTER_H
