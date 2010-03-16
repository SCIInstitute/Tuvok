/*
   The MIT License

   Copyright (c) 2010 Department Image Processing,
   Fraunhofer ITWM.


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
  \file    IASSConverter.cpp
  \author  Andre Liebscher
           Department Image Processing
           Fraunhofer ITWM
  \date    March 2010
*/

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include "boost/cstdint.hpp"
#include "3rdParty/zlib/zlib.h"
#include "IASSConverter.h"
#include <Basics/EndianConvert.h>
#include <Controller/Controller.h>

using namespace std;
using namespace tuvok;

IASSConverter::IASSConverter()
{
  m_vConverterDesc = "Fraunhofer MAVI Volume";
  m_vSupportedExt.push_back("IASS");
  m_vSupportedExt.push_back("IASS.GZ");
}

bool
IASSConverter::ConvertToRAW(const std::string& strSourceFilename,
                           const std::string& strTempDir,
                           bool, UINT64& iHeaderSkip,
                           UINT64& iComponentSize,
                           UINT64& iComponentCount,
                           bool& bConvertEndianess, bool& bSigned,
                           bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                           FLOATVECTOR3& vVolumeAspect,
                           std::string& strTitle,
                           UVFTables::ElementSemanticTable& eType,
                           std::string& strIntermediateFile,
                           bool& bDeleteIntermediateFile)
{
  MESSAGE("Attempting to convert IASS dataset %s", strSourceFilename.c_str());

  // Find out machine endianess
  bConvertEndianess = EndianConvert::IsBigEndian();

  // Check whether file is compressed and uncompress if necessary
  string strInputFile;
  if (IsZipped(strSourceFilename)) {
    MESSAGE("IASS data is GZIP compressed.");
    strInputFile = strTempDir + SysTools::GetFilename(strSourceFilename) +
                   ".uncompressed";
    if (!ExtractGZIPDataset(strSourceFilename, strInputFile, 0)) {
      WARNING("Error while decompressing %s", strSourceFilename.c_str());
      return false;
    }
  } else {
    strInputFile = strSourceFilename;
  }

  // Read header and check for "magic" values of the IASS file
  stHeader header;
  ifstream fileData(strInputFile.c_str());

  if (fileData.is_open()) {
    if(!ReadHeader(header,fileData)) {
      WARNING("The file %s is not a IASS file (missing magic)",
              strInputFile.c_str());
      return false;
    }
  } else {
    WARNING("Could not open IASS file %s", strInputFile.c_str());
    return false;
  }
  fileData.close();

  // Init data
  strTitle = "Fraunhofer MAVI Volume";
  eType = UVFTables::ES_UNDEFINED;
  vVolumeAspect = FLOATVECTOR3(1,1,1);
  iComponentCount = 1;
  vVolumeSize[0] = header.size.x;
  vVolumeSize[1] = header.size.y;
  vVolumeSize[2] = header.size.z;
  iComponentSize = header.bpp*8;
  iHeaderSkip = 0;
  bDeleteIntermediateFile = true;

  if (header.type >= MONO && header.type <= GREY_32) {
    bSigned = false;
    bIsFloat = false;
  } else if (header.type == GREY_F) {
    bSigned = true;
    bIsFloat = true;
  } else if (header.type == COLOR) {
    bSigned = false;
    bIsFloat = false;
    iComponentCount = 3;
  } else {
    T_ERROR("Unsupported image type in file %s", strInputFile.c_str());
    return false;
  }

  // Convert raw data to x-locality and decode MONO files
  LargeRAWFile zLocalData(strInputFile, header.skip);
  zLocalData.Open(false);

  if (!zLocalData.IsOpen()) {
    T_ERROR("Unable to open source file %s", strInputFile.c_str());
    return false;
  }

  strIntermediateFile = strTempDir + SysTools::GetFilename(strSourceFilename)
                        + ".x-local";
  LargeRAWFile xLocalData(strIntermediateFile);
  xLocalData.Create();

  if (!xLocalData.IsOpen()) {
    T_ERROR("Unable to open temp file %s for locality conversion",
            strIntermediateFile.c_str());
    xLocalData.Close();
    return false;
  }

  UINT64 strideZ = header.size.x*header.size.y*header.bpp-header.bpp;
  UINT64 sliceSize = header.size.y * header.size.z * header.bpp;
  unsigned char* sliceBuffer = new unsigned char[static_cast<size_t>(sliceSize)];

  if (header.type == MONO) {
    unsigned char* rleBuffer = new unsigned char[static_cast<size_t>(header.rleLength)];
    zLocalData.ReadRAW(rleBuffer,header.rleLength);

    UINT64 posRLEStream = 0;
    UINT64 posOutStream = 0;
    UINT64 currLength = *rleBuffer;
    UINT64 sliceIndex = 0;
    UINT64 currValue = 1;
    do {
      // This loop is entered if another pixel run would exceed the size of
      // the slice buffer
      while (posOutStream + currLength >= sliceSize) {
        // How many pixels are left to fill up buffer
        UINT64 restLength = sliceSize - posOutStream;

        // Set these remaining pixels
        memset(&sliceBuffer[posOutStream],
               static_cast<unsigned char>((currValue%2)*0xff), static_cast<size_t>(restLength));

        for (UINT64 y = 0; y < header.size.y; y++) {
          xLocalData.SeekPos(y*header.size.x+sliceIndex);
          for (UINT64 z = 0; z < header.size.z; z++) {
            xLocalData.WriteRAW(&sliceBuffer[y*header.size.z+z],1);
            xLocalData.SeekPos(xLocalData.GetPos()+strideZ);
          }
        }

        currLength -= restLength;
        posOutStream = 0;
        sliceIndex++;
      }

      // Set pixels of required length
      if (currLength > 0) {
        memset(&sliceBuffer[posOutStream],
               static_cast<unsigned char>((currValue%2)*0xff), static_cast<size_t>(currLength));
        posOutStream += currLength;
      }

      currValue++;
      posRLEStream++;
      currLength = *(rleBuffer + posRLEStream);
    } while (posRLEStream < header.rleLength);
   
    delete rleBuffer;
  } else {
    for (UINT64 x = 0; x < header.size.x; x++) {
      zLocalData.ReadRAW(sliceBuffer,sliceSize);
      for (UINT64 y = 0; y < header.size.y; y++) {
        xLocalData.SeekPos((y*header.size.x+x)*header.bpp);
        for (UINT64 z = 0; z < header.size.z; z++) {
          xLocalData.WriteRAW(&sliceBuffer[(y*header.size.z+z)*header.bpp],
                              header.bpp);
          xLocalData.SeekPos(xLocalData.GetPos()+strideZ);
        }
      }
    }
  }
  delete sliceBuffer;

  if ( strInputFile != strSourceFilename )
    Remove(strInputFile, Controller::Debug::Out());

  return true;
}

// unimplemented!
bool
IASSConverter::ConvertToNative(const std::string&,
                               const std::string&,
                               UINT64, UINT64,
                               UINT64, bool,
                               bool,
                               UINT64VECTOR3,
                               FLOATVECTOR3,
                               bool,
                               bool)
{
  return false;
}

bool
IASSConverter::CanRead(const std::string& fn,
                             const std::vector<int8_t>&) const
{
  std::string ext = SysTools::GetExt(fn);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 (int(*)(int))std::toupper);

  if (ext != "IASS") {
    std::string extPt1 = SysTools::GetExt(SysTools::RemoveExt(fn));
    std::transform(extPt1.begin(), extPt1.end(), extPt1.begin(),
                   (int(*)(int))std::toupper);
    ext = extPt1 + "." + ext;
  }

  return SupportedExtension(ext);
}

bool
IASSConverter::IsZipped(const std::string& strFile)
{
  gzFile file = gzopen(strFile.c_str(), "rb");
  if (file==0)
    return false;

  int rt = gzdirect(file);
  gzclose(file);
  return rt == 1 ? false : true;
}

IASSConverter::stHeader::stHeader()
  : type(INVALID), bpp(0), skip(0), rleLength(0), size(), spacing(),
    creator(""), history("") {}

void
IASSConverter::stHeader::Reset()
{
  type = INVALID;
  bpp = 0;
  skip = 0;
  rleLength = 0;
  size.x = 0;
  size.y = 0;
  size.z = 0;
  spacing.x = 0.0;
  spacing.y = 0.0;
  spacing.z = 0.0;
  creator = "";
  history = "";
}

bool
IASSConverter::ReadHeader(stHeader& header, std::istream& inputStream)
{
  header.Reset();

  if(!inputStream.good())
    return false;

  std::string strLine;

  // read magic
  getline(inputStream, strLine);
  if (!(strLine.substr(0, 9) == "SVstatmat" ||
      strLine.substr(0, 4) == "a4iL") ||
      inputStream.fail()) {
    return false;
  }

  // read header
  do {
    getline(inputStream, strLine);

    if (inputStream.fail())
      return false;

    if (strLine.substr(0, 11) == "# SPACING: ") {
      std::istringstream in(strLine.substr(11, strLine.length()));
      in >> header.spacing.x >> header.spacing.y >> header.spacing.z;
    }
    else if (strLine.substr(0, 11) == "# CREATOR: ")
      header.creator = strLine.substr(11, strLine.length());
    else if (strLine.substr(0, 11) == "# HISTORY: ")
      header.history = strLine.substr(11, strLine.length());
    else if (strLine.substr(0, 8) == "# TYPE: ") {
      if (isdigit(strLine[8])) {
        std::string tmp = strLine.substr(8, strLine.length());
        std::istringstream input_helper(tmp);
        unsigned int type;

        input_helper >> type;
        header.type = static_cast<ePixelType>(type);

        if (header.type < 0 || header.type >= INVALID)
          return false;
      } else {
        if (strLine.substr(8, strLine.length()) == "MONO") {
          header.type = MONO;
        } else if (strLine.substr(8, strLine.length()) == "GREY_8") {
          header.type = GREY_8;
        } else if (strLine.substr(8, strLine.length()) == "GREY_16") {
          header.type = GREY_16;
        } else if (strLine.substr(8, strLine.length()) == "GREY_32") {
          header.type = GREY_32;
        } else if (strLine.substr(8, strLine.length()) == "GREY_F") {
          header.type = GREY_F;
        } else if (strLine.substr(8, strLine.length()) == "COLOR" ||
                   strLine.substr(8, strLine.length()) == "RGB_8") {
          header.type = COLOR;
        } else if (strLine.substr(8, strLine.length()) == "COMPLEX_F") {
          header.type = COMPLEX_F;
        } else {
          return false;
        }
      }
    }
  } while (strLine[0] == '#');

  if (inputStream.fail() || inputStream.eof()) {
    return false;
  } else {
    std::istringstream input_helper(strLine);
    input_helper >> header.size.x >> header.size.y >> header.size.z;
    if (inputStream.fail()) {
      return false;
    }
  }

  if (header.type == MONO) {
    getline(inputStream, strLine);

    if (inputStream.fail() || inputStream.eof()) {
      return false;
    }

    std::istringstream input_helper(strLine);
    input_helper >> header.rleLength;
  }

  header.skip = inputStream.tellg();

  switch(header.type) {
    case(MONO):
      header.bpp = 1;
      break;
    case(GREY_8):
      header.bpp = 1;
      break;
    case(GREY_16):
      header.bpp = 2;
      break;
    case(GREY_32):
      header.bpp = 4;
      break;
    case(GREY_F):
      header.bpp = 4;
      break;
    case(COLOR):
      header.bpp = 3;
      break;
    case(COMPLEX_F):
      header.bpp = 8;
      break;
    case(INVALID):
      return false;
  };

  return true;
}
