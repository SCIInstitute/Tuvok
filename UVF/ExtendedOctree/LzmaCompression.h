#ifndef UVF_LZMA_COMPRESSION_H
#define UVF_LZMA_COMPRESSION_H

#include <cstdint>
#include <memory>
#include <array>

/**
  Generate encoded LZMA properties in 'encodedProps' based on compression level.
  @param  encodedProps dst array where the encoded properties are written
  @param  compressionLevel between 0..9 will be encoded in 'encodedProps'
  @throws std::runtime_error if something fails
  */
void lzmaProperties(std::array<uint8_t, 5>& encodedProps,
                    uint32_t compressionLevel = 4);

/**
  Decompresses data into 'dst'.
  @param  src the data to decompress
  @param  dst the output buffer
  @param  uncompressedBytes number of bytes available and expected in 'dst'
  @param  encodedProps encoded LZMA properties header
  @throws std::runtime_error if something fails
  */
void lzmaDecompress(std::shared_ptr<uint8_t> src, std::shared_ptr<uint8_t>& dst,
                    size_t uncompressedBytes,
                    std::array<uint8_t, 5> const& encodedProps);

/**
  Compresses data into 'dst' using LZMA algorithm (7z).
  @param  src the data to compress
  @param  uncompressedBytes number of bytes in 'src'
  @param  dst the output buffer that will be created of the same size as 'src'
  @param  encodedProps LZMA properties header generated during compression
  @param  compressionLevel between 0..9 that will be encoded in 'encodedProps'
  @return the number of bytes in the compressed data
  @throws std::runtime_error if something fails
  */
size_t lzmaCompress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                    std::shared_ptr<uint8_t>& dst,
                    std::array<uint8_t, 5>& encodedProps,
                    uint32_t compressionLevel = 4);

#endif /* UVF_LZMA_COMPRESSION_H */

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
