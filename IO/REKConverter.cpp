/*
   The MIT License

   Copyright (c) 2009 Institut of Mechanics and Fluid Dynamics,
   TU Bergakademie Freiberg.


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

/**
  \file    REKConverter.cpp
  \author  Andre Liebscher
           Institut of Mechanics and Fluid Dynamics
           TU Bergakademie Freiberg
  \date    March 2009
*/

#include <fstream>
#include <sstream>
#include <cstring>
#include "REKConverter.h"
#include "Controller/Controller.h"
#include "Basics/EndianConvert.h"
#include "Basics/SysTools.h"

using namespace std;

REKConverter::REKConverter()
{
  m_vConverterDesc = L"Fraunhofer Raw Volume";
  m_vSupportedExt.push_back(L"REK");
}

bool
REKConverter::ConvertToRAW(const std::wstring& strSourceFilename,
                           const std::wstring&,
                           bool, uint64_t& iHeaderSkip,
                           unsigned& iComponentSize,
                           uint64_t& iComponentCount,
                           bool& bConvertEndianess, bool& bSigned,
                           bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                           FLOATVECTOR3& vVolumeAspect,
                           std::wstring& strTitle,
                           std::wstring& strIntermediateFile,
                           bool& bDeleteIntermediateFile)
{
  MESSAGE("Attempting to convert REK dataset %s", SysTools::toNarrow(strSourceFilename).c_str());

  // Find out machine endianess
  bConvertEndianess = EndianConvert::IsBigEndian();

  // Read header an check for "magic" values of the REK file
  ifstream fileData(SysTools::toNarrow(strSourceFilename).c_str(), ifstream::in | ifstream::binary);
  char buffer[2048];

  if(fileData.is_open()) {
    fileData.read( buffer, sizeof(buffer) );
    if(Parse<uint16_t, 2>( &buffer[10], bConvertEndianess ) != 2 &&
       Parse<uint16_t, 2>( &buffer[12], bConvertEndianess ) != 4) {
      WARNING("The file %s is not a REK file", SysTools::toNarrow(strSourceFilename).c_str());
      fileData.close();
      return false;
    }
  } else {
    WARNING("Could not open REK file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  }
  fileData.close();

  // Standard values which are always true (I guess)
  strTitle = L"Fraunhofer EZRT";
  vVolumeAspect = FLOATVECTOR3(1,1,1);
  bSigned = false;
  bIsFloat = false;
  iComponentCount = 1;
  strIntermediateFile = strSourceFilename;
  bDeleteIntermediateFile = false;

  // Read file format from header
  vVolumeSize[0] = Parse<uint16_t, 2>(&buffer[0], bConvertEndianess);
  vVolumeSize[1] = Parse<uint16_t, 2>(&buffer[2], bConvertEndianess);
  vVolumeSize[2] = Parse<uint16_t, 2>(&buffer[6], bConvertEndianess);
  iComponentSize = Parse<uint16_t, 2>(&buffer[4], bConvertEndianess);
  iHeaderSkip = Parse<uint16_t, 2>(&buffer[8], bConvertEndianess);

  return true;
}

// unimplemented!
bool
REKConverter::ConvertToNative(const std::wstring&,
                              const std::wstring&,
                              uint64_t, unsigned,
                              uint64_t, bool,
                              bool,
                              UINT64VECTOR3,
                              FLOATVECTOR3,
                              bool,
                              bool)
{
  return false;
}
