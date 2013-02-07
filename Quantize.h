/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2009 Scientific Computing and Imaging Institute,
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
#pragma once

#ifndef SCIO_QUANTIZE_H
#define SCIO_QUANTIZE_H

#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <functional>
#include <limits>
#include <type_traits>
#include "Basics/BStream.h"
#include "Basics/LargeRAWFile.h"
#include "Basics/ctti.h"
#include "UVF/Histogram1DDataBlock.h"
#include "TuvokSizes.h"
#include "AbstrConverter.h"

namespace { // force internal linkage.
  // Figure out what factor we should multiply each element in the data set by
  // to quantize it.
  template <typename T>
  double QuantizationFactor(size_t max_out, const T& mn, const T& mx)
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
  double QuantizationFactor(size_t max_out, const float& mn, const float& mx)
  {
    return max_out / (mx - mn);
  }
  // Side note: we use references here because constant template parameters
  // must be integral or reference types.
  template <>
  double QuantizationFactor(size_t max_out, const double& mn, const double& mx)
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

// For minmax ranges, we want to initialize the "max" to be the smallest value
// which can be held by the type.  e.g. a `short' should get -32768, whereas
// an `unsigned short' should get 0.
namespace {
  template<typename T> T initialmax(const unsigned_type&) {
    return 0;
  }
  template<typename T> T initialmax(const signed_type&) {
    return -(std::numeric_limits<T>::max()+1);
  }
}

/// Progress policies.  Must implement a constructor and `notify' method which
/// both take a `T'.  The constructor is given the max value; the notify method
/// is given the current value.
///    NullProgress -- nothing, use when you don't care.
///    TuvokProgress -- forward progress info to Tuvok debug logs.
///@{
template <typename T>
struct NullProgress {
  NullProgress() {};
  NullProgress(T) {};
  static void notify(T) {}
};
template <typename T>
struct TuvokProgress {
  TuvokProgress(T total) : tMax(total) {}
  void notify(const std::string& operation, T current) const {
    assert(current <= tMax);
    MESSAGE("%s (%5.3f%% complete).", operation.c_str(),
            static_cast<double>(current) / static_cast<double>(tMax)*100.0);
  }
  private: T tMax;
};
///@}

/// Data source policies.  Must implement:
///   constructor: takes an opened file.
///   size(): returns the number of elements in the file.
///   read(data, b): reads `b' bytes in to `data'.  Returns number of elems
///                  actually read.
/// ios_data_src -- data source for C++ iostreams.
/// raw_data_src -- data source for Basics' LargeRAWFile.
///@{
template <typename T>
struct ios_data_src {
  ios_data_src(std::ifstream& fs) : ifs(fs) {
    if(!ifs.is_open()) {
      throw std::runtime_error(__FILE__);
    }
    ifs.seekg(0, std::ios::cur);
  }

  uint64_t size() {
    std::streampos cur = ifs.tellg();
    ifs.seekg(0, std::ios::end);
    uint64_t retval = ifs.tellg();
    ifs.seekg(cur, std::ios::beg);
    return retval/sizeof(T);
  }
  size_t read(unsigned char *data, size_t max_bytes) {
    ifs.read(reinterpret_cast<char*>(data), std::streamsize(max_bytes));
    return ifs.gcount()/sizeof(T);
  }
  private:
    std::ifstream& ifs;
    const char *filename;
};

template <typename T>
struct raw_data_src {
  raw_data_src(LargeRAWFile& r) : raw(r) {
    if(!raw.IsOpen()) {
      throw std::runtime_error(__FILE__);
    }
    reset();
  }

  uint64_t size() { return raw.GetCurrentSize() / sizeof(T); }
  size_t read(unsigned char *data, size_t max_bytes) {
    return raw.ReadRAW(data, max_bytes)/sizeof(T);
  }

  void reset() {
    raw.SeekStart();
  }
  private: LargeRAWFile& raw;
};

template <typename T>
struct multi_raw_data_src {
  multi_raw_data_src(std::vector<LargeRAWFile> f) : files(f), cur_file(0),
                                                    total_size(0) {
    for(std::vector<LargeRAWFile>::const_iterator rf = files.begin();
        rf != files.end(); ++rf) {
      if(!rf->IsOpen()) {
        throw std::runtime_error(__FILE__ ": file not open");
      }
    }
  }

  uint64_t size() {
    if(total_size == 0) {
      for(std::vector<LargeRAWFile>::iterator rf = files.begin();
          rf != files.end(); ++rf) {
        total_size += rf->GetCurrentSize() / sizeof(T);
      }
    }
    return total_size;
  }

  size_t read(unsigned char *data, size_t max_bytes) {
    size_t bytes = files[cur_file].ReadRAW(data, max_bytes);
    while(bytes == 0) {
      cur_file++;
      if(cur_file >= files.size()) { return 0; }
      bytes = files[cur_file].ReadRAW(data, max_bytes);
    }
    return bytes / sizeof(T);
  }

  private:
    std::vector<LargeRAWFile> files;
    size_t cur_file;
    uint64_t total_size;
};
///@}

/// If we just do a standard "is the value of type T <= 4096?" then compilers
/// complain with 8bit types because the comparison is always true.  Great,
/// thanks, I didn't know that.  So, this basically gets around a warning.
/// I love C++ sometimes.
namespace { namespace Fits {
  template<typename T, size_t sz> bool inXBits(T v) {
    return v < static_cast<T>(sz);
  }
  template<> bool inXBits<int8_t, 256>(int8_t) { return true; }
  template<> bool inXBits<int8_t, 4096>(int8_t) { return true; }
  template<> bool inXBits<uint8_t, 256>(uint8_t) { return true; }
  template<> bool inXBits<uint8_t, 4096>(uint8_t) { return true; }
}; };

/// Histogram policies.  minmax can sometimes compute a 1D histogram as it
/// marches over the data.  It may happen that the data must be quantized
/// though, forcing the histogram to be recalculated.
/// Must implement:
///    bin(T): bin the given value.  return false if we shouldn't bother
///            computing the histogram anymore.
template<typename T> struct NullHistogram {
  static bool bin(T) { return false; }
};
// Calculate a 12Bit histogram, but when we encounter a value which does not
// fit (i.e., we know we'll need to quantize), don't bother anymore.
template<typename T, size_t sz>
struct UnsignedHistogram {
  UnsignedHistogram(std::vector<uint64_t>& h)
    : histo(h), calculate(true) {}

  bool bin(T value) {
    if(!calculate || !Fits::inXBits<T, sz>(value)) {
      calculate = false;
    } else {
      update(value);
    }
    return calculate;
  }

  void update(T value) {
    // Calculate our bias factor up front.
    typename ctti<T>::size_type bias;
    bias = static_cast<typename ctti<T>::size_type>(
            std::fabs(static_cast<double>(
              initialmax<T>(type_category(T()))
            ))
           );

    typename ctti<T>::size_type u_value;
    u_value = ctti<T>::is_signed ? value + bias : value;
    // Either the data are unsigned, or there exist no values s.t. the value
    // plus the bias is negative (and therefore *this* value + the bias is
    // nonnegative).
    // Unfortunately we can't assert this since compilers are too dumb: with
    // an unsigned T, they complain that "value+bias >= 0 is always true",
    // despite the fact that the comparison would never happen for unsigned
    // values.
//  assert(!ctti<T>::is_signed || (ctti<T>::is_signed && ((value+bias) >= 0)));

    if(u_value < histo.size()) {
      ++histo[static_cast<size_t>(u_value)];
    }
  }
  private:
    std::vector<uint64_t>& histo;
    bool calculate;
};

/// Computes the minimum and maximum of a conceptually one dimensional dataset.
/// Takes policies to tell it how to access data && notify external entities of
/// progress.
template <typename T, size_t sz,
          template <typename T_> class DataSrc,
          template <typename T_, size_t> class Histogram,
          class Progress>
std::pair<T,T> io_minmax(DataSrc<T> ds, Histogram<T, sz> histogram,
                         const Progress& progress, uint64_t iSize,
                         size_t iCurrentInCoreSizeBytes)
{
  std::vector<T> data(iCurrentInCoreSizeBytes/sizeof(T));
  uint64_t iPos = 0;

  // Default min is the max value representable by the data type.  Default max
  // is the smallest value representable by the data type.
  std::pair<T,T> t_minmax(std::numeric_limits<T>::max(),
                          initialmax<T>(type_category(T())));
  // ... but if the data type is unsigned, the correct default 'max' is 0.
  if(!ctti<T>::is_signed) {
    t_minmax.second = std::numeric_limits<T>::min(); // ... == 0.
  }

  while(iPos < iSize) {
    size_t n_records = ds.read((unsigned char*)(&(data.at(0))),
                               std::min(static_cast<size_t>((iSize-iPos) *
                                        sizeof(T)), iCurrentInCoreSizeBytes));
    if(n_records == 0) {
      WARNING("Short file during quantization (%llu of %llu)", iPos, iSize);
      break; // bail out if the read gave us nothing.
    }
    data.resize(n_records);

    iPos += uint64_t(n_records * sizeof(T));
    progress.notify("Computing value range",iPos);

    typedef typename std::vector<T>::const_iterator iterator;
    std::pair<iterator,iterator> cur_mm = std::minmax_element(data.begin(),
                                                              data.end());
    t_minmax.first = std::min(t_minmax.first, *cur_mm.first);
    t_minmax.second = std::max(t_minmax.second, *cur_mm.second);

    // Run over the data again and bin the data for the histogram.
    for(size_t i=0; i < n_records && histogram.bin(data[i]); ++i) { }
  }
  assert(iPos == iSize);
  return t_minmax;
}

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
                               std::min(static_cast<size_t>((iSize-iPos) *
                                        sizeof(T)), iCurrentInCoreElems));
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
  static_assert(sizeof(U) <= 2, "we assume histo sizes");

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

/// Quantizes an (already-open) file to 'strTargetFilename'.  If 'InputData'
/// doesn't need any quantization, this might be a no-op.
/// @returns true if we generated 'strTargetFilename', false if the caller
/// can just use 'InputData' as-is or on error.
template <typename T, typename U>
static bool Quantize(LargeRAWFile& InputData,
                     const BStreamDescriptor& Input,
                     const std::string& strTargetFilename,
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
  static_assert(sizeof(U) <= 2, "we assume histogram sizes");

  const size_t iCurrentInCoreSizeBytes = AbstrConverter::GetIncoreSize();
  const size_t iCurrentInCoreElems = iCurrentInCoreSizeBytes / sizeof(T);
  if(!InputData.IsOpen()) {
    T_ERROR("Open the file before you call this.");
    return false;
  }
  InputData.SeekStart();

  assert(Input.width == sizeof(T));
  const uint64_t iSize = Input.elements * Input.components * Input.timesteps *
                         Input.width;
  MESSAGE("%s should have %llu bytes.", InputData.GetFilename().c_str(), iSize);

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

  bool bDataWillbeChanged = fQuantFact != 1.0 || minmax.first != 0 ||
                            sizeof(T) > 2;

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
  if(Histogram1D) { Histogram1D->SetHistogram(aHist); }

  if (bDataWillbeChanged) {
    OutputData.Close();
    return true;
  } else {
    return false;
  }
}

/// @returns true if we generated 'strTargetFilename', false if the caller
/// can just use 'InputData' as-is or on error.
template <typename T, typename U>
static bool BinningQuantize(LargeRAWFile& InputData,
                            const std::string& strTargetFilename,
                            uint64_t iSize,
                            unsigned& iComponentSize,
                            Histogram1DDataBlock* Histogram1D=0)
{
  MESSAGE("Attempting to recover integer values by binning the data.");

  iComponentSize = sizeof(U)*8;
  const size_t iCurrentInCoreSizeBytes = AbstrConverter::GetIncoreSize();
  const size_t iCurrentInCoreElems = iCurrentInCoreSizeBytes / sizeof(T);
  if(!InputData.IsOpen()) {
    T_ERROR("'%s' is not open.", InputData.GetFilename().c_str());
    return false;
  }
  InputData.SeekStart();

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
    BStreamDescriptor bsd;
    bsd.elements = iSize / sizeof(T);
    bsd.components = 1;
    bsd.width = sizeof(T);
    bsd.is_signed = ctti<T>::is_signed;
    bsd.fp = std::is_floating_point<T>::value;
    bsd.big_endian = EndianConvert::IsBigEndian();
    bsd.timesteps = 1;
    return Quantize<T,U>(InputData, bsd, strTargetFilename, Histogram1D);
  }

  // now compute the mapping from values to bins
  std::map<T, size_t> binAssignments;
  size_t binID = 0;

  for (typename std::map<T, uint64_t>::iterator it=bins.begin();
       it != bins.end(); it++) {
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
#endif // SCIO_QUANTIZE_H
