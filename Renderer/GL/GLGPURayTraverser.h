#pragma once

#ifndef GLGPURAYTRAVERSER_H
#define GLGPURAYTRAVERSER_H

#include "../../StdTuvokDefines.h"
#include "GLRenderer.h"

class ExtendedPlane;


namespace tuvok {
  class GLVBO;

  /** \class GLGPURayTraverser
   * GPU Raycaster.
   *
   * GLGPURayTraverser is a GLSL-based raycaster for volumetric data */
  class GLGPURayTraverser : public GLRenderer {
    public:
      /** Constructs a VRer with immediate redraw, and
       * wireframe mode off.
       * \param pMasterController message routing object
       * \param bUseOnlyPowerOfTwo force power of two textures (compatibility)
       * \param bDownSampleTo8Bits force 8bit textures (compatibility) */
      GLGPURayTraverser(MasterController* pMasterController, 
                        bool bUseOnlyPowerOfTwo, 
                        bool bDownSampleTo8Bits, 
                        bool bDisableBorder);
      virtual ~GLGPURayTraverser() {}

    protected:
      GLVBO* m_pNearPlaneQuad;
      std::shared_ptr<GLVBO> m_pBBoxVBO;

      /** Deallocates GPU memory allocated during the rendering process. */
      virtual void Cleanup();

      /** Called once at startup to initialize constant GL data*/
      bool Initialize(std::shared_ptr<Context> ctx);

  };
} // tuvok namespace.

#endif // GLGPURAYTRAVERSER_H

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
