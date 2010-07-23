/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Interactive Visualization and Data Analysis Group.

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

//!    File   : RenderMeshGL.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include "RenderMeshGL.h"
#include "KDTree.h"
#include <cassert>

using namespace tuvok;

RenderMeshGL::RenderMeshGL(const Mesh& other) : 
  RenderMesh(other),
  m_bGLInitialized(false)
{
}

RenderMeshGL::RenderMeshGL(const VertVec& vertices, const NormVec& normals,
           const TexCoordVec& texcoords, const ColorVec& colors,
           const IndexVec& vIndices, const IndexVec& nIndices,
           const IndexVec& tIndices, const IndexVec& cIndices,
           bool bBuildKDTree, bool bScaleToUnitCube,
           const std::string& desc) :
  RenderMesh(vertices,normals,texcoords,colors,
             vIndices,nIndices,tIndices,cIndices, 
             bBuildKDTree, bScaleToUnitCube, desc),
  m_bGLInitialized(false)
{
}

RenderMeshGL::~RenderMeshGL() {
  if (m_bGLInitialized) 
    glDeleteBuffers(VBO_COUNT, m_VBOs);
}

void RenderMeshGL::PrepareOpaqueBuffers(bool bCreateBuffers) {
  if (bCreateBuffers) glGenBuffers(VBO_COUNT, m_VBOs);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[POSITION_INDEX_VBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32)*3, &m_VertIndices[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[POSITION_VBO]);
  glBufferData(GL_ARRAY_BUFFER, m_vertices.size()*sizeof(float)*3, &m_vertices[0], GL_STATIC_DRAW);

  if (m_NormalIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[NORMAL_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32)*3, &m_NormalIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[NORMAL_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_normals.size()*sizeof(float)*3, &m_normals[0], GL_STATIC_DRAW);
    glNormalPointer(GL_FLOAT, 0, 0);
    glEnableClientState(GL_NORMAL_ARRAY);
  }
  if (m_TCIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[TEXCOORD_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32)*3, &m_TCIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[TEXCOORD_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_texcoords.size()*sizeof(float)*2, &m_texcoords[0], GL_STATIC_DRAW);
    glTexCoordPointer(2, GL_FLOAT, 0, 0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  }
  if (m_COLIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[COLOR_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32)*3, &m_COLIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[COLOR_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_colors.size()*sizeof(float)*4, &m_colors[0], GL_STATIC_DRAW);
    glColorPointer(4, GL_FLOAT, 0, 0);
    glEnableClientState(GL_COLOR_ARRAY);
  }

}

void RenderMeshGL::RenderOpaqueGeometry() {
  if (!m_bGLInitialized) return;

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[POSITION_INDEX_VBO]);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[POSITION_VBO]);
  glVertexPointer(3, GL_FLOAT, 0, 0);
  glEnableClientState(GL_VERTEX_ARRAY);

  if (m_NormalIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[NORMAL_INDEX_VBO]);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[NORMAL_VBO]);
    glNormalPointer(GL_FLOAT, 0, 0);
    glEnableClientState(GL_NORMAL_ARRAY);
  }
  if (m_TCIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[TEXCOORD_INDEX_VBO]);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[TEXCOORD_VBO]);
    glTexCoordPointer(2, GL_FLOAT, 0, 0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  }
  if (m_COLIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[COLOR_INDEX_VBO]);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[COLOR_VBO]);
    glColorPointer(4, GL_FLOAT, 0, 0);
    glEnableClientState(GL_COLOR_ARRAY);
  }

  glDrawElements(GL_TRIANGLES, GLsizei(m_splitIndex*3), GL_UNSIGNED_INT, 0);

  glDisableClientState(GL_VERTEX_ARRAY);
  if (m_NormalIndices.size() == m_VertIndices.size())
    glDisableClientState(GL_NORMAL_ARRAY);
  if (m_TCIndices.size() == m_VertIndices.size())
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  if (m_COLIndices.size() == m_VertIndices.size())
    glDisableClientState(GL_COLOR_ARRAY);
}

void RenderMeshGL::InitRenderer() {
  m_bGLInitialized = true;
  PrepareOpaqueBuffers(true);
}

void RenderMeshGL::GeometryHasChanged(bool bUpdateAABB, bool bUpdateKDtree) {
  Mesh::GeometryHasChanged(bUpdateAABB, bUpdateKDtree);
  if (m_bGLInitialized) PrepareOpaqueBuffers(false);
}
