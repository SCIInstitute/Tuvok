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
  \file    SBVRGeogen.cpp
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
#include "SBVRGeogen.h"

SBVRGeogen::SBVRGeogen(void) :
  m_fSamplingModifier(1.0f),
  m_vGlobalSize(1,1,1),
  m_vGlobalAspect(1,1,1),
  m_vLODSize(1,1,1),
  m_vAspect(1,1,1),
  m_vSize(1,1,1),
  m_vTexCoordMin(0,0,0),
  m_vTexCoordMax(1,1,1),
  m_bClipPlaneEnabled(false)
{
  m_pfBBOXStaticVertex[0] = FLOATVECTOR3(-0.5, 0.5,-0.5);   // top,left,back
  m_pfBBOXStaticVertex[1] = FLOATVECTOR3( 0.5, 0.5,-0.5);   // top,right,back
  m_pfBBOXStaticVertex[2] = FLOATVECTOR3( 0.5, 0.5, 0.5);   // top,right,front
  m_pfBBOXStaticVertex[3] = FLOATVECTOR3(-0.5, 0.5, 0.5);   // top,left,front
  m_pfBBOXStaticVertex[4] = FLOATVECTOR3(-0.5,-0.5,-0.5);   // bottom,left,back
  m_pfBBOXStaticVertex[5] = FLOATVECTOR3( 0.5,-0.5,-0.5);   // bottom,right,back
  m_pfBBOXStaticVertex[6] = FLOATVECTOR3( 0.5,-0.5, 0.5);   // bottom,right,front
  m_pfBBOXStaticVertex[7] = FLOATVECTOR3(-0.5,-0.5, 0.5);   // bottom,left,front

  m_pfBBOXVertex[0] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[1] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[2] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[3] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[4] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[5] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[6] = FLOATVECTOR3(0,0,0);
  m_pfBBOXVertex[7] = FLOATVECTOR3(0,0,0);
}

SBVRGeogen::~SBVRGeogen(void)
{
}

float SBVRGeogen::GetOpacityCorrection() const {
  return 1.0f/m_fSamplingModifier * (FLOATVECTOR3(m_vGlobalSize)/FLOATVECTOR3(m_vLODSize)).maxVal(); //  GetLayerDistance()*m_vSize.maxVal();
}


// Should be called after updating the world or view matrices.  Causes geometry
// to be recomputed with the updated matrices.
void SBVRGeogen::MatricesUpdated()
{
  m_matViewTransform = m_matWorld * m_matView;
  InitBBOX();
  ComputeGeometry();
}

void SBVRGeogen::SetWorld(const FLOATMATRIX4& matWorld, bool bForceUpdate) {
  if (bForceUpdate || m_matWorld != matWorld) {
    m_matWorld = matWorld;
    MatricesUpdated();
  }
}
void SBVRGeogen::SetView(const FLOATMATRIX4& mTransform,
                         bool forceUpdate)
{
  if(forceUpdate || m_matView != mTransform) {
    m_matView = mTransform;
    MatricesUpdated();
  }
}

void SBVRGeogen::InitBBOX() {

  FLOATVECTOR3 vVertexScale(m_vAspect);

  m_pfBBOXVertex[0] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[0]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMax.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[1] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[1]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMax.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[2] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[2]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMax.y,m_vTexCoordMax.z));
  m_pfBBOXVertex[3] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[3]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMax.y,m_vTexCoordMax.z));
  m_pfBBOXVertex[4] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[4]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMin.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[5] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[5]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMin.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[6] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[6]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMin.y,m_vTexCoordMax.z));
  m_pfBBOXVertex[7] = POS3TEX3_VERTEX(FLOATVECTOR4(m_pfBBOXStaticVertex[7]*vVertexScale,1.0f) * m_matViewTransform, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMin.y,m_vTexCoordMax.z));
}

void SBVRGeogen::SetVolumeData(const FLOATVECTOR3& vAspect, const UINTVECTOR3& vSize) {
  m_vGlobalAspect = vAspect;
  m_vGlobalSize = vSize;
}

void SBVRGeogen::SetLODData(const UINTVECTOR3& vSize) {
  m_vLODSize = vSize;
}

void SBVRGeogen::SetBrickData(const FLOATVECTOR3& vAspect,
                              const UINTVECTOR3& vSize,
                              const FLOATVECTOR3& vTexCoordMin,
                              const FLOATVECTOR3& vTexCoordMax) {
  m_vAspect       = vAspect;
  m_vSize         = vSize;
  m_vTexCoordMin  = vTexCoordMin;
  m_vTexCoordMax  = vTexCoordMax;
  InitBBOX();
}


// Splits a triangle along a plane with the given normal.
// Assumes: plane's D == 0.
//          triangle does span the plane.
std::vector<POS3TEX3_VERTEX> SBVRGeogen::SplitTriangle(POS3TEX3_VERTEX a,
                                                  POS3TEX3_VERTEX b,
                                                  POS3TEX3_VERTEX c,
                                                  const VECTOR3<float> &normal,
                                                  const float D)
{
  std::vector<POS3TEX3_VERTEX> out;
  // We'll always throw away at least one of the generated triangles.
  out.reserve(2);
  float fa = (normal ^ a.m_vPos) + D;
  float fb = (normal ^ b.m_vPos) + D;
  float fc = (normal ^ c.m_vPos) + D;
  if(fabs(fa) < (2 * std::numeric_limits<float>::epsilon())) { fa = 0; }
  if(fabs(fb) < (2 * std::numeric_limits<float>::epsilon())) { fb = 0; }
  if(fabs(fc) < (2 * std::numeric_limits<float>::epsilon())) { fc = 0; }

  // rotation / mirroring.
  //            c
  //           o          Push `c' to be alone on one side of the plane, making
  //          / \         `a' and `b' on the other.  Later we'll be able to
  // plane ---------      assume that there will be an intersection with the
  //        /     \       clip plane along the lines `ac' and `bc'.  This
  //       o-------o      reduces the number of cases below.
  //      a         b

  // if fa*fc is non-negative, both have the same sign -- and thus are on the
  // same side of the plane.
  if(fa*fc >= 0) {
    std::swap(fb, fc);
    std::swap(b, c);
    std::swap(fa, fb);
    std::swap(a, b);
  } else if(fb*fc >= 0) {
    std::swap(fa, fc);
    std::swap(a, c);
    std::swap(fa, fb);
    std::swap(a, b);
  }

  // Find the intersection points.
  POS3TEX3_VERTEX A, B;
#ifdef _DEBUG
  const bool isect_a = RayPlaneIntersection(a,c, normal,D, A);
  const bool isect_b = RayPlaneIntersection(b,c, normal,D, B);
  assert(isect_a); // lines must cross plane
  assert(isect_b);
#else
  RayPlaneIntersection(a,c, normal,D, A);
  RayPlaneIntersection(b,c, normal,D, B);
#endif

  if(fc >= 0) {
    out.push_back(a); out.push_back(b); out.push_back(A);
    out.push_back(b); out.push_back(B); out.push_back(A);
  } else {
    out.push_back(A); out.push_back(B); out.push_back(c);
  }
  return out;
}

std::vector<POS3TEX3_VERTEX>
SBVRGeogen::ClipTriangles(const std::vector<POS3TEX3_VERTEX> &in,
              const VECTOR3<float> &normal, const float D)
{
  std::vector<POS3TEX3_VERTEX> out;
  assert(!in.empty() && in.size() > 2);
  out.reserve(in.size());

  for(std::vector<POS3TEX3_VERTEX>::const_iterator iter = in.begin();
      iter < (in.end()-2);
      iter += 3) {
    const POS3TEX3_VERTEX &a = (*iter);
    const POS3TEX3_VERTEX &b = (*(iter+1));
    const POS3TEX3_VERTEX &c = (*(iter+2));
    float fa = (normal ^ a.m_vPos) + D;
    float fb = (normal ^ b.m_vPos) + D;
    float fc = (normal ^ c.m_vPos) + D;
    if(fabs(fa) < (2 * std::numeric_limits<float>::epsilon())) { fa = 0; }
    if(fabs(fb) < (2 * std::numeric_limits<float>::epsilon())) { fb = 0; }
    if(fabs(fc) < (2 * std::numeric_limits<float>::epsilon())) { fc = 0; }
    if(fa >= 0 && fb >= 0 && fc >= 0) {        // trivial reject
      // discard -- i.e. do nothing / ignore tri.
    } else if(fa <= 0 && fb <= 0 && fc <= 0) { // trivial accept
      out.push_back(a);
      out.push_back(b);
      out.push_back(c);
    } else { // triangle spans plane -- must be split.
      const std::vector<POS3TEX3_VERTEX>& tris = SplitTriangle(a,b,c,
                                                               normal,D);
      assert(!tris.empty());
      assert(tris.size() <= 6); // vector is actually of points, not tris.

      for(std::vector<POS3TEX3_VERTEX>::const_iterator tri = tris.begin();
          tri != tris.end();
          ++tri) {
        out.push_back(*tri);
      }
    }
  }
  return out;
}

// Calculates the intersection point of a line segment lb->la which crosses the
// plane with normal `n'.
bool SBVRGeogen::RayPlaneIntersection(const POS3TEX3_VERTEX &la,
                         const POS3TEX3_VERTEX &lb,
                         const FLOATVECTOR3 &n, const float D,
                         POS3TEX3_VERTEX &hit)
{
  const FLOATVECTOR3 &va = la.m_vPos;
  const FLOATVECTOR3 &vb = lb.m_vPos;
  const float denom = n ^ (va - vb);
  if(EpsilonEqual(denom, 0.0f)) {
    return false;
  }
  const float t = ((n ^ va) + D) / denom;

  hit.m_vPos = va + (t*(vb - va));
  hit.m_vTex = la.m_vTex + t*(lb.m_vTex - la.m_vTex);

  return true;
}
