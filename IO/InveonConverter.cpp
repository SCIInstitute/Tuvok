/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Scientific Computing and Imaging Institute,
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

/**
  \file    InveonConverter.cpp
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#include "../StdTuvokDefines.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include "InveonConverter.h"
#include "Basics/EndianConvert.h"
#include "Basics/SysTools.h"

InveonConverter::InveonConverter()
{
  m_vConverterDesc = L"Inveon";
  m_vSupportedExt.push_back(L"HDR");
}

typedef std::unordered_map<std::string, std::string> LineMap;
// The 'hdr' files we are given consist of a series of lines which begin with a
// keyword, and then a series of space-separated parameters.  This searches for
// lines which begin with the strings in the keys of the map, and fills the
// values with the rest of the lines.
// Ex: a stream consists of:
//    abc 42
//    def 19
//    qwerty 12.5512 123
//    foo bar
// then if the input map contains:
//    "abc":""
//    "qwerty":""
//    "foo":""
// then the output map will contain:
//    "abc":"42"
//    "qwerty":"12.5512 123"
//    "foo":"bar"
static void
findlines(std::ifstream& ifs, LineMap& values)
{
  ifs.seekg(0);
  std::string line;

  while(ifs) {
    if(getline(ifs, line)) {
      // iterate through all our keys and see if the line begins with
      // any of them.
      for(LineMap::iterator l = values.begin(); l != values.end(); ++l) {
        if(line.find(l->first) == 0) {
          l->second = line.substr(l->first.length()+1); // +1: skip space.
        }
      }
    }
  }
}

namespace {
  template <typename T> T convert(const std::string& s) {
    T t;
    std::istringstream conv(s);
    conv >> t;
    return t;
  }
}

bool InveonConverter::ConvertToRAW(const std::wstring& strSourceFilename,
                                   const std::wstring&,
                                   bool /* bNoUserInteraction */,
                                   uint64_t& iHeaderSkip,
                                   unsigned& iComponentSize,
                                   uint64_t& iComponentCount,
                                   bool& bConvertEndianess, bool& bSigned,
                                   bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                                   FLOATVECTOR3& vVolumeAspect,
                                   std::wstring& strTitle,
                                   std::wstring& strIntermediateFile,
                                   bool& bDeleteIntermediateFile)
{
  std::ifstream infn(SysTools::toNarrow(strSourceFilename).c_str(), std::ios::in);
  if(!infn.is_open()) {
    T_ERROR("Could not open %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  }

  iHeaderSkip = 0;
  iComponentCount = 1;
  bDeleteIntermediateFile = false;
  bSigned = true; // format does not distinguish
  strTitle = L"Inveon";

  // The filename is actually stored in the header, but it includes
  // a full pathname and is thus garbage in many instances; e.g. the
  // files I have say:  "F:/somefolder/xyz/blah/whatever/file.ct.img",
  // and I'm on a Unix system, so "F:" doesn't even make sense.
  // Therefore we just ignore the filename in the header and use the
  // "hdr" filename sans the "hdr" extension, which seems to be the
  // convention.
  strIntermediateFile = SysTools::RemoveExt(strSourceFilename);

  LineMap lines;
  lines.insert(LineMap::value_type("version",""));
  lines.insert(LineMap::value_type("number_of_dimensions",""));
  lines.insert(LineMap::value_type("x_dimension",""));
  lines.insert(LineMap::value_type("y_dimension",""));
  lines.insert(LineMap::value_type("z_dimension",""));
  lines.insert(LineMap::value_type("pixel_size_x",""));
  lines.insert(LineMap::value_type("pixel_size_y",""));
  lines.insert(LineMap::value_type("pixel_size_z",""));
  lines.insert(LineMap::value_type("data_type",""));

  findlines(infn, lines);

  for(LineMap::const_iterator l = lines.begin(); l != lines.end(); ++l) {
    MESSAGE("read %s -> '%s'", l->first.c_str(), l->second.c_str());
    if(l->first == "version" && l->second != "001.910") {
      WARNING("Unknown version.  Attempting to continue, but I might "
              "be interpreting this file incorrectly.");
    } else if(l->first == "number_of_dimensions" && l->second != "3") {
      WARNING("%s dimensions instead of 3; continuing anyway...",
              l->second.c_str());
    } else if(l->first == "x_dimension") {
      vVolumeSize[0] = convert<uint64_t>(l->second);
    } else if(l->first == "y_dimension") {
      vVolumeSize[1] = convert<uint64_t>(l->second);
    } else if(l->first == "z_dimension") {
      vVolumeSize[2] = convert<uint64_t>(l->second);
    } else if(l->first == "pixel_size_x") {
      vVolumeAspect[0] = convert<float>(l->second);
    } else if(l->first == "pixel_size_y") {
      vVolumeAspect[1] = convert<float>(l->second);
    } else if(l->first == "pixel_size_z") {
      vVolumeAspect[2] = convert<float>(l->second);
    } else if(l->first == "data_type") {
      size_t type = convert<size_t>(l->second);
      switch(type) {
        case 1: // byte
          iComponentSize = 8;
          bConvertEndianess = false;
          bIsFloat = false;
          break;
        case 2: // 2-byte integer, intel style
          iComponentSize = 16;
          bConvertEndianess = EndianConvert::IsBigEndian();
          bIsFloat = false;
          break;
        case 3: // 4-byte integer, intel style
          iComponentSize = 32;
          bConvertEndianess = EndianConvert::IsBigEndian();
          bIsFloat = false;
          break;
        case 4: // 4-byte float, intel style
          iComponentSize = 32;
          bConvertEndianess = EndianConvert::IsBigEndian();
          bIsFloat = true;
          break;
        case 5: // 4-byte float, sun style
          iComponentSize = 32;
          bConvertEndianess = !EndianConvert::IsBigEndian();
          bIsFloat = true;
          break;
        case 6: // 2-byte integer, sun style
          iComponentSize = 16;
          bConvertEndianess = !EndianConvert::IsBigEndian();
          bIsFloat = false;
          break;
        case 7: // 4-byte integer, sun style
          iComponentSize = 32;
          bConvertEndianess = !EndianConvert::IsBigEndian();
          bIsFloat = false;
          break;
        default:
          T_ERROR("Unknown data type %u", static_cast<unsigned>(type));
          return false;
          break;
      }
    }
  }

  return true;
}

bool InveonConverter::ConvertToNative(
  const std::wstring& strRawFilename,
  const std::wstring& strTargetFilename,
  uint64_t iHeaderSkip,
  unsigned iComponentSize, uint64_t iComponentCount,
  bool bSigned, bool bIsFloat,
  UINT64VECTOR3 vVolumeSize, FLOATVECTOR3 vVolumeAspect,
  bool bNoUserInteraction,
  const bool bQuantizeTo8Bit)
{
  std::ofstream hdr(SysTools::toNarrow(strTargetFilename).c_str(), std::ios::out);
  if(!hdr.is_open()) {
    T_ERROR("Unable to open target file %s", SysTools::toNarrow(strTargetFilename).c_str());
    return false;
  }

  hdr << "#\n" // we use this to check if it's an Inveon file
      << "version 001.910\n"
      << "number_of_dimensions 3\n"
      << "x_dimension " << vVolumeSize[0] << "\n"
      << "y_dimension " << vVolumeSize[1] << "\n"
      << "z_dimension " << vVolumeSize[2] << "\n"
      << "pixel_size_x " << vVolumeAspect[0] << "\n"
      << "pixel_size_y " << vVolumeAspect[1] << "\n"
      << "pixel_size_z " << vVolumeAspect[2] << "\n"
      << "data_type ";
  if(iComponentSize == 8) {
    hdr << "1\n";
  } else if(iComponentSize == 16 && EndianConvert::IsBigEndian()) {
    hdr << "2\n";
  } else if(iComponentSize == 32 && EndianConvert::IsBigEndian() && !bIsFloat) {
    hdr << "3\n";
  } else if(iComponentSize == 32 && EndianConvert::IsBigEndian() && bIsFloat) {
    hdr << "4\n";
  } else if(iComponentSize == 32 && !EndianConvert::IsBigEndian() && bIsFloat) {
    hdr << "5\n";
  } else if(iComponentSize == 16 && !EndianConvert::IsBigEndian() && !bIsFloat) {
    hdr << "6\n";
  } else if(iComponentSize == 32 && !EndianConvert::IsBigEndian() && !bIsFloat) {
    hdr << "7\n";
  } else {
    T_ERROR("Unknown data type!\n");
    hdr << "0\n";
  }

  std::wstring data_file = SysTools::RemoveExt(strTargetFilename.c_str());
  if(!RAWConverter::ConvertToNative(
      strRawFilename, data_file, iHeaderSkip,  iComponentSize, iComponentCount,
      bSigned, bIsFloat, vVolumeSize, vVolumeAspect, bNoUserInteraction,
      bQuantizeTo8Bit)) {
    T_ERROR("Error creating raw file '%s'", SysTools::toNarrow(data_file).c_str());
    SysTools::RemoveFile(data_file);
    return false;
  }

  return true;
}

// checks for comment lines, ascii.
bool InveonConverter::CanRead(const std::wstring&,
                              const std::vector<int8_t>& start) const
{
  using namespace std::placeholders;

  std::string as_string(
    static_cast<const signed char*>(&*start.begin()),
    static_cast<const signed char*>(&*(start.end()-1))
  );
  if(as_string.find("Header") == std::string::npos) {
    MESSAGE("No 'Header' in our header... not mine.");
    return false;
  }

  // Are there any non-ascii characters?
  std::vector<int8_t>::const_iterator notascii = std::find_if(
    start.begin(), start.end(),
    std::bind(
      std::not_equal_to<int>(),
        std::bind(isascii, _1),
        1
      )
  );

  // first char is nothing/comment, and we couldn't find a character
  // which wasn't ascii.
  return
    (std::isspace(static_cast<int>(static_cast<unsigned char>(start[0]))) ||
     static_cast<unsigned char>(start[0]) == '#') &&
    notascii == start.end();
}
