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
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    December 2008
*/

#include "QVISConverter.h"
#include "IOManager.h"  // for the size defines
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>
#include <IO/KeyValueFileParser.h>

using namespace std;


QVISConverter::QVISConverter()
{
  m_vConverterDesc = "QVis Data";
  m_vSupportedExt.push_back("DAT");
}

bool QVISConverter::Convert(const std::string& strSourceFilename, const std::string& strTargetFilename, const std::string& strTempDir, MasterController* pMasterController)
{
  pMasterController->DebugOut()->Message("QVISConverter::Convert","Attempting to convert QVIS dataset %s to %s", strSourceFilename.c_str(), strTargetFilename.c_str());

  UINT64			  iComponentSize=8;
  UINT64			  iComponentCount=1;
  bool          bSigned=false;
  UINTVECTOR3		vVolumeSize;
  FLOATVECTOR3	vVolumeAspect;
  string        strRAWFile;

  KeyValueFileParser parser(strSourceFilename);

  if (parser.FileReadable())  {
	  KeyValPair* format = parser.GetData("FORMAT");
	  if (format == NULL) 
		  return false;
	  else {
		  if (format->strValueUpper == "UCHAR" || format->strValueUpper == "BYTE") {
  		  bSigned = false;
			  iComponentSize = 8;
			  iComponentCount = 1;
		  } else if (format->strValueUpper == "USHORT") {
  		  bSigned = false;
			  iComponentSize = 16;
			  iComponentCount = 1;
		  } else if (format->strValueUpper == "UCHAR4") {
  		  bSigned = false;
			  iComponentSize = 32;
			  iComponentCount = 4;
		  } else if (format->strValueUpper == "FLOAT") {
  		  bSigned = true;
			  iComponentSize = 32;
			  iComponentCount = 1;
		  }
	  }

	  KeyValPair* objectfilename = parser.GetData("OBJECTFILENAME");
    if (objectfilename == NULL) {
      pMasterController->DebugOut()->Warning("QVISConverter::Convert","This is not a valid QVIS dat file.");
      return false; 
    } else 
      strRAWFile = objectfilename->strValue;

	  KeyValPair* resolution = parser.GetData("RESOLUTION");
    if (resolution == NULL) {
      pMasterController->DebugOut()->Warning("QVISConverter::Convert","This is not a valid QVIS dat file.");
      return false; 
    } else 
      vVolumeSize = resolution->vuiValue;

	  KeyValPair* sliceThickness = parser.GetData("SLICETHICKNESS");
    if (sliceThickness == NULL) {
      pMasterController->DebugOut()->Warning("QVISConverter::Convert","This is not a valid QVIS dat file.");
		  vVolumeAspect = FLOATVECTOR3(1,1,1);
    } else {			
		  vVolumeAspect = sliceThickness->vfValue;
		  vVolumeAspect = vVolumeAspect / vVolumeAspect.maxVal();
	  }

	  strRAWFile = SysTools::GetPath(strSourceFilename) + strRAWFile;

    /// \todo  detect big endian DAT/RAW combinations and set the conversion parameter accordingly instead of always assuming it is little endian and thius converting if the machine is big endian 
    return ConvertRAWDataset(strRAWFile, strTargetFilename, strTempDir, pMasterController, 0, iComponentSize, iComponentCount, bSigned, EndianConvert::IsBigEndian(),
                             vVolumeSize, vVolumeAspect, "Qvis data", SysTools::GetFilename(strSourceFilename));

  } else return false;
}
