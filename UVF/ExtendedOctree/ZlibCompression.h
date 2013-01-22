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
  @return the number of bytes in the compressed data
  */
size_t zCompress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                 std::shared_ptr<uint8_t>& dst);

#endif /* UVF_ZLIB_COMPRESSION_H */
