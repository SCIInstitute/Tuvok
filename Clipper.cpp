#include "Clipper.h"
#include <algorithm>
#include <functional>

// Calculates the intersection point of a line segment lb->la which crosses the
// plane with normal 'n'.
static bool RayPlaneIntersection(const FLOATVECTOR3 &la,
                                 const FLOATVECTOR3 &lb,
                                 const FLOATVECTOR3 &n, const float D,
                                 FLOATVECTOR3 &hit)
{
  const float denom = n ^ (la - lb);

  if(EpsilonEqual(denom, 0.0f)) {
    return false;
  }

  const float t = ((n ^ la) + D) / denom;
  hit = la + (t*(lb - la));
  return true;
}


// Splits a triangle along a plane with the given normal.
// Assumes: plane's D == 0.
//          triangle does span the plane.
void SplitTriangle(FLOATVECTOR3 a,
                   FLOATVECTOR3 b,
                   FLOATVECTOR3 c,
                   float fa, 
                   float fb,
                   float fc,
                   const FLOATVECTOR3 &normal,
                   const float D,
                   std::vector<FLOATVECTOR3>& out,
                   std::vector<FLOATVECTOR3>& newVerts)
{
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
  FLOATVECTOR3 A, B;
  RayPlaneIntersection(a,c, normal,D, A);
  RayPlaneIntersection(b,c, normal,D, B);

  if(fc >= 0) {
    out.push_back(a); out.push_back(b); out.push_back(A);
    out.push_back(b); out.push_back(B); out.push_back(A);
  } else {
    out.push_back(A); out.push_back(B); out.push_back(c);
  }
  newVerts.push_back(A);
  newVerts.push_back(B);
}


std::vector<FLOATVECTOR3> Clipper::TriPlane(std::vector<FLOATVECTOR3>& posData, const FLOATVECTOR3 &normal, const float D) {
  std::vector<FLOATVECTOR3> newVertices;
  std::vector<FLOATVECTOR3> out;
  if (posData.size() % 3 != 0) return newVertices;

  for(auto iter = posData.begin(); iter < (posData.end()-2); iter += 3) {

    const FLOATVECTOR3 &a = (*iter);
    const FLOATVECTOR3 &b = (*(iter+1));
    const FLOATVECTOR3 &c = (*(iter+2));

    float fa = (normal ^ a) + D;
    float fb = (normal ^ b) + D;
    float fc = (normal ^ c) + D;
    if(fabs(fa) < (2 * std::numeric_limits<float>::epsilon())) { fa = 0; }
    if(fabs(fb) < (2 * std::numeric_limits<float>::epsilon())) { fb = 0; }
    if(fabs(fc) < (2 * std::numeric_limits<float>::epsilon())) { fc = 0; }
    if(fa >= 0 && fb >= 0 && fc >= 0) {        // trivial reject
      // discard -- i.e. do nothing / ignore tri.
      continue;
    } else if(fa <= 0 && fb <= 0 && fc <= 0) { // trivial accept
      out.push_back(a);
      out.push_back(b);
      out.push_back(c);
    } else { // triangle spans plane -- must be split.
      std::vector<FLOATVECTOR3> tris, newVerts;
      SplitTriangle(a,b,c, fa,fb,fc,normal,D, tris, newVerts);

      // append triangles and vertices to lists
      out.insert(out.end(), tris.begin(), tris. end());
      newVertices.insert(newVertices.end(), newVerts.begin(), newVerts. end());
    }
  }

  posData = out;
  return newVertices;
}

struct {
  bool operator() (const FLOATVECTOR3& i, const FLOATVECTOR3& j) { 
    return i.x < j.x || (i.x == j.x && i.y < j.y) || (i.x == j.x && i.y == j.y && i.z < j.z);
  }
} CompSorter;

static bool AngleSorter(const FLOATVECTOR3& i, const FLOATVECTOR3& j, const FLOATVECTOR3& center, const FLOATVECTOR3& refVec, const FLOATVECTOR3& normal) { 
  FLOATVECTOR3 vecI = (i-center).normalized();
  float cosI = refVec ^ vecI;
  float sinI = (vecI % refVec)^ normal;

  FLOATVECTOR3 vecJ = (j-center).normalized();
  float cosJ = refVec ^ vecJ;
  float sinJ = (vecJ % refVec) ^ normal;

  float acI = atan2(sinI, cosI);
  float acJ = atan2(sinJ, cosJ);

  return acI > acJ;
}

void Clipper::BoxPlane(std::vector<FLOATVECTOR3>& posData, const FLOATVECTOR3 &normal, const float D) {

  std::vector<FLOATVECTOR3> newVertices = Clipper::TriPlane(posData, normal, D);

  if (newVertices.size() < 3) return;
  
  // remove duplicate vertices
  std::sort(newVertices.begin(), newVertices.end(), CompSorter);
  newVertices.erase(std::unique(newVertices.begin(), newVertices.end()), newVertices.end());

  // sort counter clockwise
  using namespace std::placeholders;

  FLOATVECTOR3 center;
  for (auto vertex = newVertices.begin();vertex<newVertices.end();++vertex) {
    center += *vertex;
  }
  center /= float(newVertices.size());

  std::sort(newVertices.begin(), newVertices.end(), std::bind(AngleSorter, _1, _2, center, (newVertices[0]-center).normalized(), normal ));

  // create a triangle fan with the newly created vertices to close the polytope
  for (auto vertex = newVertices.begin()+2;vertex<newVertices.end();++vertex) {
    posData.push_back(newVertices[0]);
    posData.push_back(*(vertex-1));
    posData.push_back(*(vertex));
  }


}

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.


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
