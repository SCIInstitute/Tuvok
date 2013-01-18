#ifndef UVF_ZLIB_COMPRESSION_H
#define UVF_ZLIB_COMPRESSION_H

#include <cstdint>
#include <memory>

void zdecompress(std::shared_ptr<uint8_t> in, std::shared_ptr<uint8_t>& out,
                 unsigned int n);
/** compresses data into 'out'.
 * @parameter in the data to compress
 * @parameter n  number of bytes in 'in'
 * @parameter out the output buffer created
 * @returns the number of bytes in the compressed data */
unsigned int zcompress(std::shared_ptr<uint8_t> in, unsigned int bytes,
                       std::shared_ptr<uint8_t>& out);

#endif /* UVF_ZLIB_COMPRESSION_H */
