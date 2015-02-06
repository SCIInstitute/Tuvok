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

//!    File   : MobileGeoConverter.cpp
//!    Author : Georg Tamm
//!             DFKI, Saarbruecken
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI Institute

#include "MobileGeoConverter.h"
#include "Controller/Controller.h"
#include "SysTools.h"
#include "Mesh.h"
#include "TuvokIOError.h"

using namespace tuvok;
using namespace std;

MobileGeoConverter::MobileGeoConverter() :
  AbstrGeoConverter()
{
  m_vConverterDesc = "Mobile Geometry File";
  m_vSupportedExt.push_back("G3D");
  m_vSupportedExt.push_back("G3DX");
}

bool MobileGeoConverter::ConvertToNative(const Mesh& m,
                                         const std::string& strTargetFilename)
{
  float * colors = nullptr;
  // Shallow conversion that directly references the mesh's data.
  std::shared_ptr<const G3D::GeometrySoA> g3d = ConvertToNative(m, colors);
  if (g3d == nullptr)
    return false;

  G3D::write(strTargetFilename, g3d.get());

  // Clean up if we created a color array and did not reference the source mesh's color data.
  if (colors != nullptr) {
    delete[] colors;
    colors = nullptr;
  }
  return true;
}

std::shared_ptr<const G3D::GeometrySoA>
MobileGeoConverter::ConvertToNative(const Mesh& m, float*& colors)
{
  return ConvertToNative(m, colors, false);
}

std::shared_ptr<G3D::GeometrySoA>
MobileGeoConverter::ConvertToNative(const Mesh& m, float*& colors, bool bCopy)
{
  if (m.GetVertices().empty())
    return nullptr;
  if (colors != nullptr)
    return nullptr; // expecting a fresh pointer
  /*
  // include this once we have mesh downsampling implemented
  bool bSimplify = SysTools::ToUpperCase(
  SysTools::GetExt(strTargetFilename)
  ) == "G3D";
  */
  std::shared_ptr<G3D::GeometrySoA> geometry = std::make_shared<G3D::GeometrySoA>();
  geometry->info.isOpaque = false;
  geometry->info.numberPrimitives =
    uint32_t(m.GetVertexIndices().size() / m.GetVerticesPerPoly());

  if (geometry->info.numberPrimitives == 0) {
    T_ERROR("No primitives to export.");
    return nullptr; // nothing to export
  }

  geometry->info.primitiveType = (m.GetMeshType() == Mesh::MT_TRIANGLES)
    ? G3D::Triangle : G3D::Line;
  geometry->info.numberIndices = uint32_t(m.GetVertexIndices().size());
  geometry->info.numberVertices = uint32_t(m.GetVertices().size());
  uint32_t vertexFloats = 0;
  {
    geometry->info.attributeSemantics.push_back(G3D::Position);
    float * vertices = nullptr;
    if (bCopy) {
      size_t floats = geometry->info.numberVertices * G3D::floats(G3D::Position);
      vertices = new float[floats];
      const float * const src = (const float * const)&m.GetVertices().at(0);
      std::copy(src, src + floats, vertices);
    } else {
      vertices = (float*)&m.GetVertices().at(0);
    }
    geometry->vertexAttributes.push_back(vertices);
    vertexFloats += G3D::floats(G3D::Position);
  }
  if (m.GetNormals().size() == m.GetVertices().size())
  {
    geometry->info.attributeSemantics.push_back(G3D::Normal);
    float * normals = nullptr;
    if (bCopy) {
      size_t floats = geometry->info.numberVertices * G3D::floats(G3D::Normal);
      normals = new float[floats];
      const float * const src = (const float * const)&m.GetNormals().at(0);
      std::copy(src, src + floats, normals);
    } else {
      normals = (float*)&m.GetNormals().at(0);
    }
    geometry->vertexAttributes.push_back(normals);
    vertexFloats += G3D::floats(G3D::Normal);
  }
  if (m.GetTexCoords().size() == m.GetVertices().size())
  {
    geometry->info.attributeSemantics.push_back(G3D::Tex);
    float * texCoords = nullptr;
    if (bCopy) {
      size_t floats = geometry->info.numberVertices * G3D::floats(G3D::Tex);
      texCoords = new float[floats];
      const float * const src = (const float * const)&m.GetTexCoords().at(0);
      std::copy(src, src + floats, texCoords);
    } else {
      texCoords = (float*)&m.GetTexCoords().at(0);
    }
    geometry->vertexAttributes.push_back(texCoords);
    vertexFloats += G3D::floats(G3D::Tex);
  }
  if (m.GetColors().size() == m.GetVertices().size())
  {
    geometry->info.attributeSemantics.push_back(G3D::Color);
    float * colors = nullptr; // Shadow color pointer and do not touch the methods argument.
    if (bCopy) {
      size_t floats = geometry->info.numberVertices * G3D::floats(G3D::Color);
      colors = new float[floats];
      const float * const src = (const float * const)&m.GetColors().at(0);
      std::copy(src, src + floats, colors);
    } else {
      colors = (float*)&m.GetColors().at(0);
    }
    geometry->vertexAttributes.push_back(colors);
    vertexFloats += G3D::floats(G3D::Color);
  }
  else if (m.GetColors().empty())
  {
    // Create colors from default color with given color array to allow the caller to control proper deletion.
    geometry->info.attributeSemantics.push_back(G3D::Color);
    size_t floats = geometry->info.numberVertices * G3D::floats(G3D::Color);
    colors = new float[floats];
    FLOATVECTOR4 * colors4 = (FLOATVECTOR4*)colors;
    std::generate(colors4, colors4 + geometry->info.numberVertices, std::bind(&Mesh::GetDefaultColor, m));
    geometry->vertexAttributes.push_back(colors);
    vertexFloats += G3D::floats(G3D::Color);
  }
  geometry->info.indexSize = sizeof(uint32_t);
  geometry->info.vertexSize = vertexFloats * sizeof(float);
  {
    uint32_t * indices = nullptr;
    if (bCopy) {
      indices = new uint32_t[geometry->info.numberIndices];
      const uint32_t * const src = (const uint32_t * const)&m.GetVertexIndices().at(0);
      std::copy(src, src + geometry->info.numberIndices, indices);
    } else {
      indices = (uint32_t*)&m.GetVertexIndices().at(0);
    }
    geometry->indices = indices;
  }
  return geometry;
}

std::shared_ptr<Mesh>
MobileGeoConverter::ConvertToMesh(const std::string& strFilename) {
  VertVec       vertices;
  NormVec       normals;
  TexCoordVec   texcoords;
  ColorVec      colors;

  IndexVec      VertIndices;
  IndexVec      NormalIndices;
  IndexVec      TCIndices;
  IndexVec      COLIndices; 

  G3D::GeometrySoA geometry;
  G3D::read(strFilename, &geometry);
  if (geometry.info.indexSize == sizeof(uint16_t))
  {
	  for (uint32_t i=0; i<geometry.info.numberIndices; ++i) VertIndices.push_back((uint32_t)((uint16_t*)geometry.indices)[i]);
  }
  else VertIndices = IndexVec(geometry.indices, (uint32_t*)geometry.indices + geometry.info.numberIndices);

  uint32_t i = 0;
  for (std::vector<uint32_t>::iterator it=geometry.info.attributeSemantics.begin(); it<geometry.info.attributeSemantics.end(); ++it)
  {
	  if (*it == G3D::Position) vertices = VertVec((FLOATVECTOR3*)geometry.vertexAttributes.at(i), (FLOATVECTOR3*)geometry.vertexAttributes.at(i) + geometry.info.numberVertices);
	  else if (*it == G3D::Normal) normals = NormVec((FLOATVECTOR3*)geometry.vertexAttributes.at(i), (FLOATVECTOR3*)geometry.vertexAttributes.at(i) + geometry.info.numberVertices);
	  else if (*it == G3D::Color) colors = ColorVec((FLOATVECTOR4*)geometry.vertexAttributes.at(i), (FLOATVECTOR4*)geometry.vertexAttributes.at(i) + geometry.info.numberVertices);
	  else if (*it == G3D::Tex) texcoords = TexCoordVec((FLOATVECTOR2*)geometry.vertexAttributes.at(i), (FLOATVECTOR2*)geometry.vertexAttributes.at(i) + geometry.info.numberVertices);
	  ++i;
  }

  if (vertices.size() == normals.size()) NormalIndices = VertIndices;
  if (vertices.size() == colors.size()) COLIndices = VertIndices;
  if (vertices.size() == texcoords.size()) TCIndices = VertIndices;

  bool success = false;
  Mesh::EMeshType mtype = Mesh::MT_TRIANGLES;
  switch (geometry.info.primitiveType) {
  case G3D::Point:
    T_ERROR("Unsupported primitive type.");
    break;
  case G3D::Line:
    mtype = Mesh::MT_LINES;
    success = true;
    break;
  case G3D::Triangle:
    mtype = Mesh::MT_TRIANGLES;
    success = true;
    break;
  default:
    T_ERROR("Unknown primitive type.");
    break;
  }

  G3D::clean(&geometry);

  if (!success)
    return nullptr;

  std::string desc = m_vConverterDesc + " data converted from " + SysTools::GetFilename(strFilename);
  return std::shared_ptr<Mesh>(
    new Mesh(vertices,normals,texcoords,colors,
             VertIndices,NormalIndices,TCIndices,COLIndices,
             false, false, desc, mtype)
  );
}
