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

//!    File   : VGIHeaderParser.h
//!    Author : Jens Krueger
//!             DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : June 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#pragma once

#ifndef VGIHEADERPARSER_H
#define VGIHEADERPARSER_H

#include "KeyValueFileParser.h"

/** \class VGIHeaderParser
 * VGIHeaderParser parses VGStudio header file s(*.vgi)
 * essentailly these are simple text files strucutured as
 * key = value [newline]
 * but with sections marked by
 * {SECTIONNAME}
 * and with thse subsections marked by
 * [SUBSECTIONNAME]
 * for imagevis we are interested in the section
 * {volumeN}
 * where N is number starting at 1 (!!!)
 * and within those sections we are looking for the subsection
 * [fileN]
 */
class VGIHeaderParser : public KeyValueFileParser
{
public:
  VGIHeaderParser(const std::string& strFilename);
  VGIHeaderParser(const std::wstring& wstrFilename);
  VGIHeaderParser(std::ifstream& fileData);

  ~VGIHeaderParser(void);

protected:
  bool ParseFile(const std::string& strFilename);
  bool ParseFile(std::ifstream& fileData);

  void WaitForSection(std::ifstream& fileData, const std::string& match);
  void ParseUntilInvalid(std::ifstream& fileData);
};

#endif // VGIHEADERPARSER_H
