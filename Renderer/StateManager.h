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

/**
  \file    StateManager.h
  \author  Jens Krueger
           IVDA, Saarbruecken
           SCI Institute, University of Utah
*/
#pragma once

#ifndef STATEMANAGER_H
#define STATEMANAGER_H

#include "StdTuvokDefines.h"
#include <memory>

namespace tuvok {

  #define StateLightCount 1
  #define StateTUCount 4

  enum STATE_CULL {
    CULL_FRONT,
    CULL_BACK
  };

  enum STATE_TEX {
    TEX_1D,
    TEX_2D,
    TEX_3D,
    TEX_NONE
  };

  enum BLEND_FUNC {
    BF_ZERO,
    BF_ONE,
    BF_SRC_COLOR,
    BF_ONE_MINUS_SRC_COLOR,
    BF_DST_COLOR,
    BF_ONE_MINUS_DST_COLOR,
    BF_SRC_ALPHA,
    BF_ONE_MINUS_SRC_ALPHA,
    BF_DST_ALPHA,
    BF_ONE_MINUS_DST_ALPHA,
    BF_SRC_ALPHA_SATURATE
  };

  enum BLEND_EQUATION {
    BE_FUNC_ADD,
    BE_FUNC_SUBTRACT,
    BE_FUNC_REVERSE_SUBTRACT,
    BE_MIN,
    BE_MAX
  };

  enum DEPTH_FUNC {
    DF_NEVER,
    DF_LESS,
    DF_EQUAL,
    DF_LEQUAL,
    DF_GREATER,
    DF_NOTEQUAL,
    DF_GEQUAL,
    DF_ALWAYS
  };


  /** \class GPUState
   * Base class for all GPU state this object holds
     the rendering pipleine's state in one single object
     to the GPU.*/
  class GPUState {
    public:
      GPUState() :
        enableDepthTest(true),
        depthFunc(DF_LESS),
        enableCullFace(true),
        cullState(CULL_BACK),
        enableBlend(false),
        enableScissor(false),
        enableLighting(false),
        enableColorMaterial(false),
        activeTexUnit(0),
        depthMask(true),
        colorMask(true),
        blendEquation(BE_FUNC_ADD),
        blendFuncSrc(BF_ONE_MINUS_DST_ALPHA),
        blendFuncDst(BF_ONE),
        lineWidth(1.0f)
      {
        for (size_t i = 0;i<StateLightCount;i++) enableLight[i] = false;
        for (size_t i = 0;i<StateTUCount;i++) enableTex[i] = TEX_NONE;
      }
      virtual ~GPUState() {}

      bool enableDepthTest;
      DEPTH_FUNC depthFunc;
      bool enableCullFace;
      STATE_CULL cullState;
      bool enableBlend;
      bool enableScissor;
      bool enableLighting;
      bool enableLight[StateLightCount];
      bool enableColorMaterial;
      STATE_TEX enableTex[StateTUCount];
      size_t activeTexUnit;
      bool depthMask;
      bool colorMask;
      BLEND_EQUATION blendEquation;
      BLEND_FUNC blendFuncSrc;
      BLEND_FUNC blendFuncDst;
      float lineWidth;
  };

  /** \class StateManager
   * Base class for all GPU state managers.
     A state manager applies is state object's properties
     to the GPU.*/
  class StateManager {
    public:
      StateManager() {}
      virtual ~StateManager() {}

      virtual void Apply(const GPUState& state, bool bForce=false) = 0;
      virtual const GPUState& GetCurrentState() const {return m_InternalState;}

      virtual void SetEnableDepthTest(const bool& value, bool bForce=false) = 0;
      virtual void SetDepthFunc(const DEPTH_FUNC& value, bool bForce=false) = 0;
      virtual void SetEnableCullFace(const bool& value, bool bForce=false) = 0;
      virtual void SetCullState(const STATE_CULL& value, bool bForce=false) = 0;
      virtual void SetEnableBlend(const bool& value, bool bForce=false) = 0;
      virtual void SetEnableScissor(const bool& value, bool bForce=false) = 0;
      virtual void SetEnableLighting(const bool& value, bool bForce=false) = 0;
      virtual void SetEnableColorMaterial(const bool& value, bool bForce=false) = 0;
      virtual void SetEnableLight(size_t i, const bool& value, bool bForce=false) = 0;
      virtual void SetEnableTex(size_t i, const STATE_TEX& value, bool bForce=false) = 0;
      virtual void SetActiveTexUnit(const size_t iUnit, bool bForce=false) = 0;
      virtual void SetDepthMask(const bool value, bool bForce=false) = 0;
      virtual void SetColorMask(const bool value, bool bForce=false) = 0;
      virtual void SetBlendEquation(const BLEND_EQUATION value, bool bForce=false) = 0;
      virtual void SetBlendFunction(const BLEND_FUNC src, const BLEND_FUNC dest, bool bForce=false) = 0;
      virtual void SetLineWidth(const float value, bool bForce=false) = 0;

    protected:
       GPUState m_InternalState;
  };

  typedef std::shared_ptr<StateManager> StateManagerPtr; 

}; //namespace tuvok

#endif // STATEMANAGER_H
