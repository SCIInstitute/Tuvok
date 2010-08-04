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

#include <cassert>
#include <stdexcept>
#include "RenderMeshGL.h"
#include "KDTree.h"

using namespace tuvok;

RenderMeshGL::RenderMeshGL(const Mesh& other) : 
  RenderMesh(other),
  m_bGLInitialized(false)
{
  UnrollArrays();
}

RenderMeshGL::RenderMeshGL(const VertVec& vertices, const NormVec& normals,
           const TexCoordVec& texcoords, const ColorVec& colors,
           const IndexVec& vIndices, const IndexVec& nIndices,
           const IndexVec& tIndices, const IndexVec& cIndices,
           bool bBuildKDTree, bool bScaleToUnitCube,
           const std::string& desc, EMeshType meshType) :
  RenderMesh(vertices,normals,texcoords,colors,
             vIndices,nIndices,tIndices,cIndices, 
             bBuildKDTree, bScaleToUnitCube, desc, meshType),
  m_bGLInitialized(false)
{
  UnrollArrays();
}

RenderMeshGL::~RenderMeshGL() {
  if (m_bGLInitialized) {
    glDeleteBuffers(DATA_VBO_COUNT, m_VBOs);
    glDeleteBuffers(1, &m_IndexVBOOpaque);
    glDeleteBuffers(1, &m_IndexVBOFront);
    glDeleteBuffers(1, &m_IndexVBOBehind);
    glDeleteBuffers(1, &m_IndexVBOInside);
  }
}

void RenderMeshGL::PrepareOpaqueBuffers() {
  if (m_VertIndices.empty()) return;

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexVBOOpaque);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32), &m_VertIndices[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[POSITION_VBO]);
  glBufferData(GL_ARRAY_BUFFER, m_vertices.size()*sizeof(float)*3, &m_vertices[0], GL_STATIC_DRAW);

  if (m_NormalIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[NORMAL_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_normals.size()*sizeof(float)*3, &m_normals[0], GL_STATIC_DRAW);
  }
  if (m_TCIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[TEXCOORD_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_texcoords.size()*sizeof(float)*2, &m_texcoords[0], GL_STATIC_DRAW);
  }
  if (m_COLIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[COLOR_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_colors.size()*sizeof(float)*4, &m_colors[0], GL_STATIC_DRAW);
  }
}

void RenderMeshGL::RenderGeometry(GLuint IndexVBO, size_t count) {
  if (!m_bGLInitialized || count == 0) return;

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBO);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[POSITION_VBO]);
  glVertexPointer(3, GL_FLOAT, 0, 0);
  glEnableClientState(GL_VERTEX_ARRAY);

  if (m_NormalIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[NORMAL_VBO]);
    glNormalPointer(GL_FLOAT, 0, 0);
    glEnableClientState(GL_NORMAL_ARRAY);
  } else {
    glNormal3f(2,2,2); // tells the shader to disable lighting
  }
  if (m_TCIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[TEXCOORD_VBO]);
    glTexCoordPointer(2, GL_FLOAT, 0, 0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  } else {
    glTexCoord2f(0,0);
  }
  if (m_COLIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[COLOR_VBO]);
    glColorPointer(4, GL_FLOAT, 0, 0);
    glEnableClientState(GL_COLOR_ARRAY);
  }  else {
    glColor4f(m_DefColor.x, m_DefColor.y, m_DefColor.z, m_DefColor.w);
  }

  switch (m_meshType) {
    case MT_LINES :  
      glDrawElements(GL_LINES, GLsizei(count), GL_UNSIGNED_INT, 0);
      break;
    case MT_TRIANGLES:
      glDrawElements(GL_TRIANGLES, GLsizei(count), GL_UNSIGNED_INT, 0);
      break;
    default :
      throw std::runtime_error("rendering unsupported mesh type"); 
  }

  glDisableClientState(GL_VERTEX_ARRAY);
  if (m_NormalIndices.size() == m_VertIndices.size())
    glDisableClientState(GL_NORMAL_ARRAY);
  if (m_TCIndices.size() == m_VertIndices.size())
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  if (m_COLIndices.size() == m_VertIndices.size())
    glDisableClientState(GL_COLOR_ARRAY);
}

void RenderMeshGL::RenderOpaqueGeometry() {
  RenderGeometry(m_IndexVBOOpaque, m_splitIndex);
}

void RenderMeshGL::RenderTransGeometryFront() {
  PrepareTransBuffers(m_IndexVBOFront, GetFrontPointList());
  RenderGeometry(m_IndexVBOFront, m_FrontPointList.size()*m_VerticesPerPoly);
}

void RenderMeshGL::RenderTransGeometryBehind() {
  PrepareTransBuffers(m_IndexVBOBehind, GetBehindPointList());
  RenderGeometry(m_IndexVBOBehind, m_BehindPointList.size()*m_VerticesPerPoly);
}

void RenderMeshGL::RenderTransGeometryInside() {
  PrepareTransBuffers(m_IndexVBOInside, GetSortedInPointList());
  RenderGeometry(m_IndexVBOInside, m_InPointList.size()*m_VerticesPerPoly);
}

void RenderMeshGL::InitRenderer() {
  m_bGLInitialized = true;

  glGenBuffers(DATA_VBO_COUNT, m_VBOs);
  glGenBuffers(1, &m_IndexVBOOpaque);
  glGenBuffers(1, &m_IndexVBOFront);
  glGenBuffers(1, &m_IndexVBOBehind);
  glGenBuffers(1, &m_IndexVBOInside);

  PrepareOpaqueBuffers();
}

void RenderMeshGL::GeometryHasChanged(bool bUpdateAABB, bool bUpdateKDtree) {
  RenderMesh::GeometryHasChanged(bUpdateAABB, bUpdateKDtree);
  if (m_bGLInitialized) PrepareOpaqueBuffers();
}

void RenderMeshGL::PrepareTransBuffers(GLuint IndexVBO, const SortIndexPVec& list) {
  if (list.empty()) return;

  IndexVec      VertIndices;

  VertIndices.reserve(list.size());
  for (SortIndexPVec::const_iterator index = list.begin();
       index != list.end();
       index++) {
    size_t iIndex = (*index)->m_index;
    for (size_t i = 0;i<m_VerticesPerPoly;i++)
      VertIndices.push_back(m_VertIndices[iIndex+i]);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, VertIndices.size()*sizeof(UINT32), &VertIndices[0], GL_STREAM_DRAW);
}

// since OpenGL is a "piece-of-shit" API that does only support
// a single index array and not multiple arrays per component
// e.g. one for position, color, etc. So we have to maintain
// separate color, normal, vertex and texcoord buffers, if the indices
// differ
void RenderMeshGL::UnrollArrays() {
  bool bSeparateArraysNecessary = 
    (m_NormalIndices.size() == m_VertIndices.size() && m_NormalIndices != m_VertIndices) ||
    (m_COLIndices.size() == m_VertIndices.size() && m_COLIndices != m_VertIndices) ||
    (m_TCIndices.size() == m_VertIndices.size() && m_TCIndices != m_VertIndices);

  if (!bSeparateArraysNecessary) return;

  VertVec       vertices(m_VertIndices.size());
  NormVec       normals;
  if (m_NormalIndices.size() == m_VertIndices.size())
    normals.resize(m_VertIndices.size());
  ColorVec      colors;
  if (m_COLIndices.size() == m_VertIndices.size())
    colors.resize(m_VertIndices.size());
  TexCoordVec   texcoords;
  if (m_TCIndices.size() == m_VertIndices.size())
    texcoords.resize(m_VertIndices.size());

  for (size_t i = 0;i<m_VertIndices.size();i++) {
    vertices[i] = m_vertices[m_VertIndices[i]];
    if (m_NormalIndices.size() == m_VertIndices.size()) 
      normals[i] = m_normals[m_NormalIndices[i]];
    if (m_COLIndices.size() == m_VertIndices.size()) 
      colors[i] = m_colors[m_COLIndices[i]];
    if (m_TCIndices.size() == m_VertIndices.size()) 
      texcoords[i] = m_texcoords[m_TCIndices[i]];
  }

  m_vertices = vertices;
  m_normals = normals;
  m_texcoords = texcoords;
  m_colors = colors;

  // effectively disable indices
  for (size_t i = 0;i<m_VertIndices.size();i++)  m_VertIndices[i] = i;  

  // equalize index arrays
  if (m_NormalIndices.size() == m_VertIndices.size()) m_NormalIndices = m_VertIndices;
  if (m_COLIndices.size() == m_VertIndices.size()) m_COLIndices = m_VertIndices;
  if (m_TCIndices.size() == m_VertIndices.size()) m_TCIndices = m_VertIndices;

  GeometryHasChanged(false,false);
}
