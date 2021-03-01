/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2011 Scientific Computing and Imaging Institute,
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
#pragma once

#ifndef GLSTATEMANAGER_H
#define GLSTATEMANAGER_H

#include "../StateManager.h"
#include "StdTuvokDefines.h"

namespace tuvok {

  /** \class GLStateManager
   * OpenGL state managers.
     This state manager applies is state object's properties
     to the OpenGL subsystem.*/
  class GLStateManager : public StateManager {
    public:
      GLStateManager();
     
      virtual void Apply(const GPUState& state, bool bForce=false);

      virtual void SetEnableDepthTest(const bool& value, bool bForce=false);
      virtual void SetDepthFunc(const DEPTH_FUNC& value, bool bForce=false);
      virtual void SetEnableCullFace(const bool& value, bool bForce=false);
      virtual void SetCullState(const STATE_CULL& value, bool bForce=false);
      virtual void SetEnableBlend(const bool& value, bool bForce=false);
      virtual void SetEnableScissor(const bool& value, bool bForce=false);
      virtual void SetEnableLighting(const bool& value, bool bForce=false);
      virtual void SetEnableColorMaterial(const bool& value, bool bForce=false);
      virtual void SetEnableLight(size_t i, const bool& value, bool bForce=false);
      virtual void SetEnableTex(size_t i, const STATE_TEX& value, bool bForce=false);
      virtual void SetActiveTexUnit(const size_t iUnit, bool bForce=false);
      virtual void SetDepthMask(const bool value, bool bForce=false);
      virtual void SetColorMask(const bool value, bool bForce=false);
      virtual void SetBlendEquation(const BLEND_EQUATION value, bool bForce=false);
      virtual void SetBlendFunction(const BLEND_FUNC src, const BLEND_FUNC dest, bool bForce=false);
      virtual void SetLineWidth(const float value, bool bForce=false);

  protected:
      void GetFromOpenGL();

  };

} //namespace tuvok

#endif // GLSTATEMANAGER_H
