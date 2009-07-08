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
  \file    I3MConverter.cpp
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    December 2008
*/

#include <fstream>
#include "I3MConverter.h"
#include <Controller/Controller.h>
#include <Basics/SysTools.h>
#include <IO/KeyValueFileParser.h>

using namespace std;


I3MConverter::I3MConverter()
{
  m_vConverterDesc = "ImageVis3D Mobile Data";
  m_vSupportedExt.push_back("I3M");
}

bool I3MConverter::ConvertToRAW(const std::string& strSourceFilename, 
                            const std::string& strTempDir, bool,
                            UINT64& iHeaderSkip, UINT64& iComponentSize, UINT64& iComponentCount, 
                            bool& bConvertEndianess, bool& bSigned, bool& bIsFloat, UINTVECTOR3& vVolumeSize,
                            FLOATVECTOR3& vVolumeAspect, std::string& strTitle,
                            UVFTables::ElementSemanticTable& eType, std::string& strIntermediateFile,
                            bool& bDeleteIntermediateFile) {

  MESSAGE("Attempting to convert an ImageVis3D mobile dataset %s", strSourceFilename.c_str());

  bDeleteIntermediateFile = true;
  eType             = UVFTables::ES_UNDEFINED;
  strTitle          = "ImageVis3D Mobile data";
  iHeaderSkip       = 0;
  
  // I3M files are always four component 8bit little endian
  // unsigned, whereas the first 3 component of the vector
  // are the normalized gradient/normal and the fourth is the
  // actual data value, so all we need to todo here is parse
  // the binary header for the size and aspect ratio and then
  // create an intermediate RAW file taking every fourth byte
  // after the header while doing so we make sure to write the
  // file in the endianess of this machine
  iComponentSize    = 8;
  iComponentCount   = 1;
  bIsFloat          = false;
  bSigned           = false;
  bConvertEndianess = false;

  LargeRAWFile I3MFile(strSourceFilename, 0);
  I3MFile.Open(false);

  if (!I3MFile.IsOpen()) {
    T_ERROR("Unable to open source file %s", strSourceFilename.c_str());
    return false;
  }

  // get file size -> used for verification later
  UINT64 ulFileLength = I3MFile.GetCurrentSize();

  // get magic -> should be 69426942 (42 and 69, guess that is "best of both worlds" :-) )
  UINT32 iMagic;
  I3MFile.ReadData(iMagic, false);
  if (iMagic != 69426942) {
    I3MFile.Close();
    T_ERROR("This is not a valid I3M file %s", strSourceFilename.c_str());
    return false;
  }
  MESSAGE("I3M Magic OK");

  // get version number -> should be 1 at present
  UINT32 iVersion;
  I3MFile.ReadData(iVersion, false);
  if (iVersion != 1) {
    I3MFile.Close();
    T_ERROR("Unsuported I3M version in file %s", strSourceFilename.c_str());
    return false;
  }
  MESSAGE("I3M Version OK");

  // get volume size -> every dimension must be 128 or less 
  I3MFile.ReadData(vVolumeSize.x, false);
  I3MFile.ReadData(vVolumeSize.y, false);
  I3MFile.ReadData(vVolumeSize.z, false);
  if (vVolumeSize.x > 128 || vVolumeSize.y > 128 || vVolumeSize.z > 128) {
    I3MFile.Close();
    T_ERROR("Invalid volume size detected in I3M file %s", strSourceFilename.c_str());
    return false;
  }
  MESSAGE("Volume Size (%i x %i x %i) in I3M file OK", vVolumeSize.x, vVolumeSize.y, vVolumeSize.z);

  // at this point we can check if the file has the correct size
  if (  8*4                     /* eight 32bit fields in the header */ 
      + 4*vVolumeSize.volume()  /* four component 8bit volume */ 
      != ulFileLength) {
    I3MFile.Close();
    T_ERROR("The size of the I3M file %s does not match the information in its header.", strSourceFilename.c_str());
    return false;
  }
  MESSAGE("File Size (%i) of I3M file OK", int(ulFileLength));
  
  // get volume aspect 
  I3MFile.ReadData(vVolumeAspect.x, false);
  I3MFile.ReadData(vVolumeAspect.y, false);
  I3MFile.ReadData(vVolumeAspect.z, false);
  MESSAGE("Aspect Ration (%g x %g x %g)", vVolumeAspect.x, vVolumeAspect.y, vVolumeAspect.z);

  // header is completed all test passed, now we can read the volume, simply copy every fourth byte to the target file

  MESSAGE("I3M File header scan completed, converting volume...");

  strIntermediateFile = strTempDir+SysTools::GetFilename(strSourceFilename)+".temp";

  LargeRAWFile RAWFile(strIntermediateFile, 0);
  RAWFile.Create();

  if (!RAWFile.IsOpen()) {
    T_ERROR("Unable to open intermediate file %s", strIntermediateFile.c_str());
    I3MFile.Close();
    return false;
  }

 
  unsigned char* pData = new unsigned char[4*vVolumeSize.volume()];
  // read the 4D vectors
  I3MFile.ReadRAW(pData, 4*vVolumeSize.volume());
  I3MFile.Close();
  // compress in-place
  for (UINT32 i = 1;i<vVolumeSize.volume();i++) pData[i] = pData[3+i*4];
  // write to target file
  RAWFile.WriteRAW(pData, vVolumeSize.volume());
  delete pData;
  RAWFile.Close();

  MESSAGE("Intermediate RAW file %s from I3M file %s created.", strIntermediateFile.c_str(), strSourceFilename.c_str());

  return true;
}

void I3MConverter::Compute8BitGradientVolumeInCore(unsigned char* pSourceData, unsigned char* pTargetData, const UINTVECTOR3& vSize) {
  for (size_t z = 0;z<size_t(vSize[2]);z++) {
    for (size_t y = 0;y<size_t(vSize[1]);y++) {
      for (size_t x = 0;x<size_t(vSize[0]);x++) {

        // compute 3D positions
        size_t iCenter = x+size_t(vSize[0])*y+size_t(vSize[0])*size_t(vSize[1])*z;
        size_t iLeft   = iCenter;
        size_t iRight  = iCenter;
        size_t iTop    = iCenter;
        size_t iBottom = iCenter;
        size_t iFront  = iCenter;
        size_t iBack   = iCenter;

        FLOATVECTOR3 vScale(0,0,0);

        // handle borders
        if (x > 0)          {iLeft   = iCenter-1; vScale.x++;}
        if (x < vSize[0]-1) {iRight  = iCenter+1; vScale.x++;}
        if (y > 0)          {iTop    = iCenter-size_t(vSize[0]);vScale.y++;}
        if (y < vSize[1]-1) {iBottom = iCenter+size_t(vSize[0]);vScale.y++;}
        if (z > 0)          {iFront  = iCenter-size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}
        if (z < vSize[2]-1) {iBack   = iCenter+size_t(vSize[0])*size_t(vSize[1]);vScale.z++;}

        // compte central differences
        FLOATVECTOR3 vGradient((float(pSourceData[iLeft]) -float(pSourceData[iRight]) )/(255*vScale.x),
                               (float(pSourceData[iTop])  -float(pSourceData[iBottom]))/(255*vScale.y),
                               (float(pSourceData[iFront])-float(pSourceData[iBack])  )/(255*vScale.z));
        // safe normalize 
        vGradient.normalize(0);

        // quantize to 8bit
        VECTOR4<unsigned char>  vUCharGradient = VECTOR3<unsigned char>((vGradient * 127)+127);

        // store in expanded format
        pTargetData[0+iCenter*4] = vUCharGradient.x;
        pTargetData[1+iCenter*4] = vUCharGradient.y;
        pTargetData[2+iCenter*4] = vUCharGradient.z;
        pTargetData[3+iCenter*4] = pSourceData[iCenter];
      }
    }
  }
}

bool I3MConverter::ConvertToNative(const std::string& strRawFilename, 
                                   const std::string& strTargetFilename, UINT64 iHeaderSkip,
                                   UINT64 iComponentSize, UINT64 iComponentCount, bool bSigned,
                                   bool bFloatingPoint, UINTVECTOR3 vVolumeSize, 
                                   FLOATVECTOR3 vVolumeAspect, bool ) {

  // some fitness checks first
  if (iComponentCount!=1) {
    T_ERROR("I3M only supports scalar data");
    return false;
  }
  if (iComponentSize!=8 && iComponentSize!=16) {
    T_ERROR("Only 8 and 16 bit data support for I3M conversion at the moment.");
    return false;
  }

  // next check the quantization and endianess of the volume
  // if it is not 8bit unsigned char, little endian -> convert it

  // TODO                                   
  if (iComponentSize!=8) {
    T_ERROR("Implementation in progress currently only 8 data support for I3M files.");
    return false;
  }
                                     
  // next check is size of the volume, if a dimension is bigger than 
  // 128 -> downsample the volume, otherwise simply copy

  LargeRAWFile SourceRAWFile(strRawFilename, iHeaderSkip);
  SourceRAWFile.Open(false);

  if (!SourceRAWFile.IsOpen()) {
    T_ERROR("Unable to read source file %s", strRawFilename.c_str());
    return false;
  }

  FLOATVECTOR3 vfDownSampleFactor = FLOATVECTOR3(vVolumeSize)/128.0f;

  unsigned char* pDenseData = NULL;
  UINTVECTOR3 vI3MVolumeSize;
  if (vfDownSampleFactor.x <= 1 && vfDownSampleFactor.y <= 1 && vfDownSampleFactor.z <= 1) {
    // volume is small enougth -> simply read the data into the array
    vI3MVolumeSize = vVolumeSize;
    pDenseData = new unsigned char[vI3MVolumeSize.volume()];
    SourceRAWFile.ReadRAW(pDenseData, vI3MVolumeSize.volume());
  } else {
    T_ERROR("Implementation in progress ... currently no downsample support for I3M files.");
    SourceRAWFile.Close();
    return false;

    // TODO
/*

    // volume has to be downsampled
    UINTVECTOR3 viDownSampleFactor(UINT32(ceil(vfDownSampleFactor.x)), 
                                   UINT32(ceil(vfDownSampleFactor.y)), 
                                   UINT32(ceil(vfDownSampleFactor.z)));
    vI3MVolumeSize = vVolumeSize/viDownSampleFactor;
    pDenseData = new unsigned char[vI3MVolumeSize.volume()];

*/

  }
  SourceRAWFile.Close();  
  // compute the gradients and expand data to vector format
  unsigned char* pData = new unsigned char[4*vI3MVolumeSize.volume()];
  Compute8BitGradientVolumeInCore(pDenseData, pData, vI3MVolumeSize);
  delete pDenseData;

  // write data to file
  LargeRAWFile TargetI3MFile(strTargetFilename, 0);
  TargetI3MFile.Create();

  if (!TargetI3MFile.IsOpen()) {
    T_ERROR("Unable to open I3M file %s", strTargetFilename.c_str());
    delete pData;
    return false;
  }

  MESSAGE("Writing header information to disk");

  // magic
  TargetI3MFile.WriteData<UINT32>(69426942, false);
  // version
  TargetI3MFile.WriteData<UINT32>(1, false);
  // (subsampled) domain size
  TargetI3MFile.WriteData(vI3MVolumeSize.x, false);
  TargetI3MFile.WriteData(vI3MVolumeSize.y, false);
  TargetI3MFile.WriteData(vI3MVolumeSize.z, false);
  // aspect ratio
  TargetI3MFile.WriteData(vVolumeAspect.x, false);
  TargetI3MFile.WriteData(vVolumeAspect.y, false);
  TargetI3MFile.WriteData(vVolumeAspect.z, false);

  MESSAGE("Writing volume to disk");

  TargetI3MFile.WriteRAW(pData, 4*vI3MVolumeSize.volume());

  TargetI3MFile.Close();
  delete pData;

  return true;
}
