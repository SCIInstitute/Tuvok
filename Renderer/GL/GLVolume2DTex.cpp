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
#include <cstring> // for memcpy

using namespace tuvok;
using namespace std;


GLVolume2DTex::GLVolume2DTex(uint32_t iSizeX, uint32_t iSizeY, uint32_t iSizeZ,
                             GLint internalformat, GLenum format, GLenum type,
                             uint32_t iSizePerElement,
                             const GLvoid *voxels,
                             GLint iMagFilter,
                             GLint iMinFilter,
                             GLint wrapX,
                             GLint wrapY,
                             GLint wrapZ) :
  GLVolume(iSizeX, iSizeY, iSizeZ, internalformat, format, type,
           iSizePerElement, voxels, iMagFilter, iMinFilter,wrapX,
           wrapY, wrapZ),
  m_iSizeX(iSizeX),
  m_iSizeY(iSizeY),
  m_iSizeZ(iSizeZ),
  m_internalformat(internalformat),
  m_format(format),
  m_type(type),
  m_iSizePerElement(iSizePerElement),
  m_wrapX(wrapX),
  m_wrapY(wrapY),
  m_wrapZ(wrapZ),
  m_iGPUSize(0),
  m_iCPUSize(0)
{
  m_pTextures.resize(3);
  CreateGLResources();
  SetData(voxels);
}

GLVolume2DTex::GLVolume2DTex() :
      m_iSizeX(0),
      m_iSizeY(0),
      m_iSizeZ(0),
      m_internalformat(0),
      m_format(0),
      m_type(0),
      m_iSizePerElement(0),
      m_wrapX(GL_CLAMP_TO_EDGE),
      m_wrapY(GL_CLAMP_TO_EDGE),
      m_wrapZ(GL_CLAMP_TO_EDGE),
      m_iGPUSize(0),
      m_iCPUSize(0)
{
    m_pTextures.resize(3);
}

GLVolume2DTex::~GLVolume2DTex() {
  FreeGLResources();
}

void GLVolume2DTex::Bind(uint32_t iUnit,
                         int iDepth,
                         int iStack) const {

  if (iDepth >= 0 && iDepth < static_cast<int>(m_pTextures[iStack].size())) {
    m_pTextures[iStack][iDepth]->Bind(iUnit);
  } else {
    switch (m_wrapZ) {
      default:
               WARNING("Unsupported wrap mode, falling back to GL_CLAMP");
      case GL_CLAMP :  
               GLint iPrevUint;
               GL(glGetIntegerv(GL_ACTIVE_TEXTURE, &iPrevUint));

               GL(glActiveTexture(GLenum(GL_TEXTURE0 + iUnit)));
               GL(glBindTexture(GL_TEXTURE_2D, 0));

               GL(glActiveTexture(iPrevUint));
               break;
      case GL_CLAMP_TO_EDGE : 
               if (iDepth < 0) 
                 m_pTextures[iStack][0]->Bind(iUnit);
               else
                 m_pTextures[iStack][m_pTextures[iStack].size()-1]->Bind(iUnit);
               break;
    }
  }

}

void GLVolume2DTex::CreateGLResources() {
  m_pTextures[0].resize(m_iSizeX);
  for (size_t i = 0;i<m_pTextures[0].size();i++){
    m_pTextures[0][i] = new GLTexture2D(m_iSizeZ, m_iSizeY, m_internalformat,
                                        m_format, m_type, m_iSizePerElement,
                                        NULL, m_iMagFilter, m_iMinFilter,
                                        m_wrapZ, m_wrapY);
  }
  m_pTextures[1].resize(m_iSizeY);
  for (size_t i = 0;i<m_pTextures[1].size();i++){
    m_pTextures[1][i] = new GLTexture2D(m_iSizeX, m_iSizeZ, m_internalformat,
                                        m_format, m_type, m_iSizePerElement,
                                        NULL, m_iMagFilter, m_iMinFilter,
                                        m_wrapX, m_wrapZ);
  }
  m_pTextures[2].resize(m_iSizeZ);
  for (size_t i = 0;i<m_pTextures[2].size();i++){
    m_pTextures[2][i] = new GLTexture2D(m_iSizeX, m_iSizeY, m_internalformat,
                                        m_format, m_type, m_iSizePerElement,
                                        NULL, m_iMagFilter, m_iMinFilter,
                                        m_wrapX, m_wrapY);
  }
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
  m_iCPUSize = 0;
  m_iGPUSize = 0;
}

void GLVolume2DTex::SetData(const void *voxels) {
  // push data into the stacks
  // z is easy as this matches the data layout
  // x and y are more nasty and require a
  // random acces traversal througth the data, hence
  // the complicated indexing


  const char* charPtr = static_cast<const char*>(voxels);
  char* copyBuffer = new char[static_cast<size_t>(max(
    m_pTextures[0][0]->GetCPUSize(),
    m_pTextures[1][0]->GetCPUSize())
  )];
  size_t sliceElemCount = m_iSizeY*m_iSizeX;

  for (size_t i = 0;i<m_pTextures[0].size();i++){
    size_t targetPos = 0;
    size_t sourcePos = 0;
    for (size_t y = 0;y<m_iSizeY;y++) {
      for (size_t z = 0;z<m_iSizeZ;z++) {
        // compute position in source array
        sourcePos = (i+y*m_iSizeX+z*sliceElemCount)*m_iSizePerElement;
        // copy one element into the target buffer
        memcpy(copyBuffer+targetPos,charPtr+sourcePos,m_iSizePerElement);
        targetPos += m_iSizePerElement;
      }
    }
    // copy into 2D texture slice
    m_pTextures[0][i]->SetData(copyBuffer);
  }

  for (size_t i = 0;i<m_pTextures[1].size();i++){
    size_t targetPos = 0;
    size_t sourcePos = 0;
    for (size_t z = 0;z<m_iSizeZ;z++) {
      for (size_t x = 0;x<m_iSizeX;x++) {
        // compute position in source array
        sourcePos = (x+i*m_iSizeX+z*sliceElemCount)*m_iSizePerElement;
        // copy one element into the target buffer
        memcpy(copyBuffer+targetPos,charPtr+sourcePos,m_iSizePerElement);
        targetPos += m_iSizePerElement;
      }
    }
    // copy into 2D texture slice
    m_pTextures[1][i]->SetData(copyBuffer);
  }

  delete[] copyBuffer;

  // z direction is easy 
  size_t stepping = static_cast<size_t>(m_pTextures[2][0]->GetCPUSize());

  for (size_t i = 0;i<m_pTextures[2].size();i++){
    m_pTextures[2][i]->SetData(charPtr);
    charPtr += stepping;
  }
}

uint64_t GLVolume2DTex::GetCPUSize() {
  if (m_iCPUSize) return m_iCPUSize;

  uint64_t iSize = 0;
  for (size_t iDir = 0;iDir<m_pTextures.size();iDir++){
    for (size_t i = 0;i<m_pTextures[iDir].size();i++){
      iSize += m_pTextures[iDir][i]->GetCPUSize();
    }
  }
  return iSize;
}

uint64_t GLVolume2DTex::GetGPUSize() {
  if (m_iGPUSize) return m_iGPUSize;

  uint64_t iSize = 0;
  for (size_t iDir = 0;iDir<m_pTextures.size();iDir++){
    for (size_t i = 0;i<m_pTextures[iDir].size();i++){
      iSize += m_pTextures[iDir][i]->GetGPUSize();
    }
  }
  return iSize;
}


void GLVolume2DTex::SetFilter(GLint iMagFilter, GLint iMinFilter) {
  GLVolume::SetFilter(iMagFilter, iMinFilter);

  for (size_t iDir = 0;iDir<m_pTextures.size();iDir++){
    for (size_t i = 0;i<m_pTextures[iDir].size();i++){
      if (m_pTextures[iDir][i]) {
        m_pTextures[iDir][i]->SetFilter(m_iMagFilter, m_iMinFilter);
      }
    }
  }
}
