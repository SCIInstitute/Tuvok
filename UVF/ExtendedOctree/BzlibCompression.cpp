#include <cassert>
#include <stdexcept>
#include <string>
#include "Basics/SysTools.h"
#include "Basics/nonstd.h"
#include "BzlibCompression.h"

extern "C" {
#include "IO/3rdParty/bzip2/bzlib.h"
}

namespace {

  char const* ErrorCodeToStr(int errorCode)
  {
    switch (errorCode) {
    case BZ_CONFIG_ERROR:     return "BZ_CONFIG_ERROR";
    case BZ_PARAM_ERROR:      return "BZ_PARAM_ERROR";
    case BZ_MEM_ERROR:        return "BZ_MEM_ERROR";
    case BZ_OUTBUFF_FULL:     return "BZ_OUTBUFF_FULL";
    case BZ_DATA_ERROR:       return "BZ_DATA_ERROR";
    case BZ_DATA_ERROR_MAGIC: return "BZ_DATA_ERROR_MAGIC";
    case BZ_UNEXPECTED_EOF:   return "BZ_UNEXPECTED_EOF";
    default: return "Unknown";
    }
  }

}

size_t bzCompress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                  std::shared_ptr<uint8_t>& dst,
                  uint32_t compressionLevel)
{
  // To guarantee that the compressed data will fit in its buffer, allocate an 
  // output buffer of size 1% larger than the uncompressed data, plus six 
  // hundred extra bytes.
  unsigned int upperBound = static_cast<unsigned int>(uncompressedBytes * 1.01) + 600;
  if (size_t(upperBound) < uncompressedBytes)
    std::runtime_error("Input data too big for bzip2");
  dst.reset(new uint8_t[size_t(upperBound)], nonstd::DeleteArray<uint8_t>());

  assert(compressionLevel <= 9);
  assert(compressionLevel > 0);
  if (compressionLevel > 9)
    compressionLevel = 9;
  else if (compressionLevel < 1)
    compressionLevel = 1;

  int res = BZ2_bzBuffToBuffCompress((char*)dst.get(),
                                     &upperBound,
                                     (char*)src.get(),
                                     static_cast<unsigned int>(uncompressedBytes),
                                     int(compressionLevel),
                                     0, 0);
  if (res != BZ_OK)
    throw std::runtime_error(std::string("BZ2_bzBuffToBuffCompress failed. ") +
                             std::string(ErrorCodeToStr(res)));
  else
    return upperBound; // compressed bytes
}

void bzDecompress(std::shared_ptr<uint8_t> src,
                  size_t compressedBytes,
                  std::shared_ptr<uint8_t>& dst,
                  size_t uncompressedBytes)
{
  unsigned int outputSize = static_cast<unsigned int>(uncompressedBytes);
  int res = BZ2_bzBuffToBuffDecompress((char*)dst.get(),
                                       &outputSize,
                                       (char*)src.get(),
                                       static_cast<unsigned int>(compressedBytes),
                                       0, 0);
  if (res != BZ_OK)
    throw std::runtime_error(std::string("BZ2_bzBuffToBuffDecompress failed. ")
                             + std::string(ErrorCodeToStr(res)));
  if (uncompressedBytes != size_t(outputSize))
    throw std::runtime_error("Bzip2 decompression failed, output size does not "
                             " match expected output size.");
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
