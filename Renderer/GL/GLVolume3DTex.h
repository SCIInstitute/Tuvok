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
  \file    GLVolume3DTex.h
  \author  Jens Krueger
           DFKI Saarbruecken & SCI Institute University of Utah
  \date    May 2010
*/
#pragma once

#ifndef GLVOLUME3DTEX_H
#define GLVOLUME3DTEX_H

#include "GLVolume.h"

namespace tuvok {

  class GLTexture3D;

  class GLVolume3DTex : public GLVolume {
    public:
      GLVolume3DTex(UINT32 iSizeX, UINT32 iSizeY, UINT32 iSizeZ,
                    GLint internalformat, GLenum format, GLenum type,
                    UINT32 iSizePerElement,
                    const GLvoid *pixels = 0,
                    GLint iMagFilter = GL_NEAREST,
                    GLint iMinFilter = GL_NEAREST,
                    GLint wrapX = GL_CLAMP_TO_EDGE,
                    GLint wrapY = GL_CLAMP_TO_EDGE,
                    GLint wrapZ = GL_CLAMP_TO_EDGE);
      GLVolume3DTex();
      virtual ~GLVolume3DTex();

      virtual void Bind(UINT32 iUnit=0);
      virtual void SetData(const void *voxels);

      virtual void FreeGLResources();
      virtual UINT64 GetCPUSize();
      virtual UINT64 GetGPUSize();

    private:
      GLTexture3D* m_pTexture;  
  };
};
#endif // GLVOLUME3DTEX_H