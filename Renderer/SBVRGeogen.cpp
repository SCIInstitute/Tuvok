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

#include "StdTuvokDefines.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include "SBVRGeogen.h"

using namespace tuvok;

SBVRGeogen::SBVRGeogen(void) :
  m_fSamplingModifier(1.0f),
  m_vGlobalSize(1,1,1),
  m_vGlobalAspect(1,1,1),
  m_vLODSize(1,1,1),
  m_vAspect(1,1,1),
  m_vSize(1,1,1),
  m_vTexCoordMin(0,0,0),
  m_vTexCoordMax(1,1,1),
  m_bClipPlaneEnabled(false),
  m_bClipVolume(true),
  m_bClipMesh(false)
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


void SBVRGeogen::MatricesUpdated()
{
  FLOATMATRIX4 maBricktTrans;
  maBricktTrans.Translation(m_brickTranslation.x, 
                            m_brickTranslation.y, 
                            m_brickTranslation.z);
  m_matWorldView = maBricktTrans * m_matWorld * m_matView;
}



void SBVRGeogen::SetBrickTrans(const FLOATVECTOR3& brickTranslation) {
  if (m_brickTranslation != brickTranslation) {
    m_brickTranslation = brickTranslation;
    MatricesUpdated();
  }
}

void SBVRGeogen::SetWorld(const FLOATMATRIX4& matWorld) {
  if (m_matWorld != matWorld) {
    m_matWorld = matWorld;
    MatricesUpdated();
  }
}
void SBVRGeogen::SetView(const FLOATMATRIX4& mTransform)
{
  if(m_matView != mTransform) {
    m_matView = mTransform;
    MatricesUpdated();
  }
}

void SBVRGeogen::InitBBOX() {
  m_pfBBOXVertex[0] = VERTEX_FORMAT(FLOATVECTOR4(m_pfBBOXStaticVertex[0]*m_vAspect,1.0f) * m_matWorldView, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMax.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[1] = VERTEX_FORMAT(FLOATVECTOR4(m_pfBBOXStaticVertex[1]*m_vAspect,1.0f) * m_matWorldView, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMax.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[2] = VERTEX_FORMAT(FLOATVECTOR4(m_pfBBOXStaticVertex[2]*m_vAspect,1.0f) * m_matWorldView, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMax.y,m_vTexCoordMax.z));
  m_pfBBOXVertex[3] = VERTEX_FORMAT(FLOATVECTOR4(m_pfBBOXStaticVertex[3]*m_vAspect,1.0f) * m_matWorldView, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMax.y,m_vTexCoordMax.z));
  m_pfBBOXVertex[4] = VERTEX_FORMAT(FLOATVECTOR4(m_pfBBOXStaticVertex[4]*m_vAspect,1.0f) * m_matWorldView, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMin.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[5] = VERTEX_FORMAT(FLOATVECTOR4(m_pfBBOXStaticVertex[5]*m_vAspect,1.0f) * m_matWorldView, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMin.y,m_vTexCoordMin.z));
  m_pfBBOXVertex[6] = VERTEX_FORMAT(FLOATVECTOR4(m_pfBBOXStaticVertex[6]*m_vAspect,1.0f) * m_matWorldView, FLOATVECTOR3(m_vTexCoordMax.x,m_vTexCoordMin.y,m_vTexCoordMax.z));
  m_pfBBOXVertex[7] = VERTEX_FORMAT(FLOATVECTOR4(m_pfBBOXStaticVertex[7]*m_vAspect,1.0f) * m_matWorldView, FLOATVECTOR3(m_vTexCoordMin.x,m_vTexCoordMin.y,m_vTexCoordMax.z));
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
std::vector<VERTEX_FORMAT> SBVRGeogen::SplitTriangle(VERTEX_FORMAT a,
                                                  VERTEX_FORMAT b,
                                                  VERTEX_FORMAT c,
                                                  const VECTOR3<float> &normal,
                                                  const float D)
{
  std::vector<VERTEX_FORMAT> out;
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
  VERTEX_FORMAT A, B;
  RayPlaneIntersection(a,c, normal,D, A);
  RayPlaneIntersection(b,c, normal,D, B);

  if(fc >= 0) {
    out.push_back(a); out.push_back(b); out.push_back(A);
    out.push_back(b); out.push_back(B); out.push_back(A);
  } else {
    out.push_back(A); out.push_back(B); out.push_back(c);
  }
  return out;
}

std::vector<VERTEX_FORMAT>
SBVRGeogen::ClipTriangles(const std::vector<VERTEX_FORMAT> &in,
              const VECTOR3<float> &normal, const float D)
{
  std::vector<VERTEX_FORMAT> out;
  if (in.empty()) return out;
  assert(in.size() % 3 == 0);

  // bail out even in release (otherwise we would get a crash later)
  if (in.size() % 3 != 0) return out;

  out.reserve(in.size());

  for(std::vector<VERTEX_FORMAT>::const_iterator iter = in.begin();
      iter < (in.end()-2);
      iter += 3) {

    const VERTEX_FORMAT &a = (*iter);
    const VERTEX_FORMAT &b = (*(iter+1));
    const VERTEX_FORMAT &c = (*(iter+2));

    // assume that either all or none of the tri vertices are to be clipped
    if (!a.m_bClip) {
      out.push_back(a);
      out.push_back(b);
      out.push_back(c);
      continue;
    }

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
      const std::vector<VERTEX_FORMAT>& tris = SplitTriangle(a,b,c,
                                                               normal,D);
      assert(!tris.empty());
      assert(tris.size() <= 6); // vector is actually of points, not tris.

      for(std::vector<VERTEX_FORMAT>::const_iterator tri = tris.begin();
          tri != tris.end();
          ++tri) {
        out.push_back(*tri);
      }
    }
  }
  return out;
}

// Calculates the intersection point of a line segment lb->la which crosses the
// plane with normal 'n'.
bool SBVRGeogen::RayPlaneIntersection(const VERTEX_FORMAT &la,
                         const VERTEX_FORMAT &lb,
                         const FLOATVECTOR3 &n, const float D,
                         VERTEX_FORMAT &hit)
{
  const FLOATVECTOR3 &va = la.m_vPos;
  const FLOATVECTOR3 &vb = lb.m_vPos;
  const float denom = n ^ (va - vb);
  if(EpsilonEqual(denom, 0.0f)) {
    return false;
  }
  const float t = ((n ^ va) + D) / denom;

  hit.m_vPos = va + (t*(vb - va));
  hit.m_vVertexData = la.m_vVertexData + t*(lb.m_vVertexData - la.m_vVertexData);

  return true;
}

bool SBVRGeogen::isInsideAABB(const FLOATVECTOR3& min,
                              const FLOATVECTOR3& max,
                              const FLOATVECTOR3& point) {
  return point.x >= min.x &&  point.x <= max.x &&
         point.y >= min.y &&  point.y <= max.y &&
         point.z >= min.z &&  point.z <= max.z;
}

void SBVRGeogen::AddMesh(const SortIndexPVec& mesh) {

  if (mesh.empty()) return;

  // TODO: currently only triangles are supported
  if (mesh[0]->m_mesh->GetVerticesPerPoly() != 3) return;

  FLOATVECTOR3 min = ( m_vAspect * -0.5f) + m_brickTranslation;
  FLOATVECTOR3 max = ( m_vAspect *  0.5f) + m_brickTranslation;

  for (SortIndexPVec::const_iterator index = mesh.begin();
       index != mesh.end();
       index++) {
    
    FLOATVECTOR3 c = (*index)->m_centroid;
  
    if (isInsideAABB(min, max, c)) m_mesh.push_back(*index);
  }
}

void SBVRGeogen::MeshEntryToVertexFormat(std::vector<VERTEX_FORMAT>& list, 
                                         const RenderMesh* mesh,
                                         size_t startIndex,
                                         bool bClipMesh) {
  bool bHasNormal = mesh->GetNormalIndices().size() == mesh->GetVertexIndices().size();

  VERTEX_FORMAT f;

  // currently we only support triangles, hence the 3
  for (size_t i = 0;i<3;i++) {
    size_t vertexIndex =  mesh->GetVertexIndices()[startIndex+i];
    f.m_vPos =  mesh->GetVertices()[vertexIndex];

    if (mesh->UseDefaultColor()) {
      f.m_vVertexData = mesh->GetDefaultColor().xyz();
      f.m_fOpacity = mesh->GetDefaultColor().w;
    } else {
      f.m_vVertexData = mesh->GetColors()[vertexIndex].xyz();
      f.m_fOpacity = mesh->GetColors()[vertexIndex].w;       
    }

    if (bHasNormal) 
      f.m_vNormal = mesh->GetNormals()[vertexIndex];
    else
      f.m_vNormal = FLOATVECTOR3(2,2,2);

    f.m_bClip = bClipMesh;

    list.push_back(f);
  }
}


void SBVRGeogen::SortMeshWithoutVolume(std::vector<VERTEX_FORMAT>& list) {
  if (m_mesh.size() > 0) {

    std::sort(m_mesh.begin(), m_mesh.end(), DistanceSortUnder);

    for (SortIndexPVec::const_iterator index = m_mesh.begin();
         index != m_mesh.end();
         index++) {
      
      MeshEntryToVertexFormat(list, (*index)->m_mesh, (*index)->m_index, m_bClipMesh);
    }
  }
}
