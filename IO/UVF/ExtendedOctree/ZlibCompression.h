#ifndef UVF_ZLIB_COMPRESSION_H
#define UVF_ZLIB_COMPRESSION_H

#include <cstdint>
#include <memory>

/**
  Decompresses data into 'dst'.
  @param  src the data to decompress
  @param  dst the output buffer
  @param  uncompressedBytes number of bytes available and expected in 'dst'
  */
void zDecompress(std::shared_ptr<uint8_t> src, std::shared_ptr<uint8_t>& dst,
                 size_t uncompressedBytes);

/**
  Compresses data into 'dst' using deflate algorithm (zip).
  @param  src the data to compress
  @param  uncompressedBytes number of bytes in 'src'
  @param  dst the output buffer that will be created of the same size as 'src'
  @param  compressionLevel between 0..9 ( 0 - no compression,
                                          1 - best speed, ...,
                                          9 - best compression)
  @return the number of bytes in the compressed data
  */
size_t zCompress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                 std::shared_ptr<uint8_t>& dst,
                 uint32_t compressionLevel = 1);

#endif /* UVF_ZLIB_COMPRESSION_H */

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
