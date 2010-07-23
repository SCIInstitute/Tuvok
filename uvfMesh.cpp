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

//!    File   : uvfMesh.cpp
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include <cassert>
#include <cstring>
#include <stdexcept>
#include "uvfMesh.h"
#include "UVF/GeometryDataBlock.h"

using namespace std;
using namespace tuvok;

uvfMesh::uvfMesh(const GeometryDataBlock& tsb)
{
  m_DefColor = FLOATVECTOR4(tsb.GetDefaultColor());
  m_MeshDesc = tsb.m_Desc;

  switch (tsb.GetPolySize()) {
    case 2 :  m_meshType = MT_LINES; break;
    case 3 :  m_meshType = MT_TRIANGLES; break;
    default : throw std::runtime_error("reading unsupported mesh type");
  }

  vector<float> fVec;
  fVec = tsb.GetVertices();
  assert(fVec.size()%3 == 0);
  if (fVec.size() > 0) { m_vertices.resize(fVec.size()/3);  memcpy(&m_vertices[0],&fVec[0],fVec.size()*sizeof(float));}
  fVec = tsb.GetNormals();
  assert(fVec.size()%3 == 0);
  if (fVec.size() > 0) { m_normals.resize(fVec.size()/3);  memcpy(&m_normals[0],&fVec[0],fVec.size()*sizeof(float));}
  fVec = tsb.GetTexCoords();
  assert(fVec.size()%2 == 0);
  if (fVec.size() > 0) { m_texcoords.resize(fVec.size()/2);  memcpy(&m_texcoords[0],&fVec[0],fVec.size()*sizeof(float));}
  fVec = tsb.GetColors();
  assert(fVec.size()%4 == 0);
  if (fVec.size() > 0) { m_colors.resize(fVec.size()/4);  memcpy(&m_colors[0],&fVec[0],fVec.size()*sizeof(float));}
  fVec.clear();

  m_VertIndices = tsb.GetVertexIndices();
  assert(m_VertIndices.size()%tsb.GetPolySize() == 0);
  m_NormalIndices = tsb.GetNormalIndices();
  assert(m_NormalIndices.size()%tsb.GetPolySize() == 0);
  m_TCIndices = tsb.GetTexCoordIndices();
  assert(m_TCIndices.size()%tsb.GetPolySize() == 0);
  m_COLIndices = tsb.GetColorIndices();
  assert(m_COLIndices.size()%tsb.GetPolySize() == 0);
}

