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
  \file    SBVRGeogen2D.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    September 2008
*/

#include <algorithm>
#include <cassert>
#include <StdTuvokDefines.h>
#if defined(_MSC_VER) ||                                               \
    (defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 1)) ||   \
    defined(DETECTED_OS_APPLE)
// MS puts tr1 in standard headers.  Old gcc's and all gcc's on Apple are
// broken, not including this part of tr1.
# include <cmath>
#else
# include <tr1/cmath>
#endif
#include <functional>
#include <limits>
#include "SBVRGeogen2D.h"

static bool CheckOrdering(const FLOATVECTOR3& a, const FLOATVECTOR3& b,
                          const FLOATVECTOR3& c);
static void SortPoints(std::vector<POS3TEX3_VERTEX> &fArray);

SBVRGeogen2D::SBVRGeogen2D(void) :
  SBVRGeogen()
{
  m_vSliceTrianglesOrder[0] = DIRECTION_X;
  m_vSliceTrianglesOrder[1] = DIRECTION_Y;
  m_vSliceTrianglesOrder[2] = DIRECTION_Z;
}

SBVRGeogen2D::~SBVRGeogen2D(void)
{
}

UINT32 SBVRGeogen2D::GetLayerCount(int iDir) const {
  return UINT32(ceil(m_fSamplingModifier * m_vSize[iDir]));
}


void SBVRGeogen2D::InterpolateVertices(const POS3TEX3_VERTEX& v1, const POS3TEX3_VERTEX& v2, float a, POS3TEX3_VERTEX& r) const {
  r.m_vPos = (1.0f-a)*v1.m_vPos + a*v2.m_vPos;
  r.m_vTex = (1.0f-a)*v1.m_vTex + a*v2.m_vTex;
}

void SBVRGeogen2D::ComputeGeometry() {
  m_vSliceTrianglesX.clear();
  m_vSliceTrianglesY.clear();
  m_vSliceTrianglesZ.clear();

  UINT32 iLayerCount = GetLayerCount(0);
  POS3TEX3_VERTEX pfSliceVertex[4];
  for (UINT32 x = 0;x<iLayerCount;x++) {

    float a = float(x) / float(iLayerCount-1);
    InterpolateVertices(m_pfBBOXVertex[0], m_pfBBOXVertex[1], a, pfSliceVertex[0]);
    InterpolateVertices(m_pfBBOXVertex[3], m_pfBBOXVertex[2], a, pfSliceVertex[1]);
    InterpolateVertices(m_pfBBOXVertex[4], m_pfBBOXVertex[5], a, pfSliceVertex[2]);
    InterpolateVertices(m_pfBBOXVertex[7], m_pfBBOXVertex[6], a, pfSliceVertex[3]);

    m_vSliceTrianglesX.push_back(pfSliceVertex[0]);
    m_vSliceTrianglesX.push_back(pfSliceVertex[1]);
    m_vSliceTrianglesX.push_back(pfSliceVertex[2]);

    m_vSliceTrianglesX.push_back(pfSliceVertex[1]);
    m_vSliceTrianglesX.push_back(pfSliceVertex[3]);
    m_vSliceTrianglesX.push_back(pfSliceVertex[2]);

  }
      
  iLayerCount = GetLayerCount(1);
  for (UINT32 y = 0;y<iLayerCount;y++) {

    float a = float(y) / float(iLayerCount-1);
    InterpolateVertices(m_pfBBOXVertex[0], m_pfBBOXVertex[4], a, pfSliceVertex[0]);
    InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[5], a, pfSliceVertex[1]);
    InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[6], a, pfSliceVertex[2]);
    InterpolateVertices(m_pfBBOXVertex[3], m_pfBBOXVertex[7], a, pfSliceVertex[3]);

    m_vSliceTrianglesY.push_back(pfSliceVertex[2]);
    m_vSliceTrianglesY.push_back(pfSliceVertex[1]);
    m_vSliceTrianglesY.push_back(pfSliceVertex[0]);

    m_vSliceTrianglesY.push_back(pfSliceVertex[0]);
    m_vSliceTrianglesY.push_back(pfSliceVertex[3]);
    m_vSliceTrianglesY.push_back(pfSliceVertex[2]);

  }

  for (UINT32 z = 0;z<iLayerCount;z++) {

    float a = float(z) / float(iLayerCount-1);
    InterpolateVertices(m_pfBBOXVertex[0], m_pfBBOXVertex[3], a, pfSliceVertex[0]);
    InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[2], a, pfSliceVertex[1]);
    InterpolateVertices(m_pfBBOXVertex[4], m_pfBBOXVertex[7], a, pfSliceVertex[2]);
    InterpolateVertices(m_pfBBOXVertex[5], m_pfBBOXVertex[6], a, pfSliceVertex[3]);

    m_vSliceTrianglesZ.push_back(pfSliceVertex[0]);
    m_vSliceTrianglesZ.push_back(pfSliceVertex[1]);
    m_vSliceTrianglesZ.push_back(pfSliceVertex[2]);

    m_vSliceTrianglesZ.push_back(pfSliceVertex[1]);
    m_vSliceTrianglesZ.push_back(pfSliceVertex[3]);
    m_vSliceTrianglesZ.push_back(pfSliceVertex[2]);

  }

}
