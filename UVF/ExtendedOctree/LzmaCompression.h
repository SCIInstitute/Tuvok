#ifndef UVF_LZMA_COMPRESSION_H
#define UVF_LZMA_COMPRESSION_H

#include <cstdint>
#include <memory>
#include <array>

/**
  Decompresses data into 'dst'.
  @param  src the data to decompress
  @param  dst the output buffer
  @param  uncompressedBytes number of bytes available and expected in 'dst'
  @param  encodedProps encoded LZMA properties header
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
  */
size_t lzmaCompress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                    std::shared_ptr<uint8_t>& dst,
                    std::array<uint8_t, 5>& encodedProps,
                    uint32_t compressionLevel = 4);

#endif /* UVF_LZMA_COMPRESSION_H */
