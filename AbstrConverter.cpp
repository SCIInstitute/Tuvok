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
#include <cctype>
#include <iterator>
#include <string>
#include <vector>
#include "AbstrConverter.h"
#include "Basics/SysTools.h"
#include "Controller/Controller.h"
#include "IOManager.h"  // for the size defines
#include "Quantize.h"
#include "TuvokIOError.h"
#include "TuvokSizes.h"
#include "UVF/Histogram1DDataBlock.h"

using namespace tuvok;

bool AbstrConverter::CanRead(const std::string& fn,
                             const std::vector<int8_t>&) const
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


/// @returns true if we generated 'strTargetFilename'.
bool
AbstrConverter::Process8Bits(LargeRAWFile& InputData,
                             const std::string& strTargetFilename,
                             uint64_t iSize, bool bSigned,
                             Histogram1DDataBlock* Histogram1D) {
  size_t iCurrentInCoreSize = GetIncoreSize();

  uint64_t iPercent = iSize / 100;

  if (!InputData.IsOpen()) return false;

  std::vector<uint64_t> aHist(256);
  std::fill(aHist.begin(), aHist.end(), 0);

  bool generated_file = false;
  if (bSigned)  {
    MESSAGE("Changing signed to unsigned char and computing 1D histogram...");
    LargeRAWFile OutputData(strTargetFilename);
    OutputData.Create(iSize);

    if (!OutputData.IsOpen()) {
      T_ERROR("Failed opening/creating '%s'", strTargetFilename.c_str());
      InputData.Close();
      return false;
    }

    signed char* pInData = new signed char[iCurrentInCoreSize];

    uint64_t iPos = 0;
    while (iPos < iSize)  {
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData, iCurrentInCoreSize);
      if (iRead == 0) break;

      for (size_t i = 0;i<iRead;i++) {
        pInData[i] += 128;
        if (Histogram1D) aHist[(unsigned char)pInData[i]]++;
      }
      OutputData.WriteRAW((unsigned char*)pInData, iRead);
      iPos += uint64_t(iRead);
    }

    if (iPos < iSize) {
      WARNING("Specified size and real datasize mismatch");
    }

    delete [] pInData;
    generated_file = true;
    OutputData.Close();
  } else {
    if (Histogram1D) {
      MESSAGE("Computing 1D Histogram...");
      unsigned char* pInData = new unsigned char[iCurrentInCoreSize];

      uint64_t iPos = 0;
      uint64_t iDivLast = 0;
      while (iPos < iSize)  {
        size_t iRead = InputData.ReadRAW((unsigned char*)pInData,
                                         iCurrentInCoreSize);
        if (iRead == 0) break;
        std::for_each(pInData, pInData+iRead,
                      [&](unsigned char i) { ++aHist[i]; });
        iPos += uint64_t(iRead);

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
    generated_file = false;
  }

  if ( Histogram1D ) Histogram1D->SetHistogram(aHist);

  return generated_file;
}

size_t AbstrConverter::GetIncoreSize() {
  if (Controller::Instance().IOMan())
    return size_t(Controller::Instance().IOMan()->GetIncoresize());
  else
    return DEFAULT_INCORESIZE;
}

bool
AbstrConverter::QuantizeTo8Bit(LargeRAWFile& rawfile,
                               const std::string& strTargetFilename,
                               uint64_t iComponentSize,
                               uint64_t iSize,
                               bool bSigned,
                               bool bIsFloat,
                               Histogram1DDataBlock* Histogram1D) {
  bool generated_target = false;
  if(!rawfile.IsOpen()) {
    T_ERROR("Could not open '%s' for 8bit quantization.",
            rawfile.GetFilename().c_str());
    return false;
  }

  BStreamDescriptor bsd;
  bsd.components = 1;
  bsd.width = iComponentSize / 8;
  bsd.elements = iSize / iComponentSize;
  bsd.is_signed = bSigned;
  bsd.fp = bIsFloat;
  // at this point the stream should be in whatever the native form of the
  // system is.
  bsd.big_endian = EndianConvert::IsBigEndian();
  bsd.timesteps = 1;

  switch (iComponentSize) {
    case 8:
      generated_target = Process8Bits(rawfile, strTargetFilename, iSize,
                                      bSigned, Histogram1D);
      break;
    case 16 :
      if(bSigned) {
        generated_target =
          Quantize<short, unsigned char>(rawfile, bsd, strTargetFilename,
                                         Histogram1D);
      } else {
        generated_target =
          Quantize<unsigned short, unsigned char>(
            rawfile, bsd, strTargetFilename, Histogram1D
          );
      }
      break;
    case 32 :
      if (bIsFloat) {
        generated_target =
          Quantize<float, unsigned char>(rawfile, bsd, strTargetFilename,
                                         Histogram1D);
      } else {
        if(bSigned) {
          generated_target =
            Quantize<int, unsigned char>(rawfile, bsd, strTargetFilename,
                                         Histogram1D);
        } else {
          generated_target =
            Quantize<unsigned, unsigned char>(rawfile, bsd, strTargetFilename,
                                              Histogram1D);
        }
      }
      break;
    case 64 :
      if (bIsFloat) {
        generated_target =
          Quantize<double, unsigned char>(rawfile, bsd, strTargetFilename,
                                          Histogram1D);
      } else {
        if(bSigned) {
          generated_target = Quantize<int64_t, unsigned char>(
            rawfile, bsd, strTargetFilename, Histogram1D
          );
        } else {
          generated_target =
            Quantize<uint64_t, unsigned char>(rawfile, bsd, strTargetFilename,
                                              Histogram1D);
        }
      }
      break;
  }

  return generated_target;
}
