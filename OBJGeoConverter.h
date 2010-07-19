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

//!    File   : OBJGeoConverter.h
//!    Author : Jens Krueger
//!             IVCI & DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : July 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef OBJGEOCONVERTER_H
#define OBJGEOCONVERTER_H

#include "../StdTuvokDefines.h"
#include "AbstrGeoConverter.h"

class Mesh;

class OBJGeoConverter : public AbstrGeoConverter {
public:
  OBJGeoConverter();
  virtual ~OBJGeoConverter() {}
  virtual Mesh* ConvertToMesh(const std::string& strFilename);
protected:
  int CountOccurences(const std::string& str, const std::string& substr);
  std::string TrimToken(const std::string& Src, 
                        const std::string& c = " \r\n\t", 
                        bool bOnlyFirst = false);

};

#endif // OBJGEOCONVERTER_H
