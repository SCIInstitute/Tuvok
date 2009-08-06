#include <algorithm>
#include <functional>
#include <vector>

#include <cxxtest/TestSuite.h>

#include "DICOM/DICOMParser.h"
#include "Basics/SysTools.h"
#include "Controller/Controller.h"
#include "IO/IOManager.h"

#include "util-test.h"

struct testdicom {
  const char *file;
};

static const struct testdicom dicoms[] = {
  {"data/dicoms/8b-00.dcm"}
};

struct size : public std::unary_function<testdicom, void> {
  void operator()(const testdicom &td) const {
    TS_TRACE(std::string("testing size of ") + td.file);
    DICOMParser dparse;
    dparse.GetDirInfo(SysTools::GetPath(td.file));
    TS_ASSERT_EQUALS(dparse.m_FileStacks.size(), static_cast<size_t>(1));
  }
};

struct stacks : public std::unary_function<testdicom, void> {
  void operator()(const testdicom &td) {
    TS_TRACE(std::string("testing stacks; ") + td.file);
    IOManager &iomgr = *(Controller::Instance().IOMan());
    std::vector<FileStackInfo*> files =
      iomgr.ScanDirectory(SysTools::GetPath(td.file));
    TS_ASSERT(!files.empty());
    std::for_each(files.begin(), files.end(), Delete<FileStackInfo>);
  }
};

class DicomTests : public CxxTest::TestSuite {
  public:
    void test_size() { for_each(dicoms, size()); }
    void test_stacks() { for_each(dicoms, stacks()); }
};
