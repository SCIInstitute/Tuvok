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
           MMCI, DFKI Saarbruecken
           SCI Institute, University of Utah
  \date    December 2009
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

#include <Windows.h>

using namespace std;
using namespace tuvok;

SBVRGeogen2D::SBVRGeogen2D(void) :
  SBVRGeogen(),
  m_eMethod(METHOD_KRUEGER)
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
  return 1.0f/(m_fSamplingModifier * m_vSize[iDir] * float(sqrt(3.0)));
}


void SBVRGeogen2D::InterpolateVertices(const POS3TEX3_VERTEX& v1, const POS3TEX3_VERTEX& v2, float a, POS3TEX3_VERTEX& r) const {
  r.m_vPos = (1.0f-a)*v1.m_vPos + a*v2.m_vPos;
  r.m_vTex = (1.0f-a)*v1.m_vTex + a*v2.m_vTex;
}

void SBVRGeogen2D::ComputeGeometry() {
  switch (m_eMethod) {
    case METHOD_REZK : ComputeGeometryRezk(); break;
    case METHOD_KRUEGER : ComputeGeometryKrueger(); break;
    default : ComputeGeometryKruegerFast(); break;
  }
}


/*
  Compute 2D geometry via
  C. Rezk-Salama et al. 2000
  "Interactive Volume Rendering on Standard PC Graphics Hardware Using Multi-Textures and Multi-Stage Rasterization"
*/
void SBVRGeogen2D::ComputeGeometryRezk() {

  // compute optimal stack
  FLOATVECTOR3 vCenter = (m_pfBBOXVertex[0].m_vPos+m_pfBBOXVertex[6].m_vPos) / 2.0f;
  FLOATVECTOR3 vCoordFrame[3] = {(m_pfBBOXVertex[0].m_vPos-m_pfBBOXVertex[1].m_vPos),  // X
                                 (m_pfBBOXVertex[0].m_vPos-m_pfBBOXVertex[4].m_vPos),  // Y
                                 (m_pfBBOXVertex[0].m_vPos-m_pfBBOXVertex[3].m_vPos)}; // Z
  for (size_t i = 0;i<3;i++) vCoordFrame[i].normalize();
  float fCosX = vCenter^vCoordFrame[0];
  float fCosY = vCenter^vCoordFrame[1];
  float fCosZ = vCenter^vCoordFrame[2];
  int iStack = 2; bool bFlipStack=fCosZ<0;

  if (fabs(fCosX) > fabs(fCosY) && fabs(fCosX) > fabs(fCosZ)) {
    iStack = 0;
    bFlipStack = fCosX>0;
  } else {
    if (fabs(fCosY) > fabs(fCosX) && fabs(fCosY) > fabs(fCosZ)) {
      iStack = 1;
      bFlipStack=fCosY<0;
    }
  }

  m_vSliceTrianglesX.clear();
  m_vSliceTrianglesY.clear();
  m_vSliceTrianglesZ.clear();

  POS3TEX3_VERTEX pfSliceVertex[4];
  float fDelta = GetDelta(iStack);
  UINT32 iLayerCount = UINT32(floor(1.0f/fDelta));
  float fDepth = 0;
  if (bFlipStack) {
    fDelta *= -1;
    fDepth = 1;
  }

  switch (iStack) {
    case 0 :{
              for (UINT32 x = 0;x<iLayerCount;x++) {
                InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[0], fDepth, pfSliceVertex[0]);
                InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[3], fDepth, pfSliceVertex[1]);
                InterpolateVertices(m_pfBBOXVertex[5], m_pfBBOXVertex[4], fDepth, pfSliceVertex[2]);
                InterpolateVertices(m_pfBBOXVertex[6], m_pfBBOXVertex[7], fDepth, pfSliceVertex[3]);

                m_vSliceTrianglesX.push_back(pfSliceVertex[0]);
                m_vSliceTrianglesX.push_back(pfSliceVertex[1]);
                m_vSliceTrianglesX.push_back(pfSliceVertex[2]);

                m_vSliceTrianglesX.push_back(pfSliceVertex[1]);
                m_vSliceTrianglesX.push_back(pfSliceVertex[3]);
                m_vSliceTrianglesX.push_back(pfSliceVertex[2]);

                fDepth+=fDelta;
              }
            } break;
    case 1 :{

              for (UINT32 y = 0;y<iLayerCount;y++) {

                InterpolateVertices(m_pfBBOXVertex[0], m_pfBBOXVertex[4], fDepth, pfSliceVertex[0]);
                InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[5], fDepth, pfSliceVertex[1]);
                InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[6], fDepth, pfSliceVertex[2]);
                InterpolateVertices(m_pfBBOXVertex[3], m_pfBBOXVertex[7], fDepth, pfSliceVertex[3]);

                m_vSliceTrianglesY.push_back(pfSliceVertex[2]);
                m_vSliceTrianglesY.push_back(pfSliceVertex[1]);
                m_vSliceTrianglesY.push_back(pfSliceVertex[0]);

                m_vSliceTrianglesY.push_back(pfSliceVertex[0]);
                m_vSliceTrianglesY.push_back(pfSliceVertex[3]);
                m_vSliceTrianglesY.push_back(pfSliceVertex[2]);
                fDepth+=fDelta;
              }
            }  break;
    case 2 :{
              for (UINT32 z = 0;z<iLayerCount;z++) {
                InterpolateVertices(m_pfBBOXVertex[0], m_pfBBOXVertex[3], fDepth, pfSliceVertex[0]);
                InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[2], fDepth, pfSliceVertex[1]);
                InterpolateVertices(m_pfBBOXVertex[4], m_pfBBOXVertex[7], fDepth, pfSliceVertex[2]);
                InterpolateVertices(m_pfBBOXVertex[5], m_pfBBOXVertex[6], fDepth, pfSliceVertex[3]);

                m_vSliceTrianglesZ.push_back(pfSliceVertex[0]);
                m_vSliceTrianglesZ.push_back(pfSliceVertex[1]);
                m_vSliceTrianglesZ.push_back(pfSliceVertex[2]);

                m_vSliceTrianglesZ.push_back(pfSliceVertex[1]);
                m_vSliceTrianglesZ.push_back(pfSliceVertex[3]);
                m_vSliceTrianglesZ.push_back(pfSliceVertex[2]);
                fDepth+=fDelta;
              }
            } break;
  }
}

/*
  Compute 2D geometry alike
  Krüger 2010
  "A new sampling scheme for slice based volume rendering"
  but with a very slow approach, should be used only for demonstation
*/
void SBVRGeogen2D::ComputeGeometryKrueger() {
static float fMinCos = 0.01f;

  // at first find the planes to clip the geometry with
  // this is done by shoting rays from the eye-point
  // trougth the middle of the edges of the bounding cube
  // if those rays enter the cube after the intersection
  // we need to clip geometry at those edges


  // the annotations below are only for the untransformed
  // state, but they still help to understand the
  // orientation of the edges


  // cube's local coordinate frame
  FLOATVECTOR3 vCoordFrame[3] = {(m_pfBBOXVertex[1].m_vPos-m_pfBBOXVertex[0].m_vPos),  // X
                                 (m_pfBBOXVertex[0].m_vPos-m_pfBBOXVertex[4].m_vPos),  // Y
                                 (m_pfBBOXVertex[3].m_vPos-m_pfBBOXVertex[0].m_vPos)}; // Z
  for (size_t i = 0;i<3;i++) vCoordFrame[i].normalize();




  // edge defintion
  pair<int,int> vEdges[12] = {make_pair(3,2), // top, front, left to right
                              make_pair(0,1), // top, back, left to right
                              make_pair(3,0), // top, front to back, left
                              make_pair(2,1), // top, front to back, right
                              make_pair(7,6), // bottom, front, left to right
                              make_pair(4,5), // bottom, back, left to right
                              make_pair(7,4), // bottom, front to back, left
                              make_pair(6,5), // bottom, front to back, right
                              make_pair(3,7), // top-bottom, front, left
                              make_pair(2,6), // top-bottom, front, right 
                              make_pair(1,5), // top-bottom, back, left
                              make_pair(0,4)};// top-bottom, back, right

  // faces
  // 0 left    = -X
  // 1 right   =  X
  // 2 bottom  = -Y
  // 3 top     =  Y
  // 4 back    = -Z
  // 5 front   =  Z


  // indices of the faces adjacent to the edges
  pair<int,int> vAdjFaces[12] = {make_pair(3,5),
                                 make_pair(3,4),
                                 make_pair(3,0),
                                 make_pair(3,1),
                                 make_pair(2,5),
                                 make_pair(2,4),
                                 make_pair(2,0),
                                 make_pair(2,1),
                                 make_pair(0,5),
                                 make_pair(5,1),
                                 make_pair(1,4),
                                 make_pair(4,0)};


  // centerpoints of the edges
  FLOATVECTOR3 vEdgeCenters[12];
  for (size_t i = 0;i<12;i++) {
    vEdgeCenters[i] = (m_pfBBOXVertex[vEdges[i].first].m_vPos+m_pfBBOXVertex[vEdges[i].second].m_vPos)/2.0f;
  }

  // face normals (pointing inwards)
  FLOATVECTOR3 vFaceNormals[6] = { vCoordFrame[0],
                                  -vCoordFrame[0],
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
  for (size_t i = 0;i<vIntersects.size();i++) {
    // construct intersection planes
    size_t iEdgeIndex = vIntersects[i];
    vPlanes.push_back(
      FLOATPLANE(m_pfBBOXVertex[vEdges[iEdgeIndex].first].m_vPos,
                 m_pfBBOXVertex[vEdges[iEdgeIndex].second].m_vPos,
                 m_pfBBOXVertex[vEdges[iEdgeIndex].second].m_vPos+
                 vEdgeCenters[iEdgeIndex])
                 );
    (vPlanes.end()-1)->normalize();
  }

  // compute the face center vertices
  FLOATVECTOR3 vFaceVec[6];
  vFaceVec[0] = (m_pfBBOXVertex[0].m_vPos+m_pfBBOXVertex[4].m_vPos+
                 m_pfBBOXVertex[3].m_vPos+m_pfBBOXVertex[7].m_vPos)/4.0f;
  vFaceVec[1] = (m_pfBBOXVertex[6].m_vPos+m_pfBBOXVertex[2].m_vPos+
                 m_pfBBOXVertex[1].m_vPos+m_pfBBOXVertex[5].m_vPos)/4.0f;
  vFaceVec[2] = (m_pfBBOXVertex[4].m_vPos+m_pfBBOXVertex[5].m_vPos+
                 m_pfBBOXVertex[6].m_vPos+m_pfBBOXVertex[7].m_vPos)/4.0f;
  vFaceVec[3] = (m_pfBBOXVertex[0].m_vPos+m_pfBBOXVertex[1].m_vPos+
                 m_pfBBOXVertex[2].m_vPos+m_pfBBOXVertex[3].m_vPos)/4.0f;
  vFaceVec[4] = (m_pfBBOXVertex[0].m_vPos+m_pfBBOXVertex[1].m_vPos+
                 m_pfBBOXVertex[4].m_vPos+m_pfBBOXVertex[5].m_vPos)/4.0f;
  vFaceVec[5] = (m_pfBBOXVertex[3].m_vPos+m_pfBBOXVertex[2].m_vPos+
                 m_pfBBOXVertex[7].m_vPos+m_pfBBOXVertex[6].m_vPos)/4.0f;
  for (int i = 0;i<6;i++) vFaceVec[i].normalize();

  float fCosAngleX = max(vFaceVec[0]^ vCoordFrame[0],vFaceVec[1]^-vCoordFrame[0]);
  float fCosAngleY = max(vFaceVec[2]^ vCoordFrame[1],vFaceVec[3]^-vCoordFrame[1]);
  float fCosAngleZ = max(vFaceVec[4]^ vCoordFrame[2],vFaceVec[5]^-vCoordFrame[2]);

  float normalization = sqrt(fCosAngleX*fCosAngleX+fCosAngleY*fCosAngleY+fCosAngleZ*fCosAngleZ);
  fCosAngleX /= normalization;
  fCosAngleY /= normalization;
  fCosAngleZ /= normalization;

  m_vSliceTrianglesX.clear();
  m_vSliceTrianglesY.clear();
  m_vSliceTrianglesZ.clear();

  POS3TEX3_VERTEX pfSliceVertex[4];

  m_fDelta.x = GetDelta(0)*fCosAngleX;
  m_fDelta.y = GetDelta(1)*fCosAngleY;
  m_fDelta.z = GetDelta(2)*fCosAngleZ;

  // if something of the x stack is visible
  if (fCosAngleX > fMinCos) {
    UINT32 iLayerCount = UINT32(floor(1.0f/m_fDelta.x));

    // detemine if we a moving back to front or front to back
    // (as seen from the untransfrormed orientation)
    float a = 0;
    if ((vFaceVec[0]^vCoordFrame[0]) > 0.0f) {
      m_fDelta.x *= -1;
      a = 1;
    }

    // generate ALL stack quads
    for (UINT32 x = 0;x<iLayerCount;x++) {

      InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[3], a, pfSliceVertex[0]);
      InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[0], a, pfSliceVertex[1]);
      InterpolateVertices(m_pfBBOXVertex[5], m_pfBBOXVertex[4], a, pfSliceVertex[2]);
      InterpolateVertices(m_pfBBOXVertex[6], m_pfBBOXVertex[7], a, pfSliceVertex[3]);

      m_vSliceTrianglesX.push_back(pfSliceVertex[2]);
      m_vSliceTrianglesX.push_back(pfSliceVertex[1]);
      m_vSliceTrianglesX.push_back(pfSliceVertex[0]);

      m_vSliceTrianglesX.push_back(pfSliceVertex[0]);
      m_vSliceTrianglesX.push_back(pfSliceVertex[3]);
      m_vSliceTrianglesX.push_back(pfSliceVertex[2]);

      a+=m_fDelta.x;
    }

    // clip at layer seperation planes
    for (size_t i = 0;i<vPlanes.size();i++) {
      size_t edge = vIntersects[i];
      if (edge == 10 || edge == 8 || edge == 3 || edge == 6)
         m_vSliceTrianglesX = ClipTriangles(m_vSliceTrianglesX,  vPlanes[i].xyz(), vPlanes[i].d());
      if (edge == 7   || edge == 9 || edge == 2 || edge == 11 )
         m_vSliceTrianglesX = ClipTriangles(m_vSliceTrianglesX, -vPlanes[i].xyz(), vPlanes[i].d());
    }
  }
  
  
  // if something of the y stack is visible
  if (fCosAngleY > fMinCos) {

    UINT32 iLayerCount = UINT32(floor(1.0f/m_fDelta.y));
    float a = 0;

    // detemine if we a moving back to front or front to back
    // (as seen from the untransfrormed orientation)
    if ((vFaceVec[2]^vCoordFrame[1]) > 0.0f) {
      m_fDelta.y *= -1;
      a = 1;
    }

    // generate ALL stack quads
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

      a+=m_fDelta.y;
    }

    // clip at layer seperation planes
    for (size_t i = 0;i<vPlanes.size();i++) {
      size_t edge = vIntersects[i];
      if ( edge == 4 || edge == 1 || edge == 2 || edge == 7)
         m_vSliceTrianglesY = ClipTriangles(m_vSliceTrianglesY, vPlanes[i].xyz(), vPlanes[i].d());
      if (edge == 0  || edge == 5 || edge == 3 || edge == 6)
         m_vSliceTrianglesY = ClipTriangles(m_vSliceTrianglesY, -vPlanes[i].xyz(), vPlanes[i].d());
    }
  }

  // if something of the z stack is visible
  if (fCosAngleZ > fMinCos) {
    UINT32 iLayerCount = UINT32(floor(1.0f/m_fDelta.z));
    float a = 0;

    // detemine if we a moving back to front or front to back
    // (as seen from the untransfrormed orientation)
    if ((vFaceVec[4]^vCoordFrame[2]) > 0.0f){
      m_fDelta.z *= -1;
      a = 1;
    }

   // generate ALL stack quads
    for (UINT32 z = 0;z<iLayerCount;z++) {
      InterpolateVertices(m_pfBBOXVertex[3], m_pfBBOXVertex[0], a, pfSliceVertex[0]);
      InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[1], a, pfSliceVertex[1]);
      InterpolateVertices(m_pfBBOXVertex[6], m_pfBBOXVertex[5], a, pfSliceVertex[2]);
      InterpolateVertices(m_pfBBOXVertex[7], m_pfBBOXVertex[4], a, pfSliceVertex[3]);

      m_vSliceTrianglesZ.push_back(pfSliceVertex[2]);
      m_vSliceTrianglesZ.push_back(pfSliceVertex[1]);
      m_vSliceTrianglesZ.push_back(pfSliceVertex[0]);

      m_vSliceTrianglesZ.push_back(pfSliceVertex[0]);
      m_vSliceTrianglesZ.push_back(pfSliceVertex[3]);
      m_vSliceTrianglesZ.push_back(pfSliceVertex[2]);
      a+=m_fDelta.z;
    }

    // clip at layer seperation planes
    for (size_t i = 0;i<vPlanes.size();i++) {
      size_t edge = vIntersects[i];
      if (edge == 11 || edge == 5 || edge == 0 || edge == 9)
        m_vSliceTrianglesZ = ClipTriangles(m_vSliceTrianglesZ, vPlanes[i].xyz(), vPlanes[i].d());
      if (edge == 10 || edge == 8 || edge == 1 || edge == 4)
        m_vSliceTrianglesZ = ClipTriangles(m_vSliceTrianglesZ, -vPlanes[i].xyz(), vPlanes[i].d());
    }
  }
  
}




/*
  Compute 2D geometry via
  Krüger 2010
  "A new sampling scheme for slice based volume rendering"
*/
void SBVRGeogen2D::ComputeGeometryKruegerFast() {
  // TODO
}
