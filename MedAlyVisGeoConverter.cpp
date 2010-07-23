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

//!    File   : MedAlyVisGeoConverter.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include "MedAlyVisGeoConverter.h"
#include "Controller/Controller.h"
#include "SysTools.h"
#include "Mesh.h"
#include <fstream>
#include "TuvokIOError.h"

using namespace tuvok;
using namespace std;

MedAlyVisGeoConverter::MedAlyVisGeoConverter()
{
  m_vConverterDesc = "MedAlyVis Hull File";
  m_vSupportedExt.push_back("TRI");
}


Mesh* MedAlyVisGeoConverter::ConvertToMesh(const std::string& strFilename) {
  ifstream trisoup(strFilename.c_str(), ios::binary);
  if(!trisoup) {
    // hack, we really want some kind of 'file not found' exception.
    throw tuvok::io::DSOpenFailed(strFilename.c_str(), __FILE__, __LINE__);
  }

  unsigned n_vertices;
  unsigned n_triangles;
  trisoup.read(reinterpret_cast<char*>(&n_vertices), sizeof(unsigned));
  trisoup.read(reinterpret_cast<char*>(&n_triangles), sizeof(unsigned));

  MESSAGE("%u vertices and %u triangles.", n_vertices, n_triangles);

  assert(n_vertices > 0);
  assert(n_triangles > 0);

  VertVec vertices(n_vertices);

  // read in the world space coords of each vertex
  MESSAGE("reading %u vertices (each 3x floats)...", n_vertices);
  for(unsigned i=0; trisoup && i < n_vertices; ++i) {
    trisoup.read(reinterpret_cast<char*>(&(vertices[i].x)), sizeof(float));
    trisoup.read(reinterpret_cast<char*>(&(vertices[i].y)), sizeof(float));
    trisoup.read(reinterpret_cast<char*>(&(vertices[i].z)), sizeof(float));
  }

  if(!trisoup) {
    throw tuvok::io::DSVerificationFailed("file ends before triangle indices.",
                                          __FILE__, __LINE__);
  }

  IndexVec VertIndices(n_triangles);

  // read in the triangle indices
  MESSAGE("reading %u triangles...", n_triangles);
  for(unsigned i=0; trisoup && i < n_triangles; ++i) {
    trisoup.read(reinterpret_cast<char*>(&(VertIndices[i].x)), sizeof(unsigned));
    trisoup.read(reinterpret_cast<char*>(&(VertIndices[i].y)), sizeof(unsigned));
    trisoup.read(reinterpret_cast<char*>(&(VertIndices[i].z)), sizeof(unsigned));
  }
  trisoup.close();

  std::string desc = m_vConverterDesc + " data converted from " + SysTools::GetFilename(strFilename);

  Mesh* m = new Mesh(vertices,NormVec(),TexCoordVec(),ColorVec(),
                     VertIndices,IndexVec(),IndexVec(),IndexVec(),
                     false,false,desc);
  return m;
}
