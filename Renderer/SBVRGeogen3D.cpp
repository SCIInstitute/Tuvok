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
  \file    SBVRGeogen3D.cpp
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
#include "SBVRGeogen3D.h"

static bool CheckOrdering(const FLOATVECTOR3& a, const FLOATVECTOR3& b,
                          const FLOATVECTOR3& c);
static void SortPoints(std::vector<POS3TEX3_VERTEX> &fArray);

SBVRGeogen3D::SBVRGeogen3D(void) :
  SBVRGeogen(),
  m_fMinZ(0)
{
}

SBVRGeogen3D::~SBVRGeogen3D(void)
{
}

// Finds the point with the minimum position in Z.
struct vertex_min_z : public std::binary_function<POS3TEX3_VERTEX,
                                                  POS3TEX3_VERTEX,
                                                  bool> {
  bool operator()(const POS3TEX3_VERTEX &a, const POS3TEX3_VERTEX &b) const {
    return (a.m_vPos.z < b.m_vPos.z);
  }
};


void SBVRGeogen3D::InitBBOX() {
  SBVRGeogen::InitBBOX();
  // find the minimum z value
  m_fMinZ = (*std::min_element(m_pfBBOXVertex, m_pfBBOXVertex+8, vertex_min_z())).m_vPos.z;
}


bool SBVRGeogen3D::DepthPlaneIntersection(float z,
                                const POS3TEX3_VERTEX &plA,
                                const POS3TEX3_VERTEX &plB,
                                std::vector<POS3TEX3_VERTEX>& vHits)
{
  /*
     returns NO INTERSECTION if the line of the 2 points a,b is
     1. in front of the intersection plane
     2. behind the intersection plane
     3. parallel to the intersection plane (both points have "pretty much" the same z)
  */
  if ((z > plA.m_vPos.z && z > plB.m_vPos.z) ||
      (z < plA.m_vPos.z && z < plB.m_vPos.z) ||
      (EpsilonEqual(plA.m_vPos.z, plB.m_vPos.z))) {
    return false;
  }

  float fAlpha = (z - plA.m_vPos.z) /
                 (plA.m_vPos.z - plB.m_vPos.z);

  POS3TEX3_VERTEX vHit;

  vHit.m_vPos.x = plA.m_vPos.x + (plA.m_vPos.x - plB.m_vPos.x) * fAlpha;
  vHit.m_vPos.y = plA.m_vPos.y + (plA.m_vPos.y - plB.m_vPos.y) * fAlpha;
  vHit.m_vPos.z = z;

  vHit.m_vTex = plA.m_vTex + (plA.m_vTex - plB.m_vTex) * fAlpha;

  vHits.push_back(vHit);

  return true;
}


// Functor to identify the point with the lowest `y' coordinate.
struct vertex_min : public std::binary_function<POS3TEX3_VERTEX,
                                                POS3TEX3_VERTEX,
                                                bool> {
  bool operator()(const POS3TEX3_VERTEX &a, const POS3TEX3_VERTEX &b) const {
    return (a.m_vPos.y < b.m_vPos.y);
  }
};

// Sorts a vector
void SBVRGeogen3D::SortByGradient(std::vector<POS3TEX3_VERTEX>& fArray)
{
  // move bottom element to front of array
  if(fArray.empty()) { return; }
  std::swap(fArray[0],
            *std::min_element(fArray.begin(), fArray.end(), vertex_min()));
  if(fArray.size() > 2) {
    // sort points according to gradient
    SortPoints(fArray);
  }
}

void SBVRGeogen3D::Triangulate(std::vector<POS3TEX3_VERTEX> &fArray) {
  SortByGradient(fArray);

  // convert to triangles
  for (UINT32 i=0; i<(fArray.size()-2) ; i++) {
    m_vSliceTriangles.push_back(fArray[0]);
    m_vSliceTriangles.push_back(fArray[i+1]);
    m_vSliceTriangles.push_back(fArray[i+2]);
  }
}


bool SBVRGeogen3D::ComputeLayerGeometry(float fDepth) {
  std::vector<POS3TEX3_VERTEX> vLayerPoints;
  vLayerPoints.reserve(12);

  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[0], m_pfBBOXVertex[1],
                      vLayerPoints);
  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[1], m_pfBBOXVertex[2],
                      vLayerPoints);
  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[2], m_pfBBOXVertex[3],
                      vLayerPoints);
  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[3], m_pfBBOXVertex[0],
                      vLayerPoints);

  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[4], m_pfBBOXVertex[5],
                      vLayerPoints);
  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[5], m_pfBBOXVertex[6],
                      vLayerPoints);
  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[6], m_pfBBOXVertex[7],
                      vLayerPoints);
  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[7], m_pfBBOXVertex[4],
                      vLayerPoints);

  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[4], m_pfBBOXVertex[0],
                      vLayerPoints);
  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[5], m_pfBBOXVertex[1],
                      vLayerPoints);
  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[6], m_pfBBOXVertex[2],
                      vLayerPoints);
  DepthPlaneIntersection(fDepth, m_pfBBOXVertex[7], m_pfBBOXVertex[3],
                      vLayerPoints);

  if (vLayerPoints.size() <= 2) {
    return false;
  }

  Triangulate(vLayerPoints);
  return true;
}

float SBVRGeogen3D::GetLayerDistance() const {
  return (0.5f * 1.0f/m_fSamplingModifier * (m_vAspect/FLOATVECTOR3(m_vSize))).minVal();
}


void SBVRGeogen3D::ComputeGeometry() {
  m_vSliceTriangles.clear();

  float fDepth = m_fMinZ;
  float fLayerDistance = GetLayerDistance();

  // I hit this every time I fiddle with the integration of an
  // application.  If an app doesn't set brick metadata properly, we'll
  // calculate a bad minimum Z value of nan. nan + anything is still nan,
  // so we end up with an infinite loop computing geometry below.
#if defined(_MSC_VER)
  assert(_finite(fDepth));
#elif defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 1) || \
      defined(DETECTED_OS_APPLE)
  /* nothing... bleh. */
#elif (defined(__GNUC__) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 1))
  assert(!std::tr1::isnan(fDepth));
#endif

  while (ComputeLayerGeometry(fDepth)) fDepth += fLayerDistance;

  if(m_bClipPlaneEnabled) {
    PLANE<float> transformed = m_ClipPlane * m_matView;
    const FLOATVECTOR3 normal(transformed.xyz());
    const float d = transformed.d();
    m_vSliceTriangles = ClipTriangles(m_vSliceTriangles, normal, d);
  }
}

// Checks the ordering of two points relative to a third.
static bool CheckOrdering(const FLOATVECTOR3& a,
                          const FLOATVECTOR3& b,
                          const FLOATVECTOR3& c) {
  float g1 = (a.y-c.y)/(a.x-c.x),
        g2 = (b.y-c.y)/(b.x-c.x);

  if (EpsilonEqual(a.x,c.x))
    return (g2 < 0) || (EpsilonEqual(g2,0.0f) && b.x < c.x);
  if (EpsilonEqual(b.x,c.x))
    return (g1 > 0) || (EpsilonEqual(g1,0.0f) && a.x > c.x);


  if (a.x < c.x)
    if (b.x < c.x) return g1 < g2; else return false;
  else
    if (b.x < c.x) return true; else return g1 < g2;
}

/// @todo: should be replaced with std::sort.
static void SortPoints(std::vector<POS3TEX3_VERTEX> &fArray) {
  // for small arrays, this bubble sort actually beats qsort.
  for (UINT32 i= 1;i<fArray.size();++i)
    for (UINT32 j = 1;j<fArray.size()-i;++j)
      if (!CheckOrdering(fArray[j].m_vPos,fArray[j+1].m_vPos,fArray[0].m_vPos))
        std::swap(fArray[j], fArray[j+1]);
}
