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
  \file    AbstrConverter.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    December 2008
*/

#include "AbstrConverter.h"
#include "IOManager.h"  // for the size defines
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>

using namespace std;

const string AbstrConverter::QuantizeShortTo12Bits(UINT64 iHeaderSkip, const string& strFilename, const string& strTargetFilename, size_t iSize) {
  LargeRAWFile InputData(strFilename);
  InputData.Open(false);

  if (!InputData.IsOpen()) return "";

  InputData.SeekPos(iHeaderSkip);

  // determine max and min
  unsigned short iMax = 0;
  unsigned short iMin = 65535;
  unsigned short* pInData = new unsigned short[BRICKSIZE*BRICKSIZE*BRICKSIZE*2];
  UINT64 iPos = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, BRICKSIZE*BRICKSIZE*BRICKSIZE*2)/2;

    for (size_t i = 0;i<iRead;i++) {
      if ( iMax < pInData[i])
        iMax = pInData[i];
      if (iMin > pInData[i]) 
        iMin = pInData[i];
    }

    iPos += UINT64(iRead);

    if (iMin == 0 && iMax == 65535) break;
  }

  // if file uses less or equal than 12 bits quit here
  if (iMax < 4096) {
    delete [] pInData;
    InputData.Close();
    return strFilename;
  }

  // otherwise quantize
  LargeRAWFile OutputData(strTargetFilename);
  OutputData.Create(iSize*2);

  if (!OutputData.IsOpen()) {
    delete [] pInData;
    InputData.Close();
    return "";
  }


  UINT64 iRange = iMax-iMin;
  
  InputData.SeekPos(iHeaderSkip);
  iPos = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, BRICKSIZE*BRICKSIZE*BRICKSIZE*2)/2;
    for (size_t i = 0;i<iRead;i++) {
      pInData[i] = min<unsigned short>(4095, (unsigned short)((UINT64(pInData[i]-iMin) * 4095)/iRange));
    }
    iPos += UINT64(iRead);

    OutputData.WriteRAW((unsigned char*)pInData, 2*iRead);
  }

  delete [] pInData;
  OutputData.Close();
  InputData.Close();

  return strTargetFilename;
}

const string AbstrConverter::QuantizeFloatTo12Bits(UINT64 iHeaderSkip, const string& strFilename, const string& strTargetFilename, size_t iSize) {
  LargeRAWFile InputData(strFilename);
  InputData.Open(false);

  if (!InputData.IsOpen()) return "";

  InputData.SeekPos(iHeaderSkip);

  // determine max and min
  float fMax = -numeric_limits<float>::max();
  float fMin = numeric_limits<float>::max();
  float* pInData = new float[BRICKSIZE*BRICKSIZE*BRICKSIZE*2];
  UINT64 iPos = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, BRICKSIZE*BRICKSIZE*BRICKSIZE*4)/4;

    for (size_t i = 0;i<iRead;i++) {
      if (fMax < pInData[i]) fMax = pInData[i];
      if (fMin > pInData[i]) fMin = pInData[i];
    }

    iPos += UINT64(iRead);
  }

  // quantize
  LargeRAWFile OutputData(strTargetFilename);
  OutputData.Create(iSize*2);

  if (!OutputData.IsOpen()) {
    delete [] pInData;
    InputData.Close();
    return "";
  }

  float fQuantFact = 4095.0f / float(fMax-fMin);
  unsigned short* pOutData = new unsigned short[BRICKSIZE*BRICKSIZE*BRICKSIZE*2];
  
  InputData.SeekPos(iHeaderSkip);
  iPos = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, BRICKSIZE*BRICKSIZE*BRICKSIZE*4)/4;
    for (size_t i = 0;i<iRead;i++) {
      pOutData[i] = min<unsigned short>(4095, ((pInData[i]-fMin) * fQuantFact));
    }
    iPos += UINT64(iRead);

    OutputData.WriteRAW((unsigned char*)pOutData, 2*iRead);
  }

  delete [] pInData;
  delete [] pOutData;
  OutputData.Close();
  InputData.Close();

  return strTargetFilename;
}

