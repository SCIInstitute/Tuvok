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

/**
  \file    BOVConverter.h
  \author  Tom Fogal
           SCI Institute
           University of Utah
*/
#include <cstdarg>
#include <fstream>
#include <sstream>
#include "BOVConverter.h"
#include "IO/KeyValueFileParser.h"
#include "Controller/Controller.h"
#include "Basics/SysTools.h"
#include "exception/FileNotFound.h"

enum DataType {
  UnknownType,
  Short,
  UShort, /* extension! */
  Float,
  Integer,
  Char
};

static DataType bov_type(const KeyValueFileParser &kvp);
static const KeyValPair *find_header(const KeyValueFileParser &,
                                     const char *s, ...);

BOVConverter::BOVConverter()
{
  m_vConverterDesc = L"Brick of Values";
  m_vSupportedExt.push_back(L"BOV");
}

bool BOVConverter::ConvertToRAW(
                            const std::wstring& strSourceFilename,
                            const std::wstring&, bool,
                            uint64_t& iHeaderSkip, unsigned& iComponentSize,
                            uint64_t& iComponentCount, bool& bConvertEndianness,
                            bool& bSigned, bool& bIsFloat,
                            UINT64VECTOR3& vVolumeSize,
                            FLOATVECTOR3& vVolumeAspect, std::wstring& strTitle,
                            std::wstring& strIntermediateFile,
                            bool& bDeleteIntermediateFile)
{
  MESSAGE("Attempting to convert BOV: %s", SysTools::toNarrow(strSourceFilename).c_str());
  KeyValueFileParser hdr(strSourceFilename.c_str());

  if(!hdr.FileReadable()) {
    T_ERROR("Could not parse %s; could not open.", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  }
  KeyValPair *file = hdr.GetData("DATA_FILE");
  const KeyValPair *size = find_header(hdr, "DATA SIZE", "DATA_SIZE", NULL);
  KeyValPair *aspect_x = hdr.GetData("BRICK X_AXIS");
  KeyValPair *aspect_y = hdr.GetData("BRICK Y_AXIS");
  KeyValPair *aspect_z = hdr.GetData("BRICK Z_AXIS");
  strTitle = L"BOV Volume";

  {
    iHeaderSkip = 0;
    strIntermediateFile = SysTools::toWide(SysTools::CanonicalizePath(file->strValue));
    // Try the path the file gave first..
    if(!SysTools::FileExists(strIntermediateFile)) {
      // .. but if that didn't work, prepend the directory of the .bov file.
      strIntermediateFile = SysTools::toWide(SysTools::CanonicalizePath(
                                SysTools::toNarrow(SysTools::GetPath(strSourceFilename)) +
                              file->strValue
                            ));
      if(!SysTools::FileExists(strIntermediateFile)) {
        std::ostringstream err;
        err << "Data file referenced in BOV (" << SysTools::toNarrow(strIntermediateFile)
            << ") not found!";
        T_ERROR("%s", err.str().c_str());
        throw FILE_NOT_FOUND(err.str().c_str());
      }
    }
    MESSAGE("Reading data from %s", SysTools::toNarrow(strIntermediateFile).c_str());
    bDeleteIntermediateFile = false;
  }
  {
    std::istringstream ss(size->strValue);
    ss >> vVolumeSize[0] >> vVolumeSize[1] >> vVolumeSize[2];
    MESSAGE("Dimensions: %ux%ux%u",
            uint32_t(vVolumeSize[0]), uint32_t(vVolumeSize[1]), uint32_t(vVolumeSize[2]));
    iHeaderSkip = 0;
  }
  {
    iComponentCount = 1;
    iComponentSize = 8;
    bConvertEndianness = false;
    bIsFloat = false;
    bSigned = false;

    switch(bov_type(hdr)) {
      case Char:
        iComponentSize = 8;
        bSigned = true;
        break;
      case Short:
        iComponentSize = 16;
        bSigned = true;
        break;
      case UShort: // extension!
        iComponentSize = 16;
        break;
      case Integer:
        iComponentSize = 32;
        bSigned = true;
        break;
      case Float:
        iComponentSize = 32;
        bSigned = true;
        bIsFloat = true;
        break;
      default:
        T_ERROR("Unknown BOV data type.");
        return false;
    }
    MESSAGE("%lu-bit %s, %s data", iComponentSize,
            bSigned ? "signed" : "unsigned",
            bIsFloat ? "floating point" : "integer");
  }
  {
    // e.g. "BRICK X_AXIS 1.000 0.000 0.000".  Might not exist.
    vVolumeAspect[0] = vVolumeAspect[1] = vVolumeAspect[2] = 1.0;
    if(aspect_x && aspect_y && aspect_z) {
      std::stringstream x(aspect_x->strValue);
      std::stringstream y(aspect_y->strValue);
      std::stringstream z(aspect_z->strValue);
      float junk;
      x >> vVolumeAspect[0];
      y >> junk >> vVolumeAspect[1];
      z >> junk >> junk >> vVolumeAspect[2];
    }
    MESSAGE("Aspect: %2.2fx%2.2fx%2.2f",
            vVolumeAspect[0], vVolumeAspect[1], vVolumeAspect[2]);
  }
  return true;
}

bool BOVConverter::ConvertToNative(const std::wstring& raw,
                                   const std::wstring& target,
                                   uint64_t skip, unsigned component_size,
                                   uint64_t n_components, bool is_signed,
                                   bool fp, UINT64VECTOR3 dimensions,
                                   FLOATVECTOR3 aspect, bool batch,
                                   const bool bQuantizeTo8Bit)
{
  std::wstring fn_data = SysTools::RemoveExt(target);

  std::ofstream header(SysTools::toNarrow(target).c_str(), std::ios::out | std::ios::trunc);

  std::wstring targetRaw = target+L".data";

  std::string data_format;
  switch(component_size) {
    case 8: data_format = "BYTE"; break;
    case 16: data_format = "SHORT"; break;
    case 32:
      if(fp) { data_format = "FLOAT"; }
      else   { data_format = "INT"; }
      break;
    case 64:
      // In BOV, a 64bit integer dataset is a 2-component 32-bit integer
      // dataset.  As far as I can tell...
      if(fp) { data_format = "DOUBLE"; }
      else   { data_format = "INT"; }
      break;
  }
  header << "DATA_FILE: " << SysTools::toNarrow(SysTools::GetFilename(targetRaw)) << std::endl
         << "DATA SIZE: "
            << dimensions[0] << " " << dimensions[1] << " " << dimensions[2]
         << std::endl
         << "DATA FORMAT: " << data_format << std::endl
         << "DATA_COMPONENTS: " << n_components << std::endl
         << "VARIABLE: from_imagevis3d" << std::endl
         << "BRICK_SIZE: "
            << dimensions[0] << " " << dimensions[1] << " " << dimensions[2]
         << std::endl
         << "CENTERING: nodal" << std::endl;
  header.close();

  // copy the raw file.
  return RAWConverter::ConvertToNative(raw, targetRaw, skip,
                                       component_size, n_components, is_signed,
                                       fp, dimensions, aspect, batch,
                                       bQuantizeTo8Bit);
}

static DataType
bov_type(const KeyValueFileParser &kvp)
{
  const KeyValPair *format = NULL;
  DataType retval = UnknownType;

  // Search a list of strings until we find one.
  static const char formats[][16] = {
    "DATA FORMAT", "DATA_FORMAT", "FORMAT"
  };
  for(size_t i=0; i < sizeof(formats); ++i) {
    format = kvp.GetData(formats[i]);
    if(format != NULL) { break; }
  }

  // Might not have found any of those.
  if(format == NULL) {
    T_ERROR("Could not determine data format.  "
            "Is this a BOV file?  Potentially corrupt.");
    return UnknownType;
  }

  // Data type is specified by strings, but the exact string isn't quite
  // uniform.  Another search.
  static const char floats[][16] = {
    "FLOAT", "FLOATS"
  };
  for(size_t i=0; i < sizeof(floats); ++i) {
    if(format->strValueUpper == std::string(floats[i])) {
      retval = Float;
      break;
    }
  }
  if(format->strValueUpper == std::string("SHORT")) {
    retval = Short;
  }
  if(format->strValueUpper == std::string("USHORT")) {
    retval = UShort;
  }
  if(format->strValueUpper == std::string("BYTE")) {
    retval = Char;
  }

  if(retval != Float && retval != Short && retval != Char) {
    WARNING("Unknown BOV data type '%s'", format->strValue.c_str());
  }
  return retval;
}

// Given a list of strings, returns the first KeyValPair that exists.  Useful
// since some bovs use "_" between words in a key, some don't...
// The argument list must be NULL-terminated.
static const KeyValPair *
find_header(const KeyValueFileParser &kvp, const char *s, ...)
{
  va_list args;
  const KeyValPair *ret;

  // First try 's'.
  ret = kvp.GetData(s);
  if(ret) { return ret; }

  va_start(args, s);
    const char *key = va_arg(args, const char *);
    while(key) {
      ret = kvp.GetData(key);
      if(ret) { return ret; }
    }
  va_end(args);
  return NULL;
}
