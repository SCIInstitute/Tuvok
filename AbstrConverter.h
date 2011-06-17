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
#include <map>
#include "boost/cstdint.hpp"

#include "Basics/Vectors.h"
#include "Controller/Controller.h"
#include "Quantize.h"
#include "UVF/UVF.h"
#include "UVF/Histogram1DDataBlock.h"

using boost::int64_t;
using boost::int8_t;
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

namespace { // force internal linkage.
  // Figure out what factor we should multiply each element in the data set by
  // to quantize it.
  template <typename T>
  double QuantizationFactor(size_t max_out,
                                   const T& mn, const T& mx)
  {
    double quant = max_out / (static_cast<double>(mx) - mn);
    // ensure we don't "stretch" the data values, only "compress" them if
    // necessary.
    return std::min(quant, 1.0);
  }
  // For FP data we allow the range to be stretched.  Even if the
  // max/min range is 0.3, we could still have a million different
  // values in there.  Attempting to `compress' the range and that to
  // integers is going to leave us with 1 value.
  template <>
  double QuantizationFactor(size_t max_out,
                                   const float& mn, const float& mx)
  {
    return max_out / (mx - mn);
  }
  // Side note: we use references here because constant template parameters
  // must be integral or reference types.
  template <>
  double QuantizationFactor(size_t max_out,
                                   const double& mn, const double& mx)
  {
    return max_out / (mx - mn);
  }
}

class AbstrConverter {
public:
  virtual ~AbstrConverter() {}

  virtual bool ConvertToUVF(const std::string& strSourceFilename,
                            const std::string& strTargetFilename,
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
                            const std::string& strTempDir,
                            bool bNoUserInteraction,
                            UINT64& iHeaderSkip, UINT64& iComponentSize,
                            UINT64& iComponentCount,
                            bool& bConvertEndianess, bool& bSigned,
                            bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                            FLOATVECTOR3& vVolumeAspect, std::string& strTitle,
                            UVFTables::ElementSemanticTable& eType,
                            std::string& strIntermediateFile,
                            bool& bDeleteIntermediateFile) = 0;

  virtual bool ConvertToNative(const std::string& strRawFilename,
                               const std::string& strTargetFilename,
                               UINT64 iHeaderSkip, UINT64 iComponentSize,
                               UINT64 iComponentCount, bool bSigned,
                               bool bFloatingPoint,
                               UINT64VECTOR3 vVolumeSize,
                               FLOATVECTOR3 vVolumeAspect,
                               bool bNoUserInteraction,
                               const bool bQuantizeTo8Bit) = 0;

  virtual bool Analyze(const std::string& strSourceFilename,
                       const std::string& strTempDir,
                       bool bNoUserInteraction, RangeInfo& info) = 0;

  /// @param filename the file in question
  /// @param start the first few bytes of the file
  /// @return SupportedExtension() for the file's extension; ignores "start".
  virtual bool CanRead(const std::string& fn,
                       const std::vector<int8_t>& start) const;

  const std::vector<std::string>& SupportedExt() { return m_vSupportedExt; }
  virtual const std::string& GetDesc() { return m_vConverterDesc; }

  virtual bool CanExportData() const { return false; }

  template <typename T, typename U>
  static const std::string ApplyMapping(const UINT64 iSize,
                                        const size_t iCurrentInCoreSizeBytes,
                                        raw_data_src<T>& ds,
                                        const std::string& strTargetFilename,
                                        std::map<T, size_t>& binAssignments,
                                        Histogram1DDataBlock* Histogram1D,
                                        TuvokProgress<UINT64> progress) {
    
    const size_t iCurrentInCoreElems = iCurrentInCoreSizeBytes / sizeof(U);

    ds.reset();

    LargeRAWFile TargetData(strTargetFilename);
    TargetData.Create();

    std::vector<U> targetData(iCurrentInCoreElems);
    std::vector<T> sourceData(iCurrentInCoreElems);
    UINT64 iPos = 0;

    while(iPos < iSize) {
      size_t n_records = ds.read((unsigned char*)(&(sourceData.at(0))),
                                  std::min(static_cast<size_t>((iSize - iPos)*sizeof(T)),
                                          iCurrentInCoreElems));
      if(n_records == 0) {
        WARNING("Short file during mapping.");
        break; // bail out if the read gave us nothing.
      }
      sourceData.resize(n_records);

      iPos += UINT64(n_records);
      progress.notify("Mapping data values to bins",iPos);

      // Run over the in-core data and apply mapping
      for(size_t i=0; i < n_records; ++i) {
        targetData[i] = U(binAssignments[sourceData[i]]);
      }

      TargetData.WriteRAW((unsigned char*)&targetData[0], sizeof(U)*n_records);
    }

    TargetData.Close();

    LargeRAWFile MappedData(strTargetFilename);
    MappedData.Open(false);
    if(!MappedData.IsOpen()) {
      T_ERROR("Could not open intermediate file '%s'", strTargetFilename.c_str());
      return "";
    }

    // now compute histogram
    size_t hist_size = binAssignments.size();
    if(sizeof(U) == 1) { hist_size = 256; }
    assert(sizeof(U) <= 2);

    std::vector<UINT64> aHist(hist_size, 0);
    std::pair<T,T> minmax;

    const size_t sz = (sizeof(U) == 2 ? 4096 : 256);
    minmax = io_minmax(raw_data_src<U>(MappedData),
                        UnsignedHistogram<U, sz>(aHist),
                        TuvokProgress<UINT64>(iSize), iSize,
                        iCurrentInCoreSizeBytes);
    assert(minmax.second >= minmax.first);

    MappedData.Close();

    if(Histogram1D) { Histogram1D->SetHistogram(aHist); }

    return strTargetFilename;
  }

  template <typename T, typename U>
  static const std::string BinningQuantize(UINT64 iHeaderSkip,
                                    const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    UINT64 iSize,
                                    UINT64& iComponentSize,
                                    Histogram1DDataBlock* Histogram1D=0)
  {
    MESSAGE("Attempting to recover integer values by binning the data.");

    iComponentSize = sizeof(U)*8;
    const size_t iCurrentInCoreSizeBytes = GetIncoreSize();
    const size_t iCurrentInCoreElems = iCurrentInCoreSizeBytes / sizeof(T);
    LargeRAWFile InputData(strFilename, iHeaderSkip);
    InputData.Open(false);
    if(!InputData.IsOpen()) {
      T_ERROR("Could not open '%s'", strFilename.c_str());
      return "";
    }

    MESSAGE("Counting number of unique values in the data");

    raw_data_src<T> ds(InputData);
    std::vector<T> data(iCurrentInCoreElems);
    TuvokProgress<UINT64> progress(iSize);
    UINT64 iPos = 0;
    bool bBinningPossible = true;

    std::map<T, UINT64> bins;


    while(bBinningPossible && iPos < iSize) {
      size_t n_records = ds.read((unsigned char*)(&(data.at(0))),
                                  std::min(static_cast<size_t>((iSize - iPos)*sizeof(T)),
                                          iCurrentInCoreElems));
      if(n_records == 0) {
        WARNING("Short file during counting.");
        break; // bail out if the read gave us nothing.
      }
      data.resize(n_records);

      iPos += UINT64(n_records);
      progress.notify("Counting number of unique values in the data",iPos);


      // Run over the in core data and sort it into bins
      for(size_t i=0; i < n_records; ++i) {
      
          bins[data[i]]++;

          if (bins.size() > 1<<(sizeof(U)*8) ) {
            bBinningPossible = false;
            break;
          }
      }

    }

    data.clear();

    // too many values, need to actually quantize the data
    if (!bBinningPossible) {
      return Quantize<T,U>(iHeaderSkip, strFilename,
                      strTargetFilename, iSize,
                      Histogram1D);
    }
    
    // now compute the mapping from values to bins
    std::map<T, size_t> binAssignments;
    size_t binID = 0;

    for (typename std::map<T, UINT64>::iterator it=bins.begin();
         it != bins.end(); it++ ) {
      binAssignments[(*it).first] = binID;
      binID++;
    }

    bins.clear();

    // apply this mapping
    MESSAGE("Binning possible, applying mapping");

    if (binAssignments.size() < 256) {
      iComponentSize = 8; // now we are only using 8 bits
      return ApplyMapping<T,unsigned char>(iSize, iCurrentInCoreSizeBytes, ds,
                                           strTargetFilename, binAssignments,
                                           Histogram1D, progress);
    } else {
      return ApplyMapping<T,U>(iSize, iCurrentInCoreSizeBytes, ds,
                               strTargetFilename, binAssignments, Histogram1D,
                               progress);
    }       
  }


  template <typename T, typename U>
  static const std::string Quantize(UINT64 iHeaderSkip,
                                    const std::string& strFilename,
                                    const std::string& strTargetFilename,
                                    UINT64 iSize,
                                    Histogram1DDataBlock* Histogram1D=0,
                                    size_t* iBinCount=0)
  {
    if (iBinCount) 
      *iBinCount = 0;
    size_t hist_size = 4096;
    if(sizeof(U) == 1) { hist_size = 256; }
    assert(sizeof(U) <= 2);

    const size_t iCurrentInCoreSizeBytes = GetIncoreSize();
    const size_t iCurrentInCoreElems = iCurrentInCoreSizeBytes / sizeof(T);
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
                       TuvokProgress<UINT64>(iSize), iSize,
                       iCurrentInCoreSizeBytes);
    assert(minmax.second >= minmax.first);

    // Unsigned N bit data does not need to be biased/quantized.
    if(!ctti<T>::is_signed && minmax.second < static_cast<T>(hist_size) &&
       sizeof(T) <= ((hist_size == 256) ? 1 : 2)) {
      MESSAGE("Returning early; data does not need processing.");
      InputData.Close();

      // if we have very few values, let the calling function know
      // how much exactly, so we can reduce the the bit depth of 
      // the data
      if (iBinCount) {
        for (size_t bin = 0;bin<aHist.size();++bin) 
          if (aHist[bin] != 0) (*iBinCount)++;
      }
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

    size_t max_output_val = (1 << (sizeof(U)*8)) - 1;
    if(hist_size == 256) { max_output_val = 255; }
    double fQuantFact = QuantizationFactor(max_output_val, minmax.first,
                                           minmax.second);
    double fQuantFactHist = QuantizationFactor(hist_size-1, minmax.first,
                                               minmax.second);

    T* pInData = new T[iCurrentInCoreElems];
    U* pOutData = new U[iCurrentInCoreElems];

    InputData.SeekStart();
    UINT64 iPos = 0;
    UINT64 iLastDisplayedPercent = 0;

    while(iPos < iSize) {
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData,
                                       std::min((iSize - iPos)*sizeof(T),
                                        UINT64(iCurrentInCoreSizeBytes)))/sizeof(T);
      if(iRead == 0) { break; } // bail if the read gave us nothing

      // calculate hist + quantize to output file.
      for(size_t i=0; i < iRead; ++i) {
        U iNewVal = std::min<U>(static_cast<U>(max_output_val),
          static_cast<U>((pInData[i]-minmax.first) * fQuantFact)
        );
        U iHistIndex = std::min<U>(static_cast<U>(hist_size-1),
                                   static_cast<U>((pInData[i]-minmax.first) *
                                     fQuantFactHist)
        );
        pOutData[i] = iNewVal;
        aHist[iHistIndex]++;
      }
      iPos += static_cast<UINT64>(iRead);

      if((100*iPos)/iSize > iLastDisplayedPercent) {
        std::ostringstream qmsg;

        if (fQuantFact == 1.0) 
          qmsg << "Quantizing to " << (minmax.second-minmax.first)+1
               << " integer values (input data has range from "
               << minmax.first << " to " << minmax.second << ")\n"
               << (100*iPos)/iSize << "% complete";
        else
          qmsg << "Quantizing to " << max_output_val
               << " integer values (input data has range from "
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

  static const std::string Process8Bits(UINT64 iHeaderSkip,
                                        const std::string& strFilename,
                                        const std::string& strTargetFilename,
                                        UINT64 iSize, bool bSigned,
                                        Histogram1DDataBlock* Histogram1D=0);

  static const std::string QuantizeTo8Bit(UINT64 iHeaderSkip,
                                          const std::string& strFilename,
                                          const std::string& strTargetFilename,
                                          UINT64 iComponentSize, UINT64 iSize,
                                          bool bSigned, bool bIsFloat,
                                          Histogram1DDataBlock* Histogram1D=0);

  static size_t GetIncoreSize();

protected:
  /// @param ext the extension for the filename
  /// @return true if the filename is a supported extension for this converter
  bool SupportedExtension(const std::string& ext) const;

protected:
  std::string               m_vConverterDesc;
  std::vector<std::string>  m_vSupportedExt;

};

#endif // ABSTRCONVERTER_H
