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
  \file    GLVolume2DTex.h
  \author  Jens Krueger
           DFKI Saarbruecken & SCI Institute University of Utah
  \date    May 2010
*/
#pragma once

#ifndef GLVOLUME2DTEX_H
#define GLVOLUME2DTEX_H

#include "GLVolume.h"
#include <vector>

namespace tuvok {
  class GLTexture2D;

  /// Emulates a 3D volume using stacks of 3D textures.
  class GLVolume2DTex : public GLVolume {
    public:
      GLVolume2DTex(uint32_t iSizeX, uint32_t iSizeY, uint32_t iSizeZ,
                    GLint internalformat, GLenum format, GLenum type,
                    const GLvoid *voxels = 0,
                    GLint iMagFilter = GL_NEAREST,
                    GLint iMinFilter = GL_NEAREST,
                    GLint wrapX = GL_CLAMP_TO_EDGE,
                    GLint wrapY = GL_CLAMP_TO_EDGE,
                    GLint wrapZ = GL_CLAMP_TO_EDGE);
      GLVolume2DTex();
      virtual ~GLVolume2DTex();

      virtual void Bind(uint32_t iUnit, int depth, int iStack) const;
      virtual void SetData(const void *voxels);

      virtual uint64_t GetCPUSize() const;
      virtual uint64_t GetGPUSize() const;

      uint32_t GetSizeX() {return m_iSizeX;}
      uint32_t GetSizeY() {return m_iSizeY;}
      uint32_t GetSizeZ() {return m_iSizeZ;}

      virtual void SetFilter(GLint iMagFilter = GL_NEAREST,
                             GLint iMinFilter = GL_NEAREST);

    private:
      std::vector< std::vector<GLTexture2D*>> m_pTextures;

      uint32_t m_iSizeX;
      uint32_t m_iSizeY;
      uint32_t m_iSizeZ;
      GLint  m_internalformat;
      GLenum m_format;
      GLenum m_type;
      GLint  m_wrapX;
      GLint  m_wrapY;
      GLint  m_wrapZ;

      uint64_t m_iGPUSize;
      uint64_t m_iCPUSize;

      void CreateGLResources();
      void FreeGLResources();
  };
}
#endif // GLVOLUME2DTEX_H
