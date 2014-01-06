#pragma once

#ifndef UVFBASIC_H
#define UVFBASIC_H

#ifndef UVFVERSION
  #define UVFVERSION 5
#else
  #if UVFVERSION != 5
    #error [UVFBasic.h] Version mismatch
  #endif
#endif

#include "Basics/StdDefines.h"
#define UVF_INVALID UINT64_INVALID

#include "Basics/LargeRAWFile.h"

class DataBlock;

#endif // UVFBASIC_H
