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
  \file    GLTexture.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    August 2008
*/

#include "GLTexture.h"
#include <cassert>

#include "GLSLProgram.h"

using namespace tuvok;


GLTexture::GLTexture(GLint internalformat, GLenum format,
                     GLenum type, GLint iMagFilter,
                     GLint iMinFilter) : 
m_iGLID(UINT32_INVALID),
m_iMagFilter(iMagFilter),
m_iMinFilter(iMinFilter),
m_internalformat(internalformat),
m_format(format),
m_type(type)
{
}

GLTexture::~GLTexture() {
  /*! \todo We'd like to call ::Delete() here, but we're not guaranteed to be
   *        in the correct context.  Instead, we'll make sure the texture was
   *        previously Deleted, or at least never initialized. */
  assert(m_iGLID == UINT32_INVALID);
}

void GLTexture::Delete() {
  glDeleteTextures(1,&m_iGLID);
  m_iGLID = UINT32_INVALID;
}


void GLTexture::Bind(GLSLProgram& shader, const std::string& name) const {
  shader.SetTexture(name, (*this));
}

void GLTexture::SetFilter(GLint iMagFilter, GLint iMinFilter) {
  m_iMagFilter = iMagFilter;
  m_iMinFilter = iMinFilter;
}

GLsizei GLTexture::SizePerElement(GLint internalformat) {
  switch(internalformat) {
    // deprecated formats, assume 8bit per component
    case GL_INTENSITY: return 8;
    case GL_LUMINANCE: return 8;
    case GL_RGB: return 3*8;
    case GL_RGBA: return 4*8;
    case GL_ALPHA : return 8;

    case GL_INTENSITY4:
    case GL_LUMINANCE4 :
    case GL_ALPHA4 : return 4;

    case GL_INTENSITY8:
    case GL_LUMINANCE6_ALPHA2:
    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE8:
    case GL_ALPHA8 :
    case GL_R8I :
    case GL_R8UI :
    case GL_RGBA2 : 
    case GL_R3_G3_B2 :
    case GL_R8_SNORM :
    case GL_R8 : return 8;

    case GL_INTENSITY12:
    case GL_LUMINANCE12:
    case GL_ALPHA12 :
    case GL_RGB4 : return 12;

    case GL_RGB5 : return 15;

    case GL_INTENSITY16 :
    case GL_LUMINANCE12_ALPHA4:
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE8_ALPHA8:
    case GL_LUMINANCE16:
    case GL_DEPTH_COMPONENT16:
    case GL_ALPHA16 :
    case GL_RG8I :
    case GL_RG8UI :
    case GL_R16I :
    case GL_R16UI :
    case GL_R16F :
    case GL_RGB5_A1 :
    case GL_RGBA4 :
    case GL_RG8 :
    case GL_RG8_SNORM :
    case GL_R16_SNORM :
    case GL_R16 : return 16;

    case GL_LUMINANCE12_ALPHA12:
    case GL_DEPTH_COMPONENT24 :
    case GL_RGB8I :
    case GL_RGB8UI :
    case GL_SRGB8 :
    case GL_RGB8_SNORM :
    case GL_RGB8 : return 24;

    case GL_RGB10 : return 30;

    case GL_LUMINANCE16_ALPHA16:
    case GL_DEPTH_COMPONENT32:
    case GL_RGBA8I :
    case GL_RGBA8UI :
    case GL_RG16I :
    case GL_RG16UI :
    case GL_R32I :
    case GL_R32UI :
    case GL_RGB9_E5 :
    case GL_R11F_G11F_B10F :
    case GL_R32F :
    case GL_RG16F :
    case GL_SRGB8_ALPHA8 :
    case GL_RGB10_A2UI :
    case GL_RGB10_A2 :
    case GL_RGBA8 :
    case GL_RGBA8_SNORM :
    case GL_RG16 :
    case GL_RG16_SNORM : return 32;

    case GL_RGB12 : return 36;

    case GL_RGB16I :
    case GL_RGB16UI :
    case GL_RGB16F :
    case GL_RGBA12 :
    case GL_RGB16 : 
    case GL_RGB16_SNORM : return 48;
      
    case GL_RGBA16I :
    case GL_RGBA16UI :
    case GL_RG32I :
    case GL_RG32UI :
    case GL_RG32F :
    case GL_RGBA16F :
    case GL_RGBA16 : return 64;

    case GL_RGB32I :
    case GL_RGB32UI :
    case GL_RGB32F : return 96;

    case GL_RGBA32I :
    case GL_RGBA32UI :
    case GL_RGBA32F : return 128;   
  }

  // unnsuported formats
  // GL_COMPRESSED_ALPHA, GL_COMPRESSED_LUMINANCE, GL_COMPRESSED_LUMINANCE_ALPHA, GL_COMPRESSED_INTENSITY, GL_COMPRESSED_RGB, GL_COMPRESSED_RGBA
  assert(false);
  return 0;
}
