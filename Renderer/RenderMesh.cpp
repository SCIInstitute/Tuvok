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

//!    File   : RenderMesh.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include "RenderMesh.h"
#include "KDTree.h"
#include <cassert>

using namespace tuvok;

RenderMesh::RenderMesh(const Mesh& other) : 
  Mesh(other.GetVertices(),other.GetNormals(),
       other.GetTexCoords(),other.GetColors(),
       other.GetVertexIndices(),other.GetNormalIndices(),
       other.GetTexCoordIndices(),other.GetColorIndices(),
       false, false),
   m_bActive(true)
{
  SplitOpaqueFromTransparent();
  if (other.GetKDTree()) ComputeKDTree();
}

RenderMesh::RenderMesh(const VertVec& vertices, const NormVec& normals,
           const TexCoordVec& texcoords, const ColorVec& colors,
           const IndexVec& vIndices, const IndexVec& nIndices,
           const IndexVec& tIndices, const IndexVec& cIndices,
           bool bBuildKDTree, bool bScaleToUnitCube) :
  Mesh(vertices,normals,texcoords,colors,
       vIndices,nIndices,tIndices,cIndices, false, bScaleToUnitCube),
   m_bActive(true)
{
  SplitOpaqueFromTransparent();
  // moved this computation after the resorting as it invalides the indices
  // stored in the KD-Tree
  if (bBuildKDTree) m_KDTree = new KDTree(this);
}

void RenderMesh::Swap(size_t i, size_t j) {
  std::swap(m_VertIndices[i], m_VertIndices[j]);
  std::swap(m_COLIndices[i], m_COLIndices[j]);

  if (m_NormalIndices.size()) std::swap(m_NormalIndices[i], m_NormalIndices[j]);
  if (m_TCIndices.size())     std::swap(m_TCIndices[i], m_TCIndices[j]);
}

bool RenderMesh::isTransparent(size_t i, float fTreshhold) {
  return m_colors[m_COLIndices[i].x].w < fTreshhold ||
         m_colors[m_COLIndices[i].y].w < fTreshhold ||
         m_colors[m_COLIndices[i].z].w < fTreshhold;
}

void RenderMesh::SplitOpaqueFromTransparent() {
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