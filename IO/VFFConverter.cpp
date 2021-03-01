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
  \file    VFFConverter.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    December 2008
*/

#include <fstream>
#include "VFFConverter.h"
#include <Controller/Controller.h>
#include <IO/KeyValueFileParser.h>
#include "Basics/SysTools.h"

using namespace std;

VFFConverter::VFFConverter()
{
  m_vConverterDesc = L"Visualization File Format";
  m_vSupportedExt.push_back(L"VFF");
}

bool VFFConverter::ConvertToRAW(
  const std::wstring& strSourceFilename, const std::wstring&, bool,
  uint64_t& iHeaderSkip, unsigned& iComponentSize, uint64_t& iComponentCount,
  bool& bConvertEndianess, bool& bSigned, bool& bIsFloat,
  UINT64VECTOR3& vVolumeSize, FLOATVECTOR3& vVolumeAspect,
  std::wstring& strTitle, std::wstring& strIntermediateFile,
  bool& bDeleteIntermediateFile
) {
  MESSAGE("Attempting to convert VFF dataset %s", SysTools::toNarrow(strSourceFilename).c_str());

  // Check Magic value in VFF File first
  ifstream fileData(SysTools::toNarrow(strSourceFilename).c_str());
  string strFirstLine;

  if (fileData.is_open())
  {
    getline (fileData,strFirstLine);
    if (strFirstLine.substr(0,4) != "ncaa") {
      WARNING("The file %s is not a VFF file (missing magic)", SysTools::toNarrow(strSourceFilename).c_str());
      return false;
    }
  } else {
    WARNING("Could not open VFF file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  }
  fileData.close();

  // init data
  strIntermediateFile = strSourceFilename;
  bDeleteIntermediateFile = false;
  iComponentSize    = 8;
  iComponentCount   = 1;
  vVolumeSize       = UINT64VECTOR3(1,1,1);
  vVolumeAspect     = FLOATVECTOR3(1,1,1);
  bConvertEndianess = EndianConvert::IsLittleEndian();
  bSigned           = true;
  bIsFloat          = false; /// \todo check if VFF can store float values

  // read data
  string strHeaderEnd;
  strHeaderEnd.push_back(12);  // header end char of vffs is ^L = 0C = 12

  KeyValueFileParser parser(strSourceFilename, false, L"=", SysTools::toWide(strHeaderEnd));

  if (!parser.FileReadable()) {
    WARNING("Could not open VFF file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  }

  KeyValPair* kvp = parser.GetData("TYPE");
  if (kvp == NULL || kvp->strValue == ";") {
    T_ERROR("Could not find valid token \"type\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    if (kvp->strValueUpper != "RASTER;")  {
      T_ERROR("Only raster VFFs are supported at the moment");
      return false;
     }
  }

  int iDim;
  kvp = parser.GetData("RANK");
  if (kvp == NULL || kvp->strValue == ";") {
    T_ERROR("Could not find valid token \"rank\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    iDim = kvp->iValue;
  }

  kvp = parser.GetData("BANDS");
  if (kvp == NULL || kvp->strValue == ";") {
    T_ERROR("Could not find valid token \"bands\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    if (kvp->iValue != 1)  {
      T_ERROR("Only scalar VFFs are supported at the moment");
      return false;
     }
  }

  kvp = parser.GetData("FORMAT");
  if (kvp == NULL || kvp->strValue == ";") {
    T_ERROR("Could not find valid token \"format\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    if (kvp->strValueUpper != "SLICE;")  {
      T_ERROR("Only VFFs with slice layout are supported at the moment");
      return false;
     }
  }

  kvp = parser.GetData("BITS");
  if (kvp == NULL || kvp->strValue == ";") {
    T_ERROR("Could not find valid token \"bands\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    iComponentSize = kvp->iValue;
  }

  kvp = parser.GetData("SIZE");
  if (kvp == NULL || kvp->strValue == ";") {
    T_ERROR("Could not find valid token \"size\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    vVolumeSize[0] = kvp->viValue[0];
    vVolumeSize[1] = kvp->viValue[1];
    vVolumeSize[2] = 1;
    if (iDim == 3) vVolumeSize[2] = kvp->viValue[2];
    MESSAGE("%llu x %llu x %llu volume.", vVolumeSize[0], vVolumeSize[1],
            vVolumeSize[2]);
  }

  kvp = parser.GetData("SPACING");
  if (kvp == NULL || kvp->strValue == ";") {
    T_ERROR("Could not find valid token \"size\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    vVolumeAspect[0] = kvp->vfValue[0];
    vVolumeAspect[1] = kvp->vfValue[1];
    if (iDim == 3) vVolumeAspect[2] = kvp->vfValue[2];
  }

  kvp = parser.GetData("TITLE");
  if (kvp == NULL || kvp->strValue == ";") {
    strTitle = L"VFF data";
  } else {
    strTitle = kvp->wstrValue;
  }

  iHeaderSkip = parser.GetStopPos();

  return true;
}

bool VFFConverter::ConvertToNative(const std::wstring& strRawFilename, const std::wstring& strTargetFilename, uint64_t iHeaderSkip,
                             unsigned iComponentSize, uint64_t iComponentCount, bool bSigned, bool bFloatingPoint,
                             UINT64VECTOR3 vVolumeSize,FLOATVECTOR3 vVolumeAspect, bool, bool bQuantizeTo8Bit) {

  // create header textfile from metadata
  ofstream fAsciiTarget(SysTools::toNarrow(strTargetFilename).c_str());
  if (!fAsciiTarget.is_open()) {
    T_ERROR("Unable to open target file %s.", SysTools::toNarrow(strTargetFilename).c_str());
    return false;
  }

  if (bFloatingPoint) {
    T_ERROR("Floating point formats are not avaliable for vff files.");
    return false;
  }

  fAsciiTarget << "ncaa" << endl;
  fAsciiTarget << "type=raster;" << endl;
  fAsciiTarget << "rank=3;" << endl;
  fAsciiTarget << "bands=" << iComponentCount << ";"<< endl;
  fAsciiTarget << "format=slice;" << endl;
  fAsciiTarget << "bits=" << iComponentSize << ";" << endl;
  fAsciiTarget << "size=" << vVolumeSize.x << " " << vVolumeSize.y << " "<< vVolumeSize.z << ";" << endl;
  fAsciiTarget << "spacing=" << vVolumeAspect.x << " " << vVolumeAspect.y << " "<< vVolumeAspect.z << ";" << endl;

  // add the ^L header delimiter
  string strHeaderEnd;
  strHeaderEnd.push_back(12);  // header end char of vffs is ^L = 0C = 12
  fAsciiTarget << strHeaderEnd << endl;
  fAsciiTarget.close();

  // append RAW data using the parent's call
  bool bRAWSuccess = AppendRAW(strRawFilename, iHeaderSkip, strTargetFilename, iComponentSize, !EndianConvert::IsBigEndian(), !bSigned, bQuantizeTo8Bit);

  if (bRAWSuccess) {
    return true;
  } else {
    T_ERROR("Error appaneding raw data to header file %s.", SysTools::toNarrow(strTargetFilename).c_str());
    SysTools::RemoveFile(strTargetFilename);
    return false;
  }
}
