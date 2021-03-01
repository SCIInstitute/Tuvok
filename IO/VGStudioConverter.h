/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


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

//!    File   : VGStudioConverter.h
//!    Author : Jens Krueger
//!             DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : June 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef VGSTUDIOCONVERTER_H
#define VGSTUDIOCONVERTER_H

#include "../StdTuvokDefines.h"
#include "RAWConverter.h"

class VGStudioConverter : public RAWConverter {
public:
  VGStudioConverter();
  virtual ~VGStudioConverter() {}

  virtual bool ConvertToRAW(const std::wstring& strSourceFilename,
                            const std::wstring& strTempDir,
                            bool bNoUserInteraction,
                            uint64_t& iHeaderSkip, unsigned& iComponentSize,
                            uint64_t& iComponentCount,
                            bool& bConvertEndianess, bool& bSigned,
                            bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                            FLOATVECTOR3& vVolumeAspect, std::wstring& strTitle,
                            std::wstring& strIntermediateFile,
                            bool& bDeleteIntermediateFile);

  virtual bool CanExportData() const { return false; }
  virtual bool CanImportData() const { return true; }
};

#endif // VGSTUDIOCONVERTER_H
