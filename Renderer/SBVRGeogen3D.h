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

/** \class SBVRGeogen3D
 * Geometry generation for the slice-based volume renderer. */
class SBVRGeogen3D : public SBVRGeogen
{
public:
  SBVRGeogen3D(void);
  virtual ~SBVRGeogen3D(void);
  virtual void ComputeGeometry();
  std::vector<POS3TEX3_VERTEX> m_vSliceTriangles;

protected:

  float             m_fMinZ;

  virtual void InitBBOX();
  bool ComputeLayerGeometry(float fDepth);
  void Triangulate(std::vector<POS3TEX3_VERTEX> &fArray);
  float GetLayerDistance() const;

  static bool DepthPlaneIntersection(float z,
                                  const POS3TEX3_VERTEX &plA,
                                  const POS3TEX3_VERTEX &plB,
                                  std::vector<POS3TEX3_VERTEX>& vHits);
  static void SortByGradient(std::vector<POS3TEX3_VERTEX>& fArray);
};

#endif // SBVRGEOGEN3D_H
