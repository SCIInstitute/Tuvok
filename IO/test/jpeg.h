#include <algorithm>
#include <functional>

#include <cxxtest/TestSuite.h>

#include "TuvokJPEG.h"
#include "Controller/Controller.h"

#include "util-test.h"

struct testjpeg {
  const char *file;
  const size_t width;
  const size_t height;
  const size_t bpp;
};

#if 0
  {"ref/J2KP4files/codestreams_profile1/p1_01.j2k", 64,64, 3}, // dims?
  {"ref/J2KP4files/reference_class0_profile0/c0p0_01.pgx", 64,64, 3}, // dims?
  {"ref/J2KP4files/reference_class0_profile1/c0p1_01.pgx", 64,64, 3}, // dims?
  {"ref/J2KP4files/reference_class1_profile0/c1p0_01_0.pgx", 64,64, 3}, // d?
  {"ref/J2KP4files/reference_class1_profile1/c1p1_04_0.pgx", 64,64, 3}, // d?
  {"ref/J2KP4files/testfiles_jp2/file3.jp2", 64,64, 3}, // d?
#endif

static const struct testjpeg jpegs[] = {
#if 1
  {"data/einstein.jpeg", 113, 144, 3},
  {"data/ssc-small.jpeg", 450, 450, 3},
  {"data/lena-1bpp.jpeg", 512, 512, 1},
  {"data/lena-3bpp.jpeg", 512, 512, 3},
#endif
  {"data/lossless.jpeg", 224,256, 1}, // dims?
};

struct valid_from_file : public std::unary_function<struct testjpeg, void> {
  void operator()(const struct testjpeg &tj) const {
#ifdef VERBOSE
    TS_TRACE(std::string("file validity for ") + tj.file);
#endif
    tuvok::JPEG jpeg(tj.file);
    TS_ASSERT(jpeg.valid());
    TS_ASSERT_EQUALS(jpeg.width(), tj.width);
    TS_ASSERT_EQUALS(jpeg.height(), tj.height);
    TS_ASSERT_EQUALS(jpeg.components(), tj.bpp);
  }
};

struct valid_from_mem : public std::unary_function<struct testjpeg, void> {
  void operator()(const struct testjpeg &jpg) const {
#ifdef VERBOSE
    TS_TRACE(std::string("mem validity for ") + jpg.file);
#endif
    std::vector<char> buffer(filesize(jpg.file));
    std::ifstream ifs(jpg.file, std::ios::binary);
    ifs.read(&buffer.at(0), filesize(jpg.file));
    ifs.close();

    tuvok::JPEG jpeg(buffer);
    TS_ASSERT(jpeg.valid());
    TS_ASSERT_EQUALS(jpeg.width(), jpg.width);
    TS_ASSERT_EQUALS(jpeg.height(), jpg.height);
    TS_ASSERT_EQUALS(jpeg.components(), jpg.bpp);
  }
};

struct file_can_read : public std::unary_function<struct testjpeg, void> {
  void operator()(const struct testjpeg &jpg) const {
    tuvok::JPEG jpeg(jpg.file);
    TS_ASSERT(jpeg.valid());
    TS_ASSERT_EQUALS(jpeg.width(), jpg.width);
    TS_ASSERT_EQUALS(jpeg.height(), jpg.height);
    TS_ASSERT_EQUALS(jpeg.components(), jpg.bpp);
    TS_ASSERT(jpeg.data() != NULL);
  }
};

struct mem_can_read : public std::unary_function<struct testjpeg, void> {
  void operator()(const struct testjpeg &jpg) const {
#ifdef VERBOSE
    TS_TRACE(std::string("readable test for ") + jpg.file);
#endif
    std::vector<char> buffer(filesize(jpg.file));
    std::ifstream ifs(jpg.file, std::ios::binary);
    ifs.read(&buffer.at(0), filesize(jpg.file));
    ifs.close();

    tuvok::JPEG jpeg(buffer);
    TS_ASSERT_DIFFERS(jpeg.data(), static_cast<const char*>(NULL));
  }
};

class JpegTest : public CxxTest::TestSuite {
  public:
    void test_validity_file()   { for_each(jpegs, valid_from_file()); }
    void test_validity_memory() { for_each(jpegs, valid_from_mem()); }
    void test_readable_file()   { for_each(jpegs, file_can_read()); }
    void test_readable_memory() { for_each(jpegs, mem_can_read()); }
  private:
};
