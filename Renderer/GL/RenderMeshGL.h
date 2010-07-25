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

#ifndef RENDERMESHGL_H
#define RENDERMESHGL_H

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
       const std::string& desc, EMeshType meshType);
  ~RenderMeshGL();

  virtual void InitRenderer();
  virtual void RenderOpaqueGeometry();
  virtual void RenderTransGeometryFront();
  virtual void RenderTransGeometryBehind();
  virtual void GeometryHasChanged(bool bUpdateAABB, bool bUpdateKDtree);

private:
  bool   m_bGLInitialized;

  enum
  {
      POSITION_VBO = 0,
      NORMAL_VBO,
      TEXCOORD_VBO,
      COLOR_VBO,
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
  GLuint m_IndexVBOsOpaque[INDEX_VBO_COUNT];
  GLuint m_IndexVBOsFront[INDEX_VBO_COUNT];
  GLuint m_IndexVBOsBehind[INDEX_VBO_COUNT];

  void PrepareOpaqueBuffers();
  void PrepareTransBuffers(GLuint IndexVBOs[INDEX_VBO_COUNT],
                           const SortIndexList& list);
  void RenderGeometry(GLuint VBOs[INDEX_VBO_COUNT], size_t count);
};

}
#endif // RENDERMESHGL_H
