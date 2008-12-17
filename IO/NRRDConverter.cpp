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
  \file    NRRDConverter.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    December 2008
*/

#include "NRRDConverter.h"
#include "IOManager.h"  // for the size defines
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>
#include <IO/KeyValueFileParser.h>

using namespace std;


NRRDConverter::NRRDConverter()
{
  m_vSupportedExt.push_back("NRRD");
  m_vSupportedExt.push_back("NHDR");
}

bool NRRDConverter::Convert(const std::string& strSourceFilename, const std::string& strTargetFilename, const std::string& strTempDir, MasterController* pMasterController)
{

  pMasterController->DebugOut()->Message("NRRDConverter::Convert","Attempting to convertet NRRD dataset %s to %s", strSourceFilename.c_str(), strTargetFilename.c_str());

  // Check Magic value in NRRD File first
	ifstream fileData(strSourceFilename.c_str());	
  string strFirstLine;

	if (fileData.is_open())
	{
		getline (fileData,strFirstLine);
    if (strFirstLine.substr(0,7) != "NRRD000") {
      pMasterController->DebugOut()->Warning("NRRDConverter::Convert","The file %s is not a NRRD file (missing magic)", strSourceFilename.c_str());
      return false;
    }
  } else {
    pMasterController->DebugOut()->Warning("NRRDConverter::Convert","Could not open NRHD file %s", strSourceFilename.c_str());
    return false;
  }
  fileData.close();


  // read data
  UINT64			  iComponentSize=8;
  UINT64			  iComponentCount=1;
  bool          bSigned=false;
  bool          bBigEndian=false;
  UINTVECTOR3		vVolumeSize;
  FLOATVECTOR3	vVolumeAspect(1,1,1);
  string        strRAWFile;

  KeyValueFileParser parser(strSourceFilename);

  if (!parser.FileReadable()) {
    pMasterController->DebugOut()->Warning("NRRDConverter::Convert","Could not open NRHD file %s", strSourceFilename.c_str());
    return false;
  }

  KeyValPair* kvpType = parser.GetData("TYPE");
  if (kvpType == NULL) {
    pMasterController->DebugOut()->Error("NRRDConverter::Convert","Could not open find token \"type\" in file %s", strSourceFilename.c_str());
	  return false;
  } else {
	  if (kvpType->strValueUpper == "SIGNED CHAR" || kvpType->strValueUpper == "INT8" || kvpType->strValueUpper == "INT8_T") {
		  bSigned = true;
      iComponentSize = 8;
	  } else if (kvpType->strValueUpper == "UCHAR" || kvpType->strValueUpper == "UNSIGNED CHAR" ||  kvpType->strValueUpper == "UINT8" || kvpType->strValueUpper == "UINT8_T") {
		  bSigned = false;
      iComponentSize = 8;
	  } else if (kvpType->strValueUpper == "SHORT" || kvpType->strValueUpper == "SHORT INT" ||  kvpType->strValueUpper == "SIGNED SHORT" || kvpType->strValueUpper == "SIGNED SHORT INT" || kvpType->strValueUpper == "INT16" || kvpType->strValueUpper == "INT16_T") {
		  bSigned = true;
		  iComponentSize = 16;
	  } else if (kvpType->strValueUpper == "USHORT" || kvpType->strValueUpper == "UNSIGNED SHORT" || kvpType->strValueUpper == "UNSIGNED SHORT INT" || kvpType->strValueUpper == "UINT16" || kvpType->strValueUpper == "UINT16_T") {
		  bSigned = false;
		  iComponentSize = 16;
	  } else if (kvpType->strValueUpper == "INT" || kvpType->strValueUpper == "SIGNED INT" || kvpType->strValueUpper == "INT32" || kvpType->strValueUpper == "INT32_T") {
		  bSigned = true;
		  iComponentSize = 32;
	  } else if (kvpType->strValueUpper == "UINT" || kvpType->strValueUpper == "UNSIGNED INT" || kvpType->strValueUpper == "UINT32" || kvpType->strValueUpper == "UINT32_T") {
		  bSigned = false;
		  iComponentSize = 32;
	  } else if (kvpType->strValueUpper == "LONGLONG" || kvpType->strValueUpper == "LONG LONG" || kvpType->strValueUpper == "LONG LONG INT" || kvpType->strValueUpper == "SIGNED LONG LONG" || kvpType->strValueUpper == "SIGNED LONG LONG INT" || kvpType->strValueUpper == "INT64" || kvpType->strValueUpper == "INT64_T") {
		  bSigned = true;
		  iComponentSize = 64;
	  } else if (kvpType->strValueUpper == "ULONGLONG" || kvpType->strValueUpper == "UNSIGNED LONG LONG" || kvpType->strValueUpper == "UNSIGNED LONG LONG INT" || kvpType->strValueUpper == "UINT64" || kvpType->strValueUpper == "UINT64_T") {
		  bSigned = true;
		  iComponentSize = 64;
	  } else if (kvpType->strValueUpper == "FLOAT") {
		  bSigned = true;
		  iComponentSize = 32;
	  } else if (kvpType->strValueUpper == "DOUBLE") {
		  bSigned = true;
		  iComponentSize = 64;
	  } else {
      pMasterController->DebugOut()->Error("NRRDConverter::Convert","Unsupported \"type\" in file %s", strSourceFilename.c_str());
	    return false;
    }
  }

  KeyValPair* kvpDim = parser.GetData("DIMENSION");
  if (kvpDim == NULL) {
    pMasterController->DebugOut()->Error("NRRDConverter::Convert","Could not open find token \"dimension\" in file %s", strSourceFilename.c_str());
	  return false;
  } else {
    if (kvpDim->iValue != 3)  {
      pMasterController->DebugOut()->Error("NRRDConverter::Convert","Only 3D NRRDs are supported at the moment");
	    return false;
     }
  }

  KeyValPair* kvpSizes = parser.GetData("SIZES");
  if (kvpSizes == NULL) {
    pMasterController->DebugOut()->Error("NRRDConverter::Convert","Could not open find token \"sizes\" in file %s", strSourceFilename.c_str());
	  return false;
  } else {
    vVolumeSize = kvpSizes->vuiValue;
  }

  KeyValPair* kvpDataFile = parser.GetData("DATA FILE");
  if (kvpDataFile == NULL) {
    pMasterController->DebugOut()->Error("NRRDConverter::Convert","Could not open find token \"data file\" in file %s", strSourceFilename.c_str());
	  return false;
  } else {
    strRAWFile = SysTools::GetPath(strSourceFilename) + kvpDataFile->strValue;
  }
  
  KeyValPair* kvpSpacings = parser.GetData("SPACINGS");
  if (kvpSpacings != NULL) {
    vVolumeAspect = kvpSpacings->vfValue;
  }

  KeyValPair* kvpEndian = parser.GetData("ENDIAN");
  if (kvpEndian != NULL && kvpEndian->strValueUpper == "BIG") bBigEndian = true;

  KeyValPair* kvpEncoding = parser.GetData("ENCODING");
  if (kvpEncoding == NULL) {
    pMasterController->DebugOut()->Error("NRRDConverter::Convert","Could not find token \"encoding\" in file %s", strSourceFilename.c_str());
	  return false;
  } else {
    if (kvpEncoding->strValueUpper == "RAW")  {
      pMasterController->DebugOut()->Message("NRRDConverter::Convert","NRRD data is in RAW format!");

      return ConvertRAWDataset(strRAWFile, strTargetFilename, strTempDir, pMasterController, 0, iComponentSize, iComponentCount, bSigned, bBigEndian != EndianConvert::IsBigEndian(),
                               vVolumeSize, vVolumeAspect, "NRRD data", SysTools::GetFilename(strSourceFilename));
    } else
    if (kvpEncoding->strValueUpper == "TXT" || kvpEncoding->strValueUpper == "TEXT" || kvpEncoding->strValueUpper == "ASCII")  {
      pMasterController->DebugOut()->Error("NRRDConverter::Convert","NRRD data is in text format which is not supported at the moment.");
    } else
    if (kvpEncoding->strValueUpper != "HEX")  {
      pMasterController->DebugOut()->Error("NRRDConverter::Convert","NRRD data is in haxdecimal text format which is not supported at the moment.");
    } else
    if (kvpEncoding->strValueUpper != "GZ" || kvpEncoding->strValueUpper != "GZIP")  {
      pMasterController->DebugOut()->Message("NRRDConverter::Convert","NRRD data is GZIP compressed RAW format.");

      return ConvertGZIPDataset(strRAWFile, strTargetFilename, strTempDir, pMasterController, 0, iComponentSize, iComponentCount, bSigned, bBigEndian != EndianConvert::IsBigEndian(),
                               vVolumeSize, vVolumeAspect, "NRRD data", SysTools::GetFilename(strSourceFilename));

    } else
    if (kvpEncoding->strValueUpper != "BZ" || kvpEncoding->strValueUpper != "BZIP2")  {
      pMasterController->DebugOut()->Message("NRRDConverter::Convert","NRRD data is BZIP2 compressed RAW format.");

      return ConvertBZIP2Dataset(strRAWFile, strTargetFilename, strTempDir, pMasterController, 0, iComponentSize, iComponentCount, bSigned, bBigEndian != EndianConvert::IsBigEndian(),
                               vVolumeSize, vVolumeAspect, "NRRD data", SysTools::GetFilename(strSourceFilename));

    } else {
      pMasterController->DebugOut()->Error("NRRDConverter::Convert","NRRD data is in unknown \"%s\" format.");
    }
    return false;
  }
}
