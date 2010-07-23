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

//!    File   : Mesh.h
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef BASICS_MESH_H
#define BASICS_MESH_H

#include <vector>
#include <string>
#include "StdDefines.h"
#include "Basics/Vectors.h"
#include "Ray.h"

namespace tuvok {

class KDTree;

typedef std::vector<FLOATVECTOR3> VertVec;
typedef std::vector<FLOATVECTOR3> NormVec;
typedef std::vector<FLOATVECTOR2> TexCoordVec;
typedef std::vector<FLOATVECTOR4> ColorVec;
typedef std::vector<UINTVECTOR3> IndexVec;

class Mesh 
{
public:
  Mesh();
  Mesh(const VertVec& vertices, const NormVec& normals, 
       const TexCoordVec& texcoords, const ColorVec& colors,
       const IndexVec& vIndices, const IndexVec& nIndices, 
       const IndexVec& tIndices, const IndexVec& cIndices,
       bool bBuildKDTree, bool bScaleToUnitCube);
  virtual ~Mesh();

  void RecomputeNormals();
  void ScaleToUnitCube(const FLOATVECTOR3& translation = FLOATVECTOR3(0,0,0),
                       const FLOATVECTOR3& scale= FLOATVECTOR3(1,1,1));

  double Pick(const Ray& ray, FLOATVECTOR3& normal, 
              FLOATVECTOR2& tc, FLOATVECTOR4& color) {
    double tmin=0, tmax=0;
    if (!AABBIntersect(ray, tmin, tmax))
      return std::numeric_limits<double>::max();
    else
      return IntersectInternal(ray, normal, tc, color, tmin, tmax); 
  }
  void ComputeKDTree();
  const KDTree* GetKDTree() const;

  bool Validate(bool bDeepValidation=false);

  const VertVec&       GetVertices() const {return m_vertices;}
  const NormVec&       GetNormals() const {return m_normals;}
  const TexCoordVec&   GetTexCoords() const {return m_texcoords;}
  const ColorVec&      GetColors() const {return m_colors;}

  const IndexVec& GetVertexIndices() const {return m_VertIndices;}
  const IndexVec& GetNormalIndices() const {return m_NormalIndices;}
  const IndexVec& GetTexCoordIndices() const {return m_TCIndices;}
  const IndexVec& GetColorIndices() const {return m_COLIndices;}

  const FLOATVECTOR4& GetDefaultColor() const {return m_DefColor;}
  virtual void SetDefaultColor(const FLOATVECTOR4& color) {m_DefColor = color;}

protected:
  KDTree*       m_KDTree;

  VertVec       m_vertices;
  NormVec       m_normals;
  TexCoordVec   m_texcoords;
  ColorVec      m_colors;

  IndexVec      m_VertIndices;
  IndexVec      m_NormalIndices;
  IndexVec      m_TCIndices;
  IndexVec      m_COLIndices;

  FLOATVECTOR4  m_DefColor;

  void ComputeAABB();

private:
  // picking
  double IntersectInternal(const Ray& ray, FLOATVECTOR3& normal, 
                           FLOATVECTOR2& tc, FLOATVECTOR4& color,
                           double tmin, double tmax) const;
  double IntersectTriangle(size_t i, 
                           const Ray& ray, FLOATVECTOR3& normal, 
                           FLOATVECTOR2& tc, FLOATVECTOR4& color) const;

  // AABB Test
  bool AABBIntersect(const Ray& r, double& tmin, double& tmax);
  FLOATVECTOR3  m_Bounds[2];

  friend class KDTree;
};
}
#endif // BASICS_MESH_H
