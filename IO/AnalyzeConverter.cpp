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
  \file    AnalyzeConverter.cpp
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#include <fstream>
#include "AnalyzeConverter.h"
#include "Basics/SysTools.h"

AnalyzeConverter::AnalyzeConverter()
{
  m_vConverterDesc = "Analyze 7.5";
  m_vSupportedExt.push_back("HDR");
}

struct analyze_hdr {
  int hdr_size;
  char data_type[10];
  char db_name[18];
  int extents;
  short session_err;
  char regular;
  char hkey_un0;
  short dim[8]; // yes, really.
  short datatype;
  short bpp;
  float aspect[3];
  float voxel_offset;
};

enum AnalyzeDataTypes {
  DT_NONE=0,
  DT_BINARY,          // 1 bit/voxel
  DT_UNSIGNED_CHAR,   // 8 bit
  DT_SIGNED_SHORT=4,
  DT_SIGNED_INT=8,
  DT_FLOAT=16,
  DT_COMPLEX=32,
  DT_DOUBLE=64,
  DT_RGB=128,
  DT_ALL=255
};

bool AnalyzeConverter::CanRead(const std::string& fn,
                               const std::vector<int8_t>& start) const
{
  if(!AbstrConverter::CanRead(fn, start)) {
    return false;
  }

  if((start[0] == '#' && start[1] == '\n') ||
     (start[0] == ' ' && start[1] == '\n' && start[2] == '\n')) {
    WARNING("Looks like an ascii file... not mine.");
    return false;
  }

  return true;
}

bool AnalyzeConverter::ConvertToRAW(const std::string& strSourceFilename,
                                    const std::string&,
                                    bool,
                                    uint64_t& iHeaderSkip,
                                    unsigned& iComponentSize,
                                    uint64_t& iComponentCount,
                                    bool& bConvertEndianness,
                                    bool& bSigned, bool& bIsFloat,
                                    UINT64VECTOR3& vVolumeSize,
                                    FLOATVECTOR3& vVolumeAspect,
                                    std::string& strTitle,
                                    std::string& strIntermediateFile,
                                    bool& bDeleteIntermediateFile)
{
  strTitle = "from analyze converter";

  std::ifstream analyze(strSourceFilename.c_str(), std::ios::binary);
  if(!analyze) {
    T_ERROR("Could not open %s!", strSourceFilename.c_str());
    return false;
  }
  struct analyze_hdr hdr;

  analyze.read(reinterpret_cast<char*>(&hdr.hdr_size), 4);
  analyze.read(hdr.data_type, 10);
  analyze.read(hdr.db_name, 18);
  analyze.read(reinterpret_cast<char*>(&hdr.extents), 4);
  analyze.read(reinterpret_cast<char*>(&hdr.session_err), 2);
  analyze.read(&hdr.regular, 1);
  analyze.read(&hdr.hkey_un0, 1);
  short num_dimensions;
  analyze.read(reinterpret_cast<char*>(&num_dimensions), 2);
  if(num_dimensions <= 2) {
    T_ERROR("%dd data; must have at least 3 dimensions!");
    return false;
  }
  for(size_t i=0; i < 7; ++i) {
    analyze.read(reinterpret_cast<char*>(&hdr.dim[i]), 2);
  }
  // 14 bytes of unused garbage.
  analyze.seekg(14, std::ios_base::cur);
  analyze.read(reinterpret_cast<char*>(&hdr.datatype), 2); // DT_xxx ..
  analyze.read(reinterpret_cast<char*>(&hdr.bpp), 2);
  analyze.seekg(2, std::ios_base::cur); // "dim_un0", unused.
  float num_aspect;
  analyze.read(reinterpret_cast<char*>(&num_aspect), 4);
  analyze.read(reinterpret_cast<char*>(&hdr.aspect[0]), 4);
  analyze.read(reinterpret_cast<char*>(&hdr.aspect[1]), 4);
  analyze.read(reinterpret_cast<char*>(&hdr.aspect[2]), 4);
  analyze.seekg(16, std::ios_base::cur); // 4 unused aspect values
  // 'voxel_offset' really is a float that stores a byte offset.  Seriously.
  // Maybe some of the same people that wrote DICOM made Analyze as well.
  analyze.read(reinterpret_cast<char*>(&hdr.voxel_offset), 4);

  // The header size was meant to be used in case the analyze format
  // was extended.  It never was.  Thus the headers are always 348
  // bytes.  This provides a convenient check for endianness; if the
  // size isn't 348, then we need to endian convert everything.
  bConvertEndianness = false;
  if(hdr.hdr_size != 348) {
    MESSAGE("Endianness is wrong, swapping...");
    bConvertEndianness = true;
    hdr.bpp = EndianConvert::Swap<short>(hdr.bpp);
    hdr.dim[0] = EndianConvert::Swap<short>(hdr.dim[0]);
    hdr.dim[1] = EndianConvert::Swap<short>(hdr.dim[1]);
    hdr.dim[2] = EndianConvert::Swap<short>(hdr.dim[2]);
    hdr.aspect[0] = EndianConvert::Swap<float>(hdr.aspect[0]);
    hdr.aspect[1] = EndianConvert::Swap<float>(hdr.aspect[1]);
    hdr.aspect[2] = EndianConvert::Swap<float>(hdr.aspect[2]);
    hdr.voxel_offset = EndianConvert::Swap<float>(hdr.voxel_offset);
    hdr.datatype = EndianConvert::Swap<short>(hdr.datatype);
  }

  iComponentCount = 1; // always, I guess?
  iComponentSize = hdr.bpp;
  vVolumeSize = UINT64VECTOR3(hdr.dim[0], hdr.dim[1], hdr.dim[2]);
  vVolumeAspect = FLOATVECTOR3(hdr.aspect[0], hdr.aspect[1], hdr.aspect[2]);
  MESSAGE("%gx%gx%g aspect ratio", vVolumeAspect[0], vVolumeAspect[1],
          vVolumeAspect[2]);
  MESSAGE("%llu-bit %llux%llux%llu data.", iComponentSize,
          vVolumeSize[0], vVolumeSize[1], vVolumeSize[2]);
  {
    uint64_t bits=0;
    switch(hdr.datatype) {
      case DT_BINARY:        bits =  1;
                             bSigned = false;
                             bIsFloat = false;
                             MESSAGE("binary");
                             break;
      case DT_UNSIGNED_CHAR: bits =  8;
                             bSigned = false;
                             bIsFloat = false;
                             MESSAGE("uchar");
                             break;
      case DT_SIGNED_SHORT:  bits = 16;
                             bSigned = true;
                             bIsFloat = false;
                             MESSAGE("signed short");
                             break;
      case DT_SIGNED_INT:    bits = 32;
                             bSigned = true;
                             bIsFloat = false;
                             MESSAGE("int");
                             break;
      case DT_FLOAT:         bits = 32;
                             bSigned = true;
                             bIsFloat = true;
                             MESSAGE("float");
                             break;
      case DT_COMPLEX:
        T_ERROR("Don't know how to handle complex data.");
        return false;
        break;
      case DT_DOUBLE:        bits = 64;
                             bSigned = true;
                             bIsFloat = true;
                             MESSAGE("double");
                             break;
      default:
        WARNING("Unknown data type.");
        bits = 0;
        break;
    }
    if(iComponentSize != bits) {
      T_ERROR("Bits per pixel and data type disagree!  Broken file?");
      analyze.close();
      return false;
    }
  }

  // If the voxel offset is negative, then there is padding between every slice
  // in the data set.  We would need to write an intermediate file to handle
  // that; instead, we just don't handle it.
  if(hdr.voxel_offset < 0.0) {
    analyze.close();
    T_ERROR("Analyze voxel offset is negative (%g).  Intermediate file "
            "required; this converter is broken.");
    return false;
  }
  iHeaderSkip = static_cast<uint64_t>(hdr.voxel_offset);
  MESSAGE("Skipping %llu bytes.", iHeaderSkip);

  strIntermediateFile = SysTools::RemoveExt(strSourceFilename) + ".img";
  MESSAGE("Using intermediate file %s", strIntermediateFile.c_str());
  bDeleteIntermediateFile = false;

  return true;
}

bool AnalyzeConverter::ConvertToNative(const std::string&,
                                       const std::string&,
                                       uint64_t,
                                       unsigned,
                                       uint64_t, bool,
                                       bool,
                                       UINT64VECTOR3,
                                       FLOATVECTOR3,
                                       bool,
                                       const bool)
{
  return false;
}
