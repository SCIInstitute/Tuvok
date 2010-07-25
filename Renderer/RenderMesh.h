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

//!    File   : RenderMesh.h
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef RENDERMESH_H
#define RENDERMESH_H

#include "../Basics/Mesh.h"
#include <list>
#include <stdarg.h>

namespace tuvok {

class RenderMesh;

class SortIndex {
public:
  size_t m_index;
  FLOATVECTOR3 m_centroid;
  const RenderMesh* m_mesh;
  float fDistance;

  SortIndex(size_t index, const RenderMesh* m);
  void UpdateDistance();

protected:
  void ComputeCentroid();
};

inline bool DistanceSortOver(const SortIndex* e1, const SortIndex* e2)
{
  return e1->fDistance > e2->fDistance;
}

inline bool DistanceSortUnder(const SortIndex* e1, const SortIndex* e2)
{
  return e1->fDistance < e2->fDistance;
}


typedef std::vector< SortIndex > SortIndexList;
typedef std::vector< SortIndex* > SortIndexPList;


class RenderMesh : public Mesh 
{
public:
  RenderMesh(const Mesh& other, float fTransTreshhold=1.0f);
  RenderMesh(const VertVec& vertices, const NormVec& normals, 
       const TexCoordVec& texcoords, const ColorVec& colors,
       const IndexVec& vIndices, const IndexVec& nIndices, 
       const IndexVec& tIndices, const IndexVec& cIndices,
       bool bBuildKDTree, bool bScaleToUnitCube,
       const std::string& desc, EMeshType meshType,
       float fTransTreshhold=1.0f);

  virtual void InitRenderer() = 0;
  virtual void RenderOpaqueGeometry() = 0;
  virtual void RenderTransGeometryFront() = 0;
  virtual void RenderTransGeometryBehind() = 0;
  virtual void RenderTransGeometryInside() = 0;

  void SetActive(bool bActive) {m_bActive = bActive;}
  bool GetActive() const {return m_bActive;}

  void SetTransTreshhold(float fTransTreshhold);
  float GetTransTreshhold() const {return m_fTransTreshhold;}
  virtual void SetDefaultColor(const FLOATVECTOR4& color);

  // *******************************************************************
  // ****** the calls below are only used for transparent meshes *******
  // *******************************************************************

  /**\brief Specifies the position of the volume's AABB 
   * \param min the min coodinates of the AABB
   * \param max the max coodinates of the AABB
   */
  void SetVolumeAABB(const FLOATVECTOR3& min, 
                     const FLOATVECTOR3& max);

  /**\brief Accepts the transformed position of the viewer relative to the 
   *        untransformed volume
   * \param viewPoint the transformedposition of the viewer relative to the 
   *                  untransformed volume
   */
  void SetUserPos(const FLOATVECTOR3& viewPoint);

  /**\brief Returns the list of all polygons in front of the AABB as 
   *        computed by SetUserPos 
   * \result the points in front of the AABB
   */
  const SortIndexPList& GetFrontPointList();
  /**\brief Returns the list of all polygons inside the AABB as 
   *        computed by SetUserPos 
   * \result the points inside the AABB
   */
  const SortIndexPList& GetInPointList();
  /**\brief Returns the list of all polygons behind the AABB as 
   *        computed by SetUserPos this list is nor depth sorted
   * \result the points behind the AABB
   */
  const SortIndexPList& GetBehindPointList();
  
  /**\brief Returns the list of all polygons inside the AABB as 
   *        computed by SetUserPos 
   * \result the points inside the AABB
   */
  const SortIndexPList& GetSortedInPointList();
   
  virtual void GeometryHasChanged(bool bUpdateAABB, bool bUpdateKDtree);

  void EnableOverSorting(bool bOver) {m_bSortOver = bOver;}

protected:
  bool   m_bActive;
  size_t m_splitIndex;
  float  m_fTransTreshhold;
  bool   m_bSortOver;

  void Swap(size_t i, size_t j);
  bool isTransparent(size_t i);
  void SplitOpaqueFromTransparent();

  // transparent meshes
  FLOATVECTOR3 m_viewPoint;
  FLOATVECTOR3 m_VolumeMin; 
  FLOATVECTOR3 m_VolumeMax;
  bool         m_QuadrantsDirty;
  bool         m_FIBHashDirty;

  SortIndexList m_allPolys;
  std::vector< SortIndexPList > m_Quadrants;
  SortIndexPList m_FrontPointList;
  SortIndexPList m_InPointList;
  SortIndexPList m_BehindPointList;


  /**\brief If the mesh contains transparent parts this call creates 
   *        27 lists pointing to parts of the transparent mesh in the 27 
   *        quadrants defined by the 6 planes of the volume's AABB
   */
  void SortTransparentDataIntoQuadrants();

  /**\brief If the mesh contains transparent parts this call creates 
   *        three lists pointing to parts of the transparent mesh
   *        one list conatins all triangles in front of the AABB specified 
   *        by min and max as viewed from viewPoint. 
   */
  void RehashTransparentData();

  /**\brief Sorts a point into one of the 27 quadrants in and around the volume 
   * \param pos of the point to be sorted
   * \result the quadrant index
   */
  size_t PosToQuadrant(const FLOATVECTOR3& pos);

  /**\brief Appends a quadrant with index "index" to "target"
   * \param target the list to append to
   * \param index the index of the quadrant to be appended to "target"
   */
  void Append(SortIndexPList& target, size_t index) {
    target.insert(target.end(), 
                  m_Quadrants[index].begin(),
                  m_Quadrants[index].end());
  }


  void Front(int index,...);

  friend class SortIndex;
};

}
#endif // RENDERMESH_H
