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

//!    File   : PLYGeoConverter.h
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef PLYGEOCONVERTER_H
#define PLYGEOCONVERTER_H

#include "../StdTuvokDefines.h"
#include "AbstrGeoConverter.h"

#ifdef _MSC_VER
# include <tuple>
#else
# include <tr1/tuple>
#endif

namespace tuvok {

  class PLYGeoConverter : public AbstrGeoConverter {
  public:
    PLYGeoConverter();
    virtual ~PLYGeoConverter() {}
    virtual Mesh* ConvertToMesh(const std::string& strFilename);
    virtual bool ConvertToNative(const Mesh& m,
                                 const std::string& strTargetFilename);

    virtual bool CanExportData() const { return true; }

  private:
    enum
    {
      SEARCHING_MAGIC = 0,
      PARSING_GENERAL_HEADER,
      PARSING_VERTEX_HEADER,
      PARSING_FACE_HEADER,
      PARSING_EDGE_HEADER,
      PARSING_VERTEX_DATA,
      PARSING_FACE_DATA,
      PARSING_EDGE_DATA,
      PARSING_DONE,
      STATE_COUNT
    };

    enum 
    {
      FORMAT_ASCII = 0,
      FORMAT_BIN_LITTLE,
      FORMAT_BIN_BIG,
      FORMAT_COUNT
    };

    enum propType
    {
      PROPT_FLOAT = 0,
      PROPT_DOUBLE,
      PROPT_INT8,
      PROPT_UINT8,
      PROPT_INT16,
      PROPT_UINT16,
      PROPT_INT32,
      PROPT_UINT32,
      PROPT_UNKNOWN
    };

    enum vertexProp
    {
      VPROP_X = 0,
      VPROP_Y,
      VPROP_Z,
      VPROP_RED,
      VPROP_GREEN,
      VPROP_BLUE,
      VPROP_OPACITY,
      VPROP_INTENSITY,
      VPROP_CONFIDENCE,
      VPROP_UNKNOWN
    };

    enum faceProp
    {
      FPROP_LIST = 0,
      FPROP_UNKNOWN
    };

    enum edgeProp
    {
      EPROP_VERTEX1 = 0,
      EPROP_VERTEX2,
      EPROP_RED,
      EPROP_GREEN,
      EPROP_BLUE,
      EPROP_OPACITY,
      EPROP_INTENSITY,
      EPROP_UNKNOWN
    };

    std::vector< std::pair<propType, vertexProp> > vertexProps;
    std::vector< std::tr1::tuple<propType, propType, faceProp> > faceProps;
    std::vector< std::pair<propType, edgeProp> > edgeProps;

    propType StringToType(const std::string& token);
    vertexProp StringToVProp(const std::string& token);
    faceProp StringToFProp(const std::string& token);
    edgeProp StringToEProp(const std::string& token);

  };
}
#endif // PLYGEOCONVERTER_H
