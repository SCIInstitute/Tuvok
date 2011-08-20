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

//!    File   : XML3DGeoConverter.h
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef XML3DGEOCONVERTER_H
#define XML3DGEOCONVERTER_H

#include "../StdTuvokDefines.h"
#include "AbstrGeoConverter.h"

namespace tuvok {

  class XML3DGeoConverter : public AbstrGeoConverter {
  public:
    XML3DGeoConverter();
    virtual ~XML3DGeoConverter() {}
    virtual bool ConvertToNative(const Mesh& m,
                                 const std::string& strTargetFilename);

    virtual bool CanExportData() const { return true; }

  protected:
    void ConvertToNative(const Mesh& m, size_t iMeshIndex, std::ofstream& outStream) const;

    void WriteHeader(std::ofstream& outStream);
    void WriteFooter(std::ofstream& outStream, size_t iMeshCount);

  };
}
#endif // XML3DGEOCONVERTER_H
