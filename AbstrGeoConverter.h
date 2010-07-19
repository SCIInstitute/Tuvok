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

#ifndef ABSTRGEOCONVERTER_H
#define ABSTRGEOCONVERTER_H

#include "../StdTuvokDefines.h"
#include <string>
#include <vector>

class Mesh;

class AbstrGeoConverter {
public:
  virtual ~AbstrGeoConverter() {}

  virtual Mesh* ConvertToMesh(const std::string& strRawFilename) = 0;

  virtual bool ConvertToNative(const Mesh& m, 
                               const std::string& strTargetFilename);

  /// @param filename the file in question
  /// @return SupportedExtension() for the file's extension
  virtual bool CanRead(const std::string& fn) const;

  const std::vector<std::string>& SupportedExt() const  { return m_vSupportedExt; }
  const std::string& GetDesc() const { return m_vConverterDesc; }

  virtual bool CanExportData() const { return false; }

protected:
  /// @param ext the extension for the filename
  /// @return true if the filename is a supported extension for this converter
  bool SupportedExtension(const std::string& ext) const;

  std::string               m_vConverterDesc;
  std::vector<std::string>  m_vSupportedExt;

};

#endif // ABSTRGEOCONVERTER_H
