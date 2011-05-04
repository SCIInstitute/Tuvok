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
#include "GLStateManager.h"
#include "GLInclude.h"

using namespace tuvok;

GLenum BLEND_FUNCToGL(const BLEND_FUNC& func) {
  switch (func) {
    case BF_ZERO : return  GL_ZERO; break;
    case BF_ONE : return GL_ONE; break;
    case BF_SRC_COLOR : return GL_SRC_COLOR; break;
    case BF_ONE_MINUS_SRC_COLOR : return GL_ONE_MINUS_SRC_COLOR; break;
    case BF_DST_COLOR : return GL_DST_COLOR; break;
    case BF_ONE_MINUS_DST_COLOR : return GL_ONE_MINUS_DST_COLOR; break;
    case BF_SRC_ALPHA : return GL_SRC_ALPHA; break;
    case BF_ONE_MINUS_SRC_ALPHA : return GL_ONE_MINUS_SRC_ALPHA; break;
    case BF_DST_ALPHA : return GL_DST_ALPHA; break;
    case BF_ONE_MINUS_DST_ALPHA : return GL_ONE_MINUS_DST_ALPHA; break;
    case BF_SRC_ALPHA_SATURATE : return GL_SRC_ALPHA_SATURATE; break;
  }
  return GL_ONE;
}

BLEND_FUNC GLToBLEND_FUNC(const GLenum& func) {
  switch (func) {
    case GL_ZERO : return BF_ZERO; break;
    case GL_ONE : return BF_ONE; break;
    case GL_SRC_COLOR : return BF_SRC_COLOR; break;
    case GL_ONE_MINUS_SRC_COLOR : return BF_ONE_MINUS_SRC_COLOR; break;
    case GL_DST_COLOR : return BF_DST_COLOR; break;
    case GL_ONE_MINUS_DST_COLOR : return BF_ONE_MINUS_DST_COLOR; break;
    case GL_SRC_ALPHA : return BF_SRC_ALPHA; break;
    case GL_ONE_MINUS_SRC_ALPHA : return BF_ONE_MINUS_SRC_ALPHA; break;
    case GL_DST_ALPHA : return BF_DST_ALPHA; break;
    case GL_ONE_MINUS_DST_ALPHA : return BF_ONE_MINUS_DST_ALPHA; break;
    case GL_SRC_ALPHA_SATURATE : return BF_SRC_ALPHA_SATURATE; break;
  }
  return BF_ONE;
}

GLenum BLEND_EQUATIONToGL(const BLEND_EQUATION& func) {
  switch (func) {
    case BE_FUNC_ADD : return  GL_FUNC_ADD; break;
    case BE_FUNC_SUBTRACT : return GL_FUNC_SUBTRACT; break;
    case BE_FUNC_REVERSE_SUBTRACT : return GL_FUNC_REVERSE_SUBTRACT; break;
    case BE_MIN : return GL_MIN; break;
    case BE_MAX : return GL_MAX; break;
  }
  return GL_FUNC_ADD;
}

BLEND_EQUATION GLToBLEND_EQUATION(const GLenum& func) {
  switch (func) {
    case GL_FUNC_ADD : return  BE_FUNC_ADD; break;
    case GL_FUNC_SUBTRACT : return BE_FUNC_SUBTRACT; break;
    case GL_FUNC_REVERSE_SUBTRACT : return BE_FUNC_REVERSE_SUBTRACT; break;
    case GL_MIN : return BE_MIN; break;
    case GL_MAX : return BE_MAX; break;
  }
  return BE_FUNC_ADD;
}


GLenum DEPTH_FUNCToGL(const DEPTH_FUNC& func) {
  switch (func) {
    case DF_NEVER : return  GL_NEVER; break;
    case DF_LESS : return GL_LESS; break;
    case DF_EQUAL : return GL_EQUAL; break;
    case DF_LEQUAL : return GL_LEQUAL; break;
    case DF_GREATER : return GL_GREATER; break;
    case DF_NOTEQUAL : return GL_NOTEQUAL; break;
    case DF_GEQUAL : return GL_GEQUAL; break;
    case DF_ALWAYS : return GL_ALWAYS; break;
  }
  return GL_LEQUAL;
}

DEPTH_FUNC GLToDEPTH_FUNC(const GLenum& func) {
  switch (func) {
    case GL_NEVER : return  DF_NEVER; break;
    case GL_LESS : return DF_LESS; break;
    case GL_EQUAL : return DF_EQUAL; break;
    case GL_LEQUAL : return DF_LEQUAL; break;
    case GL_GREATER : return DF_GREATER; break;
    case GL_NOTEQUAL : return DF_NOTEQUAL; break;
    case GL_GEQUAL : return DF_GEQUAL; break;
    case GL_ALWAYS : return DF_ALWAYS; break;
  }
  return DF_LEQUAL;
}

void GLStateManager::Apply(const GPUState& state, bool bForce) {
  GL_CHECK();

  SetEnableDepthTest(state.enableDepthTest, bForce);
  SetDepthFunc(state.depthFunc, bForce);
  SetEnableCullFace(state.enableCullFace, bForce);
  SetCullState(state.cullState, bForce);
  SetEnableBlend(state.enableBlend, bForce);
  SetEnableScissor(state.enableScissor, bForce);
  SetEnableLighting(state.enableLighting, bForce);
  SetEnableColorMaterial(state.enableColorMaterial, bForce);

  for (size_t i = 0;i<StateLightCount;i++) {
    SetEnableLight(i, state.enableLight[i], bForce);
  }

  // do this by hand to avoid the redundant
  // glActiveTexture calls
  for (size_t i = 0;i<StateTUCount;i++) {
    if (bForce || state.enableTex[i] != m_InternalState.enableTex[i]) {
      glActiveTexture(GLenum(GL_TEXTURE0+i));
      m_InternalState.enableTex[i] = state.enableTex[i];
      switch (m_InternalState.enableTex[i]) {
        case TEX_1D:      glDisable(GL_TEXTURE_2D);
                          glDisable(GL_TEXTURE_3D);
                          glDisable(GL_TEXTURE_CUBE_MAP);
                          glEnable(GL_TEXTURE_1D);
                          break;
        case TEX_2D:      glDisable(GL_TEXTURE_3D);
                          glDisable(GL_TEXTURE_CUBE_MAP);
                          glEnable(GL_TEXTURE_2D);
                          break;
        case TEX_3D:      glDisable(GL_TEXTURE_CUBE_MAP);
                          glEnable(GL_TEXTURE_3D);
                          break;
        case TEX_NONE:    glDisable(GL_TEXTURE_1D);
                          glDisable(GL_TEXTURE_2D);
                          glDisable(GL_TEXTURE_3D);
                          glDisable(GL_TEXTURE_CUBE_MAP);
                          break;
      }
    }
  }
  m_InternalState.activeTexUnit = state.activeTexUnit;
  glActiveTexture(GLenum(GL_TEXTURE0 + m_InternalState.activeTexUnit));

  SetDepthMask(state.depthMask, bForce);
  SetColorMask(state.colorMask, bForce);
  SetBlendEquation(state.blendEquation, bForce);
  SetBlendFunction(state.blendFuncSrc, state.blendFuncDst, bForce);
  SetLineWidth(state.lineWidth, bForce);

  GL_CHECK();
}

void GLStateManager::GetFromOpenGL()
{
  GL_CHECK();

  m_InternalState.enableDepthTest         = glIsEnabled(GL_DEPTH_TEST) != 0;

  GLint e;
  glGetIntegerv(GL_DEPTH_FUNC, &e);
  m_InternalState.depthFunc = GLToDEPTH_FUNC(e);

  m_InternalState.enableCullFace          = glIsEnabled(GL_CULL_FACE)!= 0;

  glGetIntegerv(GL_CULL_FACE_MODE, &e);
  m_InternalState.cullState           = (e == GL_FRONT) ? CULL_FRONT : CULL_BACK;

  m_InternalState.enableBlend         = glIsEnabled(GL_BLEND) != 0;
  m_InternalState.enableScissor       = glIsEnabled(GL_SCISSOR_TEST) != 0;
  m_InternalState.enableLighting      = glIsEnabled(GL_LIGHTING) != 0;
  m_InternalState.enableColorMaterial = glIsEnabled(GL_COLOR_MATERIAL) != 0;

  for(size_t i=0; i < StateLightCount; ++i) {
    m_InternalState.enableLight[i] = glIsEnabled(GL_LIGHT0 + (GLenum)i)!= 0;
  }
  for(size_t i=0; i < StateTUCount; ++i) {
    glActiveTexture(GL_TEXTURE0 + GLenum(i));
    if(glIsEnabled(GL_TEXTURE_3D)) {
      m_InternalState.enableTex[i] = TEX_3D;
    } else if(glIsEnabled(GL_TEXTURE_2D)) {
      m_InternalState.enableTex[i] = TEX_2D;
    } else if(glIsEnabled(GL_TEXTURE_1D)) {
      m_InternalState.enableTex[i] = TEX_1D;
    } else {
      m_InternalState.enableTex[i] = TEX_NONE;
    }
  }
  GLboolean	 b;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &b);
  m_InternalState.depthMask = b != 0;

  GLboolean	 col[4];
  glGetBooleanv(GL_COLOR_WRITEMASK, col);
  m_InternalState.colorMask = col[0] != 0;  

  GLint src, dest;
  glGetIntegerv(GL_BLEND_SRC, &src);
  glGetIntegerv(GL_BLEND_DST, &dest);
  m_InternalState.blendFuncSrc = GLToBLEND_FUNC(src);
  m_InternalState.blendFuncDst = GLToBLEND_FUNC(dest);

  GLint equation;
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation); 
  m_InternalState.blendEquation = GLToBLEND_EQUATION(equation);

  GLfloat f;
  glGetFloatv(GL_LINE_WIDTH, &f); 
  m_InternalState.lineWidth = f;

  GL_CHECK();
}

void GLStateManager::SetEnableDepthTest(const bool& value, bool bForce) {
  if (bForce || value != m_InternalState.enableDepthTest) {
    m_InternalState.enableDepthTest = value;
    if (m_InternalState.enableDepthTest) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
}

void GLStateManager::SetDepthFunc(const DEPTH_FUNC& value, bool bForce) {
  if (bForce || value != m_InternalState.depthFunc) {
    m_InternalState.depthFunc = value;
    glDepthFunc( DEPTH_FUNCToGL(m_InternalState.depthFunc) );
  }
}

void GLStateManager::SetEnableCullFace(const bool& value, bool bForce) {
  if (bForce || value != m_InternalState.enableCullFace) {
    m_InternalState.enableCullFace = value;
    if (m_InternalState.enableCullFace) {
      glEnable(GL_CULL_FACE);
    } else {
      glDisable(GL_CULL_FACE);
    }
  }
}

void GLStateManager::SetCullState(const STATE_CULL& value, bool bForce) {
  if (bForce || value != m_InternalState.cullState) {
    m_InternalState.cullState = value;
    glCullFace((m_InternalState.cullState == CULL_FRONT) ? GL_FRONT : GL_BACK);
  }
}

void GLStateManager::SetEnableBlend(const bool& value, bool bForce) {
  if (bForce || value != m_InternalState.enableBlend) {
    m_InternalState.enableBlend = value;
    if (m_InternalState.enableBlend) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
  }
}

void GLStateManager::SetEnableScissor(const bool& value, bool bForce) {
  if (bForce || value != m_InternalState.enableScissor) {
    m_InternalState.enableScissor = value;
    if (m_InternalState.enableScissor) {
      glEnable(GL_SCISSOR_TEST);
    } else {
      glDisable( GL_SCISSOR_TEST);
    }
  }
}

void GLStateManager::SetEnableLighting(const bool& value, bool bForce) {
  if (bForce || value != m_InternalState.enableLighting) {
    m_InternalState.enableLighting = value;
    if (m_InternalState.enableLighting) {
      glEnable(GL_LIGHTING);
    } else {
      glDisable(GL_LIGHTING);
    }
  }
}

void GLStateManager::SetEnableColorMaterial(const bool& value, bool bForce) {
  if (bForce || value != m_InternalState.enableColorMaterial) {
    m_InternalState.enableColorMaterial = value;
    if (m_InternalState.enableColorMaterial) {
      glEnable(GL_COLOR_MATERIAL);
    } else {
      glDisable(GL_COLOR_MATERIAL);
    }
  }
}

void GLStateManager::SetEnableLight(size_t i, const bool& value, bool bForce) {
  if (bForce || value != m_InternalState.enableLight[i]) {
    m_InternalState.enableLight[i] = value;
    if (m_InternalState.enableLight[i]) {
      glEnable(GLenum(GL_LIGHT0+i));
    } else {
      glDisable(GLenum(GL_LIGHT0+i));
    }
  }
}

void GLStateManager::SetEnableTex(size_t i, const STATE_TEX& value, bool bForce) {
  if (bForce || value != m_InternalState.enableTex[i]) {
    glActiveTexture(GLenum(GL_TEXTURE0+i));
    m_InternalState.enableTex[i] = value;
    switch (m_InternalState.enableTex[i]) {
      case TEX_1D:      glEnable(GL_TEXTURE_1D);
                        glDisable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                        break;
      case TEX_2D:      glEnable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                        break;
      case TEX_3D:      glEnable(GL_TEXTURE_3D);
                        break;
      case TEX_NONE: glDisable(GL_TEXTURE_1D);
                        glDisable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                        break;
    }
    glActiveTexture(GLenum(GL_TEXTURE0 + m_InternalState.activeTexUnit));
  }
}

void GLStateManager::SetActiveTexUnit(const size_t iUnit, bool bForce) {
  if (bForce || iUnit != m_InternalState.activeTexUnit) {
    m_InternalState.activeTexUnit = iUnit;
    glActiveTexture(GLenum(GL_TEXTURE0 + m_InternalState.activeTexUnit));
  }
}

void GLStateManager::SetDepthMask(const bool value, bool bForce) {
  if (bForce || value != m_InternalState.depthMask) {
    m_InternalState.depthMask = value;
    glDepthMask(m_InternalState.depthMask ? 1 : 0);
  }
}

void GLStateManager::SetColorMask(const bool value, bool bForce) {
  if (bForce || value != m_InternalState.colorMask) {
    m_InternalState.colorMask = value;
    GLboolean b = m_InternalState.colorMask ? 1 : 0;
    glColorMask(b,b,b,b);
  }
}

void GLStateManager::SetBlendEquation(const BLEND_EQUATION value, bool bForce) {
  if (bForce || value != m_InternalState.blendEquation) {
    m_InternalState.blendEquation = value;
    glBlendEquation(BLEND_EQUATIONToGL(m_InternalState.blendEquation));
  }
}

void GLStateManager::SetBlendFunction(const BLEND_FUNC src, const BLEND_FUNC dest, bool bForce) {
  if (bForce || (src != m_InternalState.blendFuncSrc || dest != m_InternalState.blendFuncDst)) {
    m_InternalState.blendFuncSrc = src;
    m_InternalState.blendFuncDst = dest;
    glBlendFunc( BLEND_FUNCToGL(m_InternalState.blendFuncSrc), 
                 BLEND_FUNCToGL(m_InternalState.blendFuncDst) );
  }
}

void GLStateManager::SetLineWidth(const float value, bool bForce) {
  if (bForce || value != m_InternalState.lineWidth) {
    m_InternalState.lineWidth = value;
    glLineWidth(m_InternalState.lineWidth);
  }
}

GLStateManager::GLStateManager() : StateManager() {
  GetFromOpenGL();
}