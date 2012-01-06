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

#include "Basics/Vectors.h"
#include "Controller/Controller.h"
#include "Quantize.h"
#include "UVF/UVF.h"
#include "UVF/Histogram1DDataBlock.h"

class Histogram1DDataBlock;

class RangeInfo {
public:
  UINT64VECTOR3               m_vDomainSize;
  FLOATVECTOR3                m_vAspect;
  uint64_t                      m_iComponentSize;
  int                         m_iValueType;
  std::pair<double, double>   m_fRange;
  std::pair<int64_t, int64_t> m_iRange;
  std::pair<uint64_t, uint64_t>   m_uiRange;
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

  /// We'll need a bin for each unique value of in the data... but
  /// we can't easily compute that when the type is FP.  Just punt
  /// in that case.
  ///@{
  template<typename T>
  size_t bins_needed(std::pair<T,T> minmax) {
    return static_cast<size_t>(minmax.second-minmax.first) + 1;
  }
  template<> size_t bins_needed(std::pair<float,float>) {
    return std::numeric_limits<size_t>::max();
  }
  template<> size_t bins_needed(std::pair<double,double>) {
    return std::numeric_limits<size_t>::max();
  }
  ///@}
}

class AbstrConverter {
public:
  virtual ~AbstrConverter() {}

  virtual bool ConvertToUVF(const std::string& strSourceFilename,
                            const std::string& strTargetFilename,
                            const std::string& strTempDir,
                            const bool bNoUserInteraction,
                            const uint64_t iTargetBrickSize,
                            const uint64_t iTargetBrickOverlap,
                            const bool bQuantizeTo8Bit) = 0;

  virtual bool ConvertToUVF(const std::list<std::string>& files,
                            const std::string& strTargetFilename,
                            const std::string& strTempDir,
                            const bool bNoUserInteraction,
                            const uint64_t iTargetBrickSize,
                            const uint64_t iTargetBrickOverlap,
                            const bool bQuantizeTo8Bit) = 0;

  virtual bool ConvertToRAW(const std::string& strSourceFilename,
                            const std::string& strTempDir,
                            bool bNoUserInteraction,
                            uint64_t& iHeaderSkip, uint64_t& iComponentSize,
                            uint64_t& iComponentCount,
                            bool& bConvertEndianess, bool& bSigned,
                            bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                            FLOATVECTOR3& vVolumeAspect, std::string& strTitle,
                            UVFTables::ElementSemanticTable& eType,
                            std::string& strIntermediateFile,
                            bool& bDeleteIntermediateFile) = 0;

  virtual bool ConvertToNative(const std::string& strRawFilename,
                               const std::string& strTargetFilename,
                               uint64_t iHeaderSkip, uint64_t iComponentSize,
                               uint64_t iComponentCount, bool bSigned,
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
  virtual bool CanImportData() const { return true; }

  /// @returns false on error; true if we successfully generated
  /// 'strTargetFilename'.
  template <typename T, typename U>
  static bool ApplyMapping(const uint64_t iSize,
                           const size_t iCurrentInCoreSizeBytes,
                           raw_data_src<T>& ds,
                           const std::string& strTargetFilename,
                           std::map<T, size_t>& binAssignments,
                           Histogram1DDataBlock* Histogram1D,
                           TuvokProgress<uint64_t> progress) {
    const size_t iCurrentInCoreElems = iCurrentInCoreSizeBytes / sizeof(U);

    ds.reset();

    LargeRAWFile TargetData(strTargetFilename);
    TargetData.Create();

    std::vector<U> targetData(iCurrentInCoreElems);
    std::vector<T> sourceData(iCurrentInCoreElems);
    uint64_t iPos = 0;

    assert(iSize > 0); // our minmax assert later will fail anyway, if false.

    while(iPos < iSize) {
      size_t n_records = ds.read((unsigned char*)(&(sourceData.at(0))),
                                  std::min(static_cast<size_t>((iSize - iPos)*sizeof(T)),
                                          iCurrentInCoreElems));
      if(n_records == 0) {
        WARNING("Short file during mapping.");
        break; // bail out if the read gave us nothing.
      }
      sourceData.resize(n_records);

      iPos += uint64_t(n_records);
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
      T_ERROR("Could not open intermediate file '%s'",
              strTargetFilename.c_str());
      return false;
    }

    // now compute histogram
    size_t hist_size = binAssignments.size();
    if(sizeof(U) == 1) { hist_size = 256; }
    assert(sizeof(U) <= 2);

    std::vector<uint64_t> aHist(hist_size, 0);
    std::pair<T,T> minmax;

    const size_t sz = (sizeof(U) == 2 ? 4096 : 256);
    minmax = io_minmax(raw_data_src<U>(MappedData),
                        UnsignedHistogram<U, sz>(aHist),
                        TuvokProgress<uint64_t>(iSize), iSize,
                        iCurrentInCoreSizeBytes);
    assert(minmax.second >= minmax.first);

    MappedData.Close();

    if(Histogram1D) { Histogram1D->SetHistogram(aHist); }

    return true;
  }

  /// @returns true if we generated 'strTargetFilename', false if the caller
  /// can just use 'InputData' as-is or on error.
  template <typename T, typename U>
  static bool BinningQuantize(LargeRAWFile& InputData,
                              const std::string& strTargetFilename,
                              uint64_t iSize,
                              uint64_t& iComponentSize,
                              Histogram1DDataBlock* Histogram1D=0)
  {
    MESSAGE("Attempting to recover integer values by binning the data.");

    iComponentSize = sizeof(U)*8;
    const size_t iCurrentInCoreSizeBytes = GetIncoreSize();
    const size_t iCurrentInCoreElems = iCurrentInCoreSizeBytes / sizeof(T);
    if(!InputData.IsOpen()) {
      T_ERROR("'%s' is not open.", InputData.GetFilename().c_str());
      return false;
    }

    MESSAGE("Counting number of unique values in the data");

    raw_data_src<T> ds(InputData);
    std::vector<T> data(iCurrentInCoreElems);
    TuvokProgress<uint64_t> progress(iSize);
    uint64_t iPos = 0;
    bool bBinningPossible = true;

    std::map<T, uint64_t> bins;

    while(bBinningPossible && iPos < iSize) {
      size_t n_records = ds.read((unsigned char*)(&(data.at(0))),
                                  std::min(static_cast<size_t>((iSize - iPos)*sizeof(T)),
                                           iCurrentInCoreElems));
      if(n_records == 0) {
        WARNING("Short file during counting.");
        break; // bail out if the read gave us nothing.
      }
      data.resize(n_records);

      iPos += uint64_t(n_records);
      progress.notify("Counting number of unique values in the data", iPos);

      // Run over the in core data and sort it into bins
      for(size_t i=0; i < n_records; ++i) {
          bins[data[i]]++;

          // We max out at 4k bins for Tuvok, regardless of data size.
          const size_t max_bins = 4096;
          if (bins.size() >
              std::min(max_bins, static_cast<size_t>(1 << (sizeof(U) * 8))))
          {
            bBinningPossible = false;
            break;
          }
      }
      MESSAGE("%lu bins needed...", static_cast<unsigned long>(bins.size()));
    }

    data.clear();

    // too many values, need to actually quantize the data
    if (!bBinningPossible) {
      InputData.SeekStart();
      return Quantize<T,U>(InputData, strTargetFilename, iSize, Histogram1D);
    }
    
    // now compute the mapping from values to bins
    std::map<T, size_t> binAssignments;
    size_t binID = 0;

    for (typename std::map<T, uint64_t>::iterator it=bins.begin();
         it != bins.end(); it++ ) {
      binAssignments[(*it).first] = binID;
      binID++;
    }

    bins.clear();

    // apply this mapping
    MESSAGE("Binning possible, applying mapping");

    if (binAssignments.size() < 256) {
      iComponentSize = 8; // now we are only using 8 bits
      using namespace boost;
      return ApplyMapping<T,uint8_t>(iSize, iCurrentInCoreSizeBytes, ds,
                                     strTargetFilename, binAssignments,
                                     Histogram1D, progress);
    } else {
      return ApplyMapping<T,U>(iSize, iCurrentInCoreSizeBytes, ds,
                               strTargetFilename, binAssignments, Histogram1D,
                               progress);
    }       
  }

  /// Quantizes an (already-open) file to 'strTargetFilename'.  If 'InputData'
  /// doesn't need any quantization, this might be a no-op.
  /// @returns true if we generated 'strTargetFilename', false if the caller
  /// can just use 'InputData' as-is or on error.
  template <typename T, typename U>
  static bool Quantize(LargeRAWFile& InputData,
                       const std::string& strTargetFilename,
                       uint64_t iSize,
                       Histogram1DDataBlock* Histogram1D=0,
                       size_t* iBinCount=0)
  {
    if (iBinCount) {
      MESSAGE("Resetting bin count to 0.");
      *iBinCount = 0;
    }
    // this code won't behave correctly when quantizing to very wide data
    // types.  Make sure we only deal with 8 and 16 bit outputs.
    size_t hist_size = 4096;
    if(sizeof(U) == 1) { hist_size = 256; }
    assert(sizeof(U) <= 2);

    const size_t iCurrentInCoreSizeBytes = GetIncoreSize();
    const size_t iCurrentInCoreElems = iCurrentInCoreSizeBytes / sizeof(T);
    if(!InputData.IsOpen()) {
      T_ERROR("Open the file before you call this.");
      return false;
    }

    // figure out min/max
    std::vector<uint64_t> aHist(hist_size, 0);
    std::pair<T,T> minmax;
    const size_t sz = (sizeof(U) == 2 ? 4096 : 256);
    minmax = io_minmax(raw_data_src<T>(InputData),
                       UnsignedHistogram<T, sz>(aHist),
                       TuvokProgress<uint64_t>(iSize), iSize,
                       iCurrentInCoreSizeBytes);
    assert(minmax.second >= minmax.first);

    // Unsigned N bit data does not need to be biased/quantized.
    if(!ctti<T>::is_signed && minmax.second < static_cast<T>(hist_size) &&
       sizeof(T) <= ((hist_size == 256) ? 1 : 2)) {
      MESSAGE("Returning early; data does not need processing.");

      // if we have very few values, let the calling function know
      // how much exactly, so we can reduce the bit depth of 
      // the data
      if (iBinCount) {
        for (size_t bin = 0;bin<aHist.size();++bin) 
          if (aHist[bin] != 0) (*iBinCount)++;
      }
      return false;
    }
    // Reset the histogram.  We'll be quantizing the data.
    std::fill(aHist.begin(), aHist.end(), 0);

    if(iBinCount != NULL) {
      *iBinCount = bins_needed<T>(minmax);
      MESSAGE("We need %llu bins", static_cast<uint64_t>(*iBinCount));
    }

    size_t max_output_val = (1 << (sizeof(U)*8)) - 1;
    if(hist_size == 256) { max_output_val = 255; }
    double fQuantFact = QuantizationFactor(max_output_val, minmax.first,
                                           minmax.second);
    double fQuantFactHist = QuantizationFactor(hist_size-1, minmax.first,
                                               minmax.second);

    bool bDataWillbeChanged = fQuantFact != 1.0 || minmax.first != 0;

    LargeRAWFile OutputData(strTargetFilename);
    // if the only reason for quantization is the histogram computation
    // we don't need an output file
    if (bDataWillbeChanged) {      
      OutputData.Create(iSize*sizeof(unsigned short));
      if(!OutputData.IsOpen()) {
        InputData.Close();
        T_ERROR("Could not create output file '%s'", strTargetFilename.c_str());
        return false;
      }
    }


    T* pInData = new T[iCurrentInCoreElems];
    U* pOutData = new U[iCurrentInCoreElems];

    InputData.SeekStart();
    uint64_t iPos = 0;
    uint64_t iLastDisplayedPercent = 0;

    while(iPos < iSize) {
      size_t iRead = InputData.ReadRAW((unsigned char*)pInData,
                                       std::min((iSize - iPos)*sizeof(T),
                                       uint64_t(iCurrentInCoreSizeBytes)))/sizeof(T);
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
      iPos += static_cast<uint64_t>(iRead);

      if((100*iPos)/iSize > iLastDisplayedPercent) {
        std::ostringstream qmsg;

        if (fQuantFact == 1.0) 
          if (minmax.first == 0) 
            qmsg << "Computing quantized histogram with " << hist_size
            << " bins (input data has range from "
            << minmax.first << " to " << minmax.second << ")\n"
            << (100*iPos)/iSize << "% complete";
          else
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

      if (bDataWillbeChanged) 
        OutputData.WriteRAW(reinterpret_cast<unsigned char*>(pOutData),
                            sizeof(U)*iRead);
    }

    delete[] pInData;
    delete[] pOutData;
    InputData.Close();
    if(Histogram1D) { Histogram1D->SetHistogram(aHist); }

    if (bDataWillbeChanged) {
      OutputData.Close();
      return true;
    } else {
      return false;
    }
  }

  static bool Process8Bits(LargeRAWFile& InputData,
                           const std::string& strTargetFilename,
                           uint64_t iSize, bool bSigned,
                           Histogram1DDataBlock* Histogram1D=0);

  static bool QuantizeTo8Bit(LargeRAWFile& rawfile,
                             const std::string& strTargetFilename,
                             uint64_t iComponentSize, uint64_t iSize,
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
