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
#include <cstring>
#include <stdexcept>
#include "RenderMeshGL.h"
#include "KDTree.h"

using namespace tuvok;

RenderMeshGL::RenderMeshGL(const Mesh& other) : 
  RenderMesh(other),
  m_bGLInitialized(false),
  m_bSpheresEnabled(false)
{
  UnrollArrays();
  m_SphereColor[0] = 1.0; m_SphereColor[1] = m_SphereColor[2] = 0.0;
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
  m_bGLInitialized(false),
  m_bSpheresEnabled(false)
{
  UnrollArrays();
  m_SphereColor[0] = 1.0; m_SphereColor[1] = m_SphereColor[2] = 0.0;
}

RenderMeshGL::~RenderMeshGL() {
  if (m_bGLInitialized) {
    glDeleteBuffers(DATA_VBO_COUNT, m_VBOs);
    glDeleteBuffers(1, &m_IndexVBOOpaque);
    glDeleteBuffers(1, &m_IndexVBOFront);
    glDeleteBuffers(1, &m_IndexVBOBehind);
    glDeleteBuffers(1, &m_IndexVBOInside);
    glDeleteBuffers(1, &m_SpheresVBO);
  }
}

void RenderMeshGL::PrepareOpaqueBuffers() {
  if (m_Data.m_VertIndices.empty()) return;

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexVBOOpaque);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_splitIndex*sizeof(uint32_t), &m_Data.m_VertIndices[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[POSITION_VBO]);
  glBufferData(GL_ARRAY_BUFFER, m_Data.m_vertices.size()*sizeof(float)*3, &m_Data.m_vertices[0], GL_STATIC_DRAW);

  if (m_Data.m_NormalIndices.size() == m_Data.m_VertIndices.size()) {
    GL(glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[NORMAL_VBO]));
    GL(glBufferData(GL_ARRAY_BUFFER, m_Data.m_normals.size()*sizeof(float)*3,
                    &m_Data.m_normals[0], GL_STATIC_DRAW));
  }
  if (m_Data.m_TCIndices.size() == m_Data.m_VertIndices.size()) {
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[TEXCOORD_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_Data.m_texcoords.size()*sizeof(float)*2, &m_Data.m_texcoords[0], GL_STATIC_DRAW);
  }
  if (m_Data.m_COLIndices.size() == m_Data.m_VertIndices.size()) {
    glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[COLOR_VBO]);
    glBufferData(GL_ARRAY_BUFFER, m_Data.m_colors.size()*sizeof(float)*4, &m_Data.m_colors[0], GL_STATIC_DRAW);
  }
  // if we are rendering the mesh as a line, we want to put a sphere at each
  // vertex. generate a VBO for the sphere geometry.
  if(m_meshType == MT_LINES) {
    this->PrepareIsocahedron();
    GL(glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[SPHERE_VBO]));
    // assumes m_Isocahedron[0].size() == m_Isocahedron[i].size(), forall i
    assert(m_Isocahedron[0].size() == m_Isocahedron[1].size()); // etc.
    GL(glBufferData(GL_ARRAY_BUFFER,
                    m_Isocahedron.size() * m_Isocahedron[0].size() *
                    m_Isocahedron[0][0].size() * sizeof(float),
                    &m_Isocahedron[0], GL_STATIC_DRAW));
  }
}

void RenderMeshGL::RenderGeometry(GLuint IndexVBO, size_t count) {
  if (!m_bGLInitialized || count == 0) return;

  GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBO));
  GL(glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[POSITION_VBO]));
  GL(glVertexPointer(3, GL_FLOAT, 0, 0));
  GL(glEnableClientState(GL_VERTEX_ARRAY));

  if (m_Data.m_NormalIndices.size() == m_Data.m_VertIndices.size()) {
    GL(glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[NORMAL_VBO]));
    GL(glNormalPointer(GL_FLOAT, 0, 0));
    GL(glEnableClientState(GL_NORMAL_ARRAY));
  } else {
    GL(glNormal3f(2,2,2)); // tells the shader to disable lighting
  }
  if (m_Data.m_TCIndices.size() == m_Data.m_VertIndices.size()) {
    GL(glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[TEXCOORD_VBO]));
    GL(glTexCoordPointer(2, GL_FLOAT, 0, 0));
    GL(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
  } else {
    GL(glTexCoord2f(0,0));
  }
  if (m_Data.m_COLIndices.size() == m_Data.m_VertIndices.size()) {
    GL(glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[COLOR_VBO]));
    GL(glColorPointer(4, GL_FLOAT, 0, 0));
    GL(glEnableClientState(GL_COLOR_ARRAY));
  }  else {
    GL(glColor4f(m_DefColor.x, m_DefColor.y, m_DefColor.z, m_DefColor.w));
  }

  switch (m_meshType) {
    case MT_LINES :  
      GL(glDrawElements(GL_LINES, GLsizei(count), GL_UNSIGNED_INT, 0));
      break;
    case MT_TRIANGLES:
      GL(glDrawElements(GL_TRIANGLES, GLsizei(count), GL_UNSIGNED_INT, 0));
      break;
    default :
      throw std::runtime_error("rendering unsupported mesh type"); 
  }

  GL(glDisableClientState(GL_VERTEX_ARRAY));
  if (m_Data.m_NormalIndices.size() == m_Data.m_VertIndices.size())
    glDisableClientState(GL_NORMAL_ARRAY);
  if (m_Data.m_TCIndices.size() == m_Data.m_VertIndices.size())
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  if (m_Data.m_COLIndices.size() == m_Data.m_VertIndices.size())
    glDisableClientState(GL_COLOR_ARRAY);
}

void RenderMeshGL::RenderOpaqueGeometry() {
  RenderGeometry(m_IndexVBOOpaque, m_splitIndex);

  if(m_meshType == MT_LINES && m_bSpheresEnabled) {
    GL(glColor3f(m_SphereColor[0], m_SphereColor[1], m_SphereColor[2]));
    GL(glEnableClientState(GL_VERTEX_ARRAY));
    GL(glDisable(GL_LIGHTING));

    GL(glBindBuffer(GL_ARRAY_BUFFER, m_VBOs[SPHERE_VBO]));

    GL(glVertexPointer(3, GL_FLOAT, 0, 0));
    // neat trick: our normals are the same as our vertices!
    // .. but we don't want these lit.  Oh well.
    //GL(glNormalPointer(GL_FLOAT, 0, 0));
    for(VertVec::const_iterator v = this->m_Data.m_vertices.begin();
        v != this->m_Data.m_vertices.end(); ++v) {
      glTranslatef((*v)[0], (*v)[1], (*v)[2]);
      GL(glDrawArrays(GL_TRIANGLES, 0, m_Isocahedron.size() *
                      m_Isocahedron[0].size() * m_Isocahedron[0][0].size()));
      glTranslatef(-(*v)[0], -(*v)[1], -(*v)[2]);
    }

    GL(glDisableClientState(GL_VERTEX_ARRAY));
    GL(glEnable(GL_LIGHTING));
  }
}

void RenderMeshGL::RenderTransGeometryFront() {
  PrepareTransBuffers(m_IndexVBOFront, GetFrontPointList(true));
  RenderGeometry(m_IndexVBOFront, m_FrontPointList.size()*m_VerticesPerPoly);
}

void RenderMeshGL::RenderTransGeometryBehind() {
  PrepareTransBuffers(m_IndexVBOBehind, GetBehindPointList(true));
  RenderGeometry(m_IndexVBOBehind, m_BehindPointList.size()*m_VerticesPerPoly);
}

void RenderMeshGL::RenderTransGeometryInside() {
  PrepareTransBuffers(m_IndexVBOInside, GetInPointList(true));
  RenderGeometry(m_IndexVBOInside, m_InPointList.size()*m_VerticesPerPoly);
}

void RenderMeshGL::InitRenderer() {
  m_bGLInitialized = true;

  glGenBuffers(DATA_VBO_COUNT, m_VBOs);
  glGenBuffers(1, &m_IndexVBOOpaque);
  glGenBuffers(1, &m_IndexVBOFront);
  glGenBuffers(1, &m_IndexVBOBehind);
  glGenBuffers(1, &m_IndexVBOInside);
  glGenBuffers(1, &m_SpheresVBO);

  PrepareOpaqueBuffers();
}

void RenderMeshGL::GeometryHasChanged(bool bUpdateAABB, bool bUpdateKDtree) {
  RenderMesh::GeometryHasChanged(bUpdateAABB, bUpdateKDtree);
  if (m_bGLInitialized) PrepareOpaqueBuffers();
}

void RenderMeshGL::PrepareTransBuffers(GLuint IndexVBO, const SortIndexPVec& list) {
  if (list.empty()) return;

  IndexVec VertIndices;
  VertIndices.reserve(list.size());
  for(SortIndexPVec::const_iterator index = list.begin(); index != list.end();
      ++index) {
    size_t iIndex = (*index)->m_index;
    for(size_t i = 0;i<m_VerticesPerPoly;i++) {
      VertIndices.push_back(m_Data.m_VertIndices[iIndex+i]);
    }
  }

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexVBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, VertIndices.size()*sizeof(uint32_t),
               &VertIndices[0], GL_STREAM_DRAW);
}

// since OpenGL is a "piece-of-shit" API that does only support
// a single index array and not multiple arrays per component
// e.g. one for position, color, etc. So we have to maintain
// separate color, normal, vertex and texcoord buffers, if the indices
// differ
void RenderMeshGL::UnrollArrays() {
  bool bSeparateArraysNecessary = 
    (m_Data.m_NormalIndices.size() == m_Data.m_VertIndices.size() && m_Data.m_NormalIndices != m_Data.m_VertIndices) ||
    (m_Data.m_COLIndices.size() == m_Data.m_VertIndices.size() && m_Data.m_COLIndices != m_Data.m_VertIndices) ||
    (m_Data.m_TCIndices.size() == m_Data.m_VertIndices.size() && m_Data.m_TCIndices != m_Data.m_VertIndices);

  if (!bSeparateArraysNecessary) return;

  VertVec       vertices(m_Data.m_VertIndices.size());
  NormVec       normals;
  if (m_Data.m_NormalIndices.size() == m_Data.m_VertIndices.size())
    normals.resize(m_Data.m_VertIndices.size());
  ColorVec      colors;
  if (m_Data.m_COLIndices.size() == m_Data.m_VertIndices.size())
    colors.resize(m_Data.m_VertIndices.size());
  TexCoordVec   texcoords;
  if (m_Data.m_TCIndices.size() == m_Data.m_VertIndices.size())
    texcoords.resize(m_Data.m_VertIndices.size());

  for (size_t i = 0;i<m_Data.m_VertIndices.size();i++) {
    vertices[i] = m_Data.m_vertices[m_Data.m_VertIndices[i]];
    if (m_Data.m_NormalIndices.size() == m_Data.m_VertIndices.size()) 
      normals[i] = m_Data.m_normals[m_Data.m_NormalIndices[i]];
    if (m_Data.m_COLIndices.size() == m_Data.m_VertIndices.size()) 
      colors[i] = m_Data.m_colors[m_Data.m_COLIndices[i]];
    if (m_Data.m_TCIndices.size() == m_Data.m_VertIndices.size()) 
      texcoords[i] = m_Data.m_texcoords[m_Data.m_TCIndices[i]];
  }

  m_Data.m_vertices = vertices;
  m_Data.m_normals = normals;
  m_Data.m_texcoords = texcoords;
  m_Data.m_colors = colors;

  // effectively disable indices
  for (size_t i = 0; i < m_Data.m_VertIndices.size(); i++) {
    m_Data.m_VertIndices[i] = uint32_t(i);
  }

  // equalize index arrays
  if (m_Data.m_NormalIndices.size() == m_Data.m_VertIndices.size()) m_Data.m_NormalIndices = m_Data.m_VertIndices;
  if (m_Data.m_COLIndices.size() == m_Data.m_VertIndices.size()) m_Data.m_COLIndices = m_Data.m_VertIndices;
  if (m_Data.m_TCIndices.size() == m_Data.m_VertIndices.size()) m_Data.m_TCIndices = m_Data.m_VertIndices;

  GeometryHasChanged(false,false);
}

// generate the geometry for an isocahedron && fill m_Isocahedron.
void RenderMeshGL::PrepareIsocahedron() {
  const double X = .525731112119133606 / 500.0;
  const double Z = .850650808352039932 / 500.0;
  const GLfloat iso[12][3] = {
    {-X, 0.0, Z}, {X, 0.0, Z}, {-X, 0.0, -Z}, {X, 0.0, -Z},
    {0.0, Z, X}, {0.0, Z, -X}, {0.0, -Z, X}, {0.0, -Z, -X},
    {Z, X, 0.0}, {-Z, X, 0.0}, {Z, -X, 0.0}, {-Z, -X, 0.0}
  };
  const GLuint indices[20][3] = {
    {0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},
    {8,10,1}, {8,3,10}, {5,3,8}, {5,2,3}, {2,7,3},
    {7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6},
    {6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11}
  };

  std::memset(&m_Isocahedron[0], 0, sizeof(m_Isocahedron.size()));
  for(size_t i=0; i < m_Isocahedron.size(); ++i) { // foreach triangle
    GLuint idx[3] = { indices[i][0], indices[i][1], indices[i][2] };
    point a = {{ iso[idx[0]][0], iso[idx[0]][1], iso[idx[0]][2] }};
    point b = {{ iso[idx[1]][0], iso[idx[1]][1], iso[idx[1]][2] }};
    point c = {{ iso[idx[2]][0], iso[idx[2]][1], iso[idx[2]][2] }};
    triangle t = {{a, b, c}};
    m_Isocahedron[i] = t;
  }
}
