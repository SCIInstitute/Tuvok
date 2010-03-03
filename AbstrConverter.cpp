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

#include "StdTuvokDefines.h"

#include <algorithm>
#include <iterator>
#include <string>
#ifdef DETECTED_OS_WINDOWS
# include <array>
#else
# include <tr1/array>
#endif
#include "AbstrConverter.h"
#include "IOManager.h"  // for the size defines
#include "Controller/Controller.h"
#include "UVF/Histogram1DDataBlock.h"
#include "Quantize.h"

using namespace tuvok;

bool AbstrConverter::CanRead(const std::string& fn,
                             const std::tr1::array<int8_t, 512>&) const
{
  std::string ext = SysTools::GetExt(fn);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 (int(*)(int))std::toupper);
  return SupportedExtension(ext);
}

/// @param ext the extension for the filename
/// @return true if the filename is a supported extension for this converter
bool AbstrConverter::SupportedExtension(const std::string& ext) const
{
  return std::find(m_vSupportedExt.begin(), m_vSupportedExt.end(), ext) !=
          m_vSupportedExt.end();
}


const std::string
AbstrConverter::Process8Bits(UINT64 iHeaderSkip,
                             const std::string& strFilename,
                             const std::string& strTargetFilename,
                             UINT64 iSize, bool bSigned,
                             Histogram1DDataBlock* Histogram1D) {
  size_t iCurrentInCoreSize = GetIncoreSize();

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

    signed char* pInData = new signed char[iCurrentInCoreSize];

    UINT64 iPos = 0;
    while (iPos < iSize)  {
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData, iCurrentInCoreSize);
      if (iRead == 0) break;

      for (size_t i = 0;i<iRead;i++) {
        pInData[i] += 128;
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
      unsigned char* pInData = new unsigned char[iCurrentInCoreSize];

      UINT64 iPos = 0;
      UINT64 iDivLast = 0;
      while (iPos < iSize)  {
        size_t iRead = InputData.ReadRAW((unsigned char*)pInData, iCurrentInCoreSize);
        if (iRead == 0) break;
        for (size_t i = 0;i<iRead;i++) aHist[pInData[i]]++;
        iPos += UINT64(iRead);

        if (iPercent > 1 && (100*iPos)/iSize > iDivLast) {
          MESSAGE("Computing 1D Histogram (%u%% complete)",
                  static_cast<unsigned>((100*iPos)/iSize));
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

size_t AbstrConverter::GetIncoreSize() {
  if (Controller::Instance().IOMan())
    return size_t(Controller::Instance().IOMan()->GetIncoresize());
  else
    return DEFAULT_INCORESIZE;
}

const std::string
AbstrConverter::QuantizeTo8Bit(UINT64 iHeaderSkip,
                               const std::string& strFilename,
                               const std::string& strTargetFilename,
                               UINT64 iComponentSize,
                               UINT64 iSize,
                               bool bSigned,
                               bool bIsFloat,
                               Histogram1DDataBlock* Histogram1D) {
  std::string intermFile = "";
  switch (iComponentSize) {
    case 8:
      intermFile = Process8Bits(iHeaderSkip, strFilename,
                                strTargetFilename, iSize, bSigned,
                                Histogram1D);
      break;
    case 16 :
      if(bSigned) {
        intermFile = Quantize<short, unsigned char>(iHeaderSkip, strFilename,
                                                    strTargetFilename, iSize,
                                                    Histogram1D);
      } else {
        intermFile = Quantize<unsigned short, unsigned char>
                             (iHeaderSkip, strFilename,
                              strTargetFilename, iSize, Histogram1D);
      }
      break;
    case 32 :
      if (bIsFloat) {
        intermFile = Quantize<float, unsigned char>(iHeaderSkip, strFilename,
                                          strTargetFilename, iSize,
                                          Histogram1D);
      } else {
        if(bSigned) {
          intermFile = Quantize<int, unsigned char>(iHeaderSkip, strFilename,
                                          strTargetFilename, iSize, Histogram1D);
        } else {
          intermFile = Quantize<unsigned, unsigned char>(iHeaderSkip, strFilename,
                                               strTargetFilename, iSize,
                                               Histogram1D);
        }
      }
      break;
    case 64 :
      if (bIsFloat) {
        intermFile = Quantize<double, unsigned char>(iHeaderSkip, strFilename,
                                           strTargetFilename, iSize,
                                           Histogram1D);
      } else {
        if(bSigned) {
          intermFile = Quantize<boost::int64_t, unsigned char>(
            iHeaderSkip, strFilename, strTargetFilename, iSize, Histogram1D
          );
        } else {
          intermFile = Quantize<UINT64, unsigned char>(iHeaderSkip, strFilename,
                                             strTargetFilename, iSize,
                                             Histogram1D);
        }
      }
      break;
  }

  return intermFile;
}
