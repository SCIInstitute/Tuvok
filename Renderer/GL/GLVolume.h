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
  \file    GLVolume.h
  \author  Jens Krueger
           DFKI Saarbruecken & SCI Institute University of Utah
  \date    May 2010
  \brief   Abstracts volume creation via 2D or 3D textures.
*/
#pragma once

#ifndef GLVOLUME_H
#define GLVOLUME_H

#include "../../StdTuvokDefines.h"
#include "GLInclude.h"

namespace tuvok {
  class GLVolume  {
    public:
      GLVolume(uint32_t iSizeX, uint32_t iSizeY, uint32_t iSizeZ,
               GLint internalformat, GLenum format, GLenum type,
               uint32_t iSizePerElement,
               const GLvoid *pixels = 0,
               GLint iMagFilter = GL_NEAREST,
               GLint iMinFilter = GL_NEAREST,
               GLint wrapX = GL_CLAMP_TO_EDGE,
               GLint wrapY = GL_CLAMP_TO_EDGE,
               GLint wrapZ = GL_CLAMP_TO_EDGE);
      GLVolume();
      virtual ~GLVolume();
      virtual void SetData(const void *voxels) = 0;
      virtual void SetFilter(GLint iMagFilter = GL_NEAREST, GLint iMinFilter = GL_NEAREST);

      virtual uint64_t GetCPUSize() = 0;
      virtual uint64_t GetGPUSize() = 0;

  protected:
      GLint  m_iMagFilter;
      GLint  m_iMinFilter;

  };
}
#endif // GLVOLUME_H
