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

#include <algorithm>
#include "AbstrConverter.h"
#include "IOManager.h"  // for the size defines
#include "Controller/Controller.h"
#include "UVF/Histogram1DDataBlock.h"
#include "Quantize.h"

const std::string
AbstrConverter::Process8Bits(UINT64 iHeaderSkip,
                                    const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    UINT64 iSize, bool bSigned,
                                    Histogram1DDataBlock* Histogram1D) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);
  UINT64 iPercent = iSize / 100;

  if (!InputData.IsOpen()) return "";

  std::vector<UINT64> aHist(256);
  std::fill(aHist.begin(), aHist.end(), 0);

  std::string strSignChangedFile;
  if (bSigned)  {
    MESSAGE("Changing signed to unsigned char and computing 1D histogram...");
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
      if (iRead == 0) break;

      for (size_t i = 0;i<iRead;i++) {
        pInData[i] += 127;
        if (Histogram1D) aHist[(unsigned char)pInData[i]]++;
      }
      OutputData.WriteRAW((unsigned char*)pInData, iRead);
      iPos += UINT64(iRead);
    }

    if (iPos < iSize) {
      WARNING("Specified size and real datasize mismatch");
    }

    delete [] pInData;
    strSignChangedFile = strTargetFilename;
    OutputData.Close();
  } else {
    if (Histogram1D) {
      MESSAGE("Computing 1D Histogram...");
      unsigned char* pInData = new unsigned char[INCORESIZE];

      UINT64 iPos = 0;
      UINT64 iDivLast = 0;
      while (iPos < iSize)  {
        size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE);
        if (iRead == 0) break;
        for (size_t i = 0;i<iRead;i++) aHist[pInData[i]]++;
        iPos += UINT64(iRead);

        if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
          MESSAGE("Computing 1D Histogram (%i%% complete)",
                  int((100*iPos)/iSize));
          iDivLast = (100*iPos)/iSize;
        }
      }

      if (iPos < iSize) {
        WARNING("Specified size and real datasize mismatch");
      }

      MESSAGE("1D Histogram complete");
      delete [] pInData;
    }
    strSignChangedFile = strFilename;
  }

  InputData.Close();
  if ( Histogram1D ) Histogram1D->SetHistogram(aHist);

  return strSignChangedFile;
}


const std::string
AbstrConverter::QuantizeShortTo12Bits(UINT64 iHeaderSkip,
                                      const std::string& strFilename,
                                      const std::string& strTargetFilename,
                                      UINT64 iSize, bool bSigned,
                                      Histogram1DDataBlock* Histogram1D) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);
  UINT64 iPercent = iSize / 100;

  if (!InputData.IsOpen()) return "";

  // determine max and min
  unsigned short iMax = 0;
  unsigned short iMin = std::numeric_limits<unsigned short>::max();
  UINT64 iPos = 0;
  UINT64 iDivLast = 0;

  std::vector<UINT64> aHist(4096);
  std::fill(aHist.begin(), aHist.end(), 0);

  if (bSigned) {
    std::pair<short,short> minmax;
    minmax = io_minmax(raw_data_src<short>(InputData),
                       Unsigned12BitHistogram<short>(aHist),
                       TuvokProgress<UINT64>(iSize));

    iMin = minmax.first+std::numeric_limits<short>::max();
    iMax = minmax.second+std::numeric_limits<short>::max();
  } else {
    std::pair<unsigned short,unsigned short> minmax;
    minmax = io_minmax(raw_data_src<unsigned short>(InputData),
                       Unsigned12BitHistogram<unsigned short>(aHist),
                       TuvokProgress<UINT64>(iSize));
    iMin = minmax.first;
    iMax = minmax.second;
  }

  std::string strQuantFile;
  // if file uses less or equal than 12 bits quit here
  if (!bSigned && iMax < 4096) {
    MESSAGE("No quantization required (min=%i, max=%i)", int(iMin), int(iMax));
    aHist.resize(iMax+1);  // size is the maximum value plus one (the zero value)

    InputData.Close();
    strQuantFile = strFilename;
  } else {
    if (bSigned) {
      MESSAGE("Quantizing to 12 bit (input data has range from %i to %i)",
              int(iMin)-std::numeric_limits<short>::max(),
              int(iMax)-std::numeric_limits<short>::max());
    } else {
      MESSAGE("Quantizing to 12 bit (input data has range from %i to %i)",
              int(iMin), int(iMax));
    }
    std::fill(aHist.begin(), aHist.end(), 0);
    short* pInData = new short[INCORESIZE];

    // otherwise quantize
    LargeRAWFile OutputData(strTargetFilename);
    OutputData.Create(iSize*2);

    if (!OutputData.IsOpen()) {
      delete [] pInData;
      InputData.Close();
      return "";
    }

    UINT64 iRange = iMax-iMin;

    InputData.SeekStart();
    iPos = 0;
    iDivLast = 0;
    while (iPos < iSize)  {
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*2)/2;
      if(iRead == 0) { break; } // bail out if the read gave us nothing.

      for (size_t i = 0;i<iRead;i++) {
        unsigned short iValue = (bSigned) ? pInData[i] + std::numeric_limits<short>::max() : pInData[i];
        unsigned short iNewVal = std::min<unsigned short>(4095, (unsigned short)((UINT64(iValue-iMin) * 4095)/iRange));
        pInData[i] = iNewVal;
        aHist[iNewVal]++;
      }
      iPos += UINT64(iRead);

      if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
        if (bSigned) {
          MESSAGE("Quantizing to 12 bit (input data has range from %i to %i)"
                  "\n%i%% complete",
                  int(iMin) - std::numeric_limits<short>::max(),
                  int(iMax) - std::numeric_limits<short>::max(),
                  int((100*iPos)/iSize));
        } else {
          MESSAGE("Quantizing to 12 bit (input data has range from %i to %i)"
                  "\n%i%% complete", int(iMin), int(iMax), int((100*iPos)/iSize));
        }
        iDivLast = (100*iPos)/iSize;
      }

      OutputData.WriteRAW((unsigned char*)pInData, 2*iRead);
    }

    delete [] pInData;
    OutputData.Close();
    InputData.Close();

    strQuantFile = strTargetFilename;
  }

  if (Histogram1D) Histogram1D->SetHistogram(aHist);

  return strQuantFile;
}

/// For signed data, biases and quantizes to 12 bit.
/// For unsigned data, we calculate a quantized histogram, but leave the data
/// intact.
const std::string
AbstrConverter::ProcessShort(UINT64 iHeaderSkip,
                             const std::string& strFilename,
                             const std::string& strTargetFilename,
                             UINT64 iSize, bool bSigned,
                             Histogram1DDataBlock* Histogram1D) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);
  UINT64 iPercent = iSize / 100;

  if (!InputData.IsOpen()) {
    T_ERROR("Could not open input file: %s", strFilename.c_str());
    return "";
  }

  // determine max and min
  unsigned short iMax = 0;
  unsigned short iMin = std::numeric_limits<unsigned short>::max();
  UINT64 iPos = 0;
  UINT64 iDivLast = 0;

  std::vector<UINT64> aHist(4096);
  std::fill(aHist.begin(), aHist.end(), 0);

  if (bSigned) {
    MESSAGE("16bit data is signed");
    std::pair<short,short> minmax;
    minmax = io_minmax(raw_data_src<short>(InputData),
                       Unsigned12BitHistogram<short>(aHist),
                       TuvokProgress<UINT64>(iSize));

    iMin = minmax.first+std::numeric_limits<short>::max();
    iMax = minmax.second+std::numeric_limits<short>::max();
  } else {
    MESSAGE("16bit data is unsigned");
    std::pair<unsigned short,unsigned short> minmax;
    minmax = io_minmax(raw_data_src<unsigned short>(InputData),
                       Unsigned12BitHistogram<unsigned short>(aHist),
                       TuvokProgress<UINT64>(iSize));
    iMin = minmax.first;
    iMax = minmax.second;
  }

  std::string strQuantFile;

  // if the data are signed, we'll need to bias it.  if the data exceed 4096,
  // then we'll need to quantize at least the histogram.
  // In the ideal case, we have unsigned data that fit in 12 bits.  Then our
  // output is simply our input, and we can bail out here.
  bool new_file_required = bSigned || iMax >= 4096;
  if(!new_file_required) {
    MESSAGE("No histogram quantization required (min=%i, max=%i)",
            int(iMin), int(iMax));
    aHist.resize(iMax+1); // size is the maximum value plus one (the zero value)

    if (Histogram1D) {
      Histogram1D->SetHistogram(aHist);
    }
    InputData.Close();
    return strFilename;
  } else {
    LargeRAWFile OutputData(strTargetFilename);

    if (bSigned) {
      MESSAGE("Quantizing data to 12 bit "
              "(input data has range from %i to %i)",
              int(iMin)-std::numeric_limits<short>::max(),
              int(iMax)-std::numeric_limits<short>::max());
      OutputData.Create(iSize*2);
      if (!OutputData.IsOpen()) {
        T_ERROR("Could not open output file %s", strTargetFilename.c_str());
        InputData.Close();
        return "";
      }
    } else {
      MESSAGE("Quantizing histogram to 12 bit "
              "(input data has range from %i to %i)",
              int(iMin), int(iMax));
      strQuantFile = strFilename; // output = input;
    }
    // Old histogram is invalid, as we'll be biasing || quantizing the data.
    std::fill(aHist.begin(), aHist.end(), 0);

    UINT64 iRange = iMax-iMin;

    short* pInData = new short[INCORESIZE];
    InputData.SeekStart();
    iPos = 0;
    iDivLast = 0;
    while (iPos < iSize) {
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*2)/2;
      if(iRead == 0) { break; } // bail out if the read gave us nothing.

      for (size_t i = 0;i<iRead;i++) {
        unsigned short iValue = (bSigned) ?
                                pInData[i] + std::numeric_limits<short>::max()
                              : pInData[i];
        unsigned short iNewVal = std::min<unsigned short>(4095,
                                         static_cast<unsigned short>
                                                    ((static_cast<UINT64>
                                                      (iValue-iMin) * 4095) /
                                                              iRange
                                                    )
                                         );
        if(bSigned) {
          pInData[i] = iNewVal;
        }
        aHist[iNewVal]++;
      }
      iPos += UINT64(iRead);

      if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
        if (bSigned) {
          MESSAGE("Removing sign from 16bit data and "
                  "quantizing histogram to 12 bit\n"
                  "(input data has range from %i to %i)\n%i%% complete",
                  int(iMin) - std::numeric_limits<short>::max(),
                  int(iMax) - std::numeric_limits<short>::max(),
                  int((100*iPos)/iSize));
        } else {
          MESSAGE("Quantizing histogram to 12 bit "
                  "(input data has range from %i to %i)\n%i%% complete",
                  int(iMin), int(iMax), int((100*iPos)/iSize));
        }
        iDivLast = (100*iPos)/iSize;
      }

      if(bSigned) {
        OutputData.WriteRAW((unsigned char*)pInData, 2*iRead);
      }
    }

    delete [] pInData;
    OutputData.Close();
    InputData.Close();

    if(bSigned) {
      strQuantFile = strTargetFilename;
    }
  }

  if (Histogram1D) Histogram1D->SetHistogram(aHist);

  return strQuantFile;
}
const std::string
AbstrConverter::QuantizeFloatTo12Bits(UINT64 iHeaderSkip,
                                      const std::string& strFilename,
                                      const std::string& strTargetFilename,
                                      UINT64 iSize,
                                      Histogram1DDataBlock* Histogram1D) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);
  UINT64 iPercent = iSize / 100;

  if (!InputData.IsOpen()) return "";

  // determine max and min
  float fMax = -std::numeric_limits<float>::max();
  float fMin = std::numeric_limits<float>::max();
  UINT64 iPos = 0;
  UINT64 iDivLast = 0;

  std::vector<UINT64> aHist(4096);
  std::fill(aHist.begin(), aHist.end(), 0);

  std::pair<float,float> minmax;
  minmax = io_minmax(raw_data_src<float>(InputData),
                     Unsigned12BitHistogram<float>(aHist),
                     TuvokProgress<UINT64>(iSize));
  fMin = minmax.first;
  fMax = minmax.second;

  // quantize
  LargeRAWFile OutputData(strTargetFilename);
  OutputData.Create(iSize*2);

  if (!OutputData.IsOpen()) {
    InputData.Close();
    return "";
  }

  float fQuantFact = 4095.0f / float(fMax-fMin);
  float* pInData = new float[INCORESIZE];
  unsigned short* pOutData = new unsigned short[INCORESIZE];

  InputData.SeekStart();
  iPos = 0;
  iDivLast = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*4)/4;
    if(iRead == 0) { break; } // bail out if the read gave us nothing.

    for (size_t i = 0;i<iRead;i++) {
      unsigned short iNewVal = std::min<unsigned short>(4095,
                                   static_cast<unsigned short>
                                   ((pInData[i]-fMin) * fQuantFact));
      pOutData[i] = iNewVal;
      aHist[iNewVal]++;
    }
    iPos += UINT64(iRead);

    if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
      MESSAGE("Quantizing to 12 bit (input data has range from %g to %g)\n"
              "%i%% complete", fMin, fMax, int((100*iPos)/iSize));
      iDivLast = (100*iPos)/iSize;
    }

    OutputData.WriteRAW((unsigned char*)pOutData, 2*iRead);
  }

  delete [] pInData;
  delete [] pOutData;
  OutputData.Close();
  InputData.Close();

  if (Histogram1D) Histogram1D->SetHistogram(aHist);

  return strTargetFilename;
}

const std::string
AbstrConverter::QuantizeDoubleTo12Bits(UINT64 iHeaderSkip,
                                       const std::string& strFilename,
                                       const std::string& strTargetFilename,
                                       UINT64 iSize,
                                       Histogram1DDataBlock* Histogram1D) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);
  UINT64 iPercent = iSize / 100;

  if (!InputData.IsOpen()) return "";

  // determine max and min
  double fMax = -std::numeric_limits<double>::max();
  double fMin = std::numeric_limits<double>::max();
  UINT64 iPos = 0;
  UINT64 iDivLast = 0;

  std::vector<UINT64> aHist(4096);
  std::fill(aHist.begin(), aHist.end(), 0);

  std::pair<double,double> minmax;
  minmax = io_minmax(raw_data_src<double>(InputData),
                     Unsigned12BitHistogram<double>(aHist),
                     TuvokProgress<UINT64>(iSize));
  fMin = minmax.first;
  fMax = minmax.second;

  // quantize
  LargeRAWFile OutputData(strTargetFilename);
  OutputData.Create(iSize*2);

  if (!OutputData.IsOpen()) {
    InputData.Close();
    return "";
  }

  MESSAGE("Quantizing to 12 bit (input data has range from %g to %g)",
          fMin, fMax);

  double fQuantFact = 4095 / (fMax-fMin);
  double* pInData = new double[INCORESIZE];
  unsigned short* pOutData = new unsigned short[INCORESIZE];

  InputData.SeekStart();
  iPos = 0;
  iDivLast = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*8)/8;
    if(iRead == 0) { break; } // bail out if the read gave us nothing.

    for (size_t i = 0;i<iRead;i++) {
      unsigned short iNewVal = std::min<unsigned short>(4095,
                                   static_cast<unsigned short>
                                   ((pInData[i]-fMin) * fQuantFact));
      pOutData[i] = iNewVal;
      aHist[iNewVal]++;
    }
    iPos += UINT64(iRead);

    if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
      MESSAGE("Quantizing to 12 bit (input data has range from %g to %g)\n"
              "%i%% complete", fMin, fMax, int((100*iPos)/iSize));
      iDivLast = (100*iPos)/iSize;
    }

    OutputData.WriteRAW((unsigned char*)pOutData, 2*iRead);
  }

  delete [] pInData;
  delete [] pOutData;
  OutputData.Close();
  InputData.Close();

  if (Histogram1D) Histogram1D->SetHistogram(aHist);

  return strTargetFilename;
}


const std::string
AbstrConverter::QuantizeLongTo12Bits(UINT64 iHeaderSkip,
                                     const std::string& strFilename,
                                     const std::string& strTargetFilename,
                                     UINT64 iSize, bool bSigned,
                                     Histogram1DDataBlock* Histogram1D) {
  AbstrDebugOut &dbg = Controller::Debug::Out();
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);
  UINT64 iPercent = iSize / 100;

  if (!InputData.IsOpen()) return "";

  std::vector<UINT64> aHist(4096);
  std::fill(aHist.begin(), aHist.end(), 0);

  // determine max and min
  UINT64 iMax = 0;
  UINT64 iMin = std::numeric_limits<UINT64>::max();
  UINT64* pInData = new UINT64[INCORESIZE];
  UINT64 iPos = 0;
  UINT64 iDivLast = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*8)/8;
    if (iRead == 0) break;

    for (size_t i = 0;i<iRead;i++) {
      UINT64 iValue = (bSigned) ? pInData[i] + std::numeric_limits<int>::max()
                                : pInData[i];
      if (iMax < iValue)  iMax = iValue;
      if (iMin > iValue)  iMin = iValue;
      if (iMax < 4096) {
        aHist[static_cast<size_t>(iValue)]++;
      }
    }

    iPos += UINT64(iRead);

    if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
      dbg.Message(_func_, "Computing value range (%i%% complete)",
                  int((100*iPos)/iSize));
      iDivLast = (100*iPos)/iSize;
    }

    if (iMin == 0 && iMax == std::numeric_limits<UINT64>::max()) break;
  }

  if (iPos < iSize) {
    dbg.Warning(_func_, "Specified size and real datasize mismatch");
    iSize = iPos;
  }

  std::string strQuantFile;

  if (bSigned)
    dbg.Message(_func_, "Quantizing to 12 bit (input data has range from "
                        "%i to %i)",
                        int(iMin) - std::numeric_limits<int>::max(),
                        int(iMax) - std::numeric_limits<int>::max());
  else
    dbg.Message(_func_, "Quantizing to 12 bit (input data has range from "
                        "%i to %i)", int(iMin), int(iMax));

  std::fill(aHist.begin(), aHist.end(), 0);

  // otherwise quantize
  LargeRAWFile OutputData(strTargetFilename);
  OutputData.Create(iSize*2);

  if (!OutputData.IsOpen()) {
    delete [] pInData;
    InputData.Close();
    return "";
  }

  UINT64 iRange = iMax-iMin;

  InputData.SeekStart();
  iPos = 0;
  iDivLast = 0;
  while (iPos < iSize) {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*8)/8;
    if(iRead == 0) { break; } // bail out if the read gave us nothing.

    for (size_t i = 0; i < iRead; i++) {
      UINT64 iValue = (bSigned) ? pInData[i] + std::numeric_limits<int>::max()
                                : pInData[i];
      // if the range fits into 12 bits do only bias not rescale
      UINT64 iNewVal = (iRange < 4096) ?
                        iValue-iMin :
                        std::min<UINT64>(4095, (UINT32)((UINT64(iValue-iMin) * 4095)/iRange));
      ((unsigned short*)pInData)[i] = (unsigned short)iNewVal;
      aHist[static_cast<size_t>(iNewVal)]++;
    }
    iPos += UINT64(iRead);

    if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
      if (bSigned)
        dbg.Message(_func_, "Quantizing to 12 bit (input data has range "
                    "from %i to %i)\n%i%% complete",
                    int(iMin) - std::numeric_limits<int>::max(),
                    int(iMax) - std::numeric_limits<int>::max(),
                    int((100*iPos)/iSize));
      else
        dbg.Message(_func_, "Quantizing to 12 bit (input data has range "
                    "from %i to %i)\n%i%% complete", int(iMin), int(iMax),
                    int((100*iPos)/iSize));
      iDivLast = (100*iPos)/iSize;
    }

    OutputData.WriteRAW((unsigned char*)pInData, 2*iRead);
  }

  delete [] pInData;
  OutputData.Close();
  InputData.Close();

  strQuantFile = strTargetFilename;

  if (Histogram1D) Histogram1D->SetHistogram(aHist);

  return strQuantFile;
}

const std::string
AbstrConverter::QuantizeIntTo12Bits(UINT64 iHeaderSkip,
                                    const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    UINT64 iSize, bool bSigned,
                                    Histogram1DDataBlock* Histogram1D) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);
  UINT64 iPercent = iSize / 100;

  if (!InputData.IsOpen()) return "";

  std::vector<UINT64> aHist(4096);
  std::fill(aHist.begin(), aHist.end(), 0);

  // determine max and min
  UINT32 iMax = 0;
  UINT32 iMin = std::numeric_limits<UINT32>::max();
  UINT32* pInData = new UINT32[INCORESIZE];
  UINT64 iPos = 0;
  UINT64 iDivLast = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*4)/4;
    if (iRead == 0) break;

    for (size_t i = 0;i<iRead;i++) {
      UINT32 iValue = (bSigned) ? pInData[i] + std::numeric_limits<int>::max() : pInData[i];
      if (iMax < iValue)  iMax = iValue;
      if (iMin > iValue)  iMin = iValue;
      if (iMax < 4096)    aHist[iValue]++;
    }

    iPos += UINT64(iRead);

    if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
      MESSAGE("Computing value range (%i%% complete)",
              int((100*iPos)/iSize));
      iDivLast = (100*iPos)/iSize;
    }

    if (iMin == 0 && iMax == std::numeric_limits<UINT32>::max()) break;
  }

  if (iPos < iSize) {
    WARNING("Specified size and real datasize mismatch");
    iSize = iPos;
  }

  std::string strQuantFile;
  if (bSigned) {
    MESSAGE("Quantizing to 12 bit (input data has range from %i to %i)",
            int(iMin) - std::numeric_limits<int>::max(),
            int(iMax) - std::numeric_limits<int>::max());
  } else {
    MESSAGE("Quantizing to 12 bit (input data has range from %i to %i)",
            int(iMin), int(iMax));
  }
  std::fill(aHist.begin(), aHist.end(), 0);

  // otherwise quantize
  LargeRAWFile OutputData(strTargetFilename);
  OutputData.Create(iSize*2);

  if (!OutputData.IsOpen()) {
    delete [] pInData;
    InputData.Close();
    return "";
  }

  UINT64 iRange = iMax-iMin;

  InputData.SeekStart();
  iPos = 0;
  iDivLast = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*4)/4;
    if(iRead == 0) { break; } // bail out if the read gave us nothing.

    for (size_t i = 0;i<iRead;i++) {
      UINT32 iValue = (bSigned) ? pInData[i] + std::numeric_limits<int>::max() : pInData[i];
      // if the range fits into 12 bits do only bias not rescale
      UINT32 iNewVal = (iRange < 4096) ? iValue-iMin : std::min<UINT32>(4095, (UINT32)((UINT64(iValue-iMin) * 4095)/iRange));
      ((unsigned short*)pInData)[i] = (unsigned short)iNewVal;
      aHist[iNewVal]++;
    }
    iPos += UINT64(iRead);

    if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
      if (bSigned) {
        MESSAGE("Quantizing to 12 bit (input data has range from %i to %i)"
                "\n%i%% complete",
                int(iMin) - std::numeric_limits<int>::max(),
                int(iMax) - std::numeric_limits<int>::max(),
                int((100*iPos)/iSize));
      } else {
        MESSAGE("Quantizing to 12 bit (input data has range from %i to %i)"
                "\n%i%% complete", int(iMin), int(iMax), int((100*iPos)/iSize));
      }
      iDivLast = (100*iPos)/iSize;
    }

    OutputData.WriteRAW((unsigned char*)pInData, 2*iRead);
  }

  delete [] pInData;
  OutputData.Close();
  InputData.Close();

  strQuantFile = strTargetFilename;

  if (Histogram1D) Histogram1D->SetHistogram(aHist);

  return strQuantFile;
}

/********************************************************************/

const std::string
AbstrConverter::QuantizeShortTo8Bits(UINT64 iHeaderSkip,
                                     const std::string& strFilename,
                                     const std::string& strTargetFilename,
                                     UINT64 iSize, bool bSigned,
                                     Histogram1DDataBlock* Histogram1D) {
  LargeRAWFile InputData(strFilename, iHeaderSkip);
  InputData.Open(false);
  UINT64 iPercent = iSize / 100;

  if (!InputData.IsOpen()) return "";

  std::vector<UINT64> aHist(4096);
  std::fill(aHist.begin(), aHist.end(), 0);

  // determine max and min
  unsigned short iMax = 0;
  unsigned short iMin = std::numeric_limits<unsigned short>::max();
  short* pInData = new short[INCORESIZE];
  unsigned char* pOutData = new unsigned char[INCORESIZE];
  UINT64 iPos = 0;
  UINT64 iDivLast = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*2)/2;
    if (iRead == 0) break;

    for (size_t i = 0;i<iRead;i++) {
      unsigned short iValue = (bSigned) ? pInData[i] + std::numeric_limits<short>::max() : pInData[i];
      if (iMax < iValue)  iMax = iValue;
      if (iMin > iValue)  iMin = iValue;
    }

    iPos += UINT64(iRead);

    if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
      MESSAGE("Computing value range (%i%% complete)",
              int((100*iPos)/iSize));
      iDivLast = (100*iPos)/iSize;
    }

    if (iMin == 0 && iMax == 65535) break;
  }

  if (iPos < iSize) {
    WARNING("Specified size and real datasize mismatch");
    iSize = iPos;
  }

  if (bSigned) {
    MESSAGE("Quantizing to 8 bit (input data has range from %i to %i)",
            int(iMin)-std::numeric_limits<short>::max(),
            int(iMax)-std::numeric_limits<short>::max());
  } else {
    MESSAGE("Quantizing to 8 bit (input data has range from %i to %i)",
            int(iMin), int(iMax));
  }
  std::fill(aHist.begin(), aHist.end(), 0);

  // quantize
  LargeRAWFile OutputData(strTargetFilename);
  OutputData.Create(iSize);

  if (!OutputData.IsOpen()) {
    delete [] pInData;
    delete [] pOutData;
    InputData.Close();
    return "";
  }

  UINT64 iRange = iMax-iMin;

  InputData.SeekStart();
  iPos = 0;
  iDivLast = 0;
  while (iPos < iSize)  {
    size_t iRead = InputData.ReadRAW((unsigned char*)pInData, INCORESIZE*2)/2;
    if(iRead == 0) { break; } // bail out if the read gave us nothing.

    for (size_t i = 0;i<iRead;i++) {
      unsigned short iValue = (bSigned) ? pInData[i] + std::numeric_limits<short>::max() : pInData[i];
      unsigned char iNewVal = std::min<unsigned char>(255,
                                      (unsigned char)((UINT64(iValue-iMin) * 255)/iRange));
      pOutData[i] = iNewVal;
      aHist[iNewVal]++;
    }
    iPos += UINT64(iRead);

    if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
      if (bSigned) {
        MESSAGE("Quantizing to 8 bit (input data has range from %i to %i)"
                "\n%i%% complete",
                int(iMin) - std::numeric_limits<short>::max(),
                int(iMax) - std::numeric_limits<short>::max(),
                int((100*iPos)/iSize));
      } else {
        MESSAGE("Quantizing to 8 bit (input data has range from %i to %i)"
                "\n%i%% complete", int(iMin), int(iMax), int((100*iPos)/iSize));
      }
      iDivLast = (100*iPos)/iSize;
    }

    OutputData.WriteRAW((unsigned char*)pOutData, iRead);
  }

  delete [] pInData;
  delete [] pOutData;

  OutputData.Close();
  InputData.Close();


  if (Histogram1D) Histogram1D->SetHistogram(aHist);

  return strTargetFilename;
}

const std::string
AbstrConverter::QuantizeFloatTo8Bits(UINT64 iHeaderSkip,
                                     const std::string& strFilename,
                                     const std::string& strTargetFilename,
                                     UINT64 iSize,
                                     Histogram1DDataBlock* Histogram1D) {
  /// \todo doing 2 quantizations is neither the most efficient nor the numerically best way
  ///       but it will do the trick until we templatize these methods

  std::string intermFile = QuantizeFloatTo12Bits(iHeaderSkip,
                                                 strFilename,
                                                 strTargetFilename+".temp",
                                                 iSize);

  return QuantizeShortTo8Bits(0,intermFile,strTargetFilename,iSize,false,Histogram1D);
}

const std::string
AbstrConverter::QuantizeDoubleTo8Bits(UINT64 iHeaderSkip,
                                      const std::string& strFilename,
                                      const std::string& strTargetFilename,
                                      UINT64 iSize,
                                      Histogram1DDataBlock* Histogram1D) {
  /// \todo doing 2 quantizations is neither the most efficient nor the numerically best way
  ///       but it will do the trick until we templatize these methods

  std::string intermFile = QuantizeDoubleTo12Bits(iHeaderSkip,
                                                  strFilename,
                                                  strTargetFilename+".temp",
                                                  iSize);

  return QuantizeShortTo8Bits(0,intermFile,strTargetFilename,iSize,false,Histogram1D);
}


const std::string
AbstrConverter::QuantizeLongTo8Bits(UINT64 iHeaderSkip,
                                    const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    UINT64 iSize, bool bSigned,
                                    Histogram1DDataBlock* Histogram1D) {
  /// \todo doing 2 quantizations is neither the most efficient nor the numerically best way
  ///       but it will do the trick until we templatize these methods

  std::string intermFile = QuantizeLongTo12Bits(iHeaderSkip,
                                                strFilename,
                                                strTargetFilename+".temp",
                                                iSize,
                                                bSigned);

  return QuantizeShortTo8Bits(0,intermFile,strTargetFilename,iSize,false,Histogram1D);
}

const std::string
AbstrConverter::QuantizeIntTo8Bits(UINT64 iHeaderSkip,
                                   const std::string& strFilename,
                                   const std::string& strTargetFilename,
                                   UINT64 iSize, bool bSigned,
                                   Histogram1DDataBlock* Histogram1D) {
  /// \todo doing 2 quantizations is neither the most efficient nor the numerically best way
  ///       but it will do the trick until we templatize these methods

  std::string intermFile = QuantizeIntTo12Bits(iHeaderSkip,
                                               strFilename,
                                               strTargetFilename+".temp",
                                               iSize,
                                               bSigned);

  return QuantizeShortTo8Bits(0,intermFile,strTargetFilename,iSize,false,Histogram1D);
}
