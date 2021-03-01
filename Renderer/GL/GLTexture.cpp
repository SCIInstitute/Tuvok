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
#include "GLCommon.h"
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

size_t GLTexture::SizePerElement() const {
  return GLCommon::gl_byte_width(m_type) * GLCommon::gl_components(m_format);
}
