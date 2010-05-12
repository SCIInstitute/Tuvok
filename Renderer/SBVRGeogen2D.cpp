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

#include "StdTuvokDefines.h"
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
using namespace tuvok;

static const float fMinCos = 0.01f;


SBVRGeogen2D::SBVRGeogen2D(void) :
  SBVRGeogen(),
  m_eMethod(METHOD_KRUEGER_FAST)
{
  m_vSliceTrianglesOrder[0] = DIRECTION_X;
  m_vSliceTrianglesOrder[1] = DIRECTION_Y;
  m_vSliceTrianglesOrder[2] = DIRECTION_Z;
}

SBVRGeogen2D::~SBVRGeogen2D(void)
{
}

float SBVRGeogen2D::GetDelta(int iDir) const {
  return 1.0f/(m_fSamplingModifier * m_vSize[iDir] * 1.414213562f); // = sqrt(2)
}


void SBVRGeogen2D::InterpolateVertices(const POS3TEX3_VERTEX& v1, 
                                       const POS3TEX3_VERTEX& v2, 
                                       float a, POS3TEX3_VERTEX& r) const {
  r.m_vPos = (1.0f-a)*v1.m_vPos + a*v2.m_vPos;
  r.m_vTex = (1.0f-a)*v1.m_vTex + a*v2.m_vTex;
}

void SBVRGeogen2D::ComputeGeometry() {
  switch (m_eMethod) {
    case METHOD_REZK : ComputeGeometryRezk(); break;
    case METHOD_KRUEGER : ComputeGeometryKrueger(); break;
    default : ComputeGeometryKruegerFast(); break;
  }

  if(m_bClipPlaneEnabled) {
    PLANE<float> transformed = m_ClipPlane * m_matView;
    const FLOATVECTOR3 normal(transformed.xyz());
    const float d = transformed.d();
    m_vSliceTrianglesX = ClipTriangles(m_vSliceTrianglesX, normal, d);
    m_vSliceTrianglesY = ClipTriangles(m_vSliceTrianglesY, normal, d);
    m_vSliceTrianglesZ = ClipTriangles(m_vSliceTrianglesZ, normal, d);
  }

}


/*
  Compute 2D geometry via C. Rezk-Salama et al. 2000
  "Interactive Volume Rendering on Standard PC Graphics Hardware 
   Using Multi-Textures and Multi-Stage Rasterization"
*/
void SBVRGeogen2D::ComputeGeometryRezk() {

  // compute optimal stack
  FLOATVECTOR3 vCenter = (m_pfBBOXVertex[0].m_vPos+
                                      m_pfBBOXVertex[6].m_vPos) / 2.0f;
  FLOATVECTOR3 vCoordFrame[3] = {(m_pfBBOXVertex[0].m_vPos-
                                             m_pfBBOXVertex[1].m_vPos),  // X
                                 (m_pfBBOXVertex[0].m_vPos-
                                             m_pfBBOXVertex[4].m_vPos),  // Y
                                 (m_pfBBOXVertex[0].m_vPos-
                                             m_pfBBOXVertex[3].m_vPos)}; // Z
  for (size_t i = 0;i<3;i++) vCoordFrame[i].normalize();
  float fCosX = vCenter^vCoordFrame[0];
  float fCosY = vCenter^vCoordFrame[1];
  float fCosZ = vCenter^vCoordFrame[2];
  int iStack = 2; bool bFlipStack=fCosZ>0;

  if (fabs(fCosX) > fabs(fCosY) && fabs(fCosX) > fabs(fCosZ)) {
    iStack = 0;
    bFlipStack = fCosX<0;
  } else {
    if (fabs(fCosY) > fabs(fCosX) && fabs(fCosY) > fabs(fCosZ)) {
      iStack = 1;
      bFlipStack=fCosY>0;
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
                InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[0], 
                                    fDepth, pfSliceVertex[0]);
                InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[3], 
                                    fDepth, pfSliceVertex[1]);
                InterpolateVertices(m_pfBBOXVertex[5], m_pfBBOXVertex[4],
                                    fDepth, pfSliceVertex[2]);
                InterpolateVertices(m_pfBBOXVertex[6], m_pfBBOXVertex[7],
                                    fDepth, pfSliceVertex[3]);

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

                InterpolateVertices(m_pfBBOXVertex[0], m_pfBBOXVertex[4], 
                                    fDepth, pfSliceVertex[0]);
                InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[5],
                                    fDepth, pfSliceVertex[1]);
                InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[6],
                                    fDepth, pfSliceVertex[2]);
                InterpolateVertices(m_pfBBOXVertex[3], m_pfBBOXVertex[7],
                                    fDepth, pfSliceVertex[3]);

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
                InterpolateVertices(m_pfBBOXVertex[0], m_pfBBOXVertex[3],
                                    fDepth, pfSliceVertex[0]);
                InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[2],
                                    fDepth, pfSliceVertex[1]);
                InterpolateVertices(m_pfBBOXVertex[4], m_pfBBOXVertex[7],
                                    fDepth, pfSliceVertex[2]);
                InterpolateVertices(m_pfBBOXVertex[5], m_pfBBOXVertex[6],
                                    fDepth, pfSliceVertex[3]);

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
  Compute 2D geometry alike Krüger 2010
  "A new sampling scheme for slice based volume rendering"
  but with a very slow approach, should be used only for demonstation
*/
void SBVRGeogen2D::ComputeGeometryKrueger() {

  // at first find the planes to clip the geometry with
  // this is done by shoting rays from the eye-point
  // trougth the middle of the edges of the bounding cube
  // if those rays enter the cube after the intersection
  // we need to clip geometry at those edges


  // the annotations below are only for the untransformed
  // state, but they still help to understand the
  // orientation of the edges


  // cube's local coordinate frame
  FLOATVECTOR3 vCoordFrame[3] = {(m_pfBBOXVertex[1].m_vPos-
                                  m_pfBBOXVertex[0].m_vPos),  // X
                                 (m_pfBBOXVertex[0].m_vPos-
                                 m_pfBBOXVertex[4].m_vPos),  // Y
                                 (m_pfBBOXVertex[3].m_vPos-
                                 m_pfBBOXVertex[0].m_vPos)}; // Z
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
    vEdgeCenters[i] = (m_pfBBOXVertex[vEdges[i].first].m_vPos+
                              m_pfBBOXVertex[vEdges[i].second].m_vPos)/2.0f;
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

  float fCosAngleX = max(vFaceVec[0]^ vCoordFrame[0],
                         vFaceVec[1]^-vCoordFrame[0]);
  float fCosAngleY = max(vFaceVec[2]^ vCoordFrame[1],
                         vFaceVec[3]^-vCoordFrame[1]);
  float fCosAngleZ = max(vFaceVec[4]^ vCoordFrame[2],
                         vFaceVec[5]^-vCoordFrame[2]);

  float normalization = sqrt(fCosAngleX*fCosAngleX+
                             fCosAngleY*fCosAngleY+
                             fCosAngleZ*fCosAngleZ);
  fCosAngleX /= normalization;
  fCosAngleY /= normalization;
  fCosAngleZ /= normalization;

  m_vSliceTrianglesX.clear();
  m_vSliceTrianglesY.clear();
  m_vSliceTrianglesZ.clear();

  POS3TEX3_VERTEX pfSliceVertex[4];

  FLOATVECTOR3 fDelta;
  fDelta.x = GetDelta(0)*fCosAngleX;
  fDelta.y = GetDelta(1)*fCosAngleY;
  fDelta.z = GetDelta(2)*fCosAngleZ;

  // if something of the x stack is visible
  if (fCosAngleX > fMinCos) {
    UINT32 iLayerCount = UINT32(floor(1.0f/fDelta.x));

    // detemine if we a moving back to front or front to back
    // (as seen from the untransfrormed orientation)
    float a = 0;
    if ((vFaceVec[0]^vCoordFrame[0]) > 0.0f) {
      fDelta.x *= -1;
      a = 1;
    }

    // generate ALL stack quads
    for (UINT32 x = 0;x<iLayerCount;x++) {

      InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[3],
                          a, pfSliceVertex[0]);
      InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[0],
                          a, pfSliceVertex[1]);
      InterpolateVertices(m_pfBBOXVertex[5], m_pfBBOXVertex[4],
                          a, pfSliceVertex[2]);
      InterpolateVertices(m_pfBBOXVertex[6], m_pfBBOXVertex[7],
                          a, pfSliceVertex[3]);

      m_vSliceTrianglesX.push_back(pfSliceVertex[2]);
      m_vSliceTrianglesX.push_back(pfSliceVertex[1]);
      m_vSliceTrianglesX.push_back(pfSliceVertex[0]);

      m_vSliceTrianglesX.push_back(pfSliceVertex[0]);
      m_vSliceTrianglesX.push_back(pfSliceVertex[3]);
      m_vSliceTrianglesX.push_back(pfSliceVertex[2]);

      a+=fDelta.x;
    }

    // clip at layer seperation planes
    for (size_t i = 0;i<vPlanes.size();i++) {
      size_t edge = vIntersects[i];
      if (edge == 10 || edge == 8 || edge == 3 || edge == 6)
         m_vSliceTrianglesX = ClipTriangles(m_vSliceTrianglesX,
                                            vPlanes[i].xyz(), vPlanes[i].d());
      if (edge == 7   || edge == 9 || edge == 2 || edge == 11 )
         m_vSliceTrianglesX = ClipTriangles(m_vSliceTrianglesX,
                                            -vPlanes[i].xyz(), vPlanes[i].d());
    }
  }
  
  
  // if something of the y stack is visible
  if (fCosAngleY > fMinCos) {

    UINT32 iLayerCount = UINT32(floor(1.0f/fDelta.y));
    float a = 0;

    // detemine if we a moving back to front or front to back
    // (as seen from the untransfrormed orientation)
    if ((vFaceVec[2]^vCoordFrame[1]) > 0.0f) {
      fDelta.y *= -1;
      a = 1;
    }

    // generate ALL stack quads
    for (UINT32 y = 0;y<iLayerCount;y++) {

      InterpolateVertices(m_pfBBOXVertex[0], m_pfBBOXVertex[4],
                          a, pfSliceVertex[0]);
      InterpolateVertices(m_pfBBOXVertex[1], m_pfBBOXVertex[5],
                          a, pfSliceVertex[1]);
      InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[6],
                          a, pfSliceVertex[2]);
      InterpolateVertices(m_pfBBOXVertex[3], m_pfBBOXVertex[7],
                          a, pfSliceVertex[3]);

      m_vSliceTrianglesY.push_back(pfSliceVertex[2]);
      m_vSliceTrianglesY.push_back(pfSliceVertex[1]);
      m_vSliceTrianglesY.push_back(pfSliceVertex[0]);

      m_vSliceTrianglesY.push_back(pfSliceVertex[0]);
      m_vSliceTrianglesY.push_back(pfSliceVertex[3]);
      m_vSliceTrianglesY.push_back(pfSliceVertex[2]);

      a+=fDelta.y;
    }

    // clip at layer seperation planes
    for (size_t i = 0;i<vPlanes.size();i++) {
      size_t edge = vIntersects[i];
      if ( edge == 4 || edge == 1 || edge == 2 || edge == 7)
         m_vSliceTrianglesY = ClipTriangles(m_vSliceTrianglesY,
                                            vPlanes[i].xyz(), vPlanes[i].d());
      if (edge == 0  || edge == 5 || edge == 3 || edge == 6)
         m_vSliceTrianglesY = ClipTriangles(m_vSliceTrianglesY,
                                            -vPlanes[i].xyz(), vPlanes[i].d());
    }
  }

  // if something of the z stack is visible
  if (fCosAngleZ > fMinCos) {
    UINT32 iLayerCount = UINT32(floor(1.0f/fDelta.z));
    float a = 0;

    // detemine if we a moving back to front or front to back
    // (as seen from the untransfrormed orientation)
    if ((vFaceVec[4]^vCoordFrame[2]) > 0.0f){
      fDelta.z *= -1;
      a = 1;
    }

   // generate ALL stack quads
    for (UINT32 z = 0;z<iLayerCount;z++) {
      InterpolateVertices(m_pfBBOXVertex[3], m_pfBBOXVertex[0],
                          a, pfSliceVertex[0]);
      InterpolateVertices(m_pfBBOXVertex[2], m_pfBBOXVertex[1],
                          a, pfSliceVertex[1]);
      InterpolateVertices(m_pfBBOXVertex[6], m_pfBBOXVertex[5],
                          a, pfSliceVertex[2]);
      InterpolateVertices(m_pfBBOXVertex[7], m_pfBBOXVertex[4],
                          a, pfSliceVertex[3]);

      m_vSliceTrianglesZ.push_back(pfSliceVertex[2]);
      m_vSliceTrianglesZ.push_back(pfSliceVertex[1]);
      m_vSliceTrianglesZ.push_back(pfSliceVertex[0]);

      m_vSliceTrianglesZ.push_back(pfSliceVertex[0]);
      m_vSliceTrianglesZ.push_back(pfSliceVertex[3]);
      m_vSliceTrianglesZ.push_back(pfSliceVertex[2]);
      a+=fDelta.z;
    }

    // clip at layer seperation planes
    for (size_t i = 0;i<vPlanes.size();i++) {
      size_t edge = vIntersects[i];
      if (edge == 11 || edge == 5 || edge == 0 || edge == 9)
        m_vSliceTrianglesZ = ClipTriangles(m_vSliceTrianglesZ,
                                           vPlanes[i].xyz(), vPlanes[i].d());
      if (edge == 10 || edge == 8 || edge == 1 || edge == 4)
        m_vSliceTrianglesZ = ClipTriangles(m_vSliceTrianglesZ,
                                           -vPlanes[i].xyz(), vPlanes[i].d());
    }
  }
  
}

/*
  Compute 2D geometry via Krüger 2010
  "A new sampling scheme for slice based volume rendering"
*/
void SBVRGeogen2D::ComputeGeometryKruegerFast() {

  // at first find the planes to clip the geometry with
  // this is done by shoting rays from the eye-point
  // trougth the middle of the edges of the bounding cube
  // if those rays enter the cube after the intersection
  // we need to clip geometry at those edges

  // the annotations below are only for the untransformed
  // state, but they still help to understand the
  // orientation of the edges


  // cube's local coordinate frame
  FLOATVECTOR3 vCoordFrame[3] = {(m_pfBBOXVertex[1].m_vPos-
                                  m_pfBBOXVertex[0].m_vPos),  // X
                                 (m_pfBBOXVertex[0].m_vPos-
                                  m_pfBBOXVertex[4].m_vPos),  // Y 
                                 (m_pfBBOXVertex[3].m_vPos-
                                  m_pfBBOXVertex[0].m_vPos)}; // Z
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
    vEdgeCenters[i] = (m_pfBBOXVertex[vEdges[i].first].m_vPos+
                       m_pfBBOXVertex[vEdges[i].second].m_vPos)/2.0f;
  }

  // face normals (pointing inwards)
  FLOATVECTOR3 vFaceNormals[6] = { vCoordFrame[0],
                                  -vCoordFrame[0],
                                   vCoordFrame[1], 
                                  -vCoordFrame[1], 
                                   vCoordFrame[2],
                                  -vCoordFrame[2]};


  // find edges creating intersecting planes
  vector<size_t> vIntersects;
  vector<FLOATPLANE> vIntersectPlanes;
  for (size_t i = 0;i<12;i++) {
    FLOATVECTOR3 vDir = vEdgeCenters[i];

    float a = vFaceNormals[vAdjFaces[i].first]^vDir;
    float b = vFaceNormals[vAdjFaces[i].second]^vDir;

    if (a > 0 && b > 0) {
      vIntersects.push_back(i);

      FLOATPLANE v = FLOATPLANE(m_pfBBOXVertex[vEdges[i].first].m_vPos,
                                m_pfBBOXVertex[vEdges[i].second].m_vPos,
                                m_pfBBOXVertex[vEdges[i].second].m_vPos+
                                vEdgeCenters[i]);
      v.normalize();

      vIntersectPlanes.push_back(v);
    }
  }

  // compute the face center vertices
  FLOATVECTOR3 vFaceVec[8];
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
  for (int i = 0;i<6;i++) {
    vFaceVec[i].normalize();
  }

  float fCosAngleX = max(vFaceVec[0]^ vCoordFrame[0],
                         vFaceVec[1]^-vCoordFrame[0]);
  float fCosAngleY = max(vFaceVec[2]^ vCoordFrame[1],
                         vFaceVec[3]^-vCoordFrame[1]);
  float fCosAngleZ = max(vFaceVec[4]^ vCoordFrame[2],
                         vFaceVec[5]^-vCoordFrame[2]);

  if (fCosAngleX < 0 && fCosAngleY < 0 && fCosAngleZ < 0) {
    // inside the volume use christophs approach 
    ComputeGeometryRezk();
    return;
  }

  float normalization = sqrt(fCosAngleX*fCosAngleX+
                             fCosAngleY*fCosAngleY+
                             fCosAngleZ*fCosAngleZ);
  fCosAngleX /= normalization;
  fCosAngleY /= normalization;
  fCosAngleZ /= normalization;

  m_vSliceTrianglesX.clear();
  m_vSliceTrianglesY.clear();
  m_vSliceTrianglesZ.clear();

  FLOATVECTOR3 fDelta;
  fDelta.x = GetDelta(0)*fCosAngleX;
  fDelta.y = GetDelta(1)*fCosAngleY;
  fDelta.z = GetDelta(2)*fCosAngleZ;

  // if something of the x stack is visible compute geometry
  if (fCosAngleX > fMinCos) {
    const size_t vertexIndices[8] = {1,2,6,5,0,3,7,4};
    const size_t edgeIndices[8] = {3,7,10,9,2,6,11,8};
    BuildStackQuads(0,
                    fDelta.x,
                    vertexIndices,
                    edgeIndices,
                    vFaceVec,
                    vIntersects,
                    vIntersectPlanes,
                    vCoordFrame,
                    m_vSliceTrianglesX);
  }

  // if something of the y stack is visible compute geometry
  if (fCosAngleY > fMinCos) {
    const size_t vertexIndices[8] = {0,1,2,3,4,5,6,7};
    const size_t edgeIndices[8] = {1,0,2,3,5,4,6,7};
    BuildStackQuads(1,
                    fDelta.y,
                    vertexIndices,
                    edgeIndices,
                    vFaceVec,
                    vIntersects,
                    vIntersectPlanes,
                    vCoordFrame,
                    m_vSliceTrianglesY);
  }

  // if something of the z stack is visible compute geometry
  if (fCosAngleZ > fMinCos) {
    const size_t vertexIndices[8] = {3,2,6,7,0,1,5,4};
    const size_t edgeIndices[8] = {0,4,8,9,1,5,11,10};
    BuildStackQuads(2,
                    fDelta.z,
                    vertexIndices,
                    edgeIndices,
                    vFaceVec,
                    vIntersects,
                    vIntersectPlanes,
                    vCoordFrame,
                    m_vSliceTrianglesZ);
  }
}

class HitEdge {
public:
  int m_p0;
  int m_p1;
  int m_p0Sec;
  int m_iChange;

  HitEdge() :
    m_p0(0),
    m_p1(0),
    m_iChange(0)  
  {}

  void set(int p0, int p1, int p0Sec = -1) {
    m_p0 = p0;
    m_p1 = p1;
    m_p0Sec = p0Sec;
    m_iChange = (p0Sec < 0) ? 1 : 2;
  }
};

void SBVRGeogen2D::BuildStackQuads(
                     const int iDirIndex,
                     const float fDelta,
                     const size_t *vertexIndices,
                     const size_t *edgeIndices,

                     const FLOATVECTOR3* vFaceVec,
                     const vector<size_t>& vIntersects,
                     const vector<FLOATPLANE>& vIntersectPlanes,
                     const FLOATVECTOR3* vCoordFrame,

                     vector<POS3TEX3_VERTEX>& m_vSliceTriangles
                     ) {
  POS3TEX3_VERTEX pVertices[8];
  size_t pEdges[8];

  
  // set unclipped front and back vertices
  if ((vFaceVec[iDirIndex*2]^vCoordFrame[iDirIndex]) <= 0.0f){
    for (int i = 0;i<4;i++) {
      pVertices[i] = m_pfBBOXVertex[vertexIndices[i]];
      pVertices[i+4] = m_pfBBOXVertex[vertexIndices[i+4]];
      pEdges[i] = edgeIndices[i];
    } 
  } else {
    for (int i = 0;i<4;i++) {
      pVertices[i] = m_pfBBOXVertex[vertexIndices[i+4]];
      pVertices[i+4] = m_pfBBOXVertex[vertexIndices[i]];
      pEdges[i] = edgeIndices[i+4];
    }
  }

  // compute splice vertices 
  POS3TEX3_VERTEX pfSliceVertex[4];
  for (size_t i = 0;i<vIntersects.size();i++) {
    
    // get intersection planes
    size_t iEdgeIndex = vIntersects[i];
    FLOATPLANE v = vIntersectPlanes[i];

    // specify vertices that would be influenced by an intersection
    HitEdge bp[4];
    if (iEdgeIndex == pEdges[0]) {
      bp[0].set(4,7);
      bp[1].set(5,6);
      bp[2].set(7,3,4);
      bp[3].set(6,2,5);
    } else if (iEdgeIndex ==  pEdges[1]) {
      bp[0].set(7,4);
      bp[1].set(6,5);
      bp[2].set(4,0,7);
      bp[3].set(5,1,6);
    } else if (iEdgeIndex ==  pEdges[2]) {
      bp[0].set(4,5);
      bp[1].set(7,6);
      bp[2].set(5,1,4);
      bp[3].set(6,2,7);
    } else if (iEdgeIndex ==  pEdges[3]) {
      bp[0].set(5,4);
      bp[1].set(6,7);
      bp[2].set(4,0,5);
      bp[3].set(7,3,6);    
    } else continue;

    // test for intersection and shift vertices if necessary
    for (int j = 0;j<4;j++) {
      if (bp[j].m_iChange > 0) {

        float t;
        if (!v.intersect( pVertices[bp[j].m_p0].m_vPos,
                          pVertices[bp[j].m_p1].m_vPos,
                          t)) {
          continue;
        }

        if (t < 0 || t > 1) continue;

        InterpolateVertices(pVertices[bp[j].m_p0],
                            pVertices[bp[j].m_p1], t, pVertices[bp[j].m_p0] );
        if (bp[j].m_iChange > 1) {
          pVertices[bp[j].m_p0Sec] = pVertices[bp[j].m_p0];            
        }
      }
    }
  }

  // compute depth of stack
  float fDistScale = min(
      min((pVertices[0].m_vPos-pVertices[4].m_vPos).length() / 
          (m_pfBBOXVertex[vertexIndices[0]].m_vPos-
           m_pfBBOXVertex[vertexIndices[4]].m_vPos).length()
          , 
          (pVertices[1].m_vPos-pVertices[5].m_vPos).length() / 
          (m_pfBBOXVertex[vertexIndices[1]].m_vPos-
           m_pfBBOXVertex[vertexIndices[5]].m_vPos).length()),
      min((pVertices[3].m_vPos-pVertices[7].m_vPos).length() / 
          (m_pfBBOXVertex[vertexIndices[3]].m_vPos-
           m_pfBBOXVertex[vertexIndices[7]].m_vPos).length()
          , 
          (pVertices[2].m_vPos-pVertices[6].m_vPos).length() / 
          (m_pfBBOXVertex[vertexIndices[2]].m_vPos-
           m_pfBBOXVertex[vertexIndices[6]].m_vPos).length()));


  // compute number of layers to cover depth
  UINT32 iLayerCount = UINT32(floor(fDistScale/fDelta));

  m_vSliceTriangles.resize(iLayerCount*6);
  size_t iVectorIndex = 0;

  // interpolate the required stack quads
  while (iVectorIndex < iLayerCount*6) {
    float t  = (iVectorIndex/6) * fDelta/fDistScale;

    InterpolateVertices(pVertices[0], pVertices[4], t, pfSliceVertex[0]);
    InterpolateVertices(pVertices[1], pVertices[5], t, pfSliceVertex[1]);
    InterpolateVertices(pVertices[2], pVertices[6], t, pfSliceVertex[2]);
    InterpolateVertices(pVertices[3], pVertices[7], t, pfSliceVertex[3]);

    m_vSliceTriangles[iVectorIndex++] = pfSliceVertex[2];
    m_vSliceTriangles[iVectorIndex++] = pfSliceVertex[1];
    m_vSliceTriangles[iVectorIndex++] = pfSliceVertex[0];

    m_vSliceTriangles[iVectorIndex++] = pfSliceVertex[0];
    m_vSliceTriangles[iVectorIndex++] = pfSliceVertex[3];
    m_vSliceTriangles[iVectorIndex++] = pfSliceVertex[2];      
  }
}
