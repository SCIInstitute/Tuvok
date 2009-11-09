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
  \file    SBVRGeogen.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    September 2008
*/
#pragma once

#ifndef SBVRGEOGEN_H
#define SBVRGEOGEN_H

#include <vector>
#include "../Basics/Vectors.h"
#include "../StdTuvokDefines.h"

/** \class POS3TEX3_VERTEX
 * Position and texture coordinate. */
class POS3TEX3_VERTEX
{
public:
  POS3TEX3_VERTEX() : m_vPos() , m_vTex() {}
  POS3TEX3_VERTEX(const FLOATVECTOR3 &vPos, const FLOATVECTOR3 &vTex)
    : m_vPos(vPos),
      m_vTex(vTex) {}
  POS3TEX3_VERTEX(const FLOATVECTOR4 &vPos, const FLOATVECTOR3 &vTex)
    : m_vPos(vPos.xyz()),
      m_vTex(vTex) {}
  POS3TEX3_VERTEX(const FLOATVECTOR3 &vPos) : m_vPos(vPos) {
    m_vTex = m_vPos + 0.5f;
  }

  FLOATVECTOR3 m_vPos;
  FLOATVECTOR3 m_vTex;
};

/** \class SBVRGeoGen
 * Geometry generation for the slice-based volume renderer. */


class SBVRGeogen
{
public:
  SBVRGeogen(void);
  virtual ~SBVRGeogen(void);

  void SetSamplingModifier(float fSamplingModifier) {
    m_fSamplingModifier = fSamplingModifier;
  }
  void SetView(const FLOATMATRIX4 &matView, bool forceUpdate = false);
  void SetWorld(const FLOATMATRIX4& matTransform, bool bForceUpdate = false);
  void SetVolumeData(const FLOATVECTOR3& vAspect, const UINTVECTOR3& vSize);
  void SetLODData(const UINTVECTOR3& vSize);
  void SetBrickData(const FLOATVECTOR3& vAspect, const UINTVECTOR3& vSize,
                    const FLOATVECTOR3& vTexCoordMin=FLOATVECTOR3(0,0,0),
                    const FLOATVECTOR3& vTexCoordMax=FLOATVECTOR3(1,1,1));

  virtual void ComputeGeometry() = 0;
  float GetOpacityCorrection() const;

  void DisableClipPlane() { m_bClipPlaneEnabled = false; }
  void EnableClipPlane() { m_bClipPlaneEnabled = true; }
  void SetClipPlane(const PLANE<float>& plane) {
    if(m_ClipPlane != plane) {
      m_ClipPlane = plane;
      ComputeGeometry();
    }
  }

protected:

  float             m_fSamplingModifier;
  FLOATMATRIX4      m_matWorld;
  FLOATMATRIX4      m_matView;
  FLOATMATRIX4      m_matViewTransform;

  UINTVECTOR3       m_vGlobalSize;
  FLOATVECTOR3      m_vGlobalAspect;
  UINTVECTOR3       m_vLODSize;

  POS3TEX3_VERTEX   m_pfBBOXVertex[8];
  FLOATVECTOR3      m_pfBBOXStaticVertex[8];
  FLOATVECTOR3      m_vAspect;
  UINTVECTOR3       m_vSize;
  FLOATVECTOR3      m_vTexCoordMin;
  FLOATVECTOR3      m_vTexCoordMax;

  PLANE<float>      m_ClipPlane;
  bool              m_bClipPlaneEnabled;
  virtual void InitBBOX();
  void MatricesUpdated();


  // static method for clipping compuation
  static std::vector<POS3TEX3_VERTEX> SplitTriangle(POS3TEX3_VERTEX a,
                                                  POS3TEX3_VERTEX b,
                                                  POS3TEX3_VERTEX c,
                                                  const VECTOR3<float> &normal,
                                                  const float D);
  static std::vector<POS3TEX3_VERTEX>
  ClipTriangles(const std::vector<POS3TEX3_VERTEX> &in,
                const VECTOR3<float> &normal, const float D);

  static bool RayPlaneIntersection(const POS3TEX3_VERTEX &la,
                           const POS3TEX3_VERTEX &lb,
                           const FLOATVECTOR3 &n, const float D,
                           POS3TEX3_VERTEX &hit);

};

#endif // SBVRGEOGEN_H
