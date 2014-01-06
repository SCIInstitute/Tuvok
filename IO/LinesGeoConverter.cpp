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
#include "StdTuvokDefines.h"
#include <fstream>
#include <memory>
#include "Basics/Mesh.h"
#include "Controller/Controller.h"
#include "LinesGeoConverter.h"
#include "TuvokIOError.h"

namespace tuvok {

LinesGeoConverter::LinesGeoConverter() {
  m_vSupportedExt.push_back("IV3DLINES");
  m_vSupportedExt.push_back("LNE");
  m_vConverterDesc = "IV3D Hacky Lines";
}

using namespace io;
std::shared_ptr<Mesh>
LinesGeoConverter::ConvertToMesh(const std::string& rawFilename) {
  MESSAGE("Converting %s...", rawFilename.c_str());
  std::ifstream lines(rawFilename.c_str(), std::ios::in);

  if(!lines) {
    throw DSOpenFailed(rawFilename.c_str(), "open failed", __FILE__, __LINE__);
  }

  uint64_t n_vertices;
  lines >> n_vertices;
  if(!lines || n_vertices == 0) {
    throw DSParseFailed(rawFilename.c_str(), "number of vertices", __FILE__,
                        __LINE__);
  }
  size_t n_vertices_st = static_cast<size_t>(n_vertices);
  MESSAGE("%llu vertices.", n_vertices);
  VertVec vertices(n_vertices_st);
  for(size_t i=0; i < n_vertices_st; ++i) {
    FLOATVECTOR3 tmp;
    lines >> tmp[0] >> tmp[1] >> tmp[2];
    vertices[i] = tmp;
  }
  if(!lines) {
    throw DSParseFailed(rawFilename.c_str(), "vertices list short", __FILE__,
                        __LINE__);
  }

  uint64_t n_edges;
  lines >> n_edges;
  if(!lines) {
    throw DSParseFailed("number of edges", __FILE__, __LINE__);
  }
  MESSAGE("%llu edges.", n_edges);
  size_t n_edges_st = static_cast<size_t>(n_edges);
  IndexVec edges(static_cast<size_t>(n_edges*2)); // an indexVec holds a single elem, not a pair
  for(size_t i=0; i < n_edges_st; ++i) {
    UINTVECTOR2 e;
    lines >> e[0] >> e[1];
    edges[i+0] = e[0]-1;
    edges[i+1] = e[1]-1;
  }
  if(!lines) {
    throw DSParseFailed("error reading edge list", __FILE__, __LINE__);
  }

  uint64_t n_colors;
  lines >> n_colors;
  if(!lines || n_colors == 0) {
    throw DSParseFailed("number of colors", __FILE__, __LINE__);
  }
  MESSAGE("%llu colors.", n_colors);
  size_t n_colors_st = static_cast<size_t>(n_colors);
  ColorVec colors(n_colors_st);
  FLOATVECTOR3 c;
  for(size_t i=0; i < n_colors; ++i) {
    lines >> c[0] >> c[1] >> c[2];
    colors[i] = c;
  }
  if(!lines) {
    throw std::runtime_error("error reading color array");
  }

  uint64_t n_color_indices;
  lines >> n_color_indices;
  if(!lines || n_color_indices == 0) {
    throw DSParseFailed("number of color indices", __FILE__, __LINE__);
  }
  MESSAGE("%llu color indices", n_color_indices);
  size_t n_color_indices_st = static_cast<size_t>(n_color_indices);
  IndexVec c_indices(n_color_indices_st);
  uint32_t cindex;
  for(size_t i=0; i < n_color_indices_st; ++i) {
    lines >> cindex;
    c_indices[i] = cindex-1;
  }
  if(!lines) {
    throw DSParseFailed("short color index list", __FILE__, __LINE__);
  }
  lines.close();

  // we're good, except the data are in a different coordinate space.
  // oops!  Subtract 0.5 from X and Y to get it into the right place.
  for(VertVec::iterator v = vertices.begin(); v != vertices.end(); ++v) {
    *v = FLOATVECTOR3((*v)[0]-0.5f, (*v)[1]-0.5f, (*v)[2]);
  }

  return std::shared_ptr<Mesh>(new Mesh(
    vertices, NormVec(), TexCoordVec(), colors, edges,
    IndexVec(), IndexVec(), c_indices, false, false,
    "Esra-mesh", Mesh::MT_LINES
  ));
}

}
