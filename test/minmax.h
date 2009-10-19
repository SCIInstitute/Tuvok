#include <algorithm>
#include <functional>
#include <vector>

#include <cxxtest/TestSuite.h>

#include "Controller/Controller.h"
#include "Quantize.h"

#include "util-test.h"

#define PREFIX "/home/tfogal/data"

template <typename T>
struct testfile {
  const char *file;
  size_t bytes_to_skip;
  T data_min;
  T data_max;
  typedef T base_type;
};

template <typename T>
inline void check_equality(T a, T b) { TS_ASSERT_EQUALS(a, b); }

template <>
inline void check_equality<double>(double a, double b) {
  TS_ASSERT_DELTA(a,b, 0.0001);
}

template <typename T>
struct test_quant : public std::unary_function<testfile<T>, void> {
  void operator()(const testfile<T> &tf) const {
#ifdef VERBOSE
    {
      std::ostringstream trace;
      trace << "testing " << sizeof(T)*8 << "bit data in " << tf.file;
      TS_TRACE(trace.str());
    }
#endif

    std::string fn = std::string(tf.file);
    const size_t sz = filesize(fn.c_str());
    const size_t n_elems = sz / sizeof(T);

    std::vector<UINT64> hist;
    {
#ifdef VERBOSE
      TS_TRACE("raw_data_src");
#endif
      Unsigned12BitHistogram<T> histw(hist);
      LargeRAWFile raw(fn);
      raw.Open(false);
      std::pair<T,T> mm = io_minmax<T>(raw_data_src<T>(raw), histw,
                                       TuvokProgress<UINT64>(n_elems));
      check_equality(tf.data_min, mm.first);
      check_equality(tf.data_max, mm.second);
    }
    {
#ifdef VERBOSE
      TS_TRACE("ios_data_src");
#endif
      Unsigned12BitHistogram<T> histw(hist);
      std::ifstream fs(fn.c_str());
      std::pair<T,T> mm = io_minmax<T>(ios_data_src<T>(fs), histw,
                                       TuvokProgress<UINT64>(n_elems));
      check_equality(tf.data_min, mm.first);
      check_equality(tf.data_max, mm.second);
    }
  }
};

class MinMaxTesting : public CxxTest::TestSuite {
  public:
    void test_short() {
      struct testfile<short> files_short[] = {
        {"data/short",0, -32765, 32741}
      };
      for_each(files_short, test_quant<short>());
    }

    void test_ubyte() {
      struct testfile<unsigned char> files_short[] = {
        {"data/ubyte",0, 0,255}
      };
      for_each(files_short, test_quant<unsigned char>());
    }

    void test_float() {
      struct testfile<float> files_float[] = {
        {"data/float",0, 1.3389827,235.573898},
      };
      for_each(files_float, test_quant<float>());
    }

    void test_double() {
      struct testfile<double> files_short[] = {
        {"data/double",0, 1.3389827013, 235.5738983154},
      };
      for_each(files_short, test_quant<double>());
    }

//    void test_slow() {
//      struct testfile<unsigned char> files8[] = {
//        {PREFIX "/volvis/backpack.raw8",0, 0,255}
//      };
//      struct testfile<unsigned short> files16[] = {
//        {PREFIX "/volvis/backpack16.raw",0, 0,4071}
//      };
//      for_each(files8, test_quant<unsigned char>());
//      for_each(files16, test_quant<unsigned short>());
//    }
};
