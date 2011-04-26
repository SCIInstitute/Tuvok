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

GLState::GLState() {
  GetFromOpenGL();
}

GLenum blendFuncToGL(const BLEND_FUNC& func) {
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

BLEND_FUNC GLtoBlendFunc(const GLenum& func) {
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

void GLState::Apply() {
  if (enableBlend) {
    glEnable(GL_BLEND); 
  } else {
    glDisable(GL_BLEND);
  }

  if (enableCull) {
    glEnable(GL_CULL_FACE);
  } else {
    glDisable(GL_CULL_FACE);
  }

  glCullFace((cullState == CULL_FRONT) ? GL_FRONT : GL_BACK);

  if (enableScissor) {
    glEnable(GL_SCISSOR_TEST);
  } else {
    glDisable( GL_SCISSOR_TEST);
  }

  if (enableLighting) {
    glEnable(GL_LIGHTING);
  } else {
    glDisable(GL_LIGHTING);
  }

  if (enableColorMaterial) {
    glEnable(GL_COLOR_MATERIAL);
  } else {
    glDisable(GL_COLOR_MATERIAL);
  }

  if (enableLineSmooth) {
    glEnable(GL_LINE_SMOOTH);
  } else {
    glDisable(GL_LINE_SMOOTH);
  }

  for (size_t i = 0;i<StateLightCount;i++) {
    if (enableLight[i]) {
      glEnable(GLenum(GL_LIGHT0+i));
    } else {
      glDisable(GLenum(GL_LIGHT0+i));
    }
  }

  for (size_t i = 0;i<StateTUCount;i++) {
    glActiveTexture(GLenum(GL_TEXTURE+i));
    switch (enableTex[i]) {
      case TEX_1D:      glEnable(GL_TEXTURE_1D);
                        glDisable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                        break;
      case TEX_2D:      glEnable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                        break;
      case TEX_3D:      glEnable(GL_TEXTURE_3D);
                        break;
      case TEX_UNKNOWN: glDisable(GL_TEXTURE_1D);
                        glDisable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                        break;
    }
  }
  glActiveTexture(GLenum(GL_TEXTURE0 + activeTexUnit));

  glDepthMask(depthMask ? 1 : 0);
  GLboolean b = colorMask ? 1 : 0;
  glColorMask(b,b,b,b);

  glBlendFunc( blendFuncToGL(blendFuncSrc), blendFuncToGL(blendFuncDst) );

  GLenum mode = GL_FUNC_ADD;
  switch (blendEquation) {
    case BE_FUNC_ADD : mode = GL_FUNC_ADD; break;
    case BE_FUNC_SUBTRACT : mode = GL_FUNC_SUBTRACT; break;
    case BE_FUNC_REVERSE_SUBTRACT : mode = GL_FUNC_REVERSE_SUBTRACT; break;
    case BE_MIN : mode = GL_MIN; break;
    case BE_MAX : mode = GL_MAX; break;
  }
  glBlendEquation  ( mode );

}

void GLState::Apply(const GPUState& state) {
  SetEnableDepth(state.GetEnableDepth());
  SetEnableCull(state.GetEnableCull());
  SetCullState(state.GetCullState());
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
        case TEX_1D:      glEnable(GL_TEXTURE_1D);
                          glDisable(GL_TEXTURE_2D);
                          glDisable(GL_TEXTURE_3D);
                          break;
        case TEX_2D:      glEnable(GL_TEXTURE_2D);
                          glDisable(GL_TEXTURE_3D);
                          break;
        case TEX_3D:      glEnable(GL_TEXTURE_3D);
                          break;
        case TEX_UNKNOWN: glDisable(GL_TEXTURE_1D);
                          glDisable(GL_TEXTURE_2D);
                          glDisable(GL_TEXTURE_3D);
                          break;
      }
    }
  }
  activeTexUnit = state.GetActiveTexUnit();
  glActiveTexture(GLenum(GL_TEXTURE0 + activeTexUnit));

  SetDepthMask(state.GetDepthMask());
  SetColorMask(state.GetColorMask());
  SetBlendEquation(state.GetBlendEquation());
  SetBlendFunction(state.GetBlendFunctionSrc(), state.GetBlendFunctionDst());
}

void GLState::GetFromOpenGL()
{
  enableDepth         = glIsEnabled(GL_DEPTH_TEST) != 0;
  enableCull          = glIsEnabled(GL_CULL_FACE)!= 0;

  GLint e;
  glGetIntegerv(GL_CULL_FACE_MODE, &e);
  cullState           = (e == GL_FRONT) ? CULL_FRONT : CULL_BACK;

  enableBlend         = glIsEnabled(GL_BLEND) != 0;
  enableScissor       = glIsEnabled(GL_SCISSOR_TEST) != 0;
  enableLighting      = glIsEnabled(GL_LIGHTING) != 0;
  enableColorMaterial = glIsEnabled(GL_COLOR_MATERIAL) != 0;
  enableLineSmooth    = glIsEnabled(GL_LINE_SMOOTH) != 0;

  for(size_t i=0; i < StateLightCount; ++i) {
    enableLight[i] = glIsEnabled(GL_LIGHT0 + (GLenum)i)!= 0;
  }
  for(size_t i=0; i < StateTUCount; ++i) {
    glActiveTexture(GL_TEXTURE0 + GLenum(i));
    if(glIsEnabled(GL_TEXTURE_3D)) {
      enableTex[i] = TEX_3D;
    } else if(glIsEnabled(GL_TEXTURE_2D)) {
      enableTex[i] = TEX_2D;
    } else if(glIsEnabled(GL_TEXTURE_1D)) {
      enableTex[i] = TEX_1D;
    } else {
      enableTex[i] = TEX_UNKNOWN;
    }
  }
  GLboolean	 b;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &b);
  depthMask = b != 0;

  GLboolean	 col[4];
  glGetBooleanv(GL_COLOR_WRITEMASK, col);
  colorMask = col[0] != 0;  

  GLint src, dest;
  glGetIntegerv(GL_BLEND_SRC, &src);
  glGetIntegerv(GL_BLEND_DST, &dest);
  blendFuncSrc = GLtoBlendFunc(src);
  blendFuncDst = GLtoBlendFunc(dest);

  GLint equation;
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation);
 
  switch (equation) {
    case GL_FUNC_ADD : blendEquation = BE_FUNC_ADD; break;
    case GL_FUNC_SUBTRACT : blendEquation = BE_FUNC_SUBTRACT; break;
    case GL_FUNC_REVERSE_SUBTRACT : blendEquation = BE_FUNC_REVERSE_SUBTRACT; break;
    case GL_MIN : blendEquation = BE_MIN; break;
    case GL_MAX : blendEquation = BE_MAX; break;
  }
}

void GLState::SetEnableDepth(const bool& value) {
  if (value != enableDepth) {
    enableDepth = value;
    if (enableDepth) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
}

void GLState::SetEnableCull(const bool& value) {
  if (value != enableCull) {
    enableCull = value;
    if (enableCull) {
      glEnable(GL_CULL_FACE);
    } else {
      glDisable(GL_CULL_FACE);
    }
  }
}

void GLState::SetCullState(const STATE_CULL& value) {
  if (value != cullState) {
    cullState = value;
    glCullFace((cullState == CULL_FRONT) ? GL_FRONT : GL_BACK);
  }
}

void GLState::SetEnableBlend(const bool& value) {
  if (value != enableBlend) {
    enableBlend = value;
    if (enableBlend) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
  }
}

void GLState::SetEnableScissor(const bool& value) {
  if (value != enableScissor) {
    enableScissor = value;
    if (enableScissor) {
      glEnable(GL_SCISSOR_TEST);
    } else {
      glDisable( GL_SCISSOR_TEST);
    }
  }
}

void GLState::SetEnableLighting(const bool& value) {
  if (value != enableLighting) {
    enableLighting = value;
    if (enableLighting) {
      glEnable(GL_LIGHTING);
    } else {
      glDisable(GL_LIGHTING);
    }
  }
}

void GLState::SetEnableColorMaterial(const bool& value) {
  if (value != enableColorMaterial) {
    enableColorMaterial = value;
    if (enableColorMaterial) {
      glEnable(GL_COLOR_MATERIAL);
    } else {
      glDisable(GL_COLOR_MATERIAL);
    }
  }
}

void GLState::SetEnableLineSmooth(const bool& value) {
  if (value != enableLineSmooth) {
    enableLineSmooth = value;
    if (enableLineSmooth) {
      glEnable(GL_LINE_SMOOTH);
    } else {
      glDisable(GL_LINE_SMOOTH);
    }
  }
}

void GLState::SetEnableLight(size_t i, const bool& value) {
  if (value != enableLight[i]) {
    enableLight[i] = value;
    if (enableLight[i]) {
      glEnable(GLenum(GL_LIGHT0+i));
    } else {
      glDisable(GLenum(GL_LIGHT0+i));
    }
  }
}

void GLState::SetEnableTex(size_t i, const STATE_TEX& value) {
  if (value != enableTex[i]) {
    glActiveTexture(GLenum(GL_TEXTURE0+i));
    enableTex[i] = value;
    switch (enableTex[i]) {
      case TEX_1D:      glEnable(GL_TEXTURE_1D);
                        glDisable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                        break;
      case TEX_2D:      glEnable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                        break;
      case TEX_3D:      glEnable(GL_TEXTURE_3D);
                        break;
      case TEX_UNKNOWN: glDisable(GL_TEXTURE_1D);
                        glDisable(GL_TEXTURE_2D);
                        glDisable(GL_TEXTURE_3D);
                        break;
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

void GLState::SetDepthMask(const bool value) {
  if (value != depthMask) {
    depthMask = value;
    glDepthMask(depthMask ? 1 : 0);
  }
}

void GLState::SetColorMask(const bool value) {
  if (value != colorMask) {
    colorMask = value;
    GLboolean b = colorMask ? 1 : 0;
    glColorMask(b,b,b,b);
  }
}

void GLState::SetBlendEquation(const BLEND_EQUATION value) {
  if (value != blendEquation) {
    blendEquation = value;
    GLenum mode = GL_FUNC_ADD;

    switch (blendEquation) {
      case BE_FUNC_ADD : mode = GL_FUNC_ADD; break;
      case BE_FUNC_SUBTRACT : mode = GL_FUNC_SUBTRACT; break;
      case BE_FUNC_REVERSE_SUBTRACT : mode = GL_FUNC_REVERSE_SUBTRACT; break;
      case BE_MIN : mode = GL_MIN; break;
      case BE_MAX : mode = GL_MAX; break;
    }

    glBlendEquation  ( mode );
  }
}

void GLState::SetBlendFunction(const BLEND_FUNC src, const BLEND_FUNC dest) {
  if (src != blendFuncSrc || dest != blendFuncDst) {
    blendFuncSrc = src;
    blendFuncDst = dest;
    glBlendFunc( blendFuncToGL(blendFuncSrc), blendFuncToGL(blendFuncDst) );
  }
}

GLStateManager::GLStateManager() : StateManager() {
  m_InternalState = new GLState();
  m_InternalState->Apply();
}
