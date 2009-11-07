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
           University of Utah
  \date    September 2008
*/
#pragma once

#ifndef SBVRGEOGEN2D_H
#define SBVRGEOGEN2D_H

#include "SBVRGeogen.h"

/** \class SBVRGeogen2D
 * Geometry generation for the slice-based volume renderer. */
class SBVRGeogen2D : public SBVRGeogen
{
public:

  enum EDirection {
    DIRECTION_X=0,
    DIRECTION_Y,
    DIRECTION_Z
  };

  SBVRGeogen2D(void);
  virtual ~SBVRGeogen2D(void);
  virtual void ComputeGeometry();

  EDirection m_vSliceTrianglesOrder[3];

  std::vector<POS3TEX3_VERTEX> m_vSliceTrianglesX;
  std::vector<POS3TEX3_VERTEX> m_vSliceTrianglesY;
  std::vector<POS3TEX3_VERTEX> m_vSliceTrianglesZ;

protected:
  UINT32 GetLayerCount(int iDir) const;
  void InterpolateVertices(const POS3TEX3_VERTEX& v1, const POS3TEX3_VERTEX& v2, float a, POS3TEX3_VERTEX& r) const;
};

#endif // SBVRGEOGEN2D_H