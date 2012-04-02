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
  \file    GLTexture3D.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/
#pragma once

#ifndef GLTEXTURE3D_H
#define GLTEXTURE3D_H

#include "../../StdTuvokDefines.h"
#include "GLTexture.h"
#include "../../Basics/Vectors.h"

namespace tuvok {

class GLTexture3D : public GLTexture {
  public:
    GLTexture3D(uint32_t iSizeX, uint32_t iSizeY, uint32_t iSizeZ,
                GLint internalformat, GLenum format, GLenum type,
                uint32_t iSizePerElement,
                const GLvoid *pixels = 0,
                GLint iMagFilter = GL_NEAREST,
                GLint iMinFilter = GL_NEAREST,
                GLint wrapX = GL_CLAMP_TO_EDGE,
                GLint wrapY = GL_CLAMP_TO_EDGE,
                GLint wrapZ = GL_CLAMP_TO_EDGE);
    virtual ~GLTexture3D() {}

    virtual void Bind(uint32_t iUnit=0) const {
      GLint iPrevUint;
      GL(glGetIntegerv(GL_ACTIVE_TEXTURE, &iPrevUint));

      GL(glActiveTexture(GLenum(GL_TEXTURE0 + iUnit)));
      GL(glBindTexture(GL_TEXTURE_3D, m_iGLID));

      GL(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, m_iMagFilter));
      GL(glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, m_iMinFilter));

      GL(glActiveTexture(iPrevUint));
    }
    
    virtual void SetData(const void *pixels, bool bRestoreBinding=true);
    void SetData(const UINTVECTOR3& offset, const UINTVECTOR3& size,
                 const void *pixels, bool bRestoreBinding=true);

    virtual uint64_t GetCPUSize() {
      return uint64_t(m_iSizeX*m_iSizeY*m_iSizeZ*m_iSizePerElement);
    }
    virtual uint64_t GetGPUSize() {
      return uint64_t(m_iSizeX*m_iSizeY*m_iSizeZ*m_iSizePerElement);
    }

    UINTVECTOR3 GetSize() const {
      return UINTVECTOR3(uint32_t(m_iSizeX), uint32_t(m_iSizeY),
                         uint32_t(m_iSizeZ));
    }

  protected:
    GLuint m_iSizeX;
    GLuint m_iSizeY;
    GLuint m_iSizeZ;
    GLint  m_internalformat;
    GLenum m_format;
    GLenum m_type;
};
};
#endif // GLTEXTURE3D_H
