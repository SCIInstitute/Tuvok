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
  \file    GLStateManager.h
  \author  Jens Krueger
           IVDA, Saarbruecken
           SCI Institute, University of Utah
*/
#pragma once

#ifndef GLSTATEMANAGER_H
#define GLSTATEMANAGER_H

#include "../StateManager.h"
#include "StdTuvokDefines.h"

namespace tuvok {

  class GLStateManager;

  /** \class GLState
   * Base class for all OpenGL state this object holds
     the rendering pipleine's state in one single object
     to the GPU.*/
  class GLState : public GPUState{
    public:
      GLState();

      virtual void SetEnableDepth(const bool& value);
      virtual void SetEnableCull(const bool& value);
      virtual void SetEnableBlend(const bool& value);
      virtual void SetEnableScissor(const bool& value);
      virtual void SetEnableLighting(const bool& value);
      virtual void SetEnableColorMaterial(const bool& value);
      virtual void SetEnableLineSmooth(const bool& value);
      virtual void SetEnableLight(size_t i, const bool& value);
      virtual void SetEnableTex(size_t i, const STATE_TEX& value);
      virtual void SetActiveTexUnit(const size_t iUnit);

    private:
      friend class StateManager;
      friend class GLStateManager;

      virtual void Apply();
      virtual void Apply(const GPUState& state);

  };

  /** \class GLStateManager
   * OpenGL state managers.
     This state manager applies is state object's properties
     to the OpenGL subsystem.*/
  class GLStateManager : public StateManager {
    public:
      GLStateManager();    

  };

}; //namespace tuvok

#endif // GLSTATEMANAGER_H
