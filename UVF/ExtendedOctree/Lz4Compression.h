#ifndef UVF_LZ4_COMPRESSION_H
#define UVF_LZ4_COMPRESSION_H

#include <cstdint>
#include <memory>

/**
  Decompresses data into 'dst'.
  @param  src the data to decompress
  @param  dst the output buffer
  @param  uncompressedBytes number of bytes available and expected in 'dst'
  @throws std::runtime_error if something fails
  */
void lz4Decompress(std::shared_ptr<uint8_t> src, std::shared_ptr<uint8_t>& dst,
                   size_t uncompressedBytes);

/**
  Compresses data into 'dst' using LZ4 algorithm.
  @param  src the data to compress
  @param  uncompressedBytes number of bytes in 'src'
  @param  dst the output buffer that will be created of the same size as 'src'
  @param  highCompression high compression mode is disabled for now because it
                          sometimes causes bad memory accesses (see cpp file)
  @return the number of bytes in the compressed data
  @throws std::runtime_error if something fails
  */
size_t lz4Compress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                   std::shared_ptr<uint8_t>& dst,
                   bool highCompression = false);

#endif /* UVF_LZ4_COMPRESSION_H */

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
