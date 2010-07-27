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
#include <algorithm>

using namespace tuvok;


SortIndex::SortIndex(size_t index, const RenderMesh* m) :
  m_index(index),
  m_mesh(m)
{
  ComputeCentroid();
}

void SortIndex::UpdateDistance() {
  fDepth = (m_mesh->m_viewPoint-m_centroid).length();
}

void SortIndex::ComputeCentroid() {
  size_t vertexCount = m_mesh->GetVerticesPerPoly();

  m_centroid = FLOATVECTOR3(0,0,0);
  for (size_t iVertex = 0;iVertex<vertexCount;iVertex++) {
    m_centroid = m_centroid + m_mesh->GetVertices()[m_mesh->GetVertexIndices()[m_index+iVertex]];
  }
  m_centroid /= float(vertexCount);
}

RenderMesh::RenderMesh(const Mesh& other, float fTransTreshhold) : 
  Mesh(other.GetVertices(),other.GetNormals(),
       other.GetTexCoords(),other.GetColors(),
       other.GetVertexIndices(),other.GetNormalIndices(),
       other.GetTexCoordIndices(),other.GetColorIndices(),
       false, false, other.Name(),other.GetMeshType()),
   m_bActive(true),
   m_fTransTreshhold(fTransTreshhold),
   m_bSortOver(false),
   m_viewPoint(FLOATVECTOR3(0,0,0)),
   m_VolumeMin(FLOATVECTOR3(0,0,0)),
   m_VolumeMax(FLOATVECTOR3(0,0,0)),
   m_QuadrantsDirty(true),
   m_FIBHashDirty(true)
{
  m_Quadrants.resize(27);
  SplitOpaqueFromTransparent();
  GeometryHasChanged(other.GetKDTree() != 0, other.GetKDTree() != 0);
}

RenderMesh::RenderMesh(const VertVec& vertices, const NormVec& normals,
           const TexCoordVec& texcoords, const ColorVec& colors,
           const IndexVec& vIndices, const IndexVec& nIndices,
           const IndexVec& tIndices, const IndexVec& cIndices,
           bool bBuildKDTree, bool bScaleToUnitCube, const std::string& desc,
           EMeshType meshType,
           float fTransTreshhold) :
  Mesh(vertices,normals,texcoords,colors,
       vIndices,nIndices,tIndices,cIndices,
       false, bScaleToUnitCube,desc,meshType),
   m_bActive(true),
   m_fTransTreshhold(fTransTreshhold),
   m_bSortOver(false),
   m_viewPoint(FLOATVECTOR3(0,0,0)),
   m_VolumeMin(FLOATVECTOR3(0,0,0)),
   m_VolumeMax(FLOATVECTOR3(0,0,0)),
   m_QuadrantsDirty(true),
   m_FIBHashDirty(true)
{
  m_Quadrants.resize(27);
  SplitOpaqueFromTransparent();
  if (bBuildKDTree) ComputeKDTree();
}

void RenderMesh::Swap(size_t i, size_t j) {
  for (size_t iVertex = 0;iVertex<m_VerticesPerPoly;iVertex++) {
    std::swap(m_VertIndices[i+iVertex], m_VertIndices[j+iVertex]);
    // we know that this mesh has to have colors otherwise 
    // this method would not be called by the SplitOpaqueFromTransparent
    // method
    std::swap(m_COLIndices[i+iVertex], m_COLIndices[j+iVertex]);
  
    if (m_NormalIndices.size()) 
      std::swap(m_NormalIndices[i+iVertex], m_NormalIndices[j+iVertex]);
    if (m_TCIndices.size())     
      std::swap(m_TCIndices[i+iVertex], m_TCIndices[j+iVertex]);
  }
}

bool RenderMesh::isTransparent(size_t i) {
  for (size_t iVertex = 0;iVertex<m_VerticesPerPoly;iVertex++) {
    if (m_colors[m_COLIndices[i+iVertex]].w < m_fTransTreshhold)
      return true;
  }
  return false;
}


void RenderMesh::SplitOpaqueFromTransparent() {
  size_t prevIndex  = m_splitIndex;

  if (m_COLIndices.size() == 0) {
    if (m_DefColor.w < m_fTransTreshhold ) 
      m_splitIndex = 0;
    else
      m_splitIndex = m_VertIndices.size();
  } else {
    assert(Validate(true));

    // find first transparent polygon
    size_t iTarget = 0;
    for (;iTarget<m_COLIndices.size();iTarget+=m_VerticesPerPoly) {
      if (isTransparent(iTarget)) break;
    }

    // swap opaque poly with transparent
    size_t iStart = iTarget;
    for (size_t iSource = iStart+m_VerticesPerPoly;iSource<m_COLIndices.size();iSource+=m_VerticesPerPoly) {
      if (!isTransparent(iSource)) {
        Swap(iSource, iTarget);
        iTarget+=m_VerticesPerPoly;
      }
    }
    m_splitIndex = iTarget;
  }

  if (prevIndex != m_splitIndex) {
    GeometryHasChanged(false,false);
  }
}

void RenderMesh::SetTransTreshhold(float fTransTreshhold) {
  if (m_fTransTreshhold != fTransTreshhold) {
    m_fTransTreshhold = fTransTreshhold;
    SplitOpaqueFromTransparent();
    if (m_KDTree) ComputeKDTree();
  }
}


void RenderMesh::SetDefaultColor(const FLOATVECTOR4& color) {
  float prevAlpha = m_DefColor.w;
  m_DefColor = color;

  // now check if we need to rebin the colors in opaque and transparent:
  // a) if the alpha component has changed
  // b) if default color is used (i.e. if no colors are specified)
  // c) if the change in alpha 
  //      1) pushes the opacity above the threshold while it was below before
  //      2) or below the threshold while it was above before
  if (prevAlpha != color.w &&
      m_COLIndices.size() == 0 &&
      ((prevAlpha < m_fTransTreshhold && m_DefColor.w >= m_fTransTreshhold) ||
       (prevAlpha >= m_fTransTreshhold && m_DefColor.w < m_fTransTreshhold))) {
    SplitOpaqueFromTransparent();
    if (m_KDTree) ComputeKDTree();
  } 
}

void RenderMesh::GeometryHasChanged(bool bUpdateAABB, bool bUpdateKDtree) {
  Mesh::GeometryHasChanged(bUpdateAABB, bUpdateKDtree);

  // create sortindex list with all tris
  m_allPolys.clear();
  for (size_t i = m_splitIndex;i<m_VertIndices.size();i+=m_VerticesPerPoly) {
    m_allPolys.push_back(SortIndex(i, this));
  }

  m_QuadrantsDirty = true;
  m_FIBHashDirty = true;
}


void RenderMesh::SetVolumeAABB(const FLOATVECTOR3& min, 
                               const FLOATVECTOR3& max) {
  if (m_VolumeMin != min || m_VolumeMax != max) {
    m_VolumeMin = min;
    m_VolumeMax = max;
    m_QuadrantsDirty = true;
  }
}

void RenderMesh::SetUserPos(const FLOATVECTOR3& viewPoint) {
  if (m_viewPoint != viewPoint) {
    m_viewPoint = viewPoint;
    m_FIBHashDirty = true;
  }
}

// This code sorts the polygons (or more precisely their centroids) into the
// 27 quadrants "around and inside" the axis aligned volume. The quadrants
// are created by the 6 clip planes of the cube, they are enumarted
// 0 : x<box   , y<box   , z<box
// 1 : x inside, y<box   , z<box
// 2 : x>box   , y<box   , z<box
// 3 : x<box   , y inside, z<box
// ...
// so x is fastes, then y and finally z
void RenderMesh::SortTransparentDataIntoQuadrants() {
  m_QuadrantsDirty = false;
  for (int i = 0;i<27;i++) m_Quadrants[i].clear();
  m_InPointList.clear();

  // is the entire mesh opaque ?
  if (m_splitIndex == m_VertIndices.size()) return;

  for (SortIndexVec::iterator poly = m_allPolys.begin();
       poly != m_allPolys.end();
       poly++) {

    size_t index = PosToQuadrant(poly->m_centroid);
    m_Quadrants[index].push_back(&(*poly));
  }

  m_InPointList = m_Quadrants[13];
}

inline size_t RenderMesh::PosToQuadrant(const FLOATVECTOR3& pos) {
  size_t index = 0;

  if (pos.x < m_VolumeMin.x) index = 0; else 
  if (pos.x > m_VolumeMax.x) index = 2; else index = 1;

  if (pos.y < m_VolumeMin.y) index += 0; else 
  if (pos.y > m_VolumeMax.y) index += 6; else index += 3;

  if (pos.z < m_VolumeMin.z) index += 0; else 
  if (pos.z > m_VolumeMax.z) index += 18; else index += 9;

  return index;
}

#define END 27

void RenderMesh::Front(int index,...)
{
  va_list indices;
  va_start(indices,index);

  for (int i = 0;i<27;i++) {
    if (i == 13) continue; // center quadrant

    if (i == index) {
      Append(m_FrontPointList, i);
      index = va_arg(indices,int);
    } else {
      Append(m_BehindPointList, i);
    }
  }

  va_end(indices);
}


void RenderMesh::RehashTransparentData() {
  m_FIBHashDirty = false;

  for (SortIndexVec::iterator poly = m_allPolys.begin();
       poly != m_allPolys.end();
       poly++) {
    poly->UpdateDistance();
  }
  m_FrontPointList.clear();
  m_BehindPointList.clear();

  // is the entire mesh opaque ?
  if (m_splitIndex == m_VertIndices.size()) return;

  size_t index = PosToQuadrant(m_viewPoint);

  switch (index) {
    case  0 : Front( 0, 1, 2,
                     3, 4, 5,
                     6, 7, 8,

                     9,10,11,
                    12,
                    15,

                    18,19,20,
                    21,
                    24,
                    END);
              break;
    case  1 : Front( 0, 1, 2,
                     3, 4, 5,
                     6, 7, 8,

                     9,10,11,
                      
                      

                    18,19,20,


                    END);
              break;
    case  2 : Front( 0, 1, 2,
                     3, 4, 5,
                     6, 7, 8,

                     9,10,11,
                          14,
                          17,

                    18,19,20,
                          23,
                          26,
                    END);
              break;
    case  3 : Front( 0, 1, 2,
                     3, 4, 5,
                     6, 7, 8,

                     9,      
                    12,      
                    15,   

                    18,   
                    21,   
                    24,   
                    END);
              break;
    case  4 : Front( 0, 1, 2,
                     3, 4, 5,
                     6, 7, 8,
                    END);
              break;
    case  5 : Front( 0, 1, 2,
                     3, 4, 5,
                     6, 7, 8,

                          11,
                          14,
                          17,

                          20,
                          23,
                          26,
                    END);
              break;
    case  6 : Front( 0, 1, 2,
                     3, 4, 5,
                     6, 7, 8,

                     9,   
                    12,      
                    15,16,17,

                    18,      
                    21,
                    24,25,26,
                    END);
              break;
    case  7 : Front( 0, 1, 2,
                     3, 4, 5,
                     6, 7, 8,

                          
                    
                    15,16,17,

                    
                    
                    24,25,26,
                    END);
              break;
    case  8 : Front( 0, 1, 2,
                     3, 4, 5,
                     6, 7, 8,

                          11,
                          14,
                    15,16,17,

                          20,
                          23,
                    24,25,26,
                    END);
              break;
    case  9 : Front( 0, 1, 2,
                     3,
                     6,

                     9,10,11,
                    12,
                    15,

                    18,19,20,
                    21,
                    24,
                    END);
              break;
    case 10 : Front( 0, 1, 2,
                     
                     

                     9,10,11,
                    
                    

                    18,19,20,
                    
                    
                    END);
              break;
    case 11 : Front( 0, 1, 2,
                           5,
                           8,

                     9,10,11,
                          14,
                          17,

                    18,19,20,
                          23,
                          26,
                    END);
              break;
    case 12 : Front( 0,
                     3,
                     6,

                     9,
                    12,
                    15,

                    18,
                    21,
                    24,
                    END);
              break;
    case 13 : Front(END);
              break;
    case 14 : Front(       2,
                           5,
                           8,

                          11,
                          14,
                          17,

                          20,
                          23,
                          26,
                    END);
              break;
    case 15 : Front( 0,
                     3,
                     6, 7, 8,

                     9,
                    12,
                    15,16,17,

                    18,
                    21,
                    24,25,26,
                    END);
              break;
    case 16 : Front( 
                     
                     6, 7, 8,

                     
                    
                    15,16,17,

                    
                    
                    24,25,26,
                    END);
              break;
    case 17 : Front(       2,
                           5,
                     6, 7, 8,

                          11,
                          14,
                    15,16,17,

                          20,
                          23,
                    24,25,26,
                    END);
              break;
    case 18 : Front( 0, 1, 2,
                     3,
                     6,

                     9,10,11,
                    12,
                    15,

                    18,19,20,
                    21,22,23,
                    24,25,26,
                    END);
              break;
    case 19 : Front( 0, 1, 2,
                     
                     

                     9,10,11,
                    
                    

                    18,19,20,
                    21,22,23,
                    24,25,26,
                    END);
              break;
    case 20 : Front( 0, 1, 2,
                           5,
                           8,

                     9,10,11,
                          14,
                          17,

                    18,19,20,
                    21,22,23,
                    24,25,26,
                    END);
              break;
    case 21 : Front( 0,
                     3,
                     6,

                     9,
                    12,
                    15,

                    18,19,20,
                    21,22,23,
                    24,25,26,
                    END);
              break;
    case 22 : Front(18,19,20,
                    21,22,23,
                    24,25,26,
                    END);
              break;
    case 23 : Front(       2,
                           5,
                           8,

                          11,
                          14,
                          17,

                    18,19,20,
                    21,22,23,
                    24,25,26,
                    END);
              break;
    case 24 : Front( 0,
                     3,
                     6, 7, 8,

                     9,
                    12,
                    15,16,17,

                    18,19,20,
                    21,22,23,
                    24,25,26,
                    END);
              break;
    case 25 : Front( 
                     
                     6, 7, 8,

                     
                    
                    15,16,17,

                    18,19,20,
                    21,22,23,
                    24,25,26,
                    END);
              break;
    case 26 : Front(       2,
                           5,
                     6, 7, 8,

                          11,
                          14,
                    15,16,17,

                    18,19,20,
                    21,22,23,
                    24,25,26,
                    END);
              break;
  }


  // depth sort
  if (m_bSortOver) {
    std::sort(m_FrontPointList.begin(), m_FrontPointList.end(), DistanceSortOver);
    std::sort(m_BehindPointList.begin(), m_BehindPointList.end(), DistanceSortOver);
  } else {
    std::sort(m_FrontPointList.begin(), m_FrontPointList.end(), DistanceSortUnder);
    std::sort(m_BehindPointList.begin(), m_BehindPointList.end(), DistanceSortUnder);
  }
}

const SortIndexPVec& RenderMesh::GetFrontPointList() {
  if (m_QuadrantsDirty) SortTransparentDataIntoQuadrants();
  if (m_FIBHashDirty) RehashTransparentData();
  return m_FrontPointList;
}

const SortIndexPVec& RenderMesh::GetInPointList() {
  if (m_QuadrantsDirty) SortTransparentDataIntoQuadrants();
  if (m_FIBHashDirty) RehashTransparentData();
  return m_InPointList;
}

const SortIndexPVec& RenderMesh::GetBehindPointList() {
  if (m_QuadrantsDirty) SortTransparentDataIntoQuadrants();
  if (m_FIBHashDirty) RehashTransparentData();
  return m_BehindPointList;
}

const SortIndexPVec& RenderMesh::GetSortedInPointList() {
  if (m_QuadrantsDirty) SortTransparentDataIntoQuadrants();
  if (m_FIBHashDirty) RehashTransparentData();
  if (m_bSortOver) {
    std::sort(m_InPointList.begin(), m_InPointList.end(), DistanceSortOver);
  } else {
    std::sort(m_InPointList.begin(), m_InPointList.end(), DistanceSortUnder);
  }
  return m_InPointList;
}
