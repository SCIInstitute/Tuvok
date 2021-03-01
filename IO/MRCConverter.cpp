/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2012 Scientific Computing and Imaging Institute,
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

#include <cctype>
#include <fstream>
#include <iterator>
#include "MRCConverter.h"
#include "Basics/SysTools.h"

enum DataType
{
  IMAGE_8BIT_SIGNED                 = 0,  // Range: [-128, 127]
  IMAGE_16BIT_HALFWORDS             = 1,
  IMAGE_32BIT_REALS                 = 2,
  TRANSFORM_COMPLEX_16BIT_INTEGERS  = 3,
  TRANSFORM_COMPLEX_32BIT_REALS     = 4,  
  IMAGE_16BIT_UNSIGNED              = 6,  // Range: [0, 65535]
};

// Structure should be exactly 1024 bytes.
#pragma pack(push, 1)
struct MRCHeader
{
  int32_t nx;       // Number of columns (fastest changing in map)
  int32_t ny;       // Number of rows
  int32_t nz;       // Number of sections (slowest changing in map)

  int32_t mode;     // Is the dataType enumeration

  int32_t nxStart;  // Number of first column in map (Default = 0)
  int32_t nyStart;  // Number of first row in map
  int32_t nzStart;  // Number of first section in map

  int32_t mx;       // Number of intervals along X
  int32_t my;       // Number of intervals along Y
  int32_t mz;       // Number of intervals along Z

  int32_t cellA[3]; // Cell dimensions in angstroms
  int32_t cellB[3]; // Cell angles in degrees

  int32_t mapC;     // Axis corresponding to columns (1,2,3 for X,Y,Z)
  int32_t mapR;     // Axis corresponding to rows (1,2,3 for X,Y,Z)
  int32_t mapS;     // Axis corresponding to sections (1,2,3 for X,Y,Z)

  int32_t dMin;     // Minimum density value
  int32_t dMax;     // Maximum density value
  int32_t dMean;    // Mean density value

  int32_t ispc;     // Space group number 0 or 1 (default = 0)
  int32_t nSymBt;   // Number of bytes used for symmetry data (0 or 80)

  int32_t extra[25];// Extra space used for anything

  int32_t origin[3];// Origin in X,Y,Z used for transforms

  int32_t map;      // Character string 'MAP' to identify file type
  int32_t machSt;   // Machine stamp.

  int32_t rms;      // rms deviation of map from mean density

  int32_t nLabl;    // Number of labels being used
  char textLabels[10][80]; // 10 80-character text labels
};
#pragma pack(pop)

MRCConverter::MRCConverter()
{
  static_assert(sizeof(MRCHeader) == 1024, "structure must be 1024 bytes.");
  m_vConverterDesc = L"Medical Research Council's electron density format.";
  m_vSupportedExt.push_back(L"MRC");
}

bool MRCConverter::ConvertToNative(
     const std::wstring&, const std::wstring&, 
     uint64_t, unsigned, uint64_t, 
     bool, bool, UINT64VECTOR3,
     FLOATVECTOR3, bool, 
     const bool)
{
  return false;
}

bool MRCConverter::ConvertToRAW(
     const std::wstring& strSourceFilename, const std::wstring& strTempDir,
     bool, uint64_t& iHeaderSkip, unsigned& iComponentSize,
     uint64_t& iComponentCount, bool& bConvertEndianess, bool& bSigned, 
     bool& bIsFloat, UINT64VECTOR3& vVolumeSize, FLOATVECTOR3& vVolumeAspect,
     std::wstring&,
     std::wstring& strIntermediateFile, bool& bDeleteIntermediateFile)
{
  // Input file
  std::ifstream iFile(SysTools::toNarrow(strSourceFilename).c_str(), std::ios::binary);
  if(!iFile)
  {
    T_ERROR("Could not open %s!", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  }

  MRCHeader hdr;
  iFile.read(reinterpret_cast<char*>(&hdr), sizeof(MRCHeader));

  vVolumeSize[0] = hdr.nx;
  vVolumeSize[1] = hdr.ny;
  vVolumeSize[2] = hdr.nz;

  iHeaderSkip = 0; // we'll create a new, raw file.
  iComponentCount = 1;
  bConvertEndianess = true;
  vVolumeAspect = FLOATVECTOR3(1.0, 1.0, 1.0);
  strIntermediateFile = strTempDir + L"/mrc.iv3d.tmp";
  bDeleteIntermediateFile = true;

  // Output file
  std::ofstream oFile(SysTools::toNarrow(strIntermediateFile).c_str(), std::ios::binary);
  if(!oFile)
  {
    T_ERROR("Could not create intermediate file '%s'.",
      SysTools::toNarrow(strIntermediateFile).c_str());
    bDeleteIntermediateFile = false;
    return false;
  }

  // Only handling two types for now (16bit ints and 32bit floats)
  bool success = true;
  if (hdr.mode == IMAGE_16BIT_HALFWORDS)
  {
    bSigned = true;
    bIsFloat = false;
    iComponentSize = 16;
    
    // Spit out the MRC file
    std::copy(std::istreambuf_iterator<char>(iFile),
              std::istreambuf_iterator<char>(),
              std::ostreambuf_iterator<char>(oFile));
  }
  else if (hdr.mode == IMAGE_32BIT_REALS)
  {
    bSigned = true;
    bIsFloat = true;
    iComponentSize = 32;
    
    // Spit out the MRC file
    std::copy(std::istreambuf_iterator<char>(iFile),
              std::istreambuf_iterator<char>(),
              std::ostreambuf_iterator<char>(oFile));
  }
  else
  {
    success = false; // =(
  }

  return success;
}


bool MRCConverter::CanRead(const std::wstring&,
                           const std::vector<int8_t>& bytes) const
{
  /// @todo Read header and ensure 'map' corresponds to the character string
  ///       "MAP" (assuming last byte is a null terminator).
  return static_cast<char>(std::toupper(bytes[0])) == 'M' &&
         static_cast<char>(std::toupper(bytes[0])) == 'A' &&
         static_cast<char>(std::toupper(bytes[0])) == 'P';
}
