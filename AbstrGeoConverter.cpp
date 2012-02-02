/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Interactive Visualization and Data Analysis Group.

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

//!    File   : AbstrGeoConverter.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include <algorithm>
#include <cctype>
#include <vector>
#include "AbstrGeoConverter.h"
#include "SysTools.h"

using namespace tuvok;

std::tr1::shared_ptr<Mesh>
AbstrGeoConverter::ConvertToMesh(const std::string&) {
  return std::tr1::shared_ptr<Mesh>();
}

bool AbstrGeoConverter::ConvertToNative(const Mesh&,
                                        const std::string&) {
  return false;
}

bool AbstrGeoConverter::CanRead(const std::string& fn) const
{
  return SupportedExtension(SysTools::ToUpperCase(SysTools::GetExt(fn)));
}

/// @param ext the extension for the filename
/// @return true if the filename is a supported extension for this converter
bool AbstrGeoConverter::SupportedExtension(const std::string& ext) const
{
  return std::find(m_vSupportedExt.begin(), m_vSupportedExt.end(), ext) !=
          m_vSupportedExt.end();
}

// *******************************************
// code to triangulate planar, convex polygons
// *******************************************

// swaps two index entries
static void Swap(IndexVec& v, IndexVec& n, IndexVec& t, IndexVec& c, 
          size_t sI, size_t tI) {
  std::swap(v[sI],v[tI]);
  if (v.size() == n.size()) std::swap(n[sI],n[tI]);
  if (v.size() == t.size()) std::swap(t[sI],t[tI]);
  if (v.size() == c.size()) std::swap(c[sI],c[tI]);
}

// Checks the ordering of two points relative to a third.
static bool CheckOrdering(const FLOATVECTOR3& a,
                          const FLOATVECTOR3& b,
                          const FLOATVECTOR3& c,
                          size_t iPlaneX, size_t iPlaneY) {
  float g1 = (a[iPlaneY]-c[iPlaneY])/(a[iPlaneX]-c[iPlaneX]),
        g2 = (b[iPlaneY]-c[iPlaneY])/(b[iPlaneX]-c[iPlaneX]);

  if (EpsilonEqual(a[iPlaneX],c[iPlaneX]))
    return (g2 < 0) || (EpsilonEqual(g2,0.0f) && b[iPlaneX] < c[iPlaneX]);
  if (EpsilonEqual(b[iPlaneX],c[iPlaneX]))
    return (g1 > 0) || (EpsilonEqual(g1,0.0f) && a[iPlaneX] > c[iPlaneX]);


  if (a[iPlaneX] < c[iPlaneX])
    if (b[iPlaneX] < c[iPlaneX]) return g1 < g2; else return false;
  else
    if (b[iPlaneX] < c[iPlaneX]) return true; else return g1 < g2;
}

static void SortPoints(const VertVec& vertices,
                       IndexVec& v, IndexVec& n,
                       IndexVec& t, IndexVec& c,
                       size_t iPlaneX, size_t iPlaneY) {
  // for small arrays, this bubble sort actually beats qsort.
  for (size_t i= 1;i<v.size();++i) {
    bool bDidSwap = false;
    for (size_t j = 1;j<v.size()-i;++j)
      if (!CheckOrdering(vertices[v[j]],vertices[v[j+1]],vertices[v[0]],iPlaneX,iPlaneY)) {
        Swap(v,n,t,c,j,j+1);
        bDidSwap = true;
      }
    if (!bDidSwap) return;
  }

}

void AbstrGeoConverter::SortByGradient(const VertVec& vertices,
                                       IndexVec& v, IndexVec& n,
                                       IndexVec& t, IndexVec& c)
{
  if(v.size() < 2) return;

  // find AA projection direction that is not coplanar to the polygon
  size_t iPlaneX = 2;
  size_t iPlaneY = 1;

  FLOATVECTOR3 tan = (vertices[v[0]]-vertices[v[1]]).normalized();
  FLOATVECTOR3 bin = (vertices[v[0]]-vertices[v[2]]).normalized();
  FLOATVECTOR3 norm = tan%bin;

  if (norm.y != 0) {
    iPlaneX = 0;
    iPlaneY = 2;
  } else
  if (norm.z != 0) {
    iPlaneX = 0;
    iPlaneY = 1;
  } // else use default which is the x-plane

  // move bottom element to front of array
  for (size_t i = 1;i<v.size();i++) {
    if (vertices[v[0]][iPlaneY] > vertices[v[i]][iPlaneY]) {
      Swap(v,n,t,c,0,i);
    }
  }

  // *** sort points according to gradient

  SortPoints(vertices,v,n,t,c,iPlaneX,iPlaneY);
}

void AbstrGeoConverter::AddToMesh(const VertVec& vertices,
                                IndexVec& v, IndexVec& n,
                                IndexVec& t, IndexVec& c,
                                IndexVec& VertIndices, IndexVec& NormalIndices,
                                IndexVec& TCIndices, IndexVec& COLIndices) {
  if (v.size() > 3) {
    // per OBJ definition any poly with more than 3 verices has
    // to be planar and convex, so we can savely triangulate it

    SortByGradient(vertices,v,n,t,c);

    for (size_t i = 0;i<v.size()-2;i++) {
      IndexVec mv, mn, mt, mc;
      mv.push_back(v[0]);mv.push_back(v[i+1]);mv.push_back(v[i+2]);
      if (n.size() == v.size()) {mn.push_back(n[i]);mn.push_back(n[i+1]);mn.push_back(n[i+2]);}
      if (t.size() == v.size()) {mt.push_back(t[i]);mt.push_back(t[i+1]);mt.push_back(t[i+2]);}
      if (c.size() == v.size()) {mc.push_back(c[i]);mc.push_back(c[i+1]);mc.push_back(c[i+2]);}

      AddToMesh(vertices,
                mv,mn,mt,mc,
                VertIndices,
                NormalIndices,
                TCIndices,
                COLIndices);
    }

  } else {
    for (size_t i = 0;i<v.size();i++) {
      VertIndices.push_back(v[i]);
      if (n.size() == v.size()) NormalIndices.push_back(n[i]);
      if (t.size() == v.size()) TCIndices.push_back(t[i]);
      if (c.size() == v.size()) COLIndices.push_back(c[i]);
    }
  }
}

// *******************************************
// code to parse text files
// *******************************************

std::string AbstrGeoConverter::TrimToken(const std::string& Src,
                                         const std::string& delim,
                                         bool bOnlyFirst)
{
  size_t off = Src.find_first_of(delim);
  if (off == std::string::npos) return std::string();
  if (bOnlyFirst) {
    return Src.substr(off+1);
  } else {
    size_t p1 = Src.find_first_not_of(delim,off);
    if (p1 == std::string::npos) return std::string();
    return Src.substr(p1);
  }
}

std::string AbstrGeoConverter::GetToken(std::string& Src,
                                        const std::string& delim,
                                        bool bOnlyFirst) {
    size_t off = Src.find_first_of(delim);
    if (off == std::string::npos) {
      std::string result = Src;
      Src = "";
      return result;
    }
    std::string result = SysTools::ToLowerCase(SysTools::TrimStrRight(Src.substr(0,off)));
    Src = TrimToken(Src,delim,bOnlyFirst);
    return result;
}
