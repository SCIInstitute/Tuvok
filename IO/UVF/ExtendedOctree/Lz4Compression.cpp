#include <cassert>
#include <stdexcept>
#include <string>
#include "Basics/SysTools.h"
#include "Basics/nonstd.h"
#include "Lz4Compression.h"

extern "C" {
#include "lz4.h"
#include "lz4hc.h"
}

size_t lz4Compress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                   std::shared_ptr<uint8_t>& dst,
                   uint32_t compressionLevel)
{
  if (uncompressedBytes > size_t(LZ4_MAX_INPUT_SIZE))
    throw std::runtime_error("Input data too big for LZ4 (max LZ4_MAX_INPUT_SIZE)");
  int const inputSize = static_cast<int>(uncompressedBytes);
  int const upperBound = LZ4_compressBound(inputSize);
  if (upperBound < 0)
    throw std::runtime_error("Input data too big for LZ4 (max LZ4_MAX_INPUT_SIZE)");
  dst.reset(new uint8_t[size_t(upperBound)], nonstd::DeleteArray<uint8_t>());

  if (compressionLevel > 17)
    compressionLevel = 17;
  else if (compressionLevel < 1)
    compressionLevel = 1;

  int compressedBytes = 0;
  switch (compressionLevel) {
  case 1:
    compressedBytes = LZ4_compress((const char*)src.get(),
                                   (char*)dst.get(),
                                   inputSize);
    break;
  default:
    // compression level 0 is default mode which seems to equal level 9 and HC
    compressedBytes = LZ4_compressHC2((const char*)src.get(),
                                      (char*)dst.get(),
                                      inputSize,
                                      compressionLevel - 1);
    break;
  }

  assert(compressedBytes >= 0);
  if (compressedBytes <= 0)
    throw std::runtime_error(std::string("LZ4_compress[HC] failed, returned value: ") +
      SysTools::ToString(compressedBytes));
  return compressedBytes;
}

void lz4Decompress(std::shared_ptr<uint8_t> src, std::shared_ptr<uint8_t>& dst,
                   size_t uncompressedBytes)
{
  if (uncompressedBytes > size_t(LZ4_MAX_INPUT_SIZE))
    throw std::runtime_error("Expected output data too big for LZ4 (max LZ4_MAX_INPUT_SIZE)");

  int const outputSize = static_cast<int>(uncompressedBytes);
  int readBytes = LZ4_decompress_fast((const char*)src.get(),
                                 (char*)dst.get(),
                                 outputSize);
  if (readBytes < 0)
    throw std::runtime_error(std::string("LZ4_decompress_fast failed: faulty input "
                             "byte at position ") +
                             SysTools::ToString(-readBytes));
}

/*
 The MIT License
 
 Copyright (c) 2011 Interactive Visualization and Data Analysis Group
 
 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 */
