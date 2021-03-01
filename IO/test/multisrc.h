#include <cstdint>
#include <cstdlib>
#include <time.h>
#include <algorithm>
#include <functional>
#include <vector>

#include <cxxtest/TestSuite.h>

#include "Controller/Controller.h"
#include "Quantize.h"

#include "util-test.h"

using namespace tuvok;

// marchsnerrlob -- 0:255
// out.dat.raw: 0:204
class MultiDataSrcTesting: public CxxTest::TestSuite {
  public:
  void test_multi() {
    LargeRAWFile f1 = LargeRAWFile("data/MarschnerLobb.raw");
    f1.Open(false);
    LargeRAWFile f2 = LargeRAWFile("data/out.dat.raw");
    f2.Open(false);
    std::vector<LargeRAWFile> files;
    files.push_back(f1);
    files.push_back(f2);

    multi_raw_data_src<tubyte> multisrc((files));
    NullProgress<tubyte> np;

    std::pair<tubyte, tubyte> mm =
      io_minmax(multisrc, NullHistogram<tubyte>(), np,
                static_cast<size_t>(DEFAULT_INCORESIZE));
    check_equality<tubyte>(mm.first, 0);
    check_equality<tubyte>(mm.second, 254);
  }
};
