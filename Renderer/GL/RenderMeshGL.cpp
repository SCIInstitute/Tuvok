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
#include "GLInclude.h"
#include <cassert>

RenderMeshGL::RenderMeshGL(const Mesh& other) : 
  Mesh(other.GetVertices(),other.GetNormals(),
       other.GetTexCoords(),other.GetColors(),
       other.GetVertexIndices(),other.GetNormalIndices(),
       other.GetTexCoordIndices(),other.GetColorIndices(),
       false, false) {
  SplitOpaqueFromTransparent();
  if (other.GetKDTree()) ComputeKDTree();
  PrepareOpaqueBuffers();
}

RenderMeshGL::RenderMeshGL(const VertVec& vertices, const NormVec& normals,
           const TexCoordVec& texcoords, const ColorVec& colors,
           const IndexVec& vIndices, const IndexVec& nIndices,
           const IndexVec& tIndices, const IndexVec& cIndices,
           bool bBuildKDTree, bool bScaleToUnitCube) :
  Mesh(vertices,normals,texcoords,colors,
       vIndices,nIndices,tIndices,cIndices, false, bScaleToUnitCube)
{
  SplitOpaqueFromTransparent();
  // moved this computation after the resorting as it invalides the indices
  // stored in the KD-Tree
  if (bBuildKDTree) m_KDTree = new KDTree(this);
  PrepareOpaqueBuffers();
}

void RenderMeshGL::Swap(size_t i, size_t j) {
  std::swap(m_VertIndices[i], m_VertIndices[j]);
  std::swap(m_COLIndices[i], m_COLIndices[j]);

  if (m_NormalIndices.size()) std::swap(m_NormalIndices[i], m_NormalIndices[j]);
  if (m_TCIndices.size())     std::swap(m_TCIndices[i], m_TCIndices[j]);
}

RenderMeshGL::~RenderMeshGL() {
  glDeleteBuffers(8, m_VBOs);
}

bool RenderMeshGL::isTransparent(size_t i, float fTreshhold) {
  return m_colors[m_COLIndices[i].x].w < fTreshhold ||
         m_colors[m_COLIndices[i].y].w < fTreshhold ||
         m_colors[m_COLIndices[i].z].w < fTreshhold;
}

void RenderMeshGL::SplitOpaqueFromTransparent() {

  if (m_COLIndices.size() == 0) {
    m_splitIndex = m_VertIndices.size();
    return;
  }

  assert(Validate(true));

  // find first transparent triangle
  size_t iTarget = 0;
  for (;iTarget<m_COLIndices.size();iTarget++) {
    if (isTransparent(iTarget)) break;
  }

  // swap opaque triangle with transparent
  size_t iStart = iTarget;
  for (size_t iSource = iStart+1;iSource<m_COLIndices.size();iSource++) {
    if (!isTransparent(iSource)) {
      Swap(iSource, iTarget);
      iTarget++;
    }
  }
  m_splitIndex = iTarget;
}


void RenderMeshGL::PrepareOpaqueBuffers() {
  glGenBuffers(8, m_VBOs);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[POSITION_INDEX_VBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32), &m_VertIndices[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[POSITION_VBO]);
  glBufferData(GL_ARRAY_BUFFER, m_vertices.size()*sizeof(float), &m_vertices[0], GL_STATIC_DRAW);

  if (m_NormalIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[NORMAL_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32), &m_NormalIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[NORMAL_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_normals.size()*sizeof(float), &m_normals[0], GL_STATIC_DRAW);
    glNormalPointer(GL_FLOAT, 0, 0);
    glEnableClientState(GL_NORMAL_ARRAY);
  }
  if (m_TCIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[TEXCOORD_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32), &m_TCIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[TEXCOORD_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_texcoords.size()*sizeof(float), &m_texcoords[0], GL_STATIC_DRAW);
    glTexCoordPointer(2, GL_FLOAT, 0, 0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  }
  if (m_COLIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VBOs[COLOR_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32), &m_COLIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[COLOR_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_colors.size()*sizeof(float), &m_colors[0], GL_STATIC_DRAW);
    glColorPointer(4, GL_FLOAT, 0, 0);
    glEnableClientState(GL_COLOR_ARRAY);
  }


}

void RenderMeshGL::RenderOpaqueGeometry() {

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

  glDrawArrays(GL_TRIANGLES, 0, m_splitIndex);

  glDisableClientState(GL_VERTEX_ARRAY);
  if (m_NormalIndices.size() == m_VertIndices.size())
    glDisableClientState(GL_NORMAL_ARRAY);
  if (m_TCIndices.size() == m_VertIndices.size())
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  if (m_COLIndices.size() == m_VertIndices.size())
    glDisableClientState(GL_COLOR_ARRAY);
}