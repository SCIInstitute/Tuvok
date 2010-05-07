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
  \file    GLVolume2DTex.cpp
  \author  Jens Krueger
           DFKI Saarbruecken & SCI Institute University of Utah
  \date    May 2010
*/

#include "GLVolume2DTex.h"
#include "GLTexture2D.h"

using namespace tuvok;

GLVolume2DTex::GLVolume2DTex(UINT32 , UINT32 , UINT32 ,
                             GLint , GLenum , GLenum ,
                             UINT32 ,
                             const GLvoid *,
                             GLint ,
                             GLint ,
                             GLint ,
                             GLint ,
                             GLint )
/*
GLVolume2DTex::GLVolume2DTex(UINT32 iSizeX, UINT32 iSizeY, UINT32 iSizeZ,
                             GLint internalformat, GLenum format, GLenum type,
                             UINT32 iSizePerElement,
                             const GLvoid *pixels,
                             GLint iMagFilter,
                             GLint iMinFilter,
                             GLint wrapX,
                             GLint wrapY,
                             GLint wrapZ)
                             */
{
  m_pTextures.resize(3);

  // TODO
}

GLVolume2DTex::GLVolume2DTex()
{
}

GLVolume2DTex::~GLVolume2DTex() {
}

void GLVolume2DTex::Bind(UINT32 iUnit,
                         size_t depth,
                         size_t iStack) {
  m_pTextures[iStack][depth]->Bind(iUnit);
}

void GLVolume2DTex::FreeGLResources() {
  for (size_t iDir = 0;iDir<m_pTextures.size();iDir++){
    for (size_t i = 0;i<m_pTextures[iDir].size();i++){
      if (m_pTextures[iDir][i]) {
        m_pTextures[iDir][i]->Delete();
        delete m_pTextures[iDir][i];
      }
      m_pTextures[iDir][i] = NULL;
    }
    m_pTextures[iDir].resize(0);
  }
}

void GLVolume2DTex::SetData(const void *) {
  // TODO
}

UINT64 GLVolume2DTex::GetCPUSize() {
  return 0; // TODO
}

UINT64 GLVolume2DTex::GetGPUSize() {
  return 0; // TODO
}