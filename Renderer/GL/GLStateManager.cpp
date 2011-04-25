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
  \file    GLStateManager.cpp
  \author  Jens Schneider, Jens Krueger
           SCI Institute, University of Utah
  \date    October 2008
*/

#include "GLStateManager.h"
#include "GLInclude.h"

using namespace tuvok;

GLState::GLState() {
}

void GLState::Apply() {
  if (enableDepth) glEnable(GL_DEPTH_TEST);
              else glDisable(GL_DEPTH_TEST);
  if (enableCull)  glEnable(GL_CULL_FACE); 
              else glDisable(GL_CULL_FACE);
  if (enableBlend) glEnable(GL_BLEND); 
              else glDisable(GL_BLEND);
  if (enableScissor) glEnable(GL_SCISSOR_TEST); 
                else glDisable( GL_SCISSOR_TEST);
  if (enableLighting) glEnable(GL_LIGHTING); 
                 else glDisable(GL_LIGHTING);
  if (enableColorMat) glEnable(GL_COLOR_MATERIAL); 
                 else glDisable(GL_COLOR_MATERIAL);
  if (enableLineSmooth) glEnable(GL_LINE_SMOOTH); 
                   else glDisable(GL_LINE_SMOOTH);

  for (size_t i = 0;i<StateLightCount;i++) {
    if (enableLight[i]) 
      glEnable(GLenum(GL_LIGHT0+i)); 
    else 
      glDisable(GLenum(GL_LIGHT0+i));
  }

  for (size_t i = 0;i<StateTUCount;i++) {
    glActiveTexture(GLenum(GL_TEXTURE+i));
    switch (enableTex[i]) {
      case (TEX_1D) :  glEnable(GL_TEXTURE_1D); break;
      case (TEX_2D) :  glEnable(GL_TEXTURE_2D); break;
      case (TEX_3D) :  glEnable(GL_TEXTURE_3D); break;
    }
  }

  glActiveTexture(GLenum(GL_TEXTURE0 + activeTexUnit));
}

void GLState::Apply(const GPUState& state) {
  SetEnableDepth(state.GetEnableDepth());
  SetEnableCull(state.GetEnableCull());
  SetEnableBlend(state.GetEnableBlend());
  SetEnableScissor(state.GetEnableScissor());
  SetEnableLighting(state.GetEnableLighting());
  SetEnableColorMaterial(state.GetEnableColorMaterial());
  SetEnableLineSmooth(state.GetEnableLineSmooth());
 
  for (size_t i = 0;i<StateLightCount;i++) {
    SetEnableLight(i, state.GetEnableLight(i));
  }

  // do this by hand to avoid the redundant
  // glActiveTexture calls
  for (size_t i = 0;i<StateTUCount;i++) {
    if (state.GetEnableTex(i) != enableTex[i]) {
      glActiveTexture(GLenum(GL_TEXTURE+i));
      enableTex[i] = state.GetEnableTex(i);
      switch (enableTex[i]) {
        case (TEX_1D) :  glEnable(GL_TEXTURE_1D); break;
        case (TEX_2D) :  glEnable(GL_TEXTURE_2D); break;
        case (TEX_3D) :  glEnable(GL_TEXTURE_3D); break;
      }
    }
  }
  activeTexUnit = state.GetActiveTexUnit();
  glActiveTexture(GLenum(GL_TEXTURE0 + activeTexUnit));

}

void GLState::SetEnableDepth(const bool& value) {
  if (value != enableDepth) {
    enableDepth = value;
    if (enableDepth) glEnable(GL_DEPTH_TEST);
                else glDisable(GL_DEPTH_TEST);
  }
}

void GLState::SetEnableCull(const bool& value) {
  if (value != enableCull) {
    enableCull = value;
    if (enableCull)  glEnable(GL_CULL_FACE); 
                else glDisable(GL_CULL_FACE);
  }
}

void GLState::SetEnableBlend(const bool& value) {
  if (value != enableBlend) {
    enableBlend = value;
    if (enableBlend) glEnable(GL_BLEND); 
                else glDisable(GL_BLEND);
  }
}

void GLState::SetEnableScissor(const bool& value) {
  if (value != enableScissor) {
    enableScissor = value;
    if (enableScissor) glEnable(GL_SCISSOR_TEST); 
                  else glDisable( GL_SCISSOR_TEST);
  }
}

void GLState::SetEnableLighting(const bool& value) {
  if (value != enableLighting) {
    enableLighting = value;
    if (enableLighting) glEnable(GL_LIGHTING); 
                   else glDisable(GL_LIGHTING);
  }
}

void GLState::SetEnableColorMaterial(const bool& value) {
  if (value != enableColorMat) {
    enableColorMat = value;
    if (enableColorMat) glEnable(GL_COLOR_MATERIAL); 
                   else glDisable(GL_COLOR_MATERIAL);

  }
}

void GLState::SetEnableLineSmooth(const bool& value) {
  if (value != enableLineSmooth) {
    enableLineSmooth = value;
    if (enableLineSmooth) glEnable(GL_LINE_SMOOTH); 
                     else glDisable(GL_LINE_SMOOTH);
  }
}

void GLState::SetEnableLight(size_t i, const bool& value) {
  if (value != enableLight[i]) {
    enableLight[i] = value;
    if (enableLight[i]) 
      glEnable(GLenum(GL_LIGHT0+i)); 
    else 
      glDisable(GLenum(GL_LIGHT0+i));
  }
}

void GLState::SetEnableTex(size_t i, const STATE_TEX& value) {
  if (value != enableTex[i]) {
    glActiveTexture(GLenum(GL_TEXTURE+i));
    enableTex[i] = value;
    switch (enableTex[i]) {
      case (TEX_1D) :  glEnable(GL_TEXTURE_1D); break;
      case (TEX_2D) :  glEnable(GL_TEXTURE_2D); break;
      case (TEX_3D) :  glEnable(GL_TEXTURE_3D); break;
    }
    glActiveTexture(GLenum(GL_TEXTURE0 + activeTexUnit));
  }
}

void GLState::SetActiveTexUnit(const size_t iUnit) {
  if (iUnit != activeTexUnit) {
    activeTexUnit = iUnit;
    glActiveTexture(GLenum(GL_TEXTURE0 + activeTexUnit));
  }
}

GLStateManager::GLStateManager() : StateManager() {
  m_InternalState = new GLState();
  m_InternalState->Apply();
}

