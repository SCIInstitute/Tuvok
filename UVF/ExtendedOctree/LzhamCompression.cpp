#include <cassert>
#include <stdexcept>
#include <string>
#include "Basics/SysTools.h"
#include "Basics/nonstd.h"
#include "Lz4Compression.h"

#include "lzham_static_lib.h"

namespace {

  char const* ErrorCodeToStr(int errorCode)
  {
    switch (errorCode) {
    case LZHAM_Z_STREAM_END:   return "LZHAM_Z_STREAM_END";
    case LZHAM_Z_STREAM_ERROR: return "LZHAM_Z_STREAM_ERROR";
    case LZHAM_Z_PARAM_ERROR:  return "LZHAM_Z_PARAM_ERROR";
    case LZHAM_Z_BUF_ERROR:    return "LZHAM_Z_BUF_ERROR";
    default: return "Unknown";
    }
  }

}

size_t lzhamCompress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                     std::shared_ptr<uint8_t>& dst,
                     uint32_t compressionLevel)
{
  if (compressionLevel > 10)
    compressionLevel = 10;

  lzham_z_ulong const inputBytes = static_cast<lzham_z_ulong>(uncompressedBytes);
  lzham_z_ulong upperBound = lzham_z_compressBound(inputBytes);
  if (static_cast<size_t>(upperBound) < uncompressedBytes)
    throw std::runtime_error("Input data too big for LZHAM");

  dst.reset(new uint8_t[size_t(upperBound)], nonstd::DeleteArray<uint8_t>());
  int res = lzham_z_compress2((unsigned char*)dst.get(), &upperBound,
                              (const unsigned char *)src.get(), inputBytes,
                              (int)compressionLevel);
  if (res != LZHAM_Z_OK)
    throw std::runtime_error(std::string("lzham_z_compress2 failed. ") +
                             std::string(ErrorCodeToStr(res)));
  else
    return upperBound;
}

void lzhamDecompress(std::shared_ptr<uint8_t> src, size_t compressedBytes,
                     std::shared_ptr<uint8_t>& dst, size_t uncompressedBytes)
{
  lzham_z_ulong outputSize = static_cast<lzham_z_ulong>(uncompressedBytes);
  int res = lzham_z_uncompress((unsigned char*)dst.get(),
                               &outputSize,
                               (const unsigned char*)src.get(),
                               static_cast<lzham_z_ulong>(compressedBytes));
  if (res != LZHAM_Z_OK)
    throw std::runtime_error(std::string("lzham_z_uncompress failed. ") +
                             std::string(ErrorCodeToStr(res)));
  else if (uncompressedBytes != size_t(outputSize))
    throw std::runtime_error("LZHAM decompression failed, output size does not "
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
