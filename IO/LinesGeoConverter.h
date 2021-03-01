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
#pragma once

#ifndef TUVOK_LINES_GEO_CONVERTER_H
#define TUVOK_LINES_GEO_CONVERTER_H

#include "AbstrGeoConverter.h"

namespace tuvok {

/// Simple/homegrown format for getting a bunch of line geometry loaded.
/// This is specifically to get a collaborator rolling quickly and isn't really
/// a great format; it is expected to change.
/// The format is:
///    n_vertices
///    vertexX vertexY vertexZ
///    ....
///    n_edges
///    vert_index_A vert_index_B
///    ....
///    n_colors
///    R G B
///    ...
///    n_color_indices
///    color_index_0
///    color_index_1
///    ....
class LinesGeoConverter: public AbstrGeoConverter {
public:
  LinesGeoConverter();
  virtual ~LinesGeoConverter() {}

  virtual std::shared_ptr<Mesh> ConvertToMesh(const std::wstring&);

  virtual bool CanExportData() const { return false; }
  virtual bool CanImportData() const { return true; }
};

}
#endif // TUVOK_LINES_GEO_CONVERTER_H
