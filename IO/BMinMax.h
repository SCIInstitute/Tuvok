#ifndef TUVOK_BMINMAX_H
#define TUVOK_BMINMAX_H

#include "Basics/MinMaxBlock.h"
#include "Brick.h"

namespace tuvok {
class BrickedDataset;

MinMaxBlock minmax_brick(const BrickKey& bk, const BrickedDataset& ds);

}

#endif
