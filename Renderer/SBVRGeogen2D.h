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
  \file    SBVRGeogen2D.h
  \author  Jens Krueger
           SCI Institute
           DFKI Saarbruecken & University of Utah
  \date    March 2010
*/
#pragma once

#ifndef SBVRGEOGEN2D_H
#define SBVRGEOGEN2D_H

#include "SBVRGeogen.h"

namespace tuvok {

 
/** 
 \class SBVRGeogen2D
 \brief Geometry generation for 2D texture based the slice-based 
 volume renderer. 
 
 This class implements 3 different algorithms to generate the object aligned
 geometry for a 2D texture slice-based volume renderer. Those three methods
 are Christoph Resz's "traditional" stack switching method, where the one 
 stack three object aligned stacks is chosen for rendering that is most
 perpendicular to the viewing direction (i.e. the normal to viewing direction
 dot product is minimal). The other two approaches are the naive slow and the 
 optimized fast implementation of Jens Krueger's new sampling scheme for slice
 based volume rendering. Which of the three methods is used can be controlled
 by setting m_eMethod.
 */
class SBVRGeogen2D : public SBVRGeogen
{
public:
  /** 
   \brief An enum specifing the three geometry generation modes

   METHOD_REZK is Christoph Rezk-Salama et al.'s 2000 method
   METHOD_KRUEGER is Jens Krueger's 2010 naive method
   METHOD_KRUEGER_FAST is Jens Krueger's 2010 optimized method
   */
  enum ESliceMethod {
    METHOD_REZK=0,
    METHOD_KRUEGER,
    METHOD_KRUEGER_FAST
  };

  /** 
   \brief The Standard and also the only constructor  
   
   SBVRGeogen2D takes no parameters in the constructor as
   the geometry mode is by modifing m_eMethod and the view
   parametes are set via various accessor methods in the parent
   class
   */
  SBVRGeogen2D(void);
  virtual ~SBVRGeogen2D(void);

  /** 
   \brief This call invokes the actual geometry generation

   Overridden ComputeGeometry call, this call does the actual work
   of computing the object aligned slices internally it calls either
   ComputeGeometryRezk(), ComputeGeometryKrueger() or
   ComputeGeometryKruegerFast() depending on m_eMethod

   \post stores the slice geometry in m_vSliceTrianglesX, m_vSliceTrianglesY
   and m_vSliceTrianglesZ
   \sa ComputeGeometryRezk() ComputeGeometryKrueger()
   ComputeGeometryKruegerFast() m_eMethod
  */
  virtual void ComputeGeometry(bool bMeshOnly);

  //! Vector holding the slices that access the X axis aligned textures
  std::vector<VERTEX_FORMAT> m_vSliceTrianglesX;
  //! Vector holding the slices that access the Y axis aligned textures
  std::vector<VERTEX_FORMAT> m_vSliceTrianglesY;
  //! Vector holding the slices that access the Z axis aligned textures
  std::vector<VERTEX_FORMAT> m_vSliceTrianglesZ;
  
  /** 
   \brief Holds the Geometry generation method

   Geometry generation method, for values see ESliceMethod enum above
   if this value is changed ComputeGeometry() has to be called to update
   m_vSliceTrianglesX, m_vSliceTrianglesY, and m_vSliceTrianglesZ vectors
  */
  ESliceMethod m_eMethod;

protected:
  /** 
    \brief Computes the normalized distance between two object aligned slices

    \param iDir the direction (0=x, 1=y, 2=z)
    \return the slice distance in direction iDir
  */
  float GetDelta(int iDir) const;
  
  /** 
    \brief Interpolates VERTEX_FORMAT "r" between "v1" and 
    "v2" with parameter "a"

    \param v1 the first vertex
    \param v2 the second vertex
    \param a the interpolation parameter a (should be in [0..1])
    \param r the interpolated vertex structure
  */
  void InterpolateVertices(const VERTEX_FORMAT& v1, 
                           const VERTEX_FORMAT& v2, 
                           float a, VERTEX_FORMAT& r) const;

private:
  /** 
   \brief Computes 2D geometry via C. Rezk-Salama et al. 2000

   Computes 2D geometry via C. Rezk-Salama et al. 2000
   "Interactive Volume Rendering on Standard PC Graphics Hardware 
   Using Multi-Textures and Multi-Stage Rasterization"
  */
  void ComputeGeometryRezk();
  /** 
   \brief Computes 2D geometry alike Krüger 2010
   
   Computes 2D geometry alike Krüger 2010
   "A new sampling scheme for slice based volume rendering"
   but with a very slow approach, should be used only for demonstation
   */
  void ComputeGeometryKrueger();
  /** 
    \brief Computes 2D geometry via Krüger 2010

    Computes 2D geometry via Krüger 2010
    "A new sampling scheme for slice based volume rendering"
  */
  void ComputeGeometryKruegerFast();
  /** 
    \brief Computes the geometry for one direction used 
     by ComputeGeometryKruegerFast

    \param iDirIndex the direction (x=0, y=1, z=2)
    \param fDelta the slice distance
    \param vertexIndices bounding box indices as seen from direction iDirIndex
    \param edgeIndices edge indices as seen from direction iDirIndex
    \param vFaceVec connection vector from the eye point to the face centers
    \param vIntersects front-edge-plane / edge intersctions
    \param vIntersectPlanes front-edge-planes that cause intersections
    \param vCoordFrame transformed local coordinate frame
    \param vSliceTriangles the computed geometry is stored in this vector
  */
  void BuildStackQuads(const int iDirIndex,
                       const float fDelta,
                       const size_t *vertexIndices,
                       const size_t *edgeIndices,
                       const FLOATVECTOR3* vFaceVec,
                       const std::vector<size_t>& vIntersects,
                       const std::vector<FLOATPLANE>& vIntersectPlanes,
                       const FLOATVECTOR3* vCoordFrame,
                       std::vector<VERTEX_FORMAT>& vSliceTriangles);

};
};
#endif // SBVRGEOGEN2D_H
