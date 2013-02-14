#pragma once

#ifndef GLGRIDLEAPER_H
#define GLGRIDLEAPER_H

#include "../../StdTuvokDefines.h"
#include "GLGPURayTraverser.h"
#include "Renderer/VisibilityState.h"
#include "AvgMinMaxTracker.h" // for profiling
#include <fstream> // for Paper Hack file log

//#define GLGRIDLEAPER_DEBUGVIEW  // define to toggle debug view with 'D'-key
//#define GLGRIDLEAPER_WORKINGSET // define to measure per frame working set

class ExtendedPlane;


namespace tuvok {
  class GLHashTable;
  class GLVolumePool;
  class GLVBO;
  class UVFDataset;
  class ShaderDescriptor;


  /** \class GLGridLeaper
   * GPU Raycaster.
   *
   * GLGridLeaper is a GLSL-based raycaster for volumetric data */
  class GLGridLeaper : public GLGPURayTraverser {
    public:
      /** Constructs a VRer with immediate redraw, and
       * wireframe mode off.
       * \param pMasterController message routing object
       * \param bUseOnlyPowerOfTwo force power of two textures (compatibility)
       * \param bDownSampleTo8Bits force 8bit textures (compatibility) */
      GLGridLeaper(MasterController* pMasterController, 
                  bool bUseOnlyPowerOfTwo, 
                  bool bDownSampleTo8Bits, 
                  bool bDisableBorder);
      virtual ~GLGridLeaper();


      // this is work in  progress so before we start we disable all we can 
      virtual bool SupportsClearView() {return false;}
      virtual void Set1DTrans(const std::vector<unsigned char>& rgba);

      virtual std::string ClearViewDisableReason() const {
        return "this renderer is work in progress and clearview is simply not implemented yet";
      }

      virtual ERendererType GetRendererType() const {return RT_RC;}

      bool CheckForRedraw();

      virtual void SetInterpolant(Interpolant eInterpolant);

      virtual uint32_t GetFrameProgress() const { return m_bConverged ? 100 : 1; }
      virtual uint32_t GetSubFrameProgress() const { return 100; }

      /// "PH" == "paper hacks".  sorry.  delete these after pacvis.
      ///@{
      virtual void PH_ClearWorkingSet();
      virtual void PH_SetPagedBricks(size_t bricks);
      virtual size_t PH_FramePagedBricks() const;
      virtual size_t PH_SubframePagedBricks() const;
      virtual void PH_RecalculateVisibility();
      virtual bool PH_Converged() const;
      virtual double PH_BrickIOTime() const;
      virtual void PH_SetBrickIOTime(double d);
      virtual uint64_t PH_BrickIOBytes() const;
      virtual void PH_SetBrickIOBytes(uint64_t b);
      virtual double PH_RenderingTime() const;
      virtual bool PH_OpenBrickAccessLogfile(const std::string&);
      virtual bool PH_CloseBrickAccessLogfile();
      virtual bool PH_OpenLogfile(const std::string&);
      virtual bool PH_CloseLogfile();
      virtual void PH_SetOptimalFrameAverageCount(size_t);
      virtual size_t PH_GetOptimalFrameAverageCount() const;
      virtual bool PH_IsDebugViewAvailable() const;
      virtual bool PH_IsWorkingSetTrackerAvailable() const;
      ///@}

      virtual void SetDebugView(uint32_t iDebugView);
      virtual uint32_t GetDebugViewCount() const;

    protected:
      GLHashTable*    m_pglHashTable;
      GLVolumePool*   m_pVolumePool;
      std::vector<unsigned char> m_vUploadMem;
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
      GLSLProgram*    m_pProgramRayCastISO;
      GLSLProgram*    m_pProgramRayCast1DColor;
      GLSLProgram*    m_pProgramRayCast1DLightingColor;
      GLSLProgram*    m_pProgramRayCast2DColor;
      GLSLProgram*    m_pProgramRayCast2DLightingColor;
      GLSLProgram*    m_pProgramRayCastISOColor;
      UVFDataset*     m_pToCDataset;
      bool            m_bConverged;
      VisibilityState m_VisibilityState;

      // profiling
      uint32_t        m_iSubframes;
      size_t          m_iPagedBricks;
      AvgMinMaxTracker<float> m_FrameTimes;
      size_t          m_iAveragingFrameCount;
      bool            m_bAveragingFrameTimes;
      std::ofstream*  m_pLogFile;
      std::ofstream*  m_pBrickAccess;
      uint64_t        m_iFrameCount;

#ifdef GLGRIDLEAPER_DEBUGVIEW
      GLFBOTex*       m_pFBODebug;
      GLFBOTex*       m_pFBODebugNext;
#endif
#ifdef GLGRIDLEAPER_WORKINGSET
      GLHashTable*    m_pWorkingSetTable;
#endif
      double m_RenderingTime;

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
      void RecomputeBrickVisibility(bool bForceSynchronousUpdate = false);

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
      void FillBBoxVBO();
      
      bool LoadTraversalShaders(const std::vector<std::string>& vDefines = std::vector<std::string>());
      void CleanupTraversalShaders();

      // disable this function, in our implementation parameters are set once 
      // the frame begins
      virtual void UpdateLightParamsInShaders() {};
      
      bool LoadCheckShader(GLSLProgram** shader, ShaderDescriptor& sd, std::string name);

      void SetupRaycastShader(GLSLProgram* shaderProgram, RenderRegion3D& rr, EStereoID eStereoID);
  };
} // tuvok namespace.

#endif // GLGRIDLEAPER_H

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
