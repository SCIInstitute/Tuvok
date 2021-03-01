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

#include <fstream>
#include "NRRDConverter.h"
#include <Controller/Controller.h>
#include <Basics/SysTools.h>
#include <IO/KeyValueFileParser.h>
#include <cctype>
#include "TuvokIOError.h"

using namespace std;


NRRDConverter::NRRDConverter()
{
  m_vConverterDesc = L"Nearly Raw Raster Data";
  m_vSupportedExt.push_back(L"NRRD");
  m_vSupportedExt.push_back(L"NHDR");
}

bool NRRDConverter::ConvertToRAW(const std::wstring& strSourceFilename,
                                 const std::wstring& strTempDir, bool,
                                 uint64_t& iHeaderSkip,
                                 unsigned& iComponentSize,
                                 uint64_t& iComponentCount,
                                 bool& bConvertEndianess, bool& bSigned,
                                 bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                                 FLOATVECTOR3& vVolumeAspect,
                                 std::wstring& strTitle,
                                 std::wstring& strIntermediateFile,
                                 bool& bDeleteIntermediateFile)
{
  MESSAGE("Attempting to convert NRRD dataset %s", SysTools::toNarrow(strSourceFilename).c_str());

  // Check Magic value in NRRD File first
  ifstream ffileData(SysTools::toNarrow(strSourceFilename).c_str());
  string strFirstLine;

  if (ffileData.is_open())
  {
    getline (ffileData,strFirstLine);
    if (strFirstLine.substr(0,7) != "NRRD000") {
      WARNING("The file %s is not a NRRD file (missing magic)", SysTools::toNarrow(strSourceFilename).c_str());
      return false;
    }
  } else {
    WARNING("Could not open NRRD file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  }
  ffileData.close();

  // init data
  strTitle = L"NRRD data";
  iComponentSize=8;
  iComponentCount=1;
  bSigned=false;
  bool bBigEndian=false;
  vVolumeSize = UINT64VECTOR3(1,1,1);
  vVolumeAspect = FLOATVECTOR3(1,1,1);
  std::wstring strRAWFile;
  bDeleteIntermediateFile = false;

  // read data
  KeyValueFileParser parser(strSourceFilename, true);

  if (!parser.FileReadable()) {
    WARNING("Could not open NRRD file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  }

  KeyValPair* kvpType = parser.GetData("TYPE");
  if (kvpType == NULL) {
    T_ERROR("Could not open find token \"type\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    if (kvpType->strValueUpper == "SIGNED CHAR" ||
        kvpType->strValueUpper == "INT8" || kvpType->strValueUpper == "INT8_T") {
      bSigned = true;
      bIsFloat = false;
      iComponentSize = 8;
    } else if (kvpType->strValueUpper == "UCHAR" ||
               kvpType->strValueUpper == "UNSIGNED CHAR" ||
               kvpType->strValueUpper == "UINT8" ||
               kvpType->strValueUpper == "UINT8_T") {
      bSigned = false;
      bIsFloat = false;
      iComponentSize = 8;
    } else if (kvpType->strValueUpper == "SHORT" ||
               kvpType->strValueUpper == "SHORT INT" ||
               kvpType->strValueUpper == "SIGNED SHORT" ||
               kvpType->strValueUpper == "SIGNED SHORT INT" ||
               kvpType->strValueUpper == "INT16" ||
               kvpType->strValueUpper == "INT16_T") {
      bSigned = true;
      bIsFloat = false;
      iComponentSize = 16;
    } else if (kvpType->strValueUpper == "USHORT" ||
               kvpType->strValueUpper == "UNSIGNED SHORT" ||
               kvpType->strValueUpper == "UNSIGNED SHORT INT" ||
               kvpType->strValueUpper == "UINT16" ||
               kvpType->strValueUpper == "UINT16_T") {
      bSigned = false;
      bIsFloat = false;
      iComponentSize = 16;
    } else if (kvpType->strValueUpper == "INT" ||
               kvpType->strValueUpper == "SIGNED INT" ||
               kvpType->strValueUpper == "INT32" ||
               kvpType->strValueUpper == "INT32_T") {
      bSigned = true;
      bIsFloat = false;
      iComponentSize = 32;
    } else if (kvpType->strValueUpper == "UINT" ||
               kvpType->strValueUpper == "UNSIGNED INT" ||
               kvpType->strValueUpper == "UINT32" ||
               kvpType->strValueUpper == "UINT32_T") {
      bSigned = false;
      bIsFloat = false;
      iComponentSize = 32;
    } else if (kvpType->strValueUpper == "LONGLONG" ||
               kvpType->strValueUpper == "LONG LONG" ||
               kvpType->strValueUpper == "LONG LONG INT" ||
               kvpType->strValueUpper == "SIGNED LONG LONG" ||
               kvpType->strValueUpper == "SIGNED LONG LONG INT" ||
               kvpType->strValueUpper == "INT64" ||
               kvpType->strValueUpper == "INT64_T") {
      bSigned = true;
      bIsFloat = false;
      iComponentSize = 64;
    } else if (kvpType->strValueUpper == "ULONGLONG" ||
               kvpType->strValueUpper == "UNSIGNED LONG LONG" ||
               kvpType->strValueUpper == "UNSIGNED LONG LONG INT" ||
               kvpType->strValueUpper == "UINT64" ||
               kvpType->strValueUpper == "UINT64_T") {
      bSigned = true;
      bIsFloat = false;
      iComponentSize = 64;
    } else if (kvpType->strValueUpper == "FLOAT" ||
               kvpType->strValueUpper == "FLOAT32") {
      bSigned = true;
      bIsFloat = true;
      iComponentSize = 32;
      MESSAGE("Floating point nrrd.");
    } else if (kvpType->strValueUpper == "DOUBLE" ||
               kvpType->strValueUpper == "FLOAT64") {
      bSigned = true;
      bIsFloat = true;
      iComponentSize = 64;
      MESSAGE("Double-precision nrrd.");
    } else {
      T_ERROR("Unsupported \"type\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
      return false;
    }
  }

  KeyValPair* kvpSizes = parser.GetData("SIZES");
  if (kvpSizes == NULL) {
    T_ERROR("Could not open find token \"sizes\" in file %s", SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {

    size_t j = 0;
    for (size_t i = 0;i<kvpSizes->vuiValue.size();i++) {
      if (kvpSizes->vuiValue[i] > 1) {
        if (j>2) {
          T_ERROR("Only 3D NRRDs are supported at the moment");
          return false;
        }
        vVolumeSize[j] = kvpSizes->vuiValue[i];
        j++;
      }
    }
  }

  KeyValPair* kvpDim = parser.GetData("DIMENSION");
  if (kvpDim == NULL) {
    T_ERROR("Could not open find token \"dimension\" in file %s",
      SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    if (kvpDim->iValue < 3)  {
      WARNING("The dimension of this NRRD file is less than three.");
    }
    if (kvpDim->iValue > 3)  {
      WARNING("The dimension of this NRRD file is more than three.");
    }
  }

  bool bDetachedHeader;

  KeyValPair* kvpDataFile1 = parser.GetData("DATA FILE");
  KeyValPair* kvpDataFile2 = parser.GetData("DATAFILE");
  if (kvpDataFile1 == NULL && kvpDataFile2 == NULL) {
    iHeaderSkip = uint64_t(parser.GetStopPos());
    strRAWFile = strSourceFilename;
    bDetachedHeader = false;

    if (iHeaderSkip == 0) { // parser read the entire file and did not find a 
                            // data part (separated by an empty line)
      T_ERROR("NRRD file does neither specify detached data file nor does it "
              "contain data after the header, separated by an empty line.");
      return false;
    }


  } else {
    if (kvpDataFile1 && kvpDataFile2 && kvpDataFile1->strValue != kvpDataFile2->strValue)
      WARNING( "Found different 'data file' and 'datafile' fields, using 'datafile'.");

    if (kvpDataFile1) strRAWFile = SysTools::GetPath(strSourceFilename) + kvpDataFile1->wstrValue;
    if (kvpDataFile2) strRAWFile = SysTools::GetPath(strSourceFilename) + kvpDataFile2->wstrValue;

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
  
  try {
    KeyValPair* kvpSpaceDirs = parser.GetData("SPACE DIRECTIONS");
    if (kvpSpaceDirs != NULL) {
      std::vector<std::wstring> dirs = kvpSpaceDirs->vwstrValue;
      if (dirs.size() == 3) {
        for (size_t dim = 0;dim<3;dim++) {
          FLOATVECTOR3 axis;

          size_t iStart = 0;

          for (size_t vDim = 0;vDim<3;vDim++) {
            while (!isdigit(dirs[dim][iStart]) &&
                   '.' != dirs[dim][iStart] &&
                   '-' != dirs[dim][iStart] &&
                   'e' != dirs[dim][iStart]) {
              iStart++;
              if (iStart >= dirs[dim].length()) {
                throw tuvok::io::DSParseFailed("malformed 'space directions' tag.");
              }
            }

            size_t iEnd = iStart+1;
            while (isdigit(dirs[dim][iEnd]) ||
                   '.' == dirs[dim][iEnd] ||
                   '-' == dirs[dim][iEnd] ||
                   'e' == dirs[dim][iEnd]) {
              iEnd++;
              if (iEnd >= dirs[dim].length()) {
                throw tuvok::io::DSParseFailed("malformed 'space directions' tag.");
              }
            }
            
            axis[vDim] = SysTools::FromString<float>(dirs[dim].substr(iStart, 1+iStart-iEnd));

            iStart = iEnd;
          }
          vVolumeAspect[dim] *= axis.length();
        }
      } else {
        WARNING("Ignoring malformed 'space directions' tag.");
      }
    }
  } catch (const tuvok::io::DSParseFailed& err) {
    WARNING(err.what());
  }
  for(size_t i=0; i < 3; ++i) { // reset broken space directions parsing.
    if(vVolumeAspect[i] == 0.0) { vVolumeAspect[i] = 1.0f; }
  }


  int iLineSkip = 0;
  int iByteSkip = 0;
  KeyValPair* kvpLineSkip1 = parser.GetData("LINE SKIP");
  if (kvpLineSkip1 != NULL) iLineSkip = kvpLineSkip1->iValue;

  KeyValPair* kvpLineSkip2 = parser.GetData("LINESKIP");
  if (kvpLineSkip2 != NULL) {
    if (kvpLineSkip1 != NULL && iLineSkip != kvpLineSkip2->iValue)
      WARNING( "Found different 'line skip' and 'lineskip' fields, using 'lineskip'.");
    iLineSkip = kvpLineSkip2->iValue;
  }

  KeyValPair* kvpByteSkip1 = parser.GetData("BYTE SKIP");
  if (kvpByteSkip1 != NULL) iByteSkip = kvpByteSkip1->iValue;

  KeyValPair* kvpByteSkip2 = parser.GetData("BYTESKIP");
  if (kvpByteSkip2 != NULL) {
    if (kvpByteSkip1 != NULL && iByteSkip != kvpByteSkip2->iValue)
      WARNING( "Found different 'byte skip' and 'byteskip' fields, using 'byteskip'.");
    iByteSkip = kvpByteSkip2->iValue;
  }

  if (iLineSkip < 0) {
    WARNING( "Negative 'line skip' found, ignoring.");
    iLineSkip = 0;
  }

  if (iByteSkip == -1 && iLineSkip != 0) {
    WARNING( "'byte skip' = -1 'line skip' found, ignoring 'line skip'.");
    iLineSkip = 0;
  }

  std::streamoff iLineSkipBytes = 0;
  if (iLineSkip != 0) {
    ifstream fileData(SysTools::toNarrow(strRAWFile).c_str(),ios::binary);
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
        WARNING( "Invalid 'line skip', file to short, ignoring 'line skip'.");
      fileData.close();
    } else {
        WARNING( "Unable to open target file, ignoring 'line skip'.");
    }
  }

  KeyValPair* kvpEndian = parser.GetData("ENDIAN");
  if (kvpEndian != NULL && kvpEndian->strValueUpper == "BIG") bBigEndian = true;

  bConvertEndianess = bBigEndian != EndianConvert::IsBigEndian();

  KeyValPair* kvpEncoding = parser.GetData("ENCODING");
  if (kvpEncoding == NULL) {
    T_ERROR("Could not find token \"encoding\" in file %s",
      SysTools::toNarrow(strSourceFilename).c_str());
    return false;
  } else {
    if (kvpEncoding->strValueUpper == "RAW")  {
      MESSAGE("NRRD data is in RAW format!");

      if (iByteSkip == -1) {
        LargeRAWFile f(strRAWFile);
        f.Open(false);
        uint64_t iSize = f.GetCurrentSize();
        f.Close();

        iHeaderSkip = iSize - (iComponentSize/8 * iComponentCount * uint64_t(vVolumeSize.volume()));
      } else {
        if (iByteSkip != 0) {
          if (bDetachedHeader)
            iHeaderSkip = iByteSkip;
          else {
            WARNING( "Skip value in attached header found.");
            iHeaderSkip += iByteSkip;
          }
        }
      }

      iHeaderSkip += iLineSkipBytes;

      strIntermediateFile = strRAWFile;
      bDeleteIntermediateFile = false;
      return true;
    } else {
      if (iByteSkip == -1)
        WARNING( "Found illegal -1 'byte skip' in non RAW mode, ignoring 'byte skip'.");

      if (kvpEncoding->strValueUpper == "TXT" || kvpEncoding->strValueUpper == "TEXT" || kvpEncoding->strValueUpper == "ASCII")  {
        MESSAGE("NRRD data is plain textformat.");

        wstring strBinaryFile = strTempDir+SysTools::GetFilename(strSourceFilename)+L".binary";
        bool bResult = ParseTXTDataset(strRAWFile, strBinaryFile, iHeaderSkip, iComponentSize, iComponentCount, bSigned, bIsFloat, vVolumeSize);
        strIntermediateFile = strBinaryFile;
        bDeleteIntermediateFile = true;
        iHeaderSkip = 0;
        bConvertEndianess = false;
        return bResult;
      } else
      if (kvpEncoding->strValueUpper == "HEX")  {
        T_ERROR("NRRD data is in hexadecimal text format which is not supported at the moment.");
      } else
      if (kvpEncoding->strValueUpper == "GZ" || kvpEncoding->strValueUpper == "GZIP")  {
        MESSAGE("NRRD data is GZIP compressed RAW format.");

        wstring strUncompressedFile = strTempDir+SysTools::GetFilename(strSourceFilename)+L".uncompressed";
        bool bResult = ExtractGZIPDataset(strRAWFile, strUncompressedFile, iHeaderSkip);
        strIntermediateFile = strUncompressedFile;
        bDeleteIntermediateFile = true;
        iHeaderSkip = 0;
        return bResult;
      } else
      if (kvpEncoding->strValueUpper == "BZ" || kvpEncoding->strValueUpper == "BZIP2")  {
        MESSAGE("NRRD data is BZIP2 compressed RAW format.");

        wstring strUncompressedFile = strTempDir+SysTools::GetFilename(strSourceFilename)+L".uncompressed";
        bool bResult = ExtractBZIP2Dataset(strRAWFile, strUncompressedFile, iHeaderSkip);
        strIntermediateFile = strUncompressedFile;
        bDeleteIntermediateFile = true;
        iHeaderSkip = 0;
        return bResult;
      } else {
        T_ERROR("NRRD data is in unknown \"%s\" format.");
      }
    }
    return false;
  }
}

bool NRRDConverter::ConvertToNative(const std::wstring& strRawFilename, const std::wstring& strTargetFilename, uint64_t iHeaderSkip,
                             unsigned iComponentSize, uint64_t iComponentCount, bool bSigned, bool bFloatingPoint,
                             UINT64VECTOR3 vVolumeSize,FLOATVECTOR3 vVolumeAspect, bool bNoUserInteraction,
                             const bool bQuantizeTo8Bit) {

  bool bDetached = SysTools::ToLowerCase(SysTools::GetExt(strTargetFilename)) == L"nhdr";

  // compute fromat string
  string strFormat;

  bool bFormatOK = true;
  if (bFloatingPoint) {
    if (bSigned) {
      switch (iComponentSize) {
        case 32 : strFormat = "FLOAT"; break;
        case 64 : strFormat = "DOUBLE"; break;
        default : bFormatOK = false; break;
      }
    } else {
      bFormatOK = false;
    }
  } else {
    if (bSigned) {
      switch (iComponentSize) {
        case  8 : strFormat = "SIGNED CHAR"; break;
        case 16 : strFormat = "SHORT"; break;
        case 32 : strFormat = "INT"; break;
        case 64 : strFormat = "LONGLONG"; break;
        default : bFormatOK = false; break;
      }
    } else {
      switch (iComponentSize) {
        case  8 : strFormat = "UCHAR"; break;
        case 16 : strFormat = "USHORT"; break;
        case 32 : strFormat = "UINT"; break;
        case 64 : strFormat = "ULONGLONG"; break;
        default : bFormatOK = false; break;
      }
    }
  }

  if (!bFormatOK) {
    T_ERROR("This data type is not supported by NRRD files.");
    return false;
  }

  // create header textfile from metadata

  ofstream fAsciiTarget(SysTools::toNarrow(strTargetFilename).c_str());
  if (!fAsciiTarget.is_open()) {
    T_ERROR("Unable to open target file %s.", SysTools::toNarrow(strTargetFilename).c_str());
    return false;
  }

  fAsciiTarget << "NRRD0001" << endl;
  fAsciiTarget << "type: " << strFormat << endl;
  fAsciiTarget << "dimension: 3" << endl;
  fAsciiTarget << "sizes:     " << vVolumeSize.x << " " << vVolumeSize.y << " "<< vVolumeSize.z << endl;
  fAsciiTarget << "spacings: " << vVolumeAspect.x << " " << vVolumeAspect.y << " "<< vVolumeAspect.z << endl;
  fAsciiTarget << "endian: little" << endl;
  fAsciiTarget << "encoding: raw" << endl;

  if (bDetached) {
    wstring strTargetRAWFilename = strTargetFilename+L".raw";
    fAsciiTarget << "data file: ./" << SysTools::toNarrow(SysTools::GetFilename(strTargetRAWFilename)) << endl;
    fAsciiTarget.close();

    // copy RAW file using the parent's call
    bool bRAWSuccess = RAWConverter::ConvertToNative(strRawFilename, strTargetRAWFilename, iHeaderSkip,
                                                     iComponentSize, iComponentCount, bSigned, bFloatingPoint,
                                                     vVolumeSize, vVolumeAspect, bNoUserInteraction, bQuantizeTo8Bit);

    if (bRAWSuccess) {
      return true;
    } else {
      T_ERROR("Error creating raw target file %s.", SysTools::toNarrow(strTargetRAWFilename).c_str());
      SysTools::RemoveFile(strTargetFilename);
      return false;
    }

  } else {
    // add the "empty line" header delimiter
    fAsciiTarget << endl;
    fAsciiTarget.close();

    // append RAW data using the parent's call
    bool bRAWSuccess = AppendRAW(strRawFilename, iHeaderSkip, strTargetFilename, iComponentSize, bQuantizeTo8Bit);

    if (bRAWSuccess) {
      return true;
    } else {
      T_ERROR("Error appending raw data to header file %s.",
        SysTools::toNarrow(strTargetFilename).c_str());
      SysTools::RemoveFile(strTargetFilename);
      return false;
    }
  }
}
