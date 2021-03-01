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

//!    File   : RenderMeshGL.h
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef TUVOK_RENDERMESHGL_H
#define TUVOK_RENDERMESHGL_H

#include <array>
#include "../RenderMesh.h"
#include "GLInclude.h"

namespace tuvok {

class RenderMeshGL : public RenderMesh 
{
public:
  RenderMeshGL(const Mesh& other);
  RenderMeshGL(const VertVec& vertices, const NormVec& normals, 
       const TexCoordVec& texcoords, const ColorVec& colors,
       const IndexVec& vIndices, const IndexVec& nIndices, 
       const IndexVec& tIndices, const IndexVec& cIndices,
       bool bBuildKDTree, bool bScaleToUnitCube,       
       const std::wstring& desc, EMeshType meshType,
       const FLOATVECTOR4& defColor);
  ~RenderMeshGL();

  virtual void InitRenderer();
  virtual void RenderOpaqueGeometry();
  virtual void RenderTransGeometryFront();
  virtual void RenderTransGeometryBehind();
  virtual void RenderTransGeometryInside();
  virtual void GeometryHasChanged(bool bUpdateAABB, bool bUpdateKDtree);

  typedef std::array<float, 3> color;
  /// if on, adds some simple geometry at the location of every vertex.
  void EnableVertexMarkers(bool b);
  /// changes the color of the markers used for vertices.
  void SetVertexMarkerColor(color c);

private:
  bool   m_bGLInitialized;

  enum
  {
      POSITION_VBO = 0,
      NORMAL_VBO,
      TEXCOORD_VBO,
      COLOR_VBO,
      SPHERE_VBO,
      DATA_VBO_COUNT
  };

  enum
  {
      POSITION_INDEX_VBO = 0,
      NORMAL_INDEX_VBO,
      TEXCOORD_INDEX_VBO,
      COLOR_INDEX_VBO,
      INDEX_VBO_COUNT
  };

  GLuint m_VBOs[DATA_VBO_COUNT];
  GLuint m_IndexVBOOpaque;
  GLuint m_IndexVBOFront;
  GLuint m_IndexVBOBehind;
  GLuint m_IndexVBOInside;
  GLuint m_SpheresVBO;

  typedef std::array<float, 3> point;
  typedef std::array<point, 3> triangle;
  typedef std::array<triangle, 20> isocahedron;
  isocahedron m_Isocahedron;
  bool   m_bSpheresEnabled;
  color  m_SphereColor;

  void PrepareOpaqueBuffers();
  void PrepareTransBuffers(GLuint IndexVBO,
                           const SortIndexPVec& list);
  void RenderGeometry(GLuint IndexVBO, size_t count);

  void UnrollArrays();

  /// generate the geometry for an isocahedron, filling m_Isocahedron.
  void PrepareIsocahedron();
};

}
#endif // TUVOK_RENDERMESHGL_H
