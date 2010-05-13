#include <cstdlib>
#include <time.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <boost/cstdint.hpp>

#include <cxxtest/TestSuite.h>

#include "AbstrConverter.h"
#include "Controller/Controller.h"
#include "UVF/Histogram1DDataBlock.h"
#include "../Quantize.h"

#include "util-test.h"

using namespace tuvok;


template<typename T>
std::string quantize(UINT64 iHeader, const std::string& input,
                     const std::string& outfn, UINT64 values,
                     Histogram1DDataBlock* hist, T)
{
  return AbstrConverter::Quantize<T, unsigned short>(iHeader, input, outfn, values, hist);
}

template <>
std::string quantize(UINT64 iHeader, const std::string& input,
                     const std::string& outfn, UINT64 values,
                     Histogram1DDataBlock* hist, byte)
{
  return AbstrConverter::Process8Bits(iHeader, input, outfn, values, true, hist);
}


template <>
std::string quantize(UINT64 iHeader, const std::string& input,
                     const std::string& outfn, UINT64 values,
                     Histogram1DDataBlock* hist, ubyte)
{
  return AbstrConverter::Process8Bits(iHeader, input, outfn, values, false, hist);
}

template<typename T>
std::string quantize8(UINT64 iHeader, const std::string& input,
                      const std::string& outfn, UINT64 values,
                      Histogram1DDataBlock* hist, T)
{
  return AbstrConverter::Quantize<T, unsigned char>(iHeader, input, outfn, values, hist);
}

template <>
std::string quantize8(UINT64 iHeader, const std::string& input,
                     const std::string& outfn, UINT64 values,
                     Histogram1DDataBlock* hist, byte)
{
  return AbstrConverter::Process8Bits(iHeader, input, outfn, values, true, hist);
}

template <>
std::string quantize8(UINT64 iHeader, const std::string& input,
                     const std::string& outfn, UINT64 values,
                     Histogram1DDataBlock* hist, ubyte)
{
  return AbstrConverter::Process8Bits(iHeader, input, outfn, values, false, hist);
}



template<typename T> bool is_8bit(T) { return false; }
template<> bool is_8bit(byte) { return true; }
template<> bool is_8bit(ubyte) { return true; }

template<typename T>
void verify_type() {
  const size_t N_VALUES = 100;
  typedef typename ctti<T>::signed_type sT;
  const sT STARTING_NEG = ctti<T>::is_signed ? -64 : 0;
  std::string fn, outfn;
  {
    std::ofstream dataf;
    fn = mk_tmpfile(dataf, std::ios::out | std::ios::binary);
    assert(dataf.is_open());
    for(sT start=STARTING_NEG; start < static_cast<sT>(N_VALUES+STARTING_NEG);
        ++start) {
      gen_constant<sT>(dataf, 1, start);
    }
    dataf.close();
    // need a filename for a temporary output file
    outfn = mk_tmpfile(dataf, std::ios::out | std::ios::binary);
    dataf.close();
  }

  Histogram1DDataBlock hist1d;
  std::string old_outfn = outfn;
  outfn = quantize<T>(static_cast<UINT64>(0), fn, outfn,
                      static_cast<UINT64>(N_VALUES), &hist1d, T());
  if(outfn != old_outfn) {
    remove(old_outfn.c_str());
  }

  // verify data
  std::ifstream outdata;
  outdata.clear();
  outdata.open(outfn.c_str(), std::ios::in | std::ios::binary);
  assert(outdata.is_open());
  typedef typename ctti<T>::size_type uT;
  const uT bias = -STARTING_NEG;
  for(size_t i=0; i < N_VALUES; ++i) {
    if(is_8bit(T())) {
      unsigned char val;
      outdata.read(reinterpret_cast<char*>(&val), sizeof(unsigned char));
      if(ctti<T>::is_signed) {
        TS_ASSERT_EQUALS(val, STARTING_NEG+i+128);
      } else {
        TS_ASSERT_EQUALS(val, static_cast<ubyte>(STARTING_NEG+i));
      }
    } else {
      unsigned short val;
      outdata.read(reinterpret_cast<char*>(&val), sizeof(unsigned short));
      double quant = 65535.0 / N_VALUES;
      quant = std::min(quant, 1.0);
      TS_ASSERT_EQUALS(val, static_cast<unsigned short>
                                       ((STARTING_NEG+i+bias) * quant));
    }
  }
  outdata.close();
  remove(fn.c_str());
  remove(outfn.c_str());

  // now verify 1D histogram
  const std::vector<UINT64>& histo = hist1d.GetHistogram();
  for(size_t i=0; i < histo.size(); ++i) {
    // For data wider than 8 bit, we bias s.t. the minimum value ends up at 0.
    // Therefore the first N_VALUES of our histogram will have data.  For 8 bit
    // data, we don't shift correctly, because it would require an extra pass
    // over the data to get the minimum.
    if(is_8bit(T())) {
      uT low;
      if(ctti<T>::is_signed) {
        low = static_cast<uT>(STARTING_NEG + 128);
      } else {
        low = 0;
      }
      if(static_cast<uT>(low) <= i && i < low+N_VALUES) {
        TS_ASSERT_EQUALS(histo[i], static_cast<UINT64>(1));
      } else {
        TS_ASSERT_EQUALS(histo[i], static_cast<UINT64>(0));
      }
    } else {
      if(static_cast<uT>(i < N_VALUES)) {
        TS_ASSERT_EQUALS(histo[i], static_cast<UINT64>(1));
      } else {
        TS_ASSERT_EQUALS(histo[i], static_cast<UINT64>(0));
      }
    }
  }
}

template<typename T>
void verify_8b_type() {
  const size_t N_VALUES = 100;
  typedef typename ctti<T>::signed_type sT;
  const sT STARTING_NEG = ctti<T>::is_signed ? -64 : 0;
  std::string fn, outfn;
  {
    std::ofstream dataf;
    fn = mk_tmpfile(dataf, std::ios::out | std::ios::binary);
    assert(dataf.is_open());
    for(sT start=STARTING_NEG; start < static_cast<sT>(N_VALUES+STARTING_NEG);
        ++start) {
      gen_constant<sT>(dataf, 1, start);
    }
    dataf.close();
    // need a filename for a temporary output file
    outfn = mk_tmpfile(dataf, std::ios::out | std::ios::binary);
    dataf.close();
  }

  Histogram1DDataBlock hist1d;
  std::string old_outfn = outfn;
  outfn = quantize8<T>(static_cast<UINT64>(0), fn, outfn,
                      static_cast<UINT64>(N_VALUES), &hist1d, T());
  if(outfn != old_outfn) {
    remove(old_outfn.c_str());
  }

  // verify data
  std::ifstream outdata;
  outdata.clear();
  outdata.open(outfn.c_str(), std::ios::in | std::ios::binary);
  assert(outdata.is_open());
  typedef typename ctti<T>::size_type uT;
  const uT bias = -STARTING_NEG;
  for(size_t i=0; i < N_VALUES; ++i) {

    unsigned char val;
    outdata.read(reinterpret_cast<char*>(&val), sizeof(unsigned char));
    if(ctti<T>::is_signed) {
      if(is_8bit(T())) {
        TS_ASSERT_EQUALS(val, STARTING_NEG+i+128);
      } else {
        double quant = 256.0 / N_VALUES;
        quant = std::min(quant, 1.0);
        TS_ASSERT_EQUALS(val, static_cast<unsigned char>
                                         ((STARTING_NEG+i+bias) * quant));
      }
    } else {
      TS_ASSERT_EQUALS(val, static_cast<ubyte>(STARTING_NEG+i));
    }
  }
  outdata.close();
  remove(fn.c_str());
  remove(outfn.c_str());

  // now verify 1D histogram
  const std::vector<UINT64>& histo = hist1d.GetHistogram();
  for(size_t i=0; i < histo.size(); ++i) {
    // For data wider than 8 bit, we bias s.t. the minimum value ends up at 0.
    // Therefore the first N_VALUES of our histogram will have data.  For 8 bit
    // data, we don't shift correctly, because it would require an extra pass
    // over the data to get the minimum.
    if(is_8bit(T())) {
      uT low;
      if(ctti<T>::is_signed) {
        low = static_cast<uT>(STARTING_NEG + 128);
      } else {
        low = 0;
      }
      if(static_cast<uT>(low) <= i && i < low+N_VALUES) {
        TS_ASSERT_EQUALS(histo[i], static_cast<UINT64>(1));
      } else {
        TS_ASSERT_EQUALS(histo[i], static_cast<UINT64>(0));
      }
    } else {
      if(static_cast<uT>(i < N_VALUES)) {
        TS_ASSERT_EQUALS(histo[i], static_cast<UINT64>(1));
      } else {
        TS_ASSERT_EQUALS(histo[i], static_cast<UINT64>(0));
      }
    }
  }
}

class QuantizeTests : public CxxTest::TestSuite {
public:
  void test_byte() { verify_type<byte>(); }
  void test_short() { verify_type<short>(); }
  void test_float() { verify_type<float>(); }
  void test_double() { verify_type<double>(); }
  void test_ubyte() { verify_type<ubyte>(); }
  void test_ushort() { verify_type<unsigned short>(); }
  void test_int() { verify_type<int>(); }
  void test_uint() { verify_type<unsigned int>(); }
  void test_long() { verify_type<long>(); }
  void test_ulong() { verify_type<unsigned long>(); }

  void test_8b_byte() { verify_8b_type<byte>(); }
  void test_8b_ubyte() { verify_8b_type<ubyte>(); }
  void test_8b_short() { verify_8b_type<short>(); }
  void test_8b_ushort() { verify_8b_type<unsigned short>(); }
  void test_8b_int() { verify_8b_type<int>(); }
  void test_8b_uint() { verify_8b_type<unsigned int>(); }
};
