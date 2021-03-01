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
  \file    DICOMParser.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.2
  \date    September 2008
*/

#include <algorithm>
#include <functional>
#include <sys/stat.h>
#include <vector>
#include "DICOMParser.h"

#include <Controller/Controller.h>
#include <Basics/SysTools.h>

#ifdef DEBUG_DICOM
  #define DICOM_DBG(...) Console::printf(__VA_ARGS__)
  #include <sstream>
#else
  #define DICOM_DBG(...)
#endif

#ifdef DEBUG_DICOM
  #include <Basics/Console.h>
#endif

using namespace boost;

std::string DICOM_TypeStrings[28] = {
  "AE", // Application Entity string 16 bytes maximum
  "AS", // Age String string 4 bytes fixed
  "AT", // Attribute Tag string 4 bytes fixed
  "CS", // Code String string 16 bytes maximum
  "DA", // Date string 8 bytes fixed
  "DS", // Decimal String string 16 bytes maximum
  "DT", // Date Time string 26 bytes maximum
  "FL", // Floating Point Single binary 4 bytes fixed
  "FD", // Floating Point Double binary 8 bytes fixed
  "IS", // Integer String string 12 bytes maximum
  "LO", // Long String string 64 chars maximum
  "LT", // Long Text string 1024 chars maximum
  "OB", // Other Byte
  "OW", // Other Word
  "OF", // Other Float
  "PN", // Person Name string 64 chars maximum
  "SH", // Short String string 16 chars maximum
  "SL", // Signed Long binary 4 bytes fixed
  "SQ", // Sequence of Items - -
  "SS", // Signed Short binary 2 bytes fixed
  "ST", // Short Text string 1024 chars maximum
  "TM", // Time string 16 bytes maximum
  "UI", // Unique Identifier (UID) string 64 bytes maximum
  "UL", // Unsigned Long binary 4 bytes fixed
  "US", // Unsigned Short binary 2 bytes fixed
  "UT", // Unlimited Text string 232-2
  "UN", // Unknown
  "Implicit"
};

using namespace std;

DICOMParser::DICOMParser(void)
{
}

DICOMParser::~DICOMParser(void)
{
}

bool StacksSmaller ( FileStackInfo* pElem1, FileStackInfo* pElem2 )
{
  DICOMStackInfo* elem1 = (DICOMStackInfo*)pElem1;
  DICOMStackInfo* elem2 = (DICOMStackInfo*)pElem2;

  return elem1->m_iSeries < elem2->m_iSeries;
}


void DICOMParser::GetDirInfo(string  strDirectory) {
  vector<string> files = SysTools::GetDirContents(strDirectory);
  vector<DICOMFileInfo> fileInfos;

  // query directory for DICOM files
  for (size_t i = 0;i<files.size();i++) {
    MESSAGE("Looking for DICOM data in file %s", files[i].c_str());
    DICOMFileInfo info;
    if (GetDICOMFileInfo(files[i], info)) {
      fileInfos.push_back(info);
    }
  }

  // sort results into stacks
  for (size_t i = 0; i<m_FileStacks.size(); i++) delete m_FileStacks[i];
  m_FileStacks.clear();

  MESSAGE("%d files in candidate list.", fileInfos.size());
  // Ignore duplicate DICOMs.
  for (size_t i = 0; i<fileInfos.size(); i++) {
    bool bFoundMatch = false;
    for (size_t j = 0; j<m_FileStacks.size(); j++) {
      if (((DICOMStackInfo*)m_FileStacks[j])->Match(&fileInfos[i])) {
        MESSAGE("found match at %u(%s), dropping %u(%s) out.",
                static_cast<unsigned>(j), m_FileStacks[j]->m_strDesc.c_str(),
                static_cast<unsigned>(i), fileInfos[i].m_strDesc.c_str());
        bFoundMatch = true;
        break;
      }
    }
    if (!bFoundMatch) {
      DICOMStackInfo* newStack = new DICOMStackInfo(&fileInfos[i]);
      m_FileStacks.push_back(newStack);
    }
  }

  // sort stacks by sequence number
  sort( m_FileStacks.begin( ), m_FileStacks.end( ), StacksSmaller );

  // fix Z aspect ratio - which is broken in many DICOMs - using the patient position
  for (size_t i = 0; i<m_FileStacks.size(); i++) {
    if (m_FileStacks[i]->m_Elements.size() < 2) continue;
    float fZDistance = fabs(((SimpleDICOMFileInfo*)m_FileStacks[i]->m_Elements[1])->m_fvPatientPosition.z -
                            ((SimpleDICOMFileInfo*)m_FileStacks[i]->m_Elements[0])->m_fvPatientPosition.z);
    if (fZDistance != 0) m_FileStacks[i]->m_fvfAspect.z = fZDistance;
  }
}

void DICOMParser::GetDirInfo(wstring wstrDirectory) {
  string strDirectory(wstrDirectory.begin(), wstrDirectory.end());
  GetDirInfo(strDirectory);
}

void DICOMParser::ReadHeaderElemStart(ifstream& fileDICOM, short& iGroupID, short& iElementID, DICOM_eType& eElementType, uint32_t& iElemLength, bool bImplicit, bool bNeedsEndianConversion) {
  string typeString = "  ";

  fileDICOM.read((char*)&iGroupID,2);
  fileDICOM.read((char*)&iElementID,2);

  if (iGroupID == 0x2) {  // ignore input for meta block
    bImplicit = false;
    bNeedsEndianConversion = EndianConvert::IsBigEndian();
  }

  if (bNeedsEndianConversion) {
    iGroupID = EndianConvert::Swap<short>(iGroupID);
    iElementID = EndianConvert::Swap<short>(iElementID);
  }

  if (bImplicit) {
    eElementType = TYPE_Implicit;
    fileDICOM.read((char*)&iElemLength,4);
    if (bNeedsEndianConversion) iElemLength = EndianConvert::Swap<uint32_t>(iElemLength);
    DICOM_DBG("Reader read implict field iGroupID=%i, iElementID=%i, iElemLength=%i\n", int(iGroupID), int(iElementID), iElemLength);
  } else {
    fileDICOM.read(&typeString[0],2);
    short tmp;
    fileDICOM.read((char*)&tmp,2);
    if (bNeedsEndianConversion) tmp = EndianConvert::Swap<short>(tmp);
    iElemLength = tmp;
    eElementType = TYPE_UN;
    uint32_t i=0;
    for (;i<27;i++) {
      if (typeString == DICOM_TypeStrings[i]) {
        eElementType = DICOM_eType(i);
        break;
      }
    }
    if (i==27) {
      DICOM_DBG("WARNING: Reader could not interpret type %c%c (iGroupID=%i, iElementID=%i, iElemLength=%i)\n",typeString[0], typeString[1], int(iGroupID), int(iElementID), iElemLength);
    } else {
      DICOM_DBG("Read type %c%c field (iGroupID=%x (%i), iElementID= %x (%i), iElemLength=%i)\n",typeString[0], typeString[1], int(iGroupID), int(iGroupID), int(iElementID), int(iElementID), iElemLength);
    }
  }

  if ((eElementType == TYPE_OF || eElementType == TYPE_OW || eElementType == TYPE_OB || eElementType == TYPE_UT) && iElemLength == 0) {
    fileDICOM.read((char*)&iElemLength,4);
    if (bNeedsEndianConversion) iElemLength = EndianConvert::Swap<uint32_t>(iElemLength);
    DICOM_DBG("Reader found zero length %c%c field and read the length again which is now (iElemLength=%i)\n", typeString[0], typeString[1], iElemLength);
  }
}


uint32_t DICOMParser::GetUInt(ifstream& fileDICOM, const DICOM_eType eElementType, const uint32_t iElemLength, const bool bNeedsEndianConversion) {
  string value;
  uint32_t result;
  switch (eElementType) {
    case TYPE_Implicit :
    case TYPE_IS  : {
              ReadSizedElement(fileDICOM, value, iElemLength);
              result = atoi(value.c_str());
              break;
            }
    case TYPE_UL  : {
              fileDICOM.read((char*)&result,4);
              if (bNeedsEndianConversion) result = EndianConvert::Swap<uint32_t>(result);
              break;
            }
    case TYPE_US  : {
              short tmp;
              fileDICOM.read((char*)&tmp,2);
              if (bNeedsEndianConversion) tmp = EndianConvert::Swap<short>(tmp);
              result = tmp;
              break;
            }
    default : result = 0; break;
  }
  return result;
}


#ifdef DEBUG_DICOM
void DICOMParser::ParseUndefLengthSequence(ifstream& fileDICOM, short& iSeqGroupID, short& iSeqElementID, DICOMFileInfo& info, const bool bImplicit, const bool bNeedsEndianConversion, uint32_t iDepth) {
  for (int i = 0;i<int(iDepth)-1;i++) Console::printf("  ");
  Console::printf("iGroupID=%x iElementID=%x elementType=SEQUENCE (undef length)\n", iSeqGroupID, iSeqElementID);
#else
void DICOMParser::ParseUndefLengthSequence(ifstream& fileDICOM, short& , short& , DICOMFileInfo& info, const bool bImplicit, const bool bNeedsEndianConversion) {
#endif
  int iItemCount = 0;
  uint32_t iData;

  string value;
  short iGroupID, iElementID;
  DICOM_eType elementType;

  do {
    fileDICOM.read((char*)&iData,4);

    if (iData == 0xE000FFFE) {
      iItemCount++;
      fileDICOM.read((char*)&iData,4);
      #ifdef DEBUG_DICOM
        for (uint32_t i = 0;i<iDepth;i++) Console::printf("  ");
        Console::printf("START ITEM\n");
      #endif
    } else if (iData == 0xE00DFFFE) {
      iItemCount--;
      fileDICOM.read((char*)&iData,4);
      #ifdef DEBUG_DICOM
        for (uint32_t i = 0;i<iDepth;i++) Console::printf("  ");
        Console::printf("END ITEM\n");
      #endif
    } else if (iData != 0xE0DDFFFE) fileDICOM.seekg(-4, ios_base::cur);


    if (iItemCount > 0) {
      ReadHeaderElemStart(fileDICOM, iGroupID, iElementID, elementType, iData, bImplicit, bNeedsEndianConversion);

      if (elementType == TYPE_SQ) {
        fileDICOM.read((char*)&iData,4);
        if (iData == 0xFFFFFFFF) {
          #ifdef DEBUG_DICOM
          ParseUndefLengthSequence(fileDICOM, iGroupID, iElementID, info, bImplicit, bNeedsEndianConversion, 1);
          #else
          ParseUndefLengthSequence(fileDICOM, iGroupID, iElementID, info, bImplicit, bNeedsEndianConversion);
          #endif
        } else {
          // HACK: here we simply skip over the entire sequence
          value.resize(iData);
          fileDICOM.read(&value[0],iData);
          value = "SKIPPED EXPLICIT SEQUENCE";
        }
      } else {

        if (iData == 0xFFFFFFFF) {
          #ifdef DEBUG_DICOM
            ParseUndefLengthSequence(fileDICOM, iGroupID, iElementID, info, bImplicit, bNeedsEndianConversion, iDepth+1);
          #else
            ParseUndefLengthSequence(fileDICOM, iGroupID, iElementID, info, bImplicit, bNeedsEndianConversion);
          #endif
        } else {
          if(iData > 0) {
            value.resize(iData);
            fileDICOM.read(&value[0],iData);
            #ifdef DEBUG_DICOM
              for (uint32_t i = 0;i<iDepth;i++) Console::printf("  ");
              Console::printf("iGroupID=%x iElementID=%x elementType=%s value=%s\n", iGroupID, iElementID, DICOM_TypeStrings[int(elementType)].c_str(), value.c_str());
            #endif
          } else {
            #ifdef DEBUG_DICOM
              Console::printf("iGroupID=%x iElementID=%x elementType=%s value=empty\n", iGroupID, iElementID, DICOM_TypeStrings[int(elementType)].c_str());
            #endif
          }
        }
      }
    }
  } while (iData != 0xE0DDFFFE && !fileDICOM.eof());
  fileDICOM.read((char*)&iData,4);

#ifdef DEBUG_DICOM
  for (uint32_t i = 0;i<iDepth;i++) Console::printf("  ");
  Console::printf("END SEQUENCE\n");
#endif

}

void DICOMParser::ReadSizedElement(ifstream& fileDICOM, string& value, const uint32_t iElemLength) {
  value.resize(iElemLength);
  if (iElemLength) {
    fileDICOM.read(&value[0],iElemLength);
  }
}

void DICOMParser::SkipUnusedElement(ifstream& fileDICOM, string& value, const uint32_t iElemLength) {
  ReadSizedElement(fileDICOM, value, iElemLength);
}

bool DICOMParser::GetDICOMFileInfo(const string& strFilename,
                                   DICOMFileInfo& info) {
  DICOM_DBG("Processing file %s\n",strFilename.c_str());

  LARGE_STAT_BUFFER stat_buf;

  bool bImplicit    = false;
  info.m_bIsJPEGEncoded = false;
  bool bNeedsEndianConversion = EndianConvert::IsBigEndian();

  info.m_strFileName = strFilename;
  info.m_wstrFileName = wstring(strFilename.begin(), strFilename.end());
  info.m_ivSize.z = 1; // default if slices does not appear in the dicom

  // check for basic properties
  if (!SysTools::GetFileStats(strFilename, stat_buf)) {// file must exist
    MESSAGE("File '%s' can't be a DICOM -- doesn't exist.",
            strFilename.c_str());
    return false;
  }
  if (stat_buf.st_size < 128+4) { // file has minimum length ?
    MESSAGE("File '%s' can't be a DICOM -- too short.", strFilename.c_str());
    return false;
  }

  // open file
  ifstream fileDICOM(strFilename.c_str(), ios::in | ios::binary);
  fileDICOM.seekg(128);  // skip first 128 bytes

  string value;
#ifdef DEBUG_DICOM
  float fSliceSpacing = 0;
#endif
  short iGroupID, iElementID;
  uint32_t iElemLength;
  DICOM_eType elementType;  

  // check for DICM magic.
  char DICM[4];
  fileDICOM.read(DICM,4);
  if (DICM[0] != 'D' || DICM[1] != 'I' || DICM[2] != 'C' || DICM[3] != 'M') {
    MESSAGE("File '%s' does not contain DICM meta header.", strFilename.c_str());

    // DICOM supports files without the meta header, 
    // in that case you have to guess the parameters
    // we guess Implicit VR Little Endian as this is 
    // the most common type

    fileDICOM.seekg(0);
    bImplicit = true;

    ReadHeaderElemStart(fileDICOM, iGroupID, iElementID, elementType,
                        iElemLength, bImplicit, bNeedsEndianConversion);

    if (iGroupID != 0x08) {
      MESSAGE("File '%s' is not a DICM file.", strFilename.c_str());
      return false;
    }

  } else {
    // Ok, at this point we are very sure that we are dealing with a DICOM File,
    // lets find out the dimensions, the sequence numbers
#ifdef DEBUG_DICOM
    fSliceSpacing = 0;
#endif
    int iMetaHeaderEnd=0;
    bool bParsingMetaHeader = true;


    // read metadata block
    ReadHeaderElemStart(fileDICOM, iGroupID, iElementID, elementType,
                        iElemLength, bImplicit, info.m_bIsBigEndian);

    while (bParsingMetaHeader && iGroupID == 0x2 && !fileDICOM.eof()) {
      switch (iElementID) {
        case 0x0 : {  // File Meta Elements Group Len
              if (iElemLength != 4) {
                MESSAGE("Metaheader length field is invalid.");
                return false;
              }
              int iMetaHeaderLength;
              fileDICOM.read((char*)&iMetaHeaderLength,4);
              iMetaHeaderEnd  = iMetaHeaderLength + uint32_t(fileDICOM.tellg());
             } break;
        case 0x1 : {  // Version
              assert(iElemLength > 0);
              if(iElemLength == 0) { iElemLength = 1; } // guarantee progress.
              ReadSizedElement(fileDICOM, value, iElemLength);
             } break;
        case 0x10 : {  // Parse Type to find out endianess
              ReadSizedElement(fileDICOM, value, iElemLength);
              if (value[iElemLength-1] == 0) value.resize(iElemLength-1);

              if (value == "1.2.840.10008.1.2") {   // Implicit VR Little Endian
                bImplicit = true;
                bNeedsEndianConversion = EndianConvert::IsBigEndian();
                info.m_bIsBigEndian = false;
                DICOM_DBG("DICOM file is Implicit VR Little Endian\n");
              } else if (value == "1.2.840.10008.1.2.1") { // Explicit VR Little Endian
                bImplicit = false;
                bNeedsEndianConversion = EndianConvert::IsBigEndian();
                info.m_bIsBigEndian = false;
                DICOM_DBG("DICOM file is Explicit VR Little Endian\n");
              } else if (value == "1.2.840.10008.1.2.2") { // Explicit VR Big Endian
                bImplicit = false;
                bNeedsEndianConversion = EndianConvert::IsLittleEndian();
                info.m_bIsBigEndian = true;
                DICOM_DBG("DICOM file is Explicit VR Big Endian\n");
              } else if (value == "1.2.840.10008.1.2.4.50" ||   // JPEG Baseline            ( untested due to lack of example DICOMs)
                         value == "1.2.840.10008.1.2.4.51" ||   // JPEG Extended            ( untested due to lack of example DICOMs)
                         value == "1.2.840.10008.1.2.4.55" ||   // JPEG Progressive         ( untested due to lack of example DICOMs)
                         value == "1.2.840.10008.1.2.4.57" ||   // JPEG Lossless            ( untested due to lack of example DICOMs)
                         value == "1.2.840.10008.1.2.4.58" ||   // JPEG Lossless            ( untested due to lack of example DICOMs)
                         value == "1.2.840.10008.1.2.4.70" ||   // JPEG Lossless            ( untested due to lack of example DICOMs)
                         value == "1.2.840.10008.1.2.4.80" ||   // JPEG-LS Lossless         ( untested due to lack of example DICOMs)
                         value == "1.2.840.10008.1.2.4.81" ||   // JPEG-LS Near-lossless    ( untested due to lack of example DICOMs)
                         value == "1.2.840.10008.1.2.4.90" ||   // JPEG 2000 Lossless       ( untested due to lack of example DICOMs)
                         value == "1.2.840.10008.1.2.4.91" ) {  // JPEG 2000                ( untested due to lack of example DICOMs)
                info.m_bIsJPEGEncoded = true;
                bImplicit = false;
                bNeedsEndianConversion = EndianConvert::IsBigEndian();
                info.m_bIsBigEndian = false;
                DICOM_DBG("DICOM file is JPEG Explicit VR Big Endian\n");
              } else {
                WARNING("Unknown DICOM type '%s' -- not a DICOM? "
                        "Might just be something we haven't seen: please "
                        "send a debug log.", value.c_str());
                return false; // unsupported file format
              }
              fileDICOM.seekg(iMetaHeaderEnd, std::ios_base::beg);
              bParsingMetaHeader = false;
             } break;
        default : {
          SkipUnusedElement(fileDICOM, value, iElemLength);
        } break;
      }
      ReadHeaderElemStart(fileDICOM, iGroupID, iElementID, elementType,
                          iElemLength, bImplicit, bNeedsEndianConversion);
    }
  }

  do {
    if (elementType == TYPE_SQ) { // read explicit sequence
      fileDICOM.read((char*)&iElemLength,4);
      if (iElemLength == 0xFFFFFFFF) {
        #ifdef DEBUG_DICOM
        ParseUndefLengthSequence(fileDICOM, iGroupID, iElementID, info, false, bNeedsEndianConversion, 1);
        #else
        ParseUndefLengthSequence(fileDICOM, iGroupID, iElementID, info, false, bNeedsEndianConversion);
        #endif
        value = "SEQUENCE";
      } else {
        // HACK: here we simply skip over the entire sequence
        SkipUnusedElement(fileDICOM, value, iElemLength);
        value = "SKIPPED EXPLICIT SEQUENCE";
      }
    } else if (elementType == TYPE_Implicit && iElemLength == 0xFFFFFFFF) { // read implicit sequence
        #ifdef DEBUG_DICOM
        ParseUndefLengthSequence(fileDICOM, iGroupID, iElementID, info, true, bNeedsEndianConversion, 1);
        #else
        ParseUndefLengthSequence(fileDICOM, iGroupID, iElementID, info, true, bNeedsEndianConversion);
        #endif
      value = "SEQUENCE";
    } else {
      switch (iGroupID) {
        case 0x8 : switch (iElementID) {
              case 0x22 : { // Acquisition Date
                    info.m_strAcquDate.resize(iElemLength);
                    fileDICOM.read(&info.m_strAcquDate[0],iElemLength);
                    #ifdef DEBUG_DICOM
                    {
                      stringstream ss;
                      ss << info.m_strAcquDate << " (Acquisition Date: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    } break;
              case 0x32 : { // Acquisition Time
                    info.m_strAcquTime.resize(iElemLength);
                    fileDICOM.read(&info.m_strAcquTime[0],iElemLength);
                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_strAcquTime << " (Acquisition Time: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    } break;
              case 0x60 : { // Modality
                    info.m_strModality.resize(iElemLength);
                    fileDICOM.read(&info.m_strModality[0],iElemLength);
                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_strModality << " (Modality: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    } break;
              case 0x1030 : { // Study Description
                    info.m_strDesc.resize(iElemLength);
                    fileDICOM.read(&info.m_strDesc[0],iElemLength);
                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_strDesc << " (Study Description: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    } break;
              default : {
                SkipUnusedElement(fileDICOM, value, iElemLength);
              } break;
             } break;
        case 0x18 : switch (iElementID) {
              case 0x50 : { // Slice Thickness
                    ReadSizedElement(fileDICOM, value, iElemLength);
                    info.m_fvfAspect.z = float(atof(value.c_str()));
                    #ifdef DEBUG_DICOM
                    {
                      stringstream ss;
                      ss << info.m_fvfAspect.z << " (Slice Thinkness: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    } break;
              case 0x88 : { // Spacing
                    ReadSizedElement(fileDICOM, value, iElemLength);
                    #ifdef DEBUG_DICOM
                    fSliceSpacing = float(atof(value.c_str()));
                    {
                      stringstream ss;
                      ss << fSliceSpacing << " (Slice Spacing: recognized)";
                      value = ss.str();
                    }
                    #endif
                    } break;
               default : {
                SkipUnusedElement(fileDICOM, value, iElemLength);
              } break;
            }  break;
        case 0x20 : switch (iElementID) {
              case 0x11 : { // Series Number
                    info.m_iSeries = GetUInt(fileDICOM, elementType, iElemLength, bNeedsEndianConversion);
                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_iSeries << " (Series Number: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    } break;
              case 0x13 : { // Image Number
                    info.m_iImageIndex = GetUInt(fileDICOM, elementType, iElemLength, bNeedsEndianConversion);
                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_iImageIndex << " (Image Number: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    } break;
              case 0x32 : // patient position
                {
                    ReadSizedElement(fileDICOM, value, iElemLength);
                    size_t iDelimiter = value.find_first_of("\\");

                    info.m_fvPatientPosition.x = float(atof(value.substr(0,iDelimiter).c_str()));

                    value = value.substr(iDelimiter+1, value.length());
                    iDelimiter = value.find_first_of("\\");

                    info.m_fvPatientPosition.y = float(atof(value.substr(0,iDelimiter).c_str()));
                    info.m_fvPatientPosition.z = float(atof(value.substr(iDelimiter+1, value.length()).c_str()));

                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_fvPatientPosition.x << ", " <<
                            info.m_fvPatientPosition.y << ", " <<
                            info.m_fvPatientPosition.z <<
                            " (x,y,z Patient Position: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                }  break;
              default : {
                SkipUnusedElement(fileDICOM, value, iElemLength);
              } break;
             } break;
        case 0x28 : switch (iElementID) {
              case  0x2 : // component count
                    if (elementType == TYPE_Implicit) elementType = TYPE_US;
                    info.m_iComponentCount = GetUInt(fileDICOM, elementType, iElemLength, bNeedsEndianConversion);
                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_iComponentCount << " (samples per pixel: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    break;
              case  0x8 : // Slices
                    if (elementType == TYPE_Implicit) elementType = TYPE_IS;
                    info.m_ivSize.z = GetUInt(fileDICOM, elementType, iElemLength, bNeedsEndianConversion);
                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_ivSize.z << " (Slices: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    break;
              case 0x10 : // Rows
                    if (elementType == TYPE_Implicit) elementType = TYPE_US;
                    info.m_ivSize.y = GetUInt(fileDICOM, elementType, iElemLength, bNeedsEndianConversion);
                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_ivSize.y << " (Rows: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                      break;
              case 0x11 : // Columns
                    if (elementType == TYPE_Implicit) elementType = TYPE_US;
                    info.m_ivSize.x = GetUInt(fileDICOM, elementType, iElemLength, bNeedsEndianConversion);
                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_ivSize.x << " (Columns: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                      break;
              case 0x30 : // x,y spacing
                {
                    ReadSizedElement(fileDICOM, value, iElemLength);

                    size_t iDelimiter = value.find_first_of("\\");

                    info.m_fvfAspect.x = float(atof(value.substr(0,iDelimiter).c_str()));
                    info.m_fvfAspect.y = float(atof(value.substr(iDelimiter+1, value.length()).c_str()));

                    #ifdef DEBUG_DICOM
                    {
                        stringstream ss;
                      ss << info.m_fvfAspect.x << ", " << info.m_fvfAspect.y << " (x,y spacing: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                }  break;
              case 0x100 : // Allocated
                    if (elementType == TYPE_Implicit) elementType = TYPE_US;
                    info.m_iAllocated = GetUInt(fileDICOM, elementType, iElemLength, bNeedsEndianConversion);
                    #ifdef DEBUG_DICOM
                    {
                      stringstream ss;
                      ss << info.m_iAllocated << " (Allocated bits: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                      break;
              case 0x101 : // Stored
                    if (elementType == TYPE_Implicit) elementType = TYPE_US;
                    info.m_iStored = GetUInt(fileDICOM, elementType, iElemLength, bNeedsEndianConversion);
                    #ifdef DEBUG_DICOM
                    {
                      stringstream ss;
                      ss << info.m_iStored << " (Stored bits: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    break;
              case 0x0103 : // sign
                    if (elementType == TYPE_Implicit) elementType = TYPE_US;
                    info.m_bSigned = GetUInt(fileDICOM, elementType, iElemLength, bNeedsEndianConversion) == 1;
                    #ifdef DEBUG_DICOM
                    {
                      stringstream ss;
                      ss << info.m_bSigned << " (Sign bit: recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    break;
              case 0x1050: // Window Center
                ReadSizedElement(fileDICOM, value, iElemLength);
                info.m_fWindowCenter = float(atof(value.c_str()));
                #ifdef DEBUG_DICOM
                  {
                    stringstream ss;
                    ss << info.m_fWindowCenter << " (Window Center: recognized and stored)";
                    value = ss.str();
                  }
                #endif
                break;
              case 0x1051: // Window Width
                ReadSizedElement(fileDICOM, value, iElemLength);
                info.m_fWindowWidth =-float(atof(value.c_str()));
                #ifdef DEBUG_DICOM
                  {
                    stringstream ss;
                    ss << info.m_fWindowWidth << " (Window Width: recognized and stored)";
                    value = ss.str();
                  }
                #endif
                break;
              case 0x1052 : // Rescale Intercept (Bias)
                    ReadSizedElement(fileDICOM, value, iElemLength);
                    info.m_fBias = float(atof(value.c_str()));
                    #ifdef DEBUG_DICOM
                    {
                      stringstream ss;
                      ss << info.m_fBias << " (Rescale Intercept (Bias): recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    break;
              case 0x1053 : // Rescale Slope (Scale)
                    ReadSizedElement(fileDICOM, value, iElemLength);
                    info.m_fScale = float(atof(value.c_str()));
                    #ifdef DEBUG_DICOM
                    {
                      stringstream ss;
                      ss << info.m_fScale << " (Rescale Slope (Scale): recognized and stored)";
                      value = ss.str();
                    }
                    #endif
                    break;
              default : {
                SkipUnusedElement(fileDICOM, value, iElemLength);
              } break;
             } break;
        default : {
          SkipUnusedElement(fileDICOM, value, iElemLength);
        } break;

      }
    }
    #ifdef DEBUG_DICOM
    if (value != "SEQUENCE") Console::printf("iGroupID=%x iElementID=%x elementType=%s value=%s\n", iGroupID, iElementID, DICOM_TypeStrings[int(elementType)].c_str(), value.c_str());
    #endif

    ReadHeaderElemStart(fileDICOM, iGroupID, iElementID, elementType, iElemLength, bImplicit, info.m_bIsBigEndian);
  } while (iGroupID != 0x7fe0 && elementType != TYPE_UN);

  if (elementType != TYPE_UN) {
    if (!bImplicit) {
      // for an explicit file we can actually check if we found the pixel
      // data block (and not some color table)
      uint32_t iPixelDataSize = info.m_ivSize.volume() * info.m_iAllocated / 8;
      uint32_t iDataSizeInFile = iElemLength;
      if (iDataSizeInFile == 0) fileDICOM.read((char*)&iDataSizeInFile,4);

      if (info.m_bIsJPEGEncoded) {
        unsigned char iJPEGID[2];
        while (!fileDICOM.eof()) {
          fileDICOM.read((char*)iJPEGID,2);
          if (iJPEGID[0] == 0xFF && iJPEGID[1] == 0xE0 ) break;
        }
        // Try to get the offset, which can fail.  If it does, report an error
        // and fake an offset -- we're screwed at that point anyway.
        size_t offset = static_cast<size_t>(fileDICOM.tellg());
        if(static_cast<int>(fileDICOM.tellg()) == -1) {
          T_ERROR("JPEG offset unknown; DICOM parsing failed.  "
                  "Assuming offset 0.  Please send a debug log.");
          offset = 4;  // make sure it won't underflow in the next line.
        }
        offset -= 4;
        MESSAGE("JPEG is at offset: %u", static_cast<uint32_t>(offset));
        info.SetOffsetToData(static_cast<uint32_t>(offset));
      } else {
        if (iPixelDataSize != iDataSizeInFile) {
          elementType = TYPE_UN;
        } else info.SetOffsetToData(uint32_t(fileDICOM.tellg()));
      }
    } else {
      // otherwise just believe we have found the right data block
      info.SetOffsetToData(uint32_t(fileDICOM.tellg()));
    }
  }

  if (elementType == TYPE_UN) {
    // ok we encoutered some strange DICOM file (most likely that additional
    // SIEMENS header) and found an unknown tag,
    // so lets just march througth the rest of the file and search the magic
    // 0x7fe0, then use the last one found
    DICOM_DBG("Manual search for GroupId 0x7fe0\n");
    size_t iPosition   = size_t(fileDICOM.tellg());
    fileDICOM.seekg(0,ios::end);
    size_t iFileLength = size_t(fileDICOM.tellg());
    fileDICOM.seekg(iPosition,ios::beg);

    DICOM_DBG("volume size: %u\n", info.m_ivSize.volume());
    DICOM_DBG("n components: %u\n", info.m_iComponentCount);
    uint32_t iPixelDataSize = info.m_iComponentCount *
                            info.m_ivSize.volume() *
                            info.m_iAllocated / 8;
    bool bOK = false;
    do {
      iGroupID = 0;
      iPosition = size_t(fileDICOM.tellg());

      while (!fileDICOM.eof() && iGroupID != 0x7fe0 &&
             iPosition+iPixelDataSize < iFileLength) {
        iPosition++;
        fileDICOM.read((char*)&iGroupID,2);
      }
      DICOM_DBG("At eof: %d\n", fileDICOM.eof());

      // check if this 0x7fe0 is really a group ID
      if (iGroupID == 0x7fe0) {
        fileDICOM.seekg(-2, ios_base::cur);
        ReadHeaderElemStart(fileDICOM, iGroupID, iElementID, elementType,
                            iElemLength, bImplicit, info.m_bIsBigEndian);
        bOK = (elementType == TYPE_OW ||
               elementType == TYPE_OB ||
               elementType == TYPE_OF);

        if (bOK) {
          DICOM_DBG("Manual search for GroupID seemed to work.\n");
          if (!bImplicit) {
            uint32_t iVolumeDataSize = info.m_ivSize.volume() * info.m_iAllocated / 8;
            uint32_t iDataSizeInFile;
            fileDICOM.read((char*)&iDataSizeInFile,4);

            if (iVolumeDataSize != iDataSizeInFile) bOK = false;
          }

          info.SetOffsetToData(int(fileDICOM.tellg()));
        } else {
          DICOM_DBG("Manual search failed (for this iteration), "
                    "skipping element of type '%d'!", (int)elementType);
          fileDICOM.seekg(iElemLength, ios_base::cur);
        }
      }
    } while(iGroupID == 0x7fe0);

    if (!bOK) {
      // ok everthing failed than let's just use the data we have so far,
      // and let's hope that the file ends with the data
      WARNING("Trouble parsing DICOM file; assuming data starts at %u",
              static_cast<unsigned int>(iFileLength - size_t(iPixelDataSize)));
      info.SetOffsetToData(uint32_t(iFileLength - size_t(iPixelDataSize)));
    }
  }


  fileDICOM.close();

  return info.m_ivSize.volume() != 0;
}

/*************************************************************************************/

SimpleDICOMFileInfo::SimpleDICOMFileInfo(const std::string& strFileName) :
  SimpleFileInfo(strFileName),
  m_fvPatientPosition(0,0,0),
  m_iComponentCount(1),
  m_fScale(1.0f),
  m_fBias(0.0f),
  m_fWindowWidth(0),
  m_fWindowCenter(0),
  m_bSigned(false),
  m_iOffsetToData(0)
{
}

SimpleDICOMFileInfo::SimpleDICOMFileInfo(const std::wstring& wstrFileName) :
  SimpleFileInfo(wstrFileName),
  m_fvPatientPosition(0,0,0),
  m_iComponentCount(1),
  m_fScale(1.0f),
  m_fBias(0.0f),
  m_bSigned(false),
  m_iOffsetToData(0)
{
}

SimpleDICOMFileInfo::SimpleDICOMFileInfo() :
  SimpleFileInfo(),
  m_fvPatientPosition(0,0,0),
  m_iComponentCount(1),
  m_fScale(1.0f),
  m_fBias(0.0f),
  m_bSigned(false),
  m_iOffsetToData(0)
{
}

SimpleDICOMFileInfo::SimpleDICOMFileInfo(const SimpleDICOMFileInfo* other) :
  SimpleFileInfo(other),
  m_fvPatientPosition(other->m_fvPatientPosition),
  m_iComponentCount(other->m_iComponentCount),
  m_fScale(other->m_fScale),
  m_fBias(other->m_fBias),
  m_bSigned(other->m_bSigned),
  m_iOffsetToData(other->m_iOffsetToData)
{
}

uint32_t SimpleDICOMFileInfo::GetComponentCount() const {
  return m_iComponentCount;
}

bool SimpleDICOMFileInfo::GetData(std::vector<char>& vData, uint32_t iLength,
                                  uint32_t iOffset)
{
  ifstream fs;
  fs.open(m_strFileName.c_str(),fstream::binary);
  if (fs.fail()) return false;

  fs.seekg(m_iOffsetToData+iOffset, ios_base::cur);
  fs.read(&vData[0], iLength);

  fs.close();
  return true;
}

SimpleFileInfo* SimpleDICOMFileInfo::clone() {
  SimpleDICOMFileInfo* pSimpleDICOMFileInfo = new SimpleDICOMFileInfo(this);
  return (SimpleFileInfo*)pSimpleDICOMFileInfo;
}


/*************************************************************************************/

DICOMFileInfo::DICOMFileInfo() :
  SimpleDICOMFileInfo(),
  m_iSeries(0),
  m_ivSize(0,0,1),
  m_fvfAspect(1,1,1),
  m_iAllocated(0),
  m_iStored(0),
  m_bIsBigEndian(false),
  m_bIsJPEGEncoded(false),
  m_strAcquDate(""),
  m_strAcquTime(""),
  m_strModality(""),
  m_strDesc("")
{}

DICOMFileInfo::DICOMFileInfo(const std::string& strFileName) :
  SimpleDICOMFileInfo(strFileName),
  m_iSeries(0),
  m_ivSize(0,0,1),
  m_fvfAspect(1,1,1),
  m_iAllocated(0),
  m_iStored(0),
  m_bIsBigEndian(false),
  m_bIsJPEGEncoded(false),
  m_strAcquDate(""),
  m_strAcquTime(""),
  m_strModality(""),
  m_strDesc("")
{}


DICOMFileInfo::DICOMFileInfo(const std::wstring& wstrFileName) :
  SimpleDICOMFileInfo(wstrFileName),
  m_iSeries(0),
  m_ivSize(0,0,1),
  m_fvfAspect(1,1,1),
  m_iAllocated(0),
  m_iStored(0),
  m_bIsBigEndian(false),
  m_bIsJPEGEncoded(false),
  m_strAcquDate(""),
  m_strAcquTime(""),
  m_strModality(""),
  m_strDesc("")
{}

void DICOMFileInfo::SetOffsetToData(const uint32_t iOffset) {
  m_iOffsetToData = iOffset;
  m_iDataSize = m_iComponentCount*m_ivSize.volume()*m_iAllocated/8;
}

/*************************************************************************************/

DICOMStackInfo::DICOMStackInfo() :
  FileStackInfo(),
  m_iSeries(0),
  m_strAcquDate(""),
  m_strAcquTime(""),
  m_strModality("")
{}

DICOMStackInfo::DICOMStackInfo(const DICOMFileInfo* fileInfo) :
  FileStackInfo(fileInfo->m_ivSize, fileInfo->m_fvfAspect, 
                fileInfo->m_iAllocated, fileInfo->m_iStored,
                fileInfo->m_iComponentCount, fileInfo->m_bSigned,
                fileInfo->m_bIsBigEndian, 
                fileInfo->m_bIsJPEGEncoded, fileInfo->m_strDesc, "DICOM"),
  m_iSeries(fileInfo->m_iSeries),
  m_strAcquDate(fileInfo->m_strAcquDate),
  m_strAcquTime(fileInfo->m_strAcquTime),
  m_strModality(fileInfo->m_strModality)
{
  m_Elements.push_back(new SimpleDICOMFileInfo(fileInfo));
}

DICOMStackInfo::DICOMStackInfo(const DICOMStackInfo* other) :
  m_iSeries(other->m_iSeries),
  m_strAcquDate(other->m_strAcquDate),
  m_strAcquTime(other->m_strAcquTime),
  m_strModality(other->m_strModality)
{
  m_ivSize          = other->m_ivSize;
  m_fvfAspect       = other->m_fvfAspect;
  m_iAllocated      = other->m_iAllocated;
  m_iStored         = other->m_iStored;
  m_iComponentCount = other->m_iComponentCount;
  m_bSigned         = other->m_bSigned;
  m_bIsBigEndian    = other->m_bIsBigEndian;
  m_bIsJPEGEncoded  = other->m_bIsJPEGEncoded;
  m_strDesc         = other->m_strDesc;
  m_strFileType     = other->m_strFileType;

  for (size_t i=0;i<other->m_Elements.size();i++) {
    SimpleDICOMFileInfo* e = new SimpleDICOMFileInfo((SimpleDICOMFileInfo*)other->m_Elements[i]);
    m_Elements.push_back(e);
  }
}


bool DICOMStackInfo::Match(const DICOMFileInfo* info) {
  if (m_iSeries       == info->m_iSeries &&
    m_ivSize          == info->m_ivSize &&
    m_iAllocated      == info->m_iAllocated &&
    m_iStored         == info->m_iStored &&
    m_iComponentCount == info->m_iComponentCount &&
    m_bSigned         == info->m_bSigned &&
    m_fvfAspect       == info->m_fvfAspect &&
    m_bIsBigEndian    == info->m_bIsBigEndian &&
    m_bIsJPEGEncoded  == info->m_bIsJPEGEncoded &&
    m_strAcquDate     == info->m_strAcquDate &&
    //m_strAcquTime   == info->m_strAcquTime &&
    m_strModality     == info->m_strModality &&
    m_strDesc         == info->m_strDesc) {

    std::vector<SimpleFileInfo*>::iterator iter;

    for (iter = m_Elements.begin(); iter < m_Elements.end(); ++iter) {
      if ((*iter)->m_iImageIndex > info->m_iImageIndex) break;
    }

    m_Elements.insert(iter,new SimpleDICOMFileInfo(info));

    return true;
  } else return false;
}
