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

#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>
#include "StdDefines.h"
#include "Basics/Vectors.h"
#include "Ray.h"

class KDTree;

typedef std::vector<FLOATVECTOR3> VertVec;
typedef std::vector<FLOATVECTOR3> NormVec;
typedef std::vector<FLOATVECTOR2> TexCoordVec;

typedef std::vector<INTVECTOR3> IndexVec;

class Mesh 
{
public:
  Mesh(const VertVec& vertices, const NormVec& normals, 
       const TexCoordVec& texcoords, const IndexVec& vIndices, 
       const IndexVec& nIndices, const IndexVec& tIndices);
  Mesh(const std::string& filename, bool bFlipNormals, 
       const FLOATVECTOR3& translation = FLOATVECTOR3(), 
       const FLOATVECTOR3& scale = FLOATVECTOR3(1,1,1));
  ~Mesh();

  double Intersect(const Ray& ray, FLOATVECTOR3& normal, FLOATVECTOR2& tc) {
    double tmin=0, tmax=0;
    if (!AABBIntersect(ray, tmin, tmax))
      return std::numeric_limits<double>::max();
    else
      return IntersectInternal(ray, normal, tc, tmin, tmax); 
  }

  const KDTree* GetKDTree() const;
private:
  KDTree*       m_KDTree;

  VertVec       m_vertices;
  NormVec       m_normals;
  TexCoordVec   m_texcoords;

  IndexVec      m_VertIndices;
  IndexVec      m_NormalIndices;
  IndexVec      m_TCIndices;

  double IntersectInternal(const Ray& ray, FLOATVECTOR3& normal, 
                           FLOATVECTOR2& tc, double tmin, double tmax) const;

  double IntersectTriangle(size_t i, 
                           const Ray& ray, FLOATVECTOR3& normal, 
                           FLOATVECTOR2& tc) const;

  bool LoadFile(const std::string& filename, bool bFlipNormals,
                const FLOATVECTOR3& translation, const FLOATVECTOR3& scale);
  std::string TrimLeft(const std::string& Src, const std::string& c = " \r\n\t");
  std::string TrimRight(const std::string& Src, const std::string& c = " \r\n\t");
  std::string TrimToken(const std::string& Src, const std::string& c = " \r\n\t", bool bOnlyFirst = false);
  std::string Trim(const std::string& Src, const std::string& c = " \r\n\t");
  std::string ToLowerCase(const std::string& str);
  int CountOccurences(const std::string& str, const std::string& substr);

  friend class KDTree;

  // AABB Test
  bool AABBIntersect(const Ray& r, double& tmin, double& tmax);
  FLOATVECTOR3  m_Bounds[2];    

};

#endif // MESH_H
