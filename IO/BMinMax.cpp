#include <algorithm>
#include "BMinMax.h"
#include "Basics/MinMaxBlock.h"
#include "BrickedDataset.h"
#include "Controller/Controller.h"

namespace {
  template<typename T> tuvok::MinMaxBlock mm(const tuvok::BrickKey& bk,
                                             const tuvok::BrickedDataset& ds) {
    std::vector<T> data(ds.GetMaxBrickSize().volume());
    ds.GetBrick(bk, data);
    auto mmax = std::minmax_element(data.begin(), data.end());
    return tuvok::MinMaxBlock(*mmax.first, *mmax.second, DBL_MAX, -FLT_MAX);
  }
}

namespace tuvok {

MinMaxBlock minmax_brick(const BrickKey& bk, const BrickedDataset& ds) {
  // identify type (float, etc)
  const unsigned size = ds.GetBitWidth() / 8;
  assert(ds.GetComponentCount() == 1);
  const bool sign = ds.GetIsSigned();
  const bool fp = ds.GetIsFloat();

  // giant if-block to simply call the right compile-time function w/ knowledge
  // we only know at run-time.
  if(!sign && !fp && size == 1) {
    return mm<uint8_t>(bk, ds);
  } else if(!sign && !fp && size == 2) {
    return mm<uint16_t>(bk, ds);
  } else if(!sign && !fp && size == 4) {
    return mm<uint32_t>(bk, ds);
  } else if(sign && !fp && size == 1) {
    return mm<int8_t>(bk, ds);
  } else if(sign && !fp && size == 2) {
    return mm<int16_t>(bk, ds);
  } else if(sign && !fp && size == 4) {
    return mm<int32_t>(bk, ds);
  } else if(sign && fp && size == 4) {
    return mm<float>(bk, ds);
  } else {
    T_ERROR("unsupported type.");
    assert(false);
  }
  return MinMaxBlock();
}

}
