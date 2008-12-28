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
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \date    December 2008
*/

#include "AbstrConverter.h"
#include "IOManager.h"  // for the size defines
#include <Controller/MasterController.h>
#include <Basics/SysTools.h>

using namespace std;

const string AbstrConverter::Process8BitsTo8Bits(UINT64 iHeaderSkip, const string& strFilename, const string& strTargetFilename, size_t iSize, bool bSigned, Histogram1DDataBlock& Histogram1D, MasterController* m_pMasterController) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);

  if (!InputData.IsOpen()) return "";

  vector<UINT64> aHist(256); 
  for (vector<UINT64>::iterator i = aHist.begin();i<aHist.end();i++) (*i) = 0;

  string strSignChangedFile;
  if (bSigned)  {
    m_pMasterController->DebugOut()->Message("AbstrConverter::Process8BitsTo8Bits","Changing signed to unsigned char and computing 1D histogram...");
    LargeRAWFile OutputData(strTargetFilename);
    OutputData.Create(iSize);

    if (!OutputData.IsOpen()) {
      InputData.Close();
      return "";
    }

    signed char* pInData = new signed char[INCORESIZE];

    UINT64 iPos = 0;
    while (iPos < iSize)  {
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE);
      for (size_t i = 0;i<iRead;i++) {
        pInData[i] += 128;
        aHist[pInData[i]]++;
      }
      OutputData.WriteData((unsigned char*)pInData, iRead);
      iPos += UINT64(iRead);
    }
    delete [] pInData;
    strSignChangedFile = strTargetFilename;
    OutputData.Close();
  } else {
    m_pMasterController->DebugOut()->Message("AbstrConverter::Process8BitsTo8Bits","Computing 1D Histogram...");

    unsigned char* pInData = new unsigned char[INCORESIZE];

    UINT64 iPos = 0;
    while (iPos < iSize)  {
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE);
      for (size_t i = 0;i<iRead;i++) aHist[pInData[i]]++;
      iPos += UINT64(iRead);
    }
    delete [] pInData;
    strSignChangedFile = strFilename;
  }

  InputData.Close();
  Histogram1D.SetHistogram(aHist);

  return strSignChangedFile;
}


const string AbstrConverter::QuantizeShortTo12Bits(UINT64 iHeaderSkip, const string& strFilename, const string& strTargetFilename, size_t iSize, bool bSigned, Histogram1DDataBlock& Histogram1D, MasterController* m_pMasterController) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);

  if (!InputData.IsOpen()) return "";

  vector<UINT64> aHist(4096); 
  for (vector<UINT64>::iterator i = aHist.begin();i<aHist.end();i++) (*i) = 0;

  // determine max and min
  unsigned short iMax = 0;
  unsigned short iMin = numeric_limits<unsigned short>::max();
  short* pInData = new short[INCORESIZE];
  UINT64 iPos = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*2)/2;

    for (size_t i = 0;i<iRead;i++) {
      unsigned short iValue = (bSigned) ? pInData[i] + numeric_limits<short>::max() : pInData[i];
      if (iMax < iValue)  iMax = iValue;
      if (iMin > iValue)  iMin = iValue;
      if (iMax < 4096)    aHist[iValue]++;
    }

    iPos += UINT64(iRead);

    if (iMin == 0 && iMax == 65535) break;
  }

  string strQuantFile;
  // if file uses less or equal than 12 bits quit here
  if (iMax < 4096) {
    m_pMasterController->DebugOut()->Message("AbstrConverter::QuantizeShortTo12Bits","No quantization required (min=%i, max=%i)", iMin, iMax);
    aHist.resize(iMax+1);  // size is the maximum value plus one (the zero value)
    delete [] pInData;
    InputData.Close();
    strQuantFile = strFilename;
  } else {
    if (bSigned) 
      m_pMasterController->DebugOut()->Message("AbstrConverter::QuantizeShortTo12Bits","Quantizating to 12 bit (input data has range from %i to %i)", int(iMin)-numeric_limits<short>::max(), int(iMax)-numeric_limits<short>::max());
    else
      m_pMasterController->DebugOut()->Message("AbstrConverter::QuantizeShortTo12Bits","Quantizating to 12 bit (input data has range from %i to %i)", iMin, iMax);
    for (vector<UINT64>::iterator i = aHist.begin();i<aHist.end();i++) (*i) = 0;

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
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*2)/2;
      for (size_t i = 0;i<iRead;i++) {
        unsigned short iValue = (bSigned) ? pInData[i] + numeric_limits<short>::max() : pInData[i];
        unsigned short iNewVal = min<unsigned short>(4095, (unsigned short)((UINT64(iValue-iMin) * 4095)/iRange));
        pInData[i] = iNewVal;
        aHist[iNewVal]++;
      }
      iPos += UINT64(iRead);

      OutputData.WriteRAW((unsigned char*)pInData, 2*iRead);
    }

    delete [] pInData;
    OutputData.Close();
    InputData.Close();

    strQuantFile = strTargetFilename;
  }

  Histogram1D.SetHistogram(aHist);

  return strQuantFile;
}

const string AbstrConverter::QuantizeFloatTo12Bits(UINT64 iHeaderSkip, const string& strFilename, const string& strTargetFilename, size_t iSize, Histogram1DDataBlock& Histogram1D, MasterController* m_pMasterController) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);

  if (!InputData.IsOpen()) return "";

  // determine max and min
  float fMax = -numeric_limits<float>::max();
  float fMin = numeric_limits<float>::max();
  float* pInData = new float[INCORESIZE];
  UINT64 iPos = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*4)/4;

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

  m_pMasterController->DebugOut()->Message("AbstrConverter::QuantizeFloatTo12Bits","Quantizating to 12 bit (input data has range from %g to %g)", fMin, fMax);

  float fQuantFact = 4095.0f / float(fMax-fMin);
  unsigned short* pOutData = new unsigned short[INCORESIZE];
  
  vector<UINT64> aHist(4096); 
  for (vector<UINT64>::iterator i = aHist.begin();i<aHist.end();i++) (*i) = 0;

  InputData.SeekPos(iHeaderSkip);
  iPos = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*4)/4;
    for (size_t i = 0;i<iRead;i++) {
      unsigned short iNewVal = min<unsigned short>(4095, ((pInData[i]-fMin) * fQuantFact));
      pOutData[i] = iNewVal;
      aHist[iNewVal]++;
    }
    iPos += UINT64(iRead);

    OutputData.WriteRAW((unsigned char*)pOutData, 2*iRead);
  }

  delete [] pInData;
  delete [] pOutData;
  OutputData.Close();
  InputData.Close();

  Histogram1D.SetHistogram(aHist);

  return strTargetFilename;
}
