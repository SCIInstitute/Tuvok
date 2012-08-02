#pragma once

#ifndef GLTREERAYCASTER_H
#define GLTREERAYCASTER_H

#include "../../StdTuvokDefines.h"
#include "GLRenderer.h"

class ExtendedPlane;


namespace tuvok {
  class GLHashTable;
  class GLVolumePool;
  class GLVBO;


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
      virtual bool CanDoClipPlane() {return false;}


      virtual std::string ClearViewDisableReason() const {
        return "this renderer is work in progress and clearview is simply not implemented yet";
      }

      virtual ERendererType GetRendererType() const {return RT_RC;}

    protected:
      GLHashTable*  m_pglHashTable;
      GLVolumePool* m_pVolumePool;

      /** Loads GLSL vertex and fragment shaders. */
      virtual bool LoadShaders();

      /** Deallocates Shaders */
      virtual void CleanupShaders();

      /** Called once at startup to initialize constant GL data*/
      bool Initialize(std::shared_ptr<Context> ctx);

      bool Continue3DDraw();
      virtual bool Render3DRegion(RenderRegion3D& region3D);

      void RenderBox(const RenderRegion& renderRegion, bool bCullBack, 
                     int iStereoID, GLSLProgram* shader) const;

      // all functions below are not "guaranteed" yet
      // (ask Jens if you want to know what that means :-)


      GLVBO*          m_pBBoxVBO;
      GLFBOTex*       m_pFBORayEntry;
      GLSLProgram*    m_pProgramRenderFrontFaces;
      bool            m_bNoRCClipplanes;

      virtual void CreateOffscreenBuffers();

      virtual void StartFrame();
      virtual void SetDataDepShaderVars();

      FLOATMATRIX4 ComputeEyeToModelMatrix(const RenderRegion &renderRegion,
                                           int iStereoID) const;
  
      /** Deallocates GPU memory allocated during the rendering process. */
      virtual void Cleanup();

  };
} // tuvok namespace.

#endif // GLTREERAYCASTER_H

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
