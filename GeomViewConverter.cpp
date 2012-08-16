/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Interactive Visualization and Data Analysis Group.
                      Scientific Computing and Imaging Institute

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
#include "StdTuvokDefines.h"
#include <fstream>
#include <memory>
#include "Basics/Mesh.h"
#include "Controller/Controller.h"
#include "GeomViewConverter.h"
#include "TuvokIOError.h"

namespace tuvok {

GeomViewConverter::GeomViewConverter() {
  m_vSupportedExt.push_back("OFF");
  m_vConverterDesc = "GeomView OFF";
}

using namespace io;
std::shared_ptr<Mesh>
GeomViewConverter::ConvertToMesh(const std::string& rawFilename) {
  MESSAGE("Converting %s...", rawFilename.c_str());
  std::ifstream off(rawFilename.c_str(), std::ios::in);

  if(!off) {
    throw DSOpenFailed(rawFilename.c_str(), "open failed", __FILE__, __LINE__);
  }

  std::string magic;
  off >> magic;
  if(!off || magic != "OFF") {
    throw DSOpenFailed(rawFilename.c_str(), "not an OFF file.", __FILE__,
                       __LINE__);
  }

  uint64_t n_vertices, n_faces;
  off >> n_vertices >> n_faces;
  // always seems to be a 0 after that, not sure what it means yet.
  { uint64_t zero; off >> zero; }

  if(!off || n_vertices == 0) {
    throw DSParseFailed(rawFilename.c_str(), "number of vertices", __FILE__,
                        __LINE__);
  }
  MESSAGE("%llu vertices.", n_vertices);
  MESSAGE("%llu faces.", n_faces);

  size_t n_vertices_st = static_cast<size_t>(n_vertices);
  VertVec vertices(n_vertices_st);
  for(uint64_t i=0; i < n_vertices; ++i) {
    FLOATVECTOR3 tmp;
    off >> tmp[0] >> tmp[1] >> tmp[2];
    vertices[static_cast<size_t>(i)] = tmp;
  }
  if(!off) {
    throw DSParseFailed(rawFilename.c_str(), "vertices list short", __FILE__,
                        __LINE__);
  }

  // every line seems to consist of '3', 3 indices, and then a '7'.  I guess
  // the 3 means "a 3-index face is next", but what the hell does the 7 mean?
  IndexVec VertIndices, NormalIndices, TCIndices, COLIndices;
  {
    uint16_t three, seven;
    for(uint64_t i=0; i < n_faces; ++i) {
      UINTVECTOR3 face;
      off >> three >> face[0] >> face[1] >> face[2] >> seven;
      if(!off) {
        throw DSParseFailed(rawFilename.c_str(), "short face list?", __FILE__,
                            __LINE__);
      }
      if(three != 3) {
        throw DSParseFailed(rawFilename.c_str(), "unknown face type", __FILE__,
                            __LINE__);
      }
      IndexVec v,n,t,c;
      v.push_back(face[0]);
      v.push_back(face[1]);
      v.push_back(face[2]);
      AddToMesh(vertices,v,n,t,c,VertIndices,NormalIndices,TCIndices,COLIndices);
    }
  }
  off.close();

  return std::shared_ptr<Mesh>(new Mesh(
    vertices, NormVec(), TexCoordVec(), ColorVec(),
    VertIndices, NormalIndices, TCIndices, COLIndices,
    false, true, "Geomview", Mesh::MT_TRIANGLES
  ));
}

}
