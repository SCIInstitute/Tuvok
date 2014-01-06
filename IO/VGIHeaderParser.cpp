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

//!    File   : VGIHeaderParser.cpp
//!    Author : Jens Krueger
//!             DFKI & MMCI, Saarbruecken
//!             SCI Institute, University of Utah
//!    Date   : June 2010
//
//!    Copyright (C) 2010 DFKI, MMCI, SCI Institute

#include "VGIHeaderParser.h"

#include <algorithm>
#include <functional>
#include <fstream>
#include "Basics/SysTools.h"

using namespace std;
using namespace SysTools;

VGIHeaderParser::VGIHeaderParser(const string& strFilename)
{
  m_bFileReadable = ParseFile(strFilename);
}

VGIHeaderParser::VGIHeaderParser(const wstring& wstrFilename)
{
  string strFilename(wstrFilename.begin(), wstrFilename.end());
  m_bFileReadable = ParseFile(strFilename);
}


VGIHeaderParser::VGIHeaderParser(ifstream& fileData)
{
  m_bFileReadable = ParseFile(fileData);
}


VGIHeaderParser::~VGIHeaderParser()
{
}

bool VGIHeaderParser::ParseFile(const std::string& strFilename) {
  ifstream fileData(strFilename.c_str(),ios::binary);
  bool result = ParseFile(fileData);
  fileData.close();
  return result;
}

bool VGIHeaderParser::ParseFile(ifstream& fileData) {
  string line;

  m_iStopPos = 0;
  if (fileData.is_open())
  {
    WaitForSection(fileData,"{volume1}");
    WaitForSection(fileData,"[file1]");
    ParseUntilInvalid(fileData);
  } else return false;

  return true;
}


void VGIHeaderParser::WaitForSection(ifstream& fileData, const string& match) {
    string line;

    while (! fileData.eof()  )
    {
      getline (fileData,line);
      if (TrimStr(line) == match)  return;
    }

}


void VGIHeaderParser::ParseUntilInvalid(ifstream& fileData) {
  string line;

  m_iStopPos = 0;
  if (fileData.is_open())
  {
    bool bContinue = true;
    while (! fileData.eof() && bContinue )
    {
      getline (fileData,line);
      bContinue = ParseKeyValueLine(line, false, true, "=", "");

      if (!bContinue) m_iStopPos = static_cast<size_t>(fileData.tellg());
    }
  }
}
