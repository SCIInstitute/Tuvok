#include <cxxtest/TestSuite.h>
#include "RAWConverter.h"
#include "util-test.h"

// creates a temporary UVF from the specified data.  returns its filename.
static std::string mkuvf(uint64_t value) {
  std::ofstream f;
  std::string rawdata = mk_tmpfile(f, std::ios::out | std::ios::binary);
  clean inputs = cleanup(rawdata);
  gen_constant<uint64_t>(f, 1, value);
  f.flush(); f.close();

  std::string uvf = mk_tmpfile(f, std::ios::out | std::ios::binary);
  if(RAWConverter::ConvertRAWDataset(
      rawdata, uvf, ".", 0, sizeof(uint64_t)*8, 1, 1, false, false,
      false, UINT64VECTOR3(1,1,1), FLOATVECTOR3(1.0, 1.0, 1.0),
      "description", "nosrc", 64, 4, true, false, 0) != true) {
    TS_FAIL("converting data set failed.");
  }
  return uvf;
}

static std::vector<std::string> smalluvfs() {
  std::vector<std::string> uvfs;
  uvfs.push_back(mkuvf(42));
  uvfs.push_back(mkuvf(19));
  return uvfs;
}

class TestExpressions : public CxxTest::TestSuite {
public:
  void test_addition_1() {
    EnableDebugMessages edm;
    std::vector<std::string> uvf = smalluvfs();
    clean fclean = cleanup(uvf[0]).add(uvf[1]);

    const IOManager& iom = *Controller::Instance().IOMan();
    iom.EvaluateExpression("v[0] + 1", uvf, ".temp");
  }
};
