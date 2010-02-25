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
  \file    AbstrConverter.h
  \author  Jens Krueger
           SCI Institute
           University of Utah
  \version 1.0
  \date    December 2008
*/
#pragma once

#ifndef ABSTRCONVERTER_H
#define ABSTRCONVERTER_H

#include "../StdTuvokDefines.h"
#include "boost/cstdint.hpp"

#include "Basics/Vectors.h"
#include "Controller/Controller.h"
#include "Quantize.h"
#include "UVF/UVF.h"
#include "UVF/Histogram1DDataBlock.h"

using boost::int64_t;
class Histogram1DDataBlock;

class RangeInfo {
public:
  UINT64VECTOR3               m_vDomainSize;
  FLOATVECTOR3                m_vAspect;
  UINT64                      m_iComponentSize;
  int                         m_iValueType;
  std::pair<double, double>   m_fRange;
  std::pair<int64_t, int64_t> m_iRange;
  std::pair<UINT64, UINT64>   m_uiRange;
};

class AbstrConverter {
public:
  virtual ~AbstrConverter() {}

  virtual bool ConvertToUVF(const std::string& strSourceFilename, const std::string& strTargetFilename,
                            const std::string& strTempDir,
                            const bool bNoUserInteraction,
                            const UINT64 iTargetBrickSize,
                            const UINT64 iTargetBrickOverlap,
                            const bool bQuantizeTo8Bit) = 0;

  virtual bool ConvertToUVF(const std::list<std::string>& files,
                            const std::string& strTargetFilename,
                            const std::string& strTempDir,
                            const bool bNoUserInteraction,
                            const UINT64 iTargetBrickSize,
                            const UINT64 iTargetBrickOverlap,
                            const bool bQuantizeTo8Bit) = 0;

  virtual bool ConvertToRAW(const std::string& strSourceFilename,
                            const std::string& strTempDir, bool bNoUserInteraction,
                            UINT64& iHeaderSkip, UINT64& iComponentSize, UINT64& iComponentCount,
                            bool& bConvertEndianess, bool& bSigned, bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                            FLOATVECTOR3& vVolumeAspect, std::string& strTitle,
                            UVFTables::ElementSemanticTable& eType, std::string& strIntermediateFile,
                            bool& bDeleteIntermediateFile) = 0;

  virtual bool ConvertToNative(const std::string& strRawFilename, const std::string& strTargetFilename, UINT64 iHeaderSkip,
                               UINT64 iComponentSize, UINT64 iComponentCount, bool bSigned, bool bFloatingPoint,
                               UINT64VECTOR3 vVolumeSize, FLOATVECTOR3 vVolumeAspect,
                               bool bNoUserInteraction,
                               const bool bQuantizeTo8Bit) = 0;

  virtual bool Analyze(const std::string& strSourceFilename, const std::string& strTempDir,
                       bool bNoUserInteraction, RangeInfo& info) = 0;

  const std::vector<std::string>& SupportedExt() {return m_vSupportedExt;}
  virtual const std::string& GetDesc() {return m_vConverterDesc;}

  virtual bool CanExportData() const {return false;}

  template <typename T, typename U>
  static const std::string Quantize(UINT64 iHeaderSkip,
                                    const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    UINT64 iSize,
                                    Histogram1DDataBlock* Histogram1D=0)
  {
    size_t hist_size = 4096;
    if(sizeof(U) == 1) { hist_size = 256; }
    assert(sizeof(U) <= 2);

    const size_t iCurrentInCoreSize = GetIncoreSize();
    const size_t iCurrentInCoreElems = iCurrentInCoreSize / sizeof(T);
    LargeRAWFile InputData(strFilename, iHeaderSkip);
    InputData.Open(false);
    if(!InputData.IsOpen()) {
      T_ERROR("Could not open '%s'", strFilename.c_str());
      return "";
    }

    // figure out min/max
    std::vector<UINT64> aHist(hist_size, 0);
    std::pair<T,T> minmax;
    const size_t sz = (sizeof(U) == 2 ? 4096 : 256);
    minmax = io_minmax(raw_data_src<T>(InputData),
                       UnsignedHistogram<T, sz>(aHist),
                       TuvokProgress<UINT64>(iSize), iCurrentInCoreSize);
    assert(minmax.second >= minmax.first);

    // Unsigned N bit data does not need to be biased/quantized.
    if(!ctti<T>::is_signed && minmax.second < static_cast<T>(hist_size) &&
       sizeof(T) <= ((hist_size == 256) ? 1 : 2)) {
      MESSAGE("Returning early; data does not need processing.");
      InputData.Close();
      return strFilename;
    }
    // Reset the histogram.  We'll be quantizing the data.
    std::fill(aHist.begin(), aHist.end(), 0);

    LargeRAWFile OutputData(strTargetFilename);
    OutputData.Create(iSize*sizeof(unsigned short));

    if(!OutputData.IsOpen()) {
      InputData.Close();
      T_ERROR("Could not create output file '%s'", strTargetFilename.c_str());
      return "";
    }

    int max_output_val = 65535;
    if(hist_size == 256) { max_output_val = 255; }
    double fQuantFact = max_output_val / static_cast<double>(minmax.second -
                                                             minmax.first);
    // Make sure we do not `stretch' the data values, only `compress' them if
    // necessary.
    fQuantFact = std::min(fQuantFact, 1.0);
    double fQuantFactHist = hist_size-1 / static_cast<double>(minmax.second -
                                                              minmax.first);
    fQuantFactHist = std::min(fQuantFactHist, 1.0);

    T* pInData = new T[iCurrentInCoreElems];
    U* pOutData = new U[iCurrentInCoreElems];

    InputData.SeekStart();
    UINT64 iPos = 0;
    UINT64 iLastDisplayedPercent = 0;

    while(iPos < iSize) {
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData,
                                       iCurrentInCoreSize) / sizeof(T);
      if(iRead == 0) { break; } // bail if the read gave us nothing

      // calculate hist + quantize to output file.
      for(size_t i=0; i < iRead; ++i) {
        U iNewVal = std::min<U>(max_output_val,
          static_cast<U>((pInData[i]-minmax.first) * fQuantFact)
        );
        U iHistIndex = std::min<U>(hist_size-1,
                                   static_cast<U>((pInData[i]-minmax.first) *
                                     fQuantFactHist)
        );
        pOutData[i] = iNewVal;
        aHist[iHistIndex]++;
      }
      iPos += static_cast<UINT64>(iRead);

      if((100*iPos)/iSize > iLastDisplayedPercent) {
        std::ostringstream qmsg;
        qmsg << "Quantizing to " << max_output_val
             << "(input data has range from "
             << minmax.first << " to " << minmax.second << ")\n"
             << (100*iPos)/iSize << "% complete";
        MESSAGE("%s", qmsg.str().c_str());
        iLastDisplayedPercent = (100*iPos)/iSize;
      }
      OutputData.WriteRAW(reinterpret_cast<unsigned char*>(pOutData),
                          sizeof(U)*iRead);
    }

    delete[] pInData;
    delete[] pOutData;
    OutputData.Close();
    InputData.Close();

    if(Histogram1D) { Histogram1D->SetHistogram(aHist); }

    return strTargetFilename;
  }

  static const std::string Process8Bits(UINT64 iHeaderSkip, const std::string& strFilename, const std::string& strTargetFilename, UINT64 iSize, bool bSigned, Histogram1DDataBlock* Histogram1D=0);

  static const std::string QuantizeTo8Bit(UINT64 iHeaderSkip, const std::string& strFilename, const std::string& strTargetFilename, UINT64 iComponentSize, UINT64 iSize, bool bSigned, bool bIsFloat, Histogram1DDataBlock* Histogram1D=0);

protected:
  std::string               m_vConverterDesc;
  std::vector<std::string>  m_vSupportedExt;

  static size_t GetIncoreSize();
};

#endif // ABSTRCONVERTER_H
