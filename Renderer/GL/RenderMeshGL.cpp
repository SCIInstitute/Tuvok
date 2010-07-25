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
}

RenderMeshGL::~RenderMeshGL() {
  if (m_bGLInitialized) {
    glDeleteBuffers(DATA_VBO_COUNT, m_VBOs);
    glDeleteBuffers(INDEX_VBO_COUNT, m_IndexVBOsOpaque);
    glDeleteBuffers(INDEX_VBO_COUNT, m_IndexVBOsFront);
    glDeleteBuffers(INDEX_VBO_COUNT, m_IndexVBOsBehind);
  }
}

void RenderMeshGL::PrepareOpaqueBuffers() {
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexVBOsOpaque[POSITION_INDEX_VBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32), &m_VertIndices[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[POSITION_VBO]);
  glBufferData(GL_ARRAY_BUFFER, m_vertices.size()*sizeof(float)*3, &m_vertices[0], GL_STATIC_DRAW);

  if (m_NormalIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexVBOsOpaque[NORMAL_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32), &m_NormalIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[NORMAL_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_normals.size()*sizeof(float)*3, &m_normals[0], GL_STATIC_DRAW);
  }
  if (m_TCIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexVBOsOpaque[TEXCOORD_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32), &m_TCIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[TEXCOORD_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_texcoords.size()*sizeof(float)*2, &m_texcoords[0], GL_STATIC_DRAW);
  }
  if (m_COLIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexVBOsOpaque[COLOR_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(UINT32), &m_COLIndices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[COLOR_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_colors.size()*sizeof(float)*4, &m_colors[0], GL_STATIC_DRAW);
  }
}

void RenderMeshGL::RenderGeometry(GLuint IndexVBOs[INDEX_VBO_COUNT], size_t count) {
  if (!m_bGLInitialized || count == 0) return;

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOs[POSITION_INDEX_VBO]);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[POSITION_VBO]);
  glVertexPointer(3, GL_FLOAT, 0, 0);
  glEnableClientState(GL_VERTEX_ARRAY);

  if (m_NormalIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOs[NORMAL_INDEX_VBO]);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[NORMAL_VBO]);
    glNormalPointer(GL_FLOAT, 0, 0);
    glEnableClientState(GL_NORMAL_ARRAY);
  } else {
    glNormal3f(2,2,2); // tells the shader to disable lighting
  }
  if (m_TCIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOs[TEXCOORD_INDEX_VBO]);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[TEXCOORD_VBO]);
    glTexCoordPointer(2, GL_FLOAT, 0, 0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  } else {
    glTexCoord2f(0,0);
  }
  if (m_COLIndices.size() == m_VertIndices.size()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOs[COLOR_INDEX_VBO]);
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
  RenderGeometry(m_IndexVBOsOpaque, m_splitIndex);
}

void RenderMeshGL::RenderTransGeometryFront() {
  PrepareTransBuffers(m_IndexVBOsFront, GetFrontPointList());
  RenderGeometry(m_IndexVBOsFront, m_FrontPointList.size()*m_VerticesPerPoly);
}

void RenderMeshGL::RenderTransGeometryBehind() {
  PrepareTransBuffers(m_IndexVBOsBehind, GetBehindPointList());
  RenderGeometry(m_IndexVBOsBehind, m_BehindPointList.size()*m_VerticesPerPoly);
}

void RenderMeshGL::InitRenderer() {
  m_bGLInitialized = true;

  glGenBuffers(DATA_VBO_COUNT, m_VBOs);
  glGenBuffers(INDEX_VBO_COUNT, m_IndexVBOsOpaque);
  glGenBuffers(INDEX_VBO_COUNT, m_IndexVBOsFront);
  glGenBuffers(INDEX_VBO_COUNT, m_IndexVBOsBehind);

  PrepareOpaqueBuffers();
}

void RenderMeshGL::GeometryHasChanged(bool bUpdateAABB, bool bUpdateKDtree) {
  RenderMesh::GeometryHasChanged(bUpdateAABB, bUpdateKDtree);
  if (m_bGLInitialized) PrepareOpaqueBuffers();
}

void RenderMeshGL::PrepareTransBuffers(GLuint IndexVBOs[INDEX_VBO_COUNT], const SortIndexList& list) {
  if (list.size() == 0) return;

  IndexVec      VertIndices;
  IndexVec      NormalIndices;
  IndexVec      TCIndices;
  IndexVec      COLIndices;

  VertIndices.reserve(list.size());
  for (SortIndexList::const_iterator index = list.begin();
       index != list.end();
       index++) {
    size_t iIndex = index->m_index;
    for (size_t i = 0;i<m_VerticesPerPoly;i++)
      VertIndices.push_back(m_VertIndices[iIndex+i]);
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOs[POSITION_INDEX_VBO]);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, VertIndices.size()*sizeof(UINT32), &VertIndices[0], GL_STREAM_DRAW);

  if (m_NormalIndices.size() == m_VertIndices.size()) {
    NormalIndices.reserve(list.size());
    for (SortIndexList::const_iterator index = list.begin();
         index != list.end();
         index++) {
      size_t iIndex = index->m_index;
      for (size_t i = 0;i<m_VerticesPerPoly;i++)
        NormalIndices.push_back(m_NormalIndices[iIndex+i]);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOs[NORMAL_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, NormalIndices.size()*sizeof(UINT32), &NormalIndices[0], GL_STREAM_DRAW);
  }
  if (m_TCIndices.size() == m_VertIndices.size()) {
    TCIndices.reserve(list.size());
    for (SortIndexList::const_iterator index = list.begin();
         index != list.end();
         index++) {
      size_t iIndex = index->m_index;
      for (size_t i = 0;i<m_VerticesPerPoly;i++)
        TCIndices.push_back(m_TCIndices[iIndex+i]);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOs[TEXCOORD_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, TCIndices.size()*sizeof(UINT32), &TCIndices[0], GL_STREAM_DRAW);
  }
  if (m_COLIndices.size() == m_VertIndices.size()) {
    COLIndices.reserve(list.size());
    for (SortIndexList::const_iterator index = list.begin();
         index != list.end();
         index++) {
      size_t iIndex = index->m_index;
      for (size_t i = 0;i<m_VerticesPerPoly;i++)
        COLIndices.push_back(m_COLIndices[iIndex+i]);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBOs[COLOR_INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, COLIndices.size()*sizeof(UINT32), &COLIndices[0], GL_STREAM_DRAW);
  }
}