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

#include "VFFConverter.h"
#include "IOManager.h"  // for the size defines
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>
#include <IO/KeyValueFileParser.h>

using namespace std;


VFFConverter::VFFConverter()
{
  m_vConverterDesc = "Visualization File Format";
  m_vSupportedExt.push_back("VFF");
}

bool VFFConverter::Convert(const std::string& strSourceFilename, const std::string& strTargetFilename, const std::string& strTempDir, MasterController* pMasterController, bool)
{
  pMasterController->DebugOut()->Message("VFFConverter::Convert","Attempting to convert VFF dataset %s to %s", strSourceFilename.c_str(), strTargetFilename.c_str());

  // Check Magic value in VFF File first
  ifstream fileData(strSourceFilename.c_str());  
  string strFirstLine;

  if (fileData.is_open())
  {
    getline (fileData,strFirstLine);
    if (strFirstLine.substr(0,7) != "ncaa") {
      pMasterController->DebugOut()->Warning("VFFConverter::Convert","The file %s is not a VFF file (missing magic)", strSourceFilename.c_str());
      return false;
    }
  } else {
    pMasterController->DebugOut()->Warning("VFFConverter::Convert",
                                           "Could not open VFF file %s",
                                           strSourceFilename.c_str());
    return false;
  }
  fileData.close();

  // read data
  UINT64        iComponentSize=8;
  UINT64        iComponentCount=1;
  UINTVECTOR3    vVolumeSize(1,1,1);
  FLOATVECTOR3  vVolumeAspect(1,1,1);

  string strHeaderEnd;
  strHeaderEnd.push_back(12);  // header end char of vffs is ^L = 0C = 12 

  KeyValueFileParser parser(strSourceFilename, true, "=", strHeaderEnd);

  if (!parser.FileReadable()) {
    pMasterController->DebugOut()->Warning("VFFConverter::Convert","Could not open VFF file %s", strSourceFilename.c_str());
    return false;
  }

  KeyValPair* kvp = parser.GetData("TYPE");
  if (kvp == NULL) {
    pMasterController->DebugOut()->Error("VFFConverter::Convert","Could not open find token \"type\" in file %s", strSourceFilename.c_str());
    return false;
  } else {
    if (kvp->strValueUpper != "RASTER;")  {
      pMasterController->DebugOut()->Error("VFFConverter::Convert","Only raster VFFs are supported at the moment");
      return false;
     }
  }

  int iDim;
  kvp = parser.GetData("RANK");
  if (kvp == NULL) {
    pMasterController->DebugOut()->Error("VFFConverter::Convert","Could not open find token \"rank\" in file %s", strSourceFilename.c_str());
    return false;
  } else {
    iDim = kvp->iValue;
  }

  kvp = parser.GetData("BANDS");
  if (kvp == NULL) {
    pMasterController->DebugOut()->Error("VFFConverter::Convert","Could not open find token \"bands\" in file %s", strSourceFilename.c_str());
    return false;
  } else {
    if (kvp->iValue != 1)  {
      pMasterController->DebugOut()->Error("VFFConverter::Convert","Only scalar VFFs are supported at the moment");
      return false;
     }
  }

  kvp = parser.GetData("FORMAT");
  if (kvp == NULL) {
    pMasterController->DebugOut()->Error("VFFConverter::Convert","Could not open find token \"format\" in file %s", strSourceFilename.c_str());
    return false;
  } else {
    if (kvp->strValueUpper != "SLICE;")  {
      pMasterController->DebugOut()->Error("VFFConverter::Convert","Only VFFs with slice layout are supported at the moment");
      return false;
     }
  }

  kvp = parser.GetData("BITS");
  if (kvp == NULL) {
    pMasterController->DebugOut()->Error("VFFConverter::Convert","Could not open find token \"bands\" in file %s", strSourceFilename.c_str());
    return false;
  } else {
    iComponentSize = kvp->iValue;
  }

  kvp = parser.GetData("SIZE");
  if (kvp == NULL) {
    pMasterController->DebugOut()->Error("VFFConverter::Convert","Could not open find token \"size\" in file %s", strSourceFilename.c_str());
    return false;
  } else {
    vVolumeSize[0] = kvp->viValue[0];
    vVolumeSize[1] = kvp->viValue[1];
    if (iDim == 3) vVolumeSize[2] = kvp->viValue[2];
  }

  kvp = parser.GetData("SPACING");
  if (kvp == NULL) {
    pMasterController->DebugOut()->Error("VFFConverter::Convert","Could not open find token \"size\" in file %s", strSourceFilename.c_str());
    return false;
  } else {
    vVolumeAspect[0] = kvp->vfValue[0];
    vVolumeAspect[1] = kvp->vfValue[1];
    if (iDim == 3) vVolumeAspect[2] = kvp->vfValue[2];
  }

  string strTitle;
  kvp = parser.GetData("TITLE");
  if (kvp == NULL) {
    strTitle = "VFF data";
  } else {
    strTitle = kvp->strValue;
  }

  size_t iHeaderSkip = parser.GetStopPos();

  /// \todo check if really all vff files contain signed data
  return ConvertRAWDataset(strSourceFilename, strTargetFilename, strTempDir, pMasterController, iHeaderSkip, iComponentSize, iComponentCount, !EndianConvert::IsBigEndian(), true,
                           vVolumeSize, vVolumeAspect, strTitle, SysTools::GetFilename(strSourceFilename));
}
