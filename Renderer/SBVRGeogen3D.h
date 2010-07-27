/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


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

/**
  \file    SBVRGeogen3D.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    September 2008
*/
#pragma once

#ifndef SBVRGEOGEN3D_H
#define SBVRGEOGEN3D_H

#include "SBVRGeogen.h"

namespace tuvok {

/** 
 \class SBVRGeogen3D
 \brief View aligned geometry generation for 3D texture based the slice-based 
 volume renderer. 
*/
class SBVRGeogen3D : public SBVRGeogen
{
public:
  /** 
   \brief The Standard and also the only constructor  
   
   SBVRGeogen takes no parameters in the constructor as the information 
   required to compute the geometry mode is supplied later via accessor calls
   in the parent class
   */
  SBVRGeogen3D(void);
  virtual ~SBVRGeogen3D(void);

  /** 
   \brief This call invokes the actual geometry generation of the view aligned
   slices
  */
  virtual void ComputeGeometry(bool bMeshOnly);
  //! this is where ComputeGeometry() outputs the geometry to
  std::vector<VERTEX_FORMAT> m_vSliceTriangles;

protected:

  //! depth of the slice closest to the viewer
  float m_fMaxZ;

  /** 
   \brief Calls InitBBOX from the parent class which computes the transformed 
  vertices m_pfBBOXVertex from m_pfBBOXStaticVertex and in addition to this
  updates m_fMaxZ
  */
  virtual void InitBBOX();
  
  /** 
   \brief Computes a single view aligned slice at depth fDepth the slice is
   appended to m_vSliceTriangles

   \param fDepth the depth of the slice
   \result if geometry for this slice was generated, no gemeotry may be 
   generated if fDepth puts the slice in front of the bounding box or behind the 
   bounding box or if the geoemtry degenerates into less then a triangle (a line
   or a point)
  */
  bool ComputeLayerGeometry(float fDepth);

  /** 
   \brief Triangulates a planar polygon specified by the vertices in fArray,
   the result is appended to m_vSliceTriangles

   \param fArray the vertices of the polygon
  */
  void Triangulate(std::vector<VERTEX_FORMAT> &fArray);

  //! returns the distance between two slices
  float GetLayerDistance() const;

  
  /** 
   \brief Computes the intersection of a plane perpendicular to the viewing
   direction with a line (typically one of the edges of the the bounding box 
   of the volume)

   \param z distance of the plane to the viewer
   \param plA starting point of the line
   \param plB end point of the line
   \param vHits intersection point of the line and the plane
   \result true if the plane intersects the line
  */
  static bool DepthPlaneIntersection(float z,
                                  const VERTEX_FORMAT &plA,
                                  const VERTEX_FORMAT &plB,
                                  std::vector<VERTEX_FORMAT>& vHits);

  /** 
   \brief inplace sorts a number lines by their gradient, the n-1 lines defined 
   by an array of n vertices, whereas the m-th line is a connection of the
   (m-1)-th vertex to the first

   \param fArray the vertices defining the lines
  */
  static void SortByGradient(std::vector<VERTEX_FORMAT>& fArray);
};
};
#endif // SBVRGEOGEN3D_H
