#ifndef SCIO_TEST_UTIL_H
#define SCIO_TEST_UTIL_H
#include <cstring>
#include <fstream>
#if defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ <= 1))
# define BROKEN_TR1_RANDOM
#endif
#include <random>
#include "Controller/Controller.h"
using namespace tuvok;

// "tuvok" ubyte, "tuvok" byte.
// we can't just use byte because MS' compiler defines it.
typedef unsigned char tubyte;
typedef signed char tbyte;

size_t filesize(const char fn[]) {
  std::ifstream ifs(fn, std::ios::binary);
  ifs.seekg(0, std::ios::end);
  size_t retval = static_cast<size_t>(static_cast<int>(ifs.tellg()));
  ifs.seekg(0, std::ios::beg);
  return retval;
}

template <size_t n, typename element_t, typename function_t>
static function_t for_each(element_t (&x)[n], function_t func)
{ return std::for_each(x, x+n, func); }

template<class T> static void Delete(T* t) { delete t; }

/// Put one of these on your stack to enable Tuvok's debug messages for a single
/// test.  Or put it as a private member of your test class to enable debugging
/// for that whole set of tests.
struct EnableDebugMessages {
  EnableDebugMessages() {
    Controller::Debug::Out().SetOutput(true,true,true,true);
  }
  ~EnableDebugMessages() {
    Controller::Debug::Out().SetOutput(true,true,false,false);
  }
};

// An equality check that automatically switches to allowing a small epsilon
// when used with FP types.
template <typename T>
inline void check_equality(T a, T b) { TS_ASSERT_EQUALS(a, b); }
template <>
inline void check_equality<double>(double a, double b) {
  TS_ASSERT_DELTA(a,b, 0.0001);
}

// Create a temporary file and return the name.
// This isn't great -- there's a race between when we close and reopen it --
// but there's no (standard) way to turn a file descriptor into a std::fstream.
static std::string mk_tmpfile(std::ofstream& ofs, std::ios_base::openmode mode)
{
#ifdef _WIN32
  char *templ = tmpnam((char*)0);
  ofs.open(templ, mode);
#else
  char templ[64];
  strcpy(templ, "iotest.XXXXXX");
  int fd = mkstemp(templ);
  close(fd);
  ofs.open(templ, mode);
#endif
  return std::string(templ);
}

// Data generation code.
namespace {
  // Generates data with a constant value
  template <typename T>
  void gen_constant(std::ostream& os, const size_t sz, const T& val) {
    for(size_t i=0; i < sz; ++i) {
      os.write(reinterpret_cast<const char*>(&val), sizeof(T));
    }
  }

  // Generates data along a normal distribution with the given mean and
  // standard deviation.  Returns the min/max of the generated data.
  template <typename T>
  std::pair<T,T> gen_normal(std::ostream& os, const size_t sz,
                            const T& mean, const T& stddev) {
    std::pair<T,T> minmax = std::make_pair(
      std::numeric_limits<T>::max(),
      -(std::numeric_limits<T>::max()-1) // bleh, not great.
    );
    // double: RNGs are only defined for FP types.  We'll generate double
    // and just cast to T.
#ifndef BROKEN_TR1_RANDOM
    std::variate_generator<std::mt19937,
                           std::normal_distribution<double> > vg(
      std::mt19937(time(NULL)),
      std::normal_distribution<double>(mean, stddev)
    );
#endif
    for(size_t i=0; i < sz/sizeof(T); ++i) {
#ifndef BROKEN_TR1_RANDOM
      T v = static_cast<T>(vg());
#else
      T v = static_cast<T>(42);
#endif
      minmax.first = std::min(minmax.first, v);
      minmax.second = std::max(minmax.second, v);
      os.write(reinterpret_cast<const char*>(&v), sizeof(T));
    }
    return minmax;
  }
}

#endif // SCIO_TEST_UTIL_H
