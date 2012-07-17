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
  \file    GLTexture2D.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    August 2008
*/
#include "StdTuvokDefines.h"
#include <stdexcept>

#include "GLTexture2D.h"
#include "GLCommon.h"
#include "Basics/nonstd.h"

using namespace tuvok;

GLTexture2D::GLTexture2D(uint32_t iSizeX, uint32_t iSizeY, GLint internalformat,
                         GLenum format, GLenum type, uint32_t iSizePerElement,
                         const GLvoid *pixels,
                         GLint iMagFilter, GLint iMinFilter,
                         GLint wrapX, GLint wrapY) :
  GLTexture(iSizePerElement, iMagFilter, iMinFilter),
  m_iSizeX(GLuint(iSizeX)),
  m_iSizeY(GLuint(iSizeY)),
  m_internalformat(internalformat),
  m_format(format),
  m_type(type)
{
  GLint prevTex;
  GL(glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex));

  GL(glGenTextures(1, &m_iGLID));
  GL(glBindTexture(GL_TEXTURE_2D, m_iGLID));

  GL(glPixelStorei(GL_PACK_ALIGNMENT ,1));
  GL(glPixelStorei(GL_UNPACK_ALIGNMENT ,1));

  GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapX));
  GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapY));
  GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, iMagFilter));
  GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, iMinFilter));
  GL(glTexImage2D(GL_TEXTURE_2D, 0, m_internalformat,
                  GLuint(m_iSizeX),GLuint(m_iSizeY), 0, m_format, m_type,
                  (GLvoid*)pixels));

  GL(glBindTexture(GL_TEXTURE_2D, prevTex));
}

void GLTexture2D::SetData(const UINTVECTOR2& offset, const UINTVECTOR2& size, const void *pixels, bool bRestoreBinding) {
  GL(glPixelStorei(GL_PACK_ALIGNMENT ,1));
  GL(glPixelStorei(GL_UNPACK_ALIGNMENT ,1));

  GLint prevTex=0;
  if (bRestoreBinding) GL(glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex));

  GL(glBindTexture(GL_TEXTURE_2D, m_iGLID));
  GL(glTexSubImage2D(GL_TEXTURE_2D, 0, 
                     offset.x, offset.y,
                     size.x, size.y,
                     m_format, m_type, (GLvoid*)pixels));

  if (bRestoreBinding) GL(glBindTexture(GL_TEXTURE_2D, prevTex));
}

void GLTexture2D::SetData(const void *pixels, bool bRestoreBinding) {
  GL(glPixelStorei(GL_PACK_ALIGNMENT ,1));
  GL(glPixelStorei(GL_UNPACK_ALIGNMENT ,1));

  GLint prevTex=0;
  if (bRestoreBinding) GL(glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex));

  GL(glBindTexture(GL_TEXTURE_2D, m_iGLID));
  GL(glTexImage2D(GL_TEXTURE_2D, 0, m_internalformat, m_iSizeX, m_iSizeY,
                  0, m_format, m_type, (GLvoid*)pixels));

  if (bRestoreBinding) GL(glBindTexture(GL_TEXTURE_2D, prevTex));
}

std::shared_ptr<const void> GLTexture2D::GetData()
{
  GL(glPixelStorei(GL_PACK_ALIGNMENT ,1));
  GL(glPixelStorei(GL_UNPACK_ALIGNMENT ,1));
  GL(glBindTexture(GL_TEXTURE_2D, m_iGLID));

  std::shared_ptr<uint8_t> data(
    new uint8_t[gl_components(m_format) * gl_byte_width(m_type) *
                m_iSizeX*m_iSizeY],
    nonstd::DeleteArray<uint8_t>()
  );
  GL(glGetTexImage(GL_TEXTURE_2D, 0, m_format, m_type, data.get()));

  return data;
}
