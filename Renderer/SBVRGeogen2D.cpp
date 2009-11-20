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

using namespace std;

#if 0
static bool CheckOrdering(const FLOATVECTOR3& a, const FLOATVECTOR3& b,
                          const FLOATVECTOR3& c);
static void SortPoints(vector<POS3TEX3_VERTEX> &fArray);
#endif

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
  return UINT32(ceil(m_fSamplingModifier * m_vSize[iDir] * sqrt(3.0)));
}

float SBVRGeogen2D::GetDelta(int iDir) const {
  return 1.0f/(m_fSamplingModifier * m_vSize[iDir] * sqrt(3.0));
}


void SBVRGeogen2D::InterpolateVertices(const POS3TEX3_VERTEX& v1, const POS3TEX3_VERTEX& v2, float a, POS3TEX3_VERTEX& r) const {
  r.m_vPos = (1.0f-a)*v1.m_vPos + a*v2.m_vPos;
  r.m_vTex = (1.0f-a)*v1.m_vTex + a*v2.m_vTex;
}


void SBVRGeogen2D::ComputeGeometry() {

  // at first find the planes to clip the geometry with
  // this is done by shoting rays from the eye-point
  // trougth the middle of the edges of the bounding cube
  // if those rays enter the cube after the intersection
  // we need to clip geometry at those edges


  // the annotations below are only for the untransformed 
  // state, but they still help to understand the 
  // orientation of the edges

  // cube's local coordinate frame
  FLOATVECTOR3 vCoordFrame[3] = {(m_pfBBOXVertex[0].m_vPos-m_pfBBOXVertex[1].m_vPos),  // X
                                 (m_pfBBOXVertex[0].m_vPos-m_pfBBOXVertex[4].m_vPos),  // Y
                                 (m_pfBBOXVertex[0].m_vPos-m_pfBBOXVertex[3].m_vPos)}; // Z
  for (size_t i = 0;i<3;i++) vCoordFrame[i].normalize();




  // edge defintion
  pair<int,int> vEdges[12] = {make_pair(3,2), // top, front, left to right
                              make_pair(1,0), // top, back, left to right
                              make_pair(3,0), // top, front to back, left
                              make_pair(1,2), // top, front to back, right
                              make_pair(6,7), // bottom, front, left to right
                              make_pair(4,5), // bottom, back, left to right
                              make_pair(4,7), // bottom, front to back, left
                              make_pair(6,5), // bottom, front to back, right
                              make_pair(7,3), // top-bottom, front, left
                              make_pair(2,6), // top-bottom, front, right
                              make_pair(5,1), // top-bottom, back, left
                              make_pair(0,4)};// top-bottom, back, right

  // indices of the faces adjacent to the edges
  pair<int,int> vAdjFaces[12] = {make_pair(3,4),
                                 make_pair(3,5),
                                 make_pair(3,0),
                                 make_pair(3,1),
                                 make_pair(2,4),
                                 make_pair(2,5),
                                 make_pair(2,0),
                                 make_pair(2,1),
                                 make_pair(0,4),
                                 make_pair(4,1),
                                 make_pair(1,5),
                                 make_pair(5,0)};

    
  // centerpoints of the edges
  FLOATVECTOR3 vEdgeCenters[12];
  for (size_t i = 0;i<12;i++) {
    vEdgeCenters[i] = (m_pfBBOXVertex[vEdges[i].first].m_vPos+m_pfBBOXVertex[vEdges[i].second].m_vPos)/2.0f;
  }

  // face normals
  FLOATVECTOR3 vFaceNormals[6] = {-vCoordFrame[0],
                                   vCoordFrame[0],
                                   vCoordFrame[1],
                                  -vCoordFrame[1],
                                   vCoordFrame[2],
                                  -vCoordFrame[2]};


  vector<size_t> vIntersects;
  for (size_t i = 0;i<12;i++) {
    FLOATVECTOR3 vDir = vEdgeCenters[i];

    float a = vFaceNormals[vAdjFaces[i].first]^vDir;
    float b = vFaceNormals[vAdjFaces[i].second]^vDir;

    if (a > 0 && b > 0) {
      vIntersects.push_back(i);
    }
  }

  vector<FLOATPLANE> vPlanes;
  vector<FLOATPLANE> vInvPlanes;
  if (vIntersects.size() > 0)  {
    for (size_t i = 0;i<vIntersects.size();i++) {
      int iIndex = vIntersects[i];
      vPlanes.push_back(FLOATPLANE(m_pfBBOXVertex[vEdges[iIndex].first].m_vPos,
                                   m_pfBBOXVertex[vEdges[iIndex].second].m_vPos,
                                   m_pfBBOXVertex[vEdges[iIndex].second].m_vPos+vEdgeCenters[iIndex]));

      (vPlanes.end()-1)->normalize();
    }
  }

  FLOATVECTOR3 vFaceVecX0 = (m_pfBBOXVertex[6].m_vPos+m_pfBBOXVertex[2].m_vPos+m_pfBBOXVertex[1].m_vPos+m_pfBBOXVertex[5].m_vPos)/4.0f;
  FLOATVECTOR3 vFaceVecX1 = (m_pfBBOXVertex[0].m_vPos+m_pfBBOXVertex[4].m_vPos+m_pfBBOXVertex[3].m_vPos+m_pfBBOXVertex[7].m_vPos)/4.0f;
  FLOATVECTOR3 vFaceVecY0 = (m_pfBBOXVertex[0].m_vPos+m_pfBBOXVertex[1].m_vPos+m_pfBBOXVertex[2].m_vPos+m_pfBBOXVertex[3].m_vPos)/4.0f;
  FLOATVECTOR3 vFaceVecY1 = (m_pfBBOXVertex[4].m_vPos+m_pfBBOXVertex[5].m_vPos+m_pfBBOXVertex[6].m_vPos+m_pfBBOXVertex[7].m_vPos)/4.0f;
  FLOATVECTOR3 vFaceVecZ0 = (m_pfBBOXVertex[3].m_vPos+m_pfBBOXVertex[2].m_vPos+m_pfBBOXVertex[7].m_vPos+m_pfBBOXVertex[6].m_vPos)/4.0f;
  FLOATVECTOR3 vFaceVecZ1 = (m_pfBBOXVertex[0].m_vPos+m_pfBBOXVertex[1].m_vPos+m_pfBBOXVertex[4].m_vPos+m_pfBBOXVertex[5].m_vPos)/4.0f;

  vFaceVecX0.normalize();
  vFaceVecX1.normalize();
  vFaceVecY0.normalize();
  vFaceVecY1.normalize();
  vFaceVecZ0.normalize();
  vFaceVecZ1.normalize();

  float fCosAngleX = max(vFaceVecX0^vCoordFrame[0],vFaceVecX1^-vCoordFrame[0]);
  float fCosAngleY = max(vFaceVecY0^-vCoordFrame[1],vFaceVecY1^vCoordFrame[1]);
  float fCosAngleZ = max(vFaceVecZ0^vCoordFrame[2],vFaceVecZ1^-vCoordFrame[2]);

  m_vSliceTrianglesX.clear();
  m_vSliceTrianglesY.clear();
  m_vSliceTrianglesZ.clear();

  POS3TEX3_VERTEX pfSliceVertex[4];
  if (fCosAngleX > 0.0001f) {
    float fDelta = GetDelta(0)*fCosAngleX;
    UINT32 iLayerCount = UINT32(floor(1.0f/fDelta));
    float a = 0;
    for (UINT32 x = 0;x<iLayerCount;x++) {

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

      a+=fDelta;
    }
    if ((vFaceVecX1^vCoordFrame[0]) < 0.0f) 
      std::reverse(m_vSliceTrianglesX.begin(), m_vSliceTrianglesX.end());

    for (size_t i = 0;i<vPlanes.size();i++) {
      size_t edge = vIntersects[i];
      if (edge == 3 || edge == 10 || edge == 9 || edge == 7 || 
          edge == 8 || edge == 2 || edge == 11 || edge == 6) 
         m_vSliceTrianglesX = ClipTriangles(m_vSliceTrianglesX, -vPlanes[i].xyz(), vPlanes[i].d());
    }
  }

  if (fCosAngleY > 0.0001f) {
    float fDelta = GetDelta(1)*fCosAngleY;
    UINT32 iLayerCount = UINT32(floor(1.0f/fDelta));
    float a = 0;
    //iLayerCount = GetLayerCount(1);
    for (UINT32 y = 0;y<iLayerCount;y++) {

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
      a+=fDelta;
    }
    if ((vFaceVecY0^-vCoordFrame[1]) > 0.0f)
      std::reverse(m_vSliceTrianglesY.begin(), m_vSliceTrianglesY.end());

    for (size_t i = 0;i<vPlanes.size();i++) {
      size_t edge = vIntersects[i];
      if (edge == 0 || edge == 4 || edge == 1 || edge == 5)
         m_vSliceTrianglesY = ClipTriangles(m_vSliceTrianglesY, -vPlanes[i].xyz(), vPlanes[i].d());
      if (edge == 2 || edge == 6 || edge == 3 || edge == 7) 
         m_vSliceTrianglesY = ClipTriangles(m_vSliceTrianglesY, vPlanes[i].xyz(), vPlanes[i].d());
    }
  }

  if (fCosAngleZ > 0.0001f) {
    float fDelta = GetDelta(2)*fCosAngleZ;
    UINT32 iLayerCount = UINT32(floor(1.0f/fDelta));
    float a = 0;
    //iLayerCount = GetLayerCount(2);
    for (UINT32 z = 0;z<iLayerCount;z++) {

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
      a+=fDelta;
    }
    if ((vFaceVecZ0^-vCoordFrame[2]) > 0.0f)
      std::reverse(m_vSliceTrianglesZ.begin(), m_vSliceTrianglesZ.end());
    for (size_t i = 0;i<vPlanes.size();i++) {
      size_t edge = vIntersects[i];
      if (edge == 0 || edge == 8 || edge == 9 || edge == 4 || 
          edge == 11 || edge == 5 || edge == 10 || edge == 1) 
         m_vSliceTrianglesZ = ClipTriangles(m_vSliceTrianglesZ, vPlanes[i].xyz(), vPlanes[i].d());
    }
  }

}
