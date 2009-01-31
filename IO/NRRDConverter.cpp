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
  \author  Jens Krueger
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
  m_vConverterDesc = "Nearly Raw Raster Data";
  m_vSupportedExt.push_back("NRRD");
  m_vSupportedExt.push_back("NHDR");
}

bool NRRDConverter::Convert(const std::string& strSourceFilename, const std::string& strTargetFilename, const std::string& strTempDir, MasterController* pMasterController, bool)
{

  pMasterController->DebugOut()->Message("NRRDConverter::Convert","Attempting to convert NRRD dataset %s to %s", strSourceFilename.c_str(), strTargetFilename.c_str());

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
    pMasterController->DebugOut()->Warning("NRRDConverter::Convert",
                                           "Could not open NRRD file %s",
                                           strSourceFilename.c_str());
    return false;
  }
  fileData.close();

  // read data
  UINT64        iComponentSize=8;
  UINT64        iComponentCount=1;
  bool          bSigned=false;
  bool          bBigEndian=false;
  UINTVECTOR3   vVolumeSize(1,1,1);
  FLOATVECTOR3  vVolumeAspect(1,1,1);
  string        strRAWFile;

  string strExt = SysTools::ToUpperCase(SysTools::GetExt(strSourceFilename));

  KeyValueFileParser parser(strSourceFilename, true);

  if (!parser.FileReadable()) {
    pMasterController->DebugOut()->Warning("NRRDConverter::Convert","Could not open NRRD file %s", strSourceFilename.c_str());
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

  KeyValPair* kvpSizes = parser.GetData("SIZES");
  if (kvpSizes == NULL) {
    pMasterController->DebugOut()->Error("NRRDConverter::Convert","Could not open find token \"sizes\" in file %s", strSourceFilename.c_str());
    return false;
  } else {

    size_t j = 0;
    for (size_t i = 0;i<kvpSizes->vuiValue.size();i++) {
      if (kvpSizes->vuiValue[i] > 1) {
        if (j>2) {
          pMasterController->DebugOut()->Error("NRRDConverter::Convert","Only 3D NRRDs are supported at the moment");
          return false;
        }
        vVolumeSize[j] = kvpSizes->vuiValue[i];
        j++;
      }
    }
  }

  KeyValPair* kvpDim = parser.GetData("DIMENSION");
  if (kvpDim == NULL) {
    pMasterController->DebugOut()->Error("NRRDConverter::Convert","Could not open find token \"dimension\" in file %s", strSourceFilename.c_str());
    return false;
  } else {
    if (kvpDim->iValue < 3)  {
      pMasterController->DebugOut()->Warning("NRRDConverter::Convert","The dimension of this NRRD file is less than three.");
    }
    if (kvpDim->iValue > 3)  {
      pMasterController->DebugOut()->Warning("NRRDConverter::Convert","The dimension of this NRRD file is more than three.");
    }
  }

  UINT64 iHeaderSkip;
  bool bDetachedHeader;

  KeyValPair* kvpDataFile1 = parser.GetData("DATA FILE");
  KeyValPair* kvpDataFile2 = parser.GetData("DATAFILE");
  if (kvpDataFile1 == NULL && kvpDataFile2 == NULL) {
    iHeaderSkip = UINT64(parser.GetStopPos());
    strRAWFile = strSourceFilename;
    bDetachedHeader = false;
  } else {
    if (kvpDataFile1 && kvpDataFile2 && kvpDataFile1->strValue != kvpDataFile2->strValue) 
      pMasterController->DebugOut()->Warning("NRRDConverter::Convert", "Found different 'data file' and 'datafiel' fields, using 'datafile'.");

    if (kvpDataFile1) strRAWFile = SysTools::GetPath(strSourceFilename) + kvpDataFile1->strValue;
    if (kvpDataFile2) strRAWFile = SysTools::GetPath(strSourceFilename) + kvpDataFile2->strValue;
    
    iHeaderSkip = 0;
    bDetachedHeader = true;
  }

  KeyValPair* kvpSpacings = parser.GetData("SPACINGS");
  if (kvpSpacings != NULL) {
    size_t j = 0;
    for (size_t i = 0;i<kvpSizes->vuiValue.size();i++) {
      if (kvpSpacings->vfValue.size() <= i) break;
      if (kvpSizes->vuiValue[i] > 1) {
        vVolumeAspect[j] = kvpSpacings->vfValue[i];
        j++;
      }
    }
  }
  
  int iLineSkip = 0;
  int iByteSkip = 0;
  KeyValPair* kvpLineSkip1 = parser.GetData("LINE SKIP");
  if (kvpLineSkip1 != NULL) iLineSkip = kvpLineSkip1->iValue;

  KeyValPair* kvpLineSkip2 = parser.GetData("LINESKIP");
  if (kvpLineSkip2 != NULL) {
    if (kvpLineSkip1 != NULL && iLineSkip != kvpLineSkip2->iValue) 
      pMasterController->DebugOut()->Warning("NRRDConverter::Convert", "Found different 'line skip' and 'lineskip' fields, using 'lineskip'.");
    iLineSkip = kvpLineSkip2->iValue;
  }

  KeyValPair* kvpByteSkip1 = parser.GetData("BYTE SKIP");
  if (kvpByteSkip1 != NULL) iByteSkip = kvpByteSkip1->iValue;

  KeyValPair* kvpByteSkip2 = parser.GetData("BYTESKIP");
  if (kvpByteSkip2 != NULL) {
    if (kvpByteSkip1 != NULL && iByteSkip != kvpByteSkip2->iValue) 
      pMasterController->DebugOut()->Warning("NRRDConverter::Convert", "Found different 'byte skip' and 'byteskip' fields, using 'byteskip'.");
    iByteSkip = kvpByteSkip2->iValue;
  }

  if (iLineSkip < 0) {
    pMasterController->DebugOut()->Warning("NRRDConverter::Convert", "Negative 'line skip' found, ignoring.");
    iLineSkip = 0;
  }

  if (iByteSkip == -1 && iLineSkip != 0) {
    pMasterController->DebugOut()->Warning("NRRDConverter::Convert", "'byte skip' = -1 'line skip' found, ignoring 'line skip'.");
    iLineSkip = 0;
  }

  int iLineSkipBytes = 0;
  if (iLineSkip != 0) {
    ifstream fileData(strRAWFile.c_str(),ios::binary);  
    string line;
    if (fileData.is_open()) {
      int i = 0;
      while (! fileData.eof() ) {
        getline (fileData,line);
        i++;
        if (i == iLineSkip) {
          iLineSkipBytes = fileData.tellg();
          break;
        }
      }
      if (iLineSkipBytes == 0) 
        pMasterController->DebugOut()->Warning("NRRDConverter::Convert", "Invalid 'line skip', file to short, ignoring 'line skip'.");
      fileData.close();
    } else {
        pMasterController->DebugOut()->Warning("NRRDConverter::Convert", "Unable to open target file, ignoring 'line skip'.");
    }
  }

  KeyValPair* kvpEndian = parser.GetData("ENDIAN");
  if (kvpEndian != NULL && kvpEndian->strValueUpper == "BIG") bBigEndian = true;

  KeyValPair* kvpEncoding = parser.GetData("ENCODING");
  if (kvpEncoding == NULL) {
    pMasterController->DebugOut()->Error("NRRDConverter::Convert",
                                         "Could not find token \"encoding\""
                                         " in file %s",
                                         strSourceFilename.c_str());
    return false;
  } else {
    if (kvpEncoding->strValueUpper == "RAW")  {
      pMasterController->DebugOut()->Message("NRRDConverter::Convert","NRRD data is in RAW format!");

      if (iByteSkip == -1) {
        LargeRAWFile f(strRAWFile);
        f.Open(false);
        UINT64 iSize = f.GetCurrentSize();
        f.Close();

        iHeaderSkip = iSize - (iComponentSize/8 * iComponentCount * UINT64(vVolumeSize.volume()));
      } else {
        if (iByteSkip != 0) {
          if (bDetachedHeader) 
            iHeaderSkip = iByteSkip;
          else {
            pMasterController->DebugOut()->Warning("NRRDConverter::Convert", "Skip value in attached header found.");
            iHeaderSkip += iByteSkip;
          }
        }
      }

      iHeaderSkip += iLineSkipBytes;

      return ConvertRAWDataset(strRAWFile, strTargetFilename, strTempDir, pMasterController, iHeaderSkip, iComponentSize, iComponentCount, bBigEndian != EndianConvert::IsBigEndian(), bSigned,
                               vVolumeSize, vVolumeAspect, "NRRD data", SysTools::GetFilename(strSourceFilename));
    } else {
      if (iByteSkip == -1) 
        pMasterController->DebugOut()->Warning("NRRDConverter::Convert", "Found illegal -1 'byte skip' in non RAW mode, ignoring 'byte skip'.");

      if (kvpEncoding->strValueUpper == "TXT" || kvpEncoding->strValueUpper == "TEXT" || kvpEncoding->strValueUpper == "ASCII")  {
        pMasterController->DebugOut()->Message("NRRDConverter::Convert","NRRD data is plain textformat.");

        return ConvertTXTDataset(strRAWFile, strTargetFilename, strTempDir, pMasterController, iHeaderSkip, iComponentSize, iComponentCount, bSigned,
                                 vVolumeSize, vVolumeAspect, "NRRD data", SysTools::GetFilename(strSourceFilename));
      } else
      if (kvpEncoding->strValueUpper == "HEX")  {
        pMasterController->DebugOut()->Error("NRRDConverter::Convert","NRRD data is in haxdecimal text format which is not supported at the moment.");
      } else
      if (kvpEncoding->strValueUpper == "GZ" || kvpEncoding->strValueUpper == "GZIP")  {
        pMasterController->DebugOut()->Message("NRRDConverter::Convert","NRRD data is GZIP compressed RAW format.");

        return ConvertGZIPDataset(strRAWFile, strTargetFilename, strTempDir, pMasterController, iHeaderSkip, iComponentSize, iComponentCount, bBigEndian != EndianConvert::IsBigEndian(), bSigned,
                                 vVolumeSize, vVolumeAspect, "NRRD data", SysTools::GetFilename(strSourceFilename));

      } else
      if (kvpEncoding->strValueUpper == "BZ" || kvpEncoding->strValueUpper == "BZIP2")  {
        pMasterController->DebugOut()->Message("NRRDConverter::Convert","NRRD data is BZIP2 compressed RAW format.");

        return ConvertBZIP2Dataset(strRAWFile, strTargetFilename, strTempDir, pMasterController, iHeaderSkip, iComponentSize, iComponentCount, bBigEndian != EndianConvert::IsBigEndian(), bSigned,
                                 vVolumeSize, vVolumeAspect, "NRRD data", SysTools::GetFilename(strSourceFilename));

      } else {
        pMasterController->DebugOut()->Error("NRRDConverter::Convert","NRRD data is in unknown \"%s\" format.");
      }
    }
    return false;
  }
}
