#include <algorithm>
#include <fstream>
#include <stdexcept>
#include "VTKConverter.h"
#include "Basics/BStream.h"
#include "Basics/EndianConvert.h"
#include "Controller/Controller.h"

VTKConverter::VTKConverter() {
  m_vConverterDesc = "VTK";
  m_vSupportedExt.push_back("VTK");
}

bool VTKConverter::CanRead(const std::string& fn,
                           const std::vector<int8_t>& start) const {
  if(!AbstrConverter::CanRead(fn, start)) {
    MESSAGE("Base class reports we can't read it, bailing.");
    return false;
  }

  // the file should start with:
  //   # vtk DataFile Version 3.0
  // if it doesn't than we probably can't read it.
  auto nl = std::find(start.begin(), start.end(), '\n');
  if(nl == start.end()) {
    const char* comment = "# vtk DataFile Version 3.0";
    MESSAGE("missing '%s' at beginning of the file; I probably can't read "
            "this.  Bailing out...", comment);
    return false;
  }
  std::string firstline(
    static_cast<const signed char*>(&start[0]),
    static_cast<const signed char*>(&*nl)
  );
  if(firstline.find("vtk") == std::string::npos) {
    MESSAGE("No 'vtk' in first line; this is not mine.");
    return false;
  }
  if(firstline.find("DataFile") == std::string::npos) {
    MESSAGE("No 'DataFile' in first line; this is not mine.");
    return false;
  }
  return true;
}

/// scans through a file until it finds the given line.
/// a subsequent read will return data from the beginning of that line.
std::istream& scan_for_line(std::istream& is, const std::string& start) {
  std::streampos loc;
  std::string temporary;
  do {
    loc = is.tellg();
    std::getline(is, temporary);
    std::istringstream iss(temporary);
    std::string linestart;
    iss >> linestart;
    if(linestart == start) {
      is.seekg(loc);
      return is;
    }
  } while(!is.eof());
  return is;
}

/// vtk gives its types in the files as strings, e.g. "float", etc.
/// we need to get this information into type variables; this converts from
/// VTK's strings into our variables.
/// Note that only the type fields are set as a postcondition; we don't know
/// how many elements there will be, for example.
struct BStreamDescriptor vtk_to_tuvok_type(const std::string& vtktype) {
  struct BStreamDescriptor bs;
  bs.components = 1;
  bs.big_endian = true; // legacy VTK files are *always* big-endian.
  if(vtktype == "float") {
    bs.width = sizeof(float);
    bs.fp = true;
    bs.is_signed = true;
  } else {
    throw std::logic_error("unhandled vtk type case");
  }
  return bs;
}

bool VTKConverter::ConvertToRAW(
    const std::string& strSourceFilename,
    const std::string& /* tempdir */, bool /* user interaction */,
    uint64_t& iHeaderSkip, unsigned& iComponentSize, uint64_t& iComponentCount,
    bool& bConvertEndianness, bool& bSigned, bool& bIsFloat,
    UINT64VECTOR3& vVolumeSize, FLOATVECTOR3& vVolumeAspect,
    std::string& strTitle, std::string& strIntermediateFile,
    bool& bDeleteIntermediateFile
) {
  MESSAGE("Converting %s from VTK...", strSourceFilename.c_str());

  strTitle = "from VTK converter";
  std::ifstream vtk(strSourceFilename.c_str(), std::ios::binary);

  std::string current, junk;
  std::getline(vtk, current); // ignore comment line
  std::getline(vtk, current); // ignore "PsiPhi grid data"
  vtk >> current;
  if(current != "BINARY") {
    T_ERROR("I can only read binary VTK data; this is '%s'", current.c_str());
    return false;
  }
  vtk >> junk >> current;
  if(current != "STRUCTURED_POINTS") {
    T_ERROR("I can only read STRUCTURED_POINTS data; this is '%s'",
            current.c_str());
    return false;
  }
  vVolumeSize = UINT64VECTOR3(0,0,0);
  vtk >> junk >> vVolumeSize[0] >> vVolumeSize[1] >> vVolumeSize[2];
  if(vVolumeSize[0] == 0 || vVolumeSize[1] == 0 || vVolumeSize[2] == 0) {
    T_ERROR("Invalid 0-length volume size!");
    return false;
  }
  // indices are 1-based!! (cell data)
  vVolumeSize[0] -= 1;
  vVolumeSize[1] -= 1;
  vVolumeSize[2] -= 1;
  MESSAGE("VTK volume is %llux%llux%llu", vVolumeSize[0], vVolumeSize[1],
          vVolumeSize[2]);
  vtk >> junk >> junk >> junk >> junk; // ORIGIN blah blah blah
  vtk >> junk >> vVolumeAspect[0] >> vVolumeAspect[1] >> vVolumeAspect[2];
  MESSAGE("aspect: %5.3fx%5.3fx%5.3f", vVolumeAspect[0], vVolumeAspect[1],
          vVolumeAspect[2]);

  // now we now the basics of the data, but we can have multiple fields in the
  // file.  Scan through until we find the first SCALARS.
  scan_for_line(vtk, "SCALARS");
  if(vtk.eof()) {
    T_ERROR("No scalar data in file!");
    return false;
  }
  std::string type, one;
  vtk >> junk >> current >> type >> one;
  assert(junk == "SCALARS"); // if not, then scan_for_line failed.
  strTitle = current + " from VTK converter";
  MESSAGE("Reading field '%s' from the VTK file...", current.c_str());
  assert(one == "1"); // this is always "1" in the files I have...
  BStreamDescriptor bs = vtk_to_tuvok_type(type);
  iComponentSize = bs.width * 8; // bytes to bits
  iComponentCount = bs.components;
  bSigned = bs.is_signed;
  bIsFloat = bs.fp;
  // legacy VTK files are always Big endian, so we need to convert if we're
  // little endian.
  bConvertEndianness = EndianConvert::IsLittleEndian();
  vtk >> junk >> current; // "LOOKUP_TABLE default"

  // gotta get rid of a byte before we figure out where we are.
  char newline;
  vtk.read(&newline, 1);
  // we can just skip to the binary data without creating a new file.  Do that.
  iHeaderSkip = static_cast<uint64_t>(vtk.tellg());
  {
    std::ofstream raw("rawdata-from-vtk.data", std::ios::binary);
    const uint64_t elems = vVolumeSize.volume();
    float cur;
    for(uint64_t i=0; i < elems; ++i) {
      vtk.read(reinterpret_cast<char*>(&cur), sizeof(float));
      raw.write(reinterpret_cast<char*>(&cur), sizeof(float));
    }
    raw.close();
  }

  strIntermediateFile = strSourceFilename;
  bDeleteIntermediateFile = false;

  return true;
}
/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2013 IVDA Group.

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
