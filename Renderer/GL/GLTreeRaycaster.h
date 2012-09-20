#pragma once

#ifndef GLTREERAYCASTER_H
#define GLTREERAYCASTER_H

#include "../../StdTuvokDefines.h"
#include "GLRenderer.h"
#include "Renderer/VisibilityState.h"

//#define GLTREERAYCASTER_DEBUGVIEW  // define to enable debug view when pressing 'D'-key
//#define GLTREERAYCASTER_PROFILE    // define to measure frame times
//#define GLTREERAYCASTER_WORKINGSET // define to measure per frame working set

#ifdef GLTREERAYCASTER_PROFILE
#include "AvgMinMaxTracker.h"
#endif

class ExtendedPlane;


namespace tuvok {
  class GLHashTable;
  class GLVolumePool;
  class GLVBO;
  class UVFDataset;
  class ShaderDescriptor;


  /** \class GLTreeRaycaster
   * GPU Raycaster.
   *
   * GLTreeRaycaster is a GLSL-based raycaster for volumetric data */
  class GLTreeRaycaster : public GLRenderer {
    public:
      /** Constructs a VRer with immediate redraw, and
       * wireframe mode off.
       * \param pMasterController message routing object
       * \param bUseOnlyPowerOfTwo force power of two textures (compatibility)
       * \param bDownSampleTo8Bits force 8bit textures (compatibility) */
      GLTreeRaycaster(MasterController* pMasterController, 
                  bool bUseOnlyPowerOfTwo, 
                  bool bDownSampleTo8Bits, 
                  bool bDisableBorder, 
                  bool bNoRCClipplanes);
      virtual ~GLTreeRaycaster();


      // this is work in  progress so before we start we disable all we can 
      virtual bool SupportsClearView() {return false;}
      virtual void Set1DTrans(const std::vector<unsigned char>& rgba);

      virtual bool CanDoClipPlane() {return true;}

      virtual std::string ClearViewDisableReason() const {
        return "this renderer is work in progress and clearview is simply not implemented yet";
      }

      virtual ERendererType GetRendererType() const {return RT_RC;}

      bool CheckForRedraw();

      virtual void SetInterpolant(Interpolant eInterpolant);

    protected:
      GLHashTable*    m_pglHashTable;
      GLVolumePool*   m_pVolumePool;
      std::vector<unsigned char> m_vUploadMem;
      GLVBO*          m_pNearPlaneQuad;
      GLVBO*          m_pBBoxVBO;
      std::array<GLFBOTex*,2> m_pFBORayStart;
      std::array<GLFBOTex*,2> m_pFBORayStartNext;
      std::array<GLFBOTex*,2> m_pFBOStartColor;
      std::array<GLFBOTex*,2> m_pFBOStartColorNext;
      GLSLProgram*    m_pProgramRenderFrontFaces;
      GLSLProgram*    m_pProgramRenderFrontFacesNearPlane;
      GLSLProgram*    m_pProgramRayCast1D;
      GLSLProgram*    m_pProgramRayCast1DLighting;
      GLSLProgram*    m_pProgramRayCast2D;
      GLSLProgram*    m_pProgramRayCast2DLighting;
      UVFDataset*     m_pToCDataset;
      bool            m_bConverged;
      VisibilityState m_VisibilityState;

#ifdef GLTREERAYCASTER_DEBUGVIEW
      GLFBOTex*       m_pFBODebug;
      GLFBOTex*       m_pFBODebugNext;
#endif
#ifdef GLTREERAYCASTER_PROFILE
      AvgMinMaxTracker<float> m_FrameTimes;
      uint32_t        m_iSubframes;
      size_t          m_iPagedBricks;
#endif
#ifdef GLTREERAYCASTER_WORKINGSET
      GLHashTable*    m_pWorkingSetTable;
#endif

      /** Loads GLSL vertex and fragment shaders. */
      virtual bool LoadShaders();

      /** Deallocates Shaders */
      virtual void CleanupShaders();

      /** Called once at startup to initialize constant GL data*/
      bool Initialize(std::shared_ptr<Context> ctx);

      bool Continue3DDraw();

      void Raycast(RenderRegion3D& rr, EStereoID eStereoID);

      virtual bool Render3DRegion(RenderRegion3D& region3D);

      void FillRayEntryBuffer(RenderRegion3D& rr, EStereoID eStereoID);
      virtual void CreateOffscreenBuffers();

      FLOATMATRIX4 ComputeEyeToModelMatrix(const RenderRegion &renderRegion,
                                           EStereoID eStereoID) const;
  
      /** Deallocates GPU memory allocated during the rendering process. */
      virtual void Cleanup();

      bool LoadDataset(const std::string& strFilename);

      bool CreateVolumePool();
      uint32_t UpdateToVolumePool(const UINTVECTOR4& brick);
      uint32_t UpdateToVolumePool(std::vector<UINTVECTOR4>& hash);
      void RecomputeBrickVisibility();

      // catch all circumstances that change the visibility of a brick
      virtual void SetIsoValue(float fIsovalue);
      virtual void Changed2DTrans();
      virtual void Changed1DTrans();
      virtual void SetRendermode(AbstrRenderer::ERenderMode eRenderMode);

      // intercept cliplane changes
      virtual void SetClipPlane(RenderRegion *renderRegion,
                        const ExtendedPlane& plane);
      virtual void EnableClipPlane(RenderRegion *renderRegion=NULL);
      virtual void DisableClipPlane(RenderRegion *renderRegion=NULL);

      virtual void ClipPlaneRelativeLock(bool);


      virtual void SetRescaleFactors(const DOUBLEVECTOR3& vfRescale);
      void CreateVBO();
      
      bool LoadTraversalShaders();
      void CleanupTraversalShaders();

      // disable this function, in our implementation parameters are set once 
      // the frame begins
      virtual void UpdateLightParamsInShaders() {};
      
      bool LoadCheckShader(GLSLProgram** shader, ShaderDescriptor& sd, std::string name);

      void SetupRaycastShader(GLSLProgram* shaderProgram, RenderRegion3D& rr, EStereoID eStereoID);
  };
} // tuvok namespace.

#endif // GLTREERAYCASTER_H

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.


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
