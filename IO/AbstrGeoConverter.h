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

//!    File   : AbstrGeoConverter.h
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef TUVOK_ABSTRGEOCONVERTER_H
#define TUVOK_ABSTRGEOCONVERTER_H

#include "../StdTuvokDefines.h"
#include <string>
#include <memory>
#include <vector>
#include "Basics/Vectors.h"

namespace tuvok {

typedef std::vector<FLOATVECTOR3> VertVec;
typedef std::vector<uint32_t> IndexVec;
class Mesh;

class AbstrGeoConverter {
public:
  virtual ~AbstrGeoConverter() {}

  virtual std::shared_ptr<Mesh> ConvertToMesh(
    const std::wstring& strRawFilename
  );

  virtual bool ConvertToNative(const Mesh& m,
                               const std::wstring& strTargetFilename);

  /// @param filename the file in question
  /// @return SupportedExtension() for the file's extension
  virtual bool CanRead(const std::wstring& fn) const;

  const std::vector<std::wstring>& SupportedExt() const {
    return m_vSupportedExt;
  }
  const std::wstring& GetDesc() const { return m_vConverterDesc; }

  virtual bool CanExportData() const { return false; }
  virtual bool CanImportData() const { return false; }

protected:
  /// @param ext the extension for the filename
  /// @return true if the filename is a supported extension for this converter
  bool SupportedExtension(const std::wstring& ext) const;

  void SortByGradient(const VertVec& vertices,
                      IndexVec& v, IndexVec& n,
                      IndexVec& t, IndexVec& c);

  std::wstring               m_vConverterDesc;
  std::vector<std::wstring>  m_vSupportedExt;

  void AddToMesh(const VertVec& vertices,
                 IndexVec& v, IndexVec& n, 
                 IndexVec& t, IndexVec& c, 
                 IndexVec&  VertIndices, IndexVec& NormalIndices, 
                 IndexVec&  TCIndices, IndexVec& COLIndices);

  // parser helper
  std::string TrimToken(const std::string& Src, 
                        const std::string& delim = " \r\n\t",
                        bool bOnlyFirst = false);
  std::string GetToken(std::string& Src, 
                       const std::string& delim = " \r\n\t",
                       bool bOnlyFirst = false);

};
}
#endif // TUVOK_ABSTRGEOCONVERTER_H
