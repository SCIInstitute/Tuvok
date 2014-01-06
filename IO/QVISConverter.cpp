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
  \file    QVISConverter.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    December 2008
*/

#include <fstream>
#include "QVISConverter.h"
#include <Controller/Controller.h>
#include <Basics/SysTools.h>
#include <IO/KeyValueFileParser.h>

using namespace std;


QVISConverter::QVISConverter()
{
  m_vConverterDesc = "QVis Data";
  m_vSupportedExt.push_back("DAT");
}

bool QVISConverter::ConvertToRAW(
  const std::string& strSourceFilename, const std::string&, bool,
  uint64_t& iHeaderSkip, unsigned& iComponentSize, uint64_t& iComponentCount,
  bool& bConvertEndianess, bool& bSigned, bool& bIsFloat,
  UINT64VECTOR3& vVolumeSize, FLOATVECTOR3& vVolumeAspect,
  std::string& strTitle, std::string& strIntermediateFile,
  bool& bDeleteIntermediateFile
) {
  MESSAGE("Attempting to convert QVIS dataset %s", strSourceFilename.c_str());

  bDeleteIntermediateFile = false;
  strTitle          = "Qvis data";
  iHeaderSkip       = 0;
  iComponentSize    = 8;
  iComponentCount   = 1;
  bSigned           = false;
  bConvertEndianess = EndianConvert::IsBigEndian();

  KeyValueFileParser parser(strSourceFilename);

  if (parser.FileReadable())  {
    KeyValPair* format = parser.GetData("FORMAT");
    if (format == NULL)
      return false;
    else {
      // The "CHAR" here is correct; QVis cannot store signed 8bit data.
      if(format->strValueUpper == "CHAR" ||
         format->strValueUpper == "UCHAR" ||
         format->strValueUpper == "BYTE") {
        bSigned = false;
        iComponentSize = 8;
        iComponentCount = 1;
        bIsFloat = false;
      } else if (format->strValueUpper == "SHORT") {
        bSigned = true;
        iComponentSize = 16;
        iComponentCount = 1;
        bIsFloat = false;
      } else if (format->strValueUpper == "USHORT") {
        bSigned = false;
        iComponentSize = 16;
        iComponentCount = 1;
        bIsFloat = false;
      } else if (format->strValueUpper == "FLOAT") {
        bSigned = true;
        iComponentSize = 32;
        iComponentCount = 1;
        bIsFloat = true;
      } else if (format->strValueUpper == "UCHAR4") {
        bSigned = false;
        iComponentSize = 8;
        iComponentCount = 4;
        bIsFloat = false;
      } else if(format->strValueUpper == "USHORT3") {
        bSigned = false;
        iComponentSize = 16;
        iComponentCount = 3;
        bIsFloat = false;
      } else if(format->strValueUpper == "USHORT4") {
        bSigned = false;
        iComponentSize = 16;
        iComponentCount = 4;
        bIsFloat = false;
      }
    }

    KeyValPair* objectfilename = parser.GetData("OBJECTFILENAME");
    if (objectfilename == NULL) {
      WARNING("This is not a valid QVIS dat file.");
      return false;
    } else
        strIntermediateFile = objectfilename->strValue;

    KeyValPair* resolution = parser.GetData("RESOLUTION");
    if (resolution == NULL || resolution->vuiValue.size() != 3) {
      WARNING("This is not a valid QVIS dat file.");
      return false;
    } else
      vVolumeSize = UINT64VECTOR3(resolution->vuiValue);

    KeyValPair* endianess = parser.GetData("ENDIANESS");
    if (endianess && endianess->strValueUpper == "BIG")
      bConvertEndianess = !bConvertEndianess;

    KeyValPair* sliceThickness = parser.GetData("SLICETHICKNESS");
    if (sliceThickness == NULL || sliceThickness->vuiValue.size() != 3) {
      WARNING("This is not a valid QVIS dat file.");
      vVolumeAspect = FLOATVECTOR3(1,1,1);
    } else {
      vVolumeAspect = FLOATVECTOR3(sliceThickness->vfValue);
      vVolumeAspect = vVolumeAspect / vVolumeAspect.maxVal();
    }

    strIntermediateFile = SysTools::GetPath(strSourceFilename) + strIntermediateFile;
  } else return false;

  return true;
}

bool QVISConverter::ConvertToNative(const std::string& strRawFilename,
                                    const std::string& strTargetFilename,
                                    uint64_t iHeaderSkip,
                                    unsigned iComponentSize,
                                    uint64_t iComponentCount, bool bSigned,
                                    bool bFloatingPoint,
                                    UINT64VECTOR3 vVolumeSize,
                                    FLOATVECTOR3 vVolumeAspect,
                                    bool bNoUserInteraction,
                                    const bool bQuantizeTo8Bit) {
  // compute fromat string
  string strFormat;

  if (!bQuantizeTo8Bit) {
    if (!bFloatingPoint && bSigned && iComponentSize == 32 && iComponentCount == 1)
      strFormat = "FLOAT";
    else
    if (!bFloatingPoint && bSigned && iComponentSize == 8 && iComponentCount == 1)
      strFormat = "CHAR";
    else
    if (!bFloatingPoint && !bSigned && iComponentSize == 8 && iComponentCount == 1)
      strFormat = "UCHAR";
    else
    if (!bFloatingPoint && bSigned && iComponentSize == 16 && iComponentCount == 1)
      strFormat = "SHORT";
    else
    if (!bFloatingPoint && !bSigned && iComponentSize == 16 && iComponentCount == 1)
      strFormat = "USHORT";
    else
    if (!bFloatingPoint && !bSigned && iComponentSize == 8 && iComponentCount == 4)
      strFormat = "UCHAR4";
    else {
      T_ERROR("This data type is not supported by QVIS DAT/RAW files.");
      return false;
    }
  } else {
    if (bSigned)
      strFormat = "CHAR";
    else
      strFormat = "UCHAR";
  }

  // create DAT textfile from metadata
  string strTargetRAWFilename = strTargetFilename+".raw";

  ofstream fTarget(strTargetFilename.c_str());
  if (!fTarget.is_open()) {
    T_ERROR("Unable to open target file %s.", strTargetFilename.c_str());
    return false;
  }

  MESSAGE("Writing DAT File");

  fTarget << "ObjectFileName: " << SysTools::GetFilename(strTargetRAWFilename) << endl;
  fTarget << "TaggedFileName: ---" << endl;
  fTarget << "Resolution:     " << vVolumeSize.x << " " << vVolumeSize.y << " "<< vVolumeSize.z << endl;
  fTarget << "SliceThickness: " << vVolumeAspect.x << " " << vVolumeAspect.y << " "<< vVolumeAspect.z << endl;
  fTarget << "Format:         " << strFormat << endl;
  fTarget << "ObjectType:     TEXTURE_VOLUME_OBJECT" << endl;
  fTarget << "ObjectModel:    RGBA" << endl;
  fTarget << "GridType:       EQUIDISTANT" << endl;
  fTarget << "Endianess:      " << (EndianConvert::IsBigEndian() ? "BIG" : "LITTLE") << endl;

  fTarget.close();

  MESSAGE("Writing RAW File");

  // copy RAW file using the parent's call
  bool bRAWSuccess = RAWConverter::ConvertToNative(strRawFilename, strTargetRAWFilename, iHeaderSkip,
                                                   iComponentSize, iComponentCount, bSigned, bFloatingPoint,
                                                   vVolumeSize, vVolumeAspect, bNoUserInteraction,bQuantizeTo8Bit);

  if (bRAWSuccess) {
    return true;
  } else {
    T_ERROR("Error creating raw target file %s.", strTargetRAWFilename.c_str());
    remove(strTargetFilename.c_str());
    return false;
  }
}
