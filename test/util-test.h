#ifndef SCIO_TEST_UTIL_H
#define SCIO_TEST_UTIL_H
#include <fstream>

static size_t filesize(const char fn[]) {
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

#endif // SCIO_TEST_UTIL_H
