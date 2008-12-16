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
  \author    Jens Krueger
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

#include "QVISConverter.h"
#include "NRRDConverter.h"

using namespace std;

IOManager::IOManager(MasterController* masterController) :
  m_pMasterController(masterController),
  m_TempDir("./") // changed by convert dataset
{
  m_vpConverters.push_back(new QVISConverter());
  m_vpConverters.push_back(new NRRDConverter());

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

  if (parseDICOM.m_FileStacks.size() == 1)
    m_pMasterController->DebugOut()->Message("IOManager::ScanDirectory","  found a single DICOM stack");
  else
    m_pMasterController->DebugOut()->Message("IOManager::ScanDirectory","  found %i DICOM stacks", int(parseDICOM.m_FileStacks.size()));

  for (unsigned int iStackID = 0;iStackID < parseDICOM.m_FileStacks.size();iStackID++) {    
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

  for (unsigned int iStackID = 0;iStackID < parseImages.m_FileStacks.size();iStackID++) {    
    ImageStackInfo* f = new ImageStackInfo((ImageStackInfo*)parseImages.m_FileStacks[iStackID]);

    stringstream s;
    s << f->m_strFileType << " Stack: " << f->m_strDesc;
    f->m_strDesc = s.str();

    fileStacks.push_back(f);
  }

  /// \todo  add other image parsers here

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
		for (uint j = 0;j<pDICOMStack->m_Elements.size();j++) {
			pDICOMStack->m_Elements[j]->GetData((void**)&pData); // the first call does a "new" on pData 

			unsigned int iDataSize = pDICOMStack->m_Elements[j]->GetDataSize();

			if (pDICOMStack->m_bIsBigEndian) {
				switch (pDICOMStack->m_iAllocated) {
					case  8 : break;
					case 16 : {
								for (uint k = 0;k<iDataSize/2;k++)
									((short*)pData)[k] = EndianConvert::Swap<short>(((short*)pData)[k]);
							  } break;
					case 32 : {
								for (uint k = 0;k<iDataSize/4;k++)
									((float*)pData)[k] = EndianConvert::Swap<float>(((float*)pData)[k]);
							  } break;
				}
			}

      // HACK: this code assumes 3 component data is always 3*char
			if (pDICOMStack->m_iComponentCount == 3) {
				unsigned int iRGBADataSize = (iDataSize / 3 ) * 4;
				
				unsigned char *pRGBAData = new unsigned char[ iRGBADataSize ];
				for (uint k = 0;k<iDataSize/3;k++) {
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
		iSize.z *= (unsigned int)pDICOMStack->m_Elements.size();

    /// \todo evaluate pDICOMStack->m_strModality

    bool result = RAWConverter::ConvertRAWDataset(strTempMergeFilename, strTargetFilename, m_TempDir, m_pMasterController, 0,
                                    pDICOMStack->m_iAllocated, pDICOMStack->m_iComponentCount, 
                                    false,
                                    pDICOMStack->m_bIsBigEndian != EndianConvert::IsBigEndian(),
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
	      for (uint j = 0;j<pStack->m_Elements.size();j++) {
          pStack->m_Elements[j]->GetData((void**)&pData); // the first call does a "new" on pData 

          unsigned int iDataSize = pStack->m_Elements[j]->GetDataSize();
          fs.write(pData, iDataSize);
        }
        delete [] pData;


		    fs.close();
        m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","    done creating intermediate file %s", strTempMergeFilename.c_str()); 

		    UINTVECTOR3 iSize = pStack->m_ivSize;
		    iSize.z *= (unsigned int)pStack->m_Elements.size();

        bool result = RAWConverter::ConvertRAWDataset(strTempMergeFilename, strTargetFilename, m_TempDir, m_pMasterController, 0,
                                        pStack->m_iAllocated, pStack->m_iComponentCount, 
                                        false,
                                        pStack->m_bIsBigEndian != EndianConvert::IsBigEndian(),
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

bool IOManager::ConvertDataset(const std::string& strFilename, const std::string& strTargetFilename) {
  /// \todo maybe come up with something smarter for a temp dir then the target dir
  m_TempDir = SysTools::GetPath(strTargetFilename);

  m_pMasterController->DebugOut()->Message("IOManager::ConvertDataset","Request to convert dataset %s to %s received.", strFilename.c_str(), strTargetFilename.c_str());

  string strExt = SysTools::ToUpperCase(SysTools::GetExt(strFilename));

  for (size_t i = 0;i<m_vpConverters.size();i++) {
    const std::vector<std::string>& vStrSupportedExt = m_vpConverters[i]->SupportedExt();
    for (size_t j = 0;j<vStrSupportedExt.size();j++) {
      if (vStrSupportedExt[j] == strExt) {
        if (m_vpConverters[i]->Convert(strFilename, strTargetFilename, m_TempDir, m_pMasterController)) return true;
      }
    }
  }
  return false;
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
