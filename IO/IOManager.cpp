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
  \file    IOManager.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    August 2008
*/

#include "IOManager.h"
#include <Controller/MasterController.h>
#include <IO/DICOM/DICOMParser.h>
#include <IO/Images/ImageParser.h>
#include <Basics/SysTools.h>
#include <sstream>
#include <fstream>
#include <map>

#include "QVISConverter.h"
#include "NRRDConverter.h"
#include "VFFConverter.h"

using namespace std;

IOManager::IOManager(MasterController* masterController) :
  m_pMasterController(masterController),
  m_TempDir("./"), // changed by convert dataset
  m_pFinalConverter(NULL)
{
  m_vpConverters.push_back(new QVISConverter());
  m_vpConverters.push_back(new NRRDConverter());
  m_vpConverters.push_back(new VFFConverter());
}


void IOManager::RegisterExternalConverter(AbstrConverter* pConverter) {
  m_vpConverters.push_back(pConverter);
}

void IOManager::RegisterFinalConverter(AbstrConverter* pConverter) {
  if ( m_pFinalConverter ) delete m_pFinalConverter;
  m_pFinalConverter = pConverter;
}


IOManager::~IOManager()
{
  for (size_t i = 0;i<m_vpConverters.size();i++) delete m_vpConverters[i];
  m_vpConverters.clear();
}

vector<FileStackInfo*> IOManager::ScanDirectory(std::string strDirectory) {

  m_pMasterController->DebugOut()->Message("IOManager::ScanDirectory","Scanning directory %s", strDirectory.c_str());

  std::vector<FileStackInfo*> fileStacks;

  DICOMParser parseDICOM;
  parseDICOM.GetDirInfo(strDirectory);

  for (size_t iStackID = 0;iStackID < parseDICOM.m_FileStacks.size();iStackID++) {    
    DICOMStackInfo* f = new DICOMStackInfo((DICOMStackInfo*)parseDICOM.m_FileStacks[iStackID]);

    // if trying to load JPEG files. check if Qimage can handle the JPEG payload
    if (f->m_bIsJPEGEncoded) {
      void* pData = NULL;
      f->m_Elements[0]->GetData(&pData);
      UINT32 iLength = f->m_Elements[0]->GetDataSize();
      QImage image;
      if (!image.loadFromData((uchar*)pData, iLength)) {
        parseDICOM.m_FileStacks.erase(parseDICOM.m_FileStacks.begin()+iStackID);
        iStackID--;
      }
      delete [] (char*)pData;
    }

    delete f;
  }


  if (parseDICOM.m_FileStacks.size() == 1)
    m_pMasterController->DebugOut()->Message("IOManager::ScanDirectory","  found a single DICOM stack");
  else
    m_pMasterController->DebugOut()->Message("IOManager::ScanDirectory","  found %i DICOM stacks", int(parseDICOM.m_FileStacks.size()));

  for (size_t iStackID = 0;iStackID < parseDICOM.m_FileStacks.size();iStackID++) {    
    DICOMStackInfo* f = new DICOMStackInfo((DICOMStackInfo*)parseDICOM.m_FileStacks[iStackID]);

    stringstream s;
    s << f->m_strFileType << " Stack: " << f->m_strDesc;
    f->m_strDesc = s.str();

    fileStacks.push_back(f);
  }

  ImageParser parseImages;
  parseImages.GetDirInfo(strDirectory);

  if (parseImages.m_FileStacks.size() == 1)
    m_pMasterController->DebugOut()->Message("IOManager::ScanDirectory","  found a single image stack");
  else
    m_pMasterController->DebugOut()->Message("IOManager::ScanDirectory","  found %i image stacks", int(parseImages.m_FileStacks.size()));

  for (size_t iStackID = 0;iStackID < parseImages.m_FileStacks.size();iStackID++) {    
    ImageStackInfo* f = new ImageStackInfo((ImageStackInfo*)parseImages.m_FileStacks[iStackID]);

    stringstream s;
    s << f->m_strFileType << " Stack: " << f->m_strDesc;
    f->m_strDesc = s.str();

    fileStacks.push_back(f);
  }

   // add other image parsers here

  m_pMasterController->DebugOut()->Message("IOManager::ScanDirectory","  scan complete");

  return fileStacks;
}


bool IOManager::ConvertDataset(FileStackInfo* pStack, const std::string& strTargetFilename) {

  /// \todo maybe come up with something smarter for a temp dir then the target dir
  m_TempDir = SysTools::GetPath(strTargetFilename);

  m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","Request to convert stack of %s files to %s received", pStack->m_strDesc.c_str(), strTargetFilename.c_str());

  if (pStack->m_strFileType == "DICOM") {
    m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","  Detected DICOM stack, starting DICOM conversion");

    DICOMStackInfo* pDICOMStack = ((DICOMStackInfo*)pStack);

    m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","  Stack contains %i files",  int(pDICOMStack->m_Elements.size()));
    m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","    Series: %i  Bits: %i (%i)", pDICOMStack->m_iSeries, pDICOMStack->m_iAllocated, pDICOMStack->m_iStored);
    m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","    Date: %s  Time: %s", pDICOMStack->m_strAcquDate.c_str(), pDICOMStack->m_strAcquTime.c_str());
    m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","    Modality: %s  Description: %s", pDICOMStack->m_strModality.c_str(), pDICOMStack->m_strDesc.c_str());
    m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","    Aspect Ratio: %g %g %g", pDICOMStack->m_fvfAspect.x, pDICOMStack->m_fvfAspect.y, pDICOMStack->m_fvfAspect.z);

    string strTempMergeFilename = strTargetFilename + "~";

    m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","    Creating intermediate file %s", strTempMergeFilename.c_str()); 

    ofstream fs;
    fs.open(strTempMergeFilename.c_str(),fstream::binary);
    if (fs.fail())  {
      m_pMasterController->DebugOut()->Error("IOManager::ConvertDataset","Could not create temp file %s aborted conversion.", strTempMergeFilename.c_str());
      return false;
    }

    char *pData = NULL;
    for (size_t j = 0;j<pDICOMStack->m_Elements.size();j++) {
      pDICOMStack->m_Elements[j]->GetData((void**)(char**)&pData); // the first call does a "new" on pData, the strange casting here is to avoid pointer aliasing issues

      UINT32 iDataSize = pDICOMStack->m_Elements[j]->GetDataSize();


      if (pDICOMStack->m_bIsJPEGEncoded) {
        QImage image;
        if (!image.loadFromData((uchar*)pData, iDataSize)) {
          m_pMasterController->DebugOut()->Error("IOManager::ConvertDataset","QImage is unable to load JPEG block in DICOM file.");
          delete [] pData;
          return false;
        }
        if (pDICOMStack->m_iComponentCount == 1) {
          size_t i = 0;
          for (int h = 0;h<image.height();h++) {
            for (int w = 0;w<image.width();w++) {
              pData[i] = qRed(image.pixel(w,h));
              i++;
            }
          }
        } else 
        if (pDICOMStack->m_iComponentCount == 3) {
          size_t i = 0;
          for (int h = 0;h<image.height();h++) {
            for (int w = 0;w<image.width();w++) {
              pData[i+0] = qRed(image.pixel(w,h));
              pData[i+1] = qGreen(image.pixel(w,h));
              pData[i+2] = qBlue(image.pixel(w,h));
              i+=3;
            }
          }
        } else {
          m_pMasterController->DebugOut()->Error("IOManager::ConvertDataset","Only 1 and 3 component images are supported a the moment.");
          delete [] pData;
          return false;
        }
      }


      if (pDICOMStack->m_bIsBigEndian) {
        switch (pDICOMStack->m_iAllocated) {
          case  8 : break;
          case 16 : {
                for (UINT32 k = 0;k<iDataSize/2;k++)
                  ((short*)pData)[k] = EndianConvert::Swap<short>(((short*)pData)[k]);
                } break;
          case 32 : {
                for (UINT32 k = 0;k<iDataSize/4;k++)
                  ((float*)pData)[k] = EndianConvert::Swap<float>(((float*)pData)[k]);
                } break;
        }
      }

      // HACK: this code assumes 3 component data is always 3*char
      if (pDICOMStack->m_iComponentCount == 3) {
        UINT32 iRGBADataSize = (iDataSize / 3 ) * 4;

        unsigned char *pRGBAData = new unsigned char[ iRGBADataSize ];
        for (UINT32 k = 0;k<iDataSize/3;k++) {
          pRGBAData[k*4+0] = pData[k*3+0];
          pRGBAData[k*4+1] = pData[k*3+1];
          pRGBAData[k*4+2] = pData[k*3+2];
          pRGBAData[k*4+3] = 255;
        }

        fs.write((char*)pRGBAData, iRGBADataSize);
        delete [] pRGBAData;
      } else {
        fs.write(pData, iDataSize);
      }
    }
    delete [] pData;

    fs.close();
    m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","    done creating intermediate file %s", strTempMergeFilename.c_str()); 

    UINTVECTOR3 iSize = pDICOMStack->m_ivSize;
    iSize.z *= UINT32(pDICOMStack->m_Elements.size());

    /// \todo evaluate pDICOMStack->m_strModality

    bool result = RAWConverter::ConvertRAWDataset(strTempMergeFilename, strTargetFilename, m_TempDir, m_pMasterController, 0,
                                    pDICOMStack->m_iAllocated, pDICOMStack->m_iComponentCount, 
                                    pDICOMStack->m_bIsBigEndian != EndianConvert::IsBigEndian(),
                                    pDICOMStack->m_iAllocated >=32, /// \todo read sign property from DICOM file
                                    false, /// \todo read float property from DICOM file
                                    iSize, pDICOMStack->m_fvfAspect, 
                                    "DICOM stack", SysTools::GetFilename(pDICOMStack->m_Elements[0]->m_strFileName)
                                    + " to " + SysTools::GetFilename(pDICOMStack->m_Elements[pDICOMStack->m_Elements.size()-1]->m_strFileName));

    if( remove(strTempMergeFilename.c_str()) != 0 ) {
      m_pMasterController->DebugOut()->Warning("IOManager::ConvertDataset","Unable to remove temp file %s", strTempMergeFilename.c_str());
    }

    return result;
  } else {
     if (pStack->m_strFileType == "IMAGE") {
        m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","  Detected Image stack, starting image conversion");
        m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","  Stack contains %i files",  int(pStack->m_Elements.size()));

        string strTempMergeFilename = strTargetFilename + "~";
        m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","    Creating intermediate file %s", strTempMergeFilename.c_str()); 

        ofstream fs;
        fs.open(strTempMergeFilename.c_str(),fstream::binary);
        if (fs.fail())  {
          m_pMasterController->DebugOut()->Error("IOManager::ConvertDataset","Could not create temp file %s aborted conversion.", strTempMergeFilename.c_str());
          return false;
        }

        char *pData = NULL;
        for (size_t j = 0;j<pStack->m_Elements.size();j++) {
          pStack->m_Elements[j]->GetData((void**)(char**)&pData); // the first call does a "new" on pData, the strange casting here is to avoid pointer aliasing issues

          UINT32 iDataSize = pStack->m_Elements[j]->GetDataSize();
          fs.write(pData, iDataSize);
        }
        delete [] pData;


        fs.close();
        m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","    done creating intermediate file %s", strTempMergeFilename.c_str()); 

        UINTVECTOR3 iSize = pStack->m_ivSize;
        iSize.z *= UINT32(pStack->m_Elements.size());

        bool result = RAWConverter::ConvertRAWDataset(strTempMergeFilename, strTargetFilename, m_TempDir, m_pMasterController, 0,
                                        pStack->m_iAllocated, pStack->m_iComponentCount, 
                                        pStack->m_bIsBigEndian != EndianConvert::IsBigEndian(),
                                        pStack->m_iComponentCount >= 32,
                                        false,
                                        iSize, pStack->m_fvfAspect, 
                                        "Image stack", SysTools::GetFilename(pStack->m_Elements[0]->m_strFileName)
                                        + " to " + SysTools::GetFilename(pStack->m_Elements[pStack->m_Elements.size()-1]->m_strFileName));

        if( remove(strTempMergeFilename.c_str()) != 0 ) {
          m_pMasterController->DebugOut()->Warning("IOManager::ConvertDataset","Unable to remove temp file %s", strTempMergeFilename.c_str());
        }

        return result;
     }
  }


  m_pMasterController->DebugOut()->Error("IOManager::ConvertDataset","Unknown source stack type %s", pStack->m_strFileType.c_str());
  return false;
}

bool IOManager::ConvertDataset(const std::string& strFilename, const std::string& strTargetFilename, bool bNoUserInteraction) {
  /// \todo maybe come up with something smarter for a temp dir then the target dir
  m_TempDir = SysTools::GetPath(strTargetFilename);

  m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","Request to convert dataset %s to %s received.", strFilename.c_str(), strTargetFilename.c_str());

  string strExt = SysTools::ToUpperCase(SysTools::GetExt(strFilename));

  string strExtTarget = SysTools::ToUpperCase(SysTools::GetExt(strTargetFilename));

  if (strExtTarget == "UVF") {
    for (size_t i = 0;i<m_vpConverters.size();i++) {
      const std::vector<std::string>& vStrSupportedExt = m_vpConverters[i]->SupportedExt();
      for (size_t j = 0;j<vStrSupportedExt.size();j++) {
        if (vStrSupportedExt[j] == strExt) {
          if (m_vpConverters[i]->ConvertToUVF(strFilename, strTargetFilename, m_TempDir, m_pMasterController, bNoUserInteraction)) return true;
        }
      }
    }

    if (m_pFinalConverter) 
      return m_pFinalConverter->ConvertToUVF(strFilename, strTargetFilename, m_TempDir, m_pMasterController, bNoUserInteraction);
    else
      return false;
  } else {
      UINT64        iHeaderSkip; 
      UINT64        iComponentSize;
      UINT64        iComponentCount; 
      bool          bConvertEndianess;
      bool          bSigned;
      bool          bIsFloat;
      UINTVECTOR3   vVolumeSize;
      FLOATVECTOR3  vVolumeAspect;
      string        strTitle;
      string        strSource;
      UVFTables::ElementSemanticTable eType;
      string        strIntermediateFile;
      bool          bDeleteIntermediateFile;

      for (size_t i = 0;i<m_vpConverters.size();i++) {
        const std::vector<std::string>& vStrSupportedExt = m_vpConverters[i]->SupportedExt();
        for (size_t j = 0;j<vStrSupportedExt.size();j++) {
          if (vStrSupportedExt[j] == strExt) {
            bool bRAWCreated = m_vpConverters[i]->ConvertToRAW(strFilename, m_TempDir, m_pMasterController, bNoUserInteraction, 
                                            iHeaderSkip, iComponentSize, iComponentCount, bConvertEndianess, bSigned, bIsFloat, vVolumeSize, vVolumeAspect,
                                            strTitle, strSource, eType, strIntermediateFile, bDeleteIntermediateFile);
            if (!bRAWCreated) continue;
            bool bTargetCreated = false;
            for (size_t k = 0;k<m_vpConverters.size();k++) {
              const std::vector<std::string>& vStrSupportedExtTarget = m_vpConverters[k]->SupportedExt();
              for (size_t l = 0;l<vStrSupportedExtTarget.size();l++) {
                if (vStrSupportedExtTarget[l] == strExtTarget) {
                  bTargetCreated = m_vpConverters[k]->ConvertToNative(strIntermediateFile, strTargetFilename, iHeaderSkip, 
                                                                          iComponentSize, iComponentCount, bSigned, bIsFloat,
                                                                          vVolumeSize, vVolumeAspect, m_pMasterController, bNoUserInteraction);
                  if (bTargetCreated) break;
                }
              }
              if (bTargetCreated) break;
            }
            if (bDeleteIntermediateFile) remove(strIntermediateFile.c_str());
            if (bTargetCreated) return true;
          }
        }
      }

      if (m_pFinalConverter) {
        bool bRAWCreated = m_pFinalConverter->ConvertToRAW(strFilename, m_TempDir, m_pMasterController, bNoUserInteraction, 
                                        iHeaderSkip, iComponentSize, iComponentCount, bConvertEndianess, bSigned, bIsFloat, vVolumeSize, vVolumeAspect,
                                        strTitle, strSource, eType, strIntermediateFile, bDeleteIntermediateFile);
        if (!bRAWCreated) return false;
        bool bTargetCreated = false;
        for (size_t k = 0;k<m_vpConverters.size();k++) {
          const std::vector<std::string>& vStrSupportedExtTarget = m_vpConverters[k]->SupportedExt();
          for (size_t l = 0;l<vStrSupportedExtTarget.size();l++) {
            if (vStrSupportedExtTarget[l] == strExtTarget) {
              bTargetCreated = m_vpConverters[k]->ConvertToNative(strIntermediateFile, strTargetFilename, iHeaderSkip, 
                                                                      iComponentSize, iComponentCount, bSigned, bIsFloat, 
                                                                      vVolumeSize, vVolumeAspect, m_pMasterController, bNoUserInteraction);
              if (bTargetCreated) break;
            }
          }
          if (bTargetCreated) break;
        }
        if (bDeleteIntermediateFile) remove(strIntermediateFile.c_str());
        return bTargetCreated;
      } else return false;
  }
}

VolumeDataset* IOManager::ConvertDataset(FileStackInfo* pStack, const std::string& strTargetFilename, AbstrRenderer* requester) {
  if (!ConvertDataset(pStack, strTargetFilename)) return NULL;
  return LoadDataset(strTargetFilename, requester);
}

VolumeDataset* IOManager::ConvertDataset(const std::string& strFilename, const std::string& strTargetFilename, AbstrRenderer* requester) {
  if (!ConvertDataset(strFilename, strTargetFilename)) return NULL;
  return LoadDataset(strTargetFilename, requester);
}

VolumeDataset* IOManager::LoadDataset(const std::string& strFilename, AbstrRenderer* requester) {
  return m_pMasterController->MemMan()->LoadDataset(strFilename, requester);
}

bool IOManager::NeedsConversion(const std::string& strFilename, bool& bChecksumFail) {
  wstring wstrFilename(strFilename.begin(), strFilename.end());
  return !UVF::IsUVFFile(wstrFilename, bChecksumFail);
}

bool IOManager::NeedsConversion(const std::string& strFilename) {
  wstring wstrFilename(strFilename.begin(), strFilename.end());
  return !UVF::IsUVFFile(wstrFilename);
}


std::string IOManager::GetLoadDialogString() {
  string strDialog = "All known Files ( *.uvf ";
  map<string,string> descPairs;

  // first create the show all text entry
  for (size_t i = 0;i<m_vpConverters.size();i++) {
    for (size_t j = 0;j<m_vpConverters[i]->SupportedExt().size();j++) {
      string strExt = SysTools::ToLowerCase(m_vpConverters[i]->SupportedExt()[j]);
      if (descPairs.count(strExt) == 0) {
        strDialog = strDialog + "*." + strExt + " ";
        descPairs[strExt] = m_vpConverters[i]->GetDesc();
      }
    }
  }
  strDialog += ");;Universal Volume Format (*.uvf);;";

  // seperate entries
  for (size_t i = 0;i<m_vpConverters.size();i++) {
    strDialog += m_vpConverters[i]->GetDesc() + " (";
    for (size_t j = 0;j<m_vpConverters[i]->SupportedExt().size();j++) {
      string strExt = SysTools::ToLowerCase(m_vpConverters[i]->SupportedExt()[j]);
      strDialog += "*." + strExt;
      if (j<m_vpConverters[i]->SupportedExt().size()-1) 
        strDialog += " ";
    }
    strDialog += ");;";
  }

  strDialog += "All Files (*.*)";

  return strDialog;
}

std::string IOManager::GetExportDialogString() {
  std::string strDialog;
  // seperate entries
  for (size_t i = 0;i<m_vpConverters.size();i++) {
    for (size_t j = 0;j<m_vpConverters[i]->SupportedExt().size();j++) {
      string strExt = SysTools::ToLowerCase(m_vpConverters[i]->SupportedExt()[j]);
      strDialog += m_vpConverters[i]->GetDesc() + " (*." + strExt + ");;";
    }
  }

  return strDialog;
}
