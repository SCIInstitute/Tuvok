#include <cassert>
#include <stdexcept>
#include "zlib.h"
#include "Basics/nonstd.h"
#include "ZlibCompression.h"

/** if you call 'inflateInit' on a stream, you must cause inflateEnd (even if
 * inflation fails) to cleanup internal zlib memory allocations. */
struct CleanupZlibInStream {
  void operator()(z_stream* strm) const {
    inflateEnd(strm);
    delete strm;
  }
};


void zDecompress(std::shared_ptr<uint8_t> src, std::shared_ptr<uint8_t>& dst,
                 size_t uncompressedBytes)
{
  if(static_cast<uint64_t>(uncompressedBytes) >
     std::numeric_limits<uInt>::max()) {
    /* we'd have to decompress this data in chunks, this mem-based interface
     * can't work.  Just bail for now. */
    throw std::runtime_error("expected uncompressed size too large");
  }
  std::shared_ptr<z_stream> strm(new z_stream, CleanupZlibInStream());
  strm->zalloc = Z_NULL; strm->zfree = Z_NULL; strm->opaque = Z_NULL;

  strm->avail_in = 0;
  strm->next_in = Z_NULL;
  if(inflateInit(strm.get()) != Z_OK) {
    assert("zlib initialization failed" && false);
    throw std::runtime_error("zlib initialization failed");
  }
  strm->avail_in = static_cast<uInt>(uncompressedBytes);
  strm->next_in = src.get();
  strm->avail_out = static_cast<uInt>(uncompressedBytes);
  strm->next_out = dst.get();

  int ret;
  uInt bytes = 0; // processed
  do { /* until stream ends */
    strm->avail_in = static_cast<uInt>(uncompressedBytes) - bytes;
    if(strm->avail_in == 0) { break; }

    strm->next_in = src.get() + bytes;

    const uInt save = static_cast<uInt>(uncompressedBytes) - bytes;
    strm->avail_out = static_cast<uInt>(uncompressedBytes) - bytes;
    strm->next_out = dst.get() + bytes;
    ret = inflate(strm.get(), Z_FINISH);
    assert(ret != Z_STREAM_ERROR); // only happens w/ invalid params
    assert(ret != Z_MEM_ERROR); // only happens if 'out' is not big enough
    assert(ret != Z_BUF_ERROR); // ditto above, for our case (Z_FINISH)
    assert(ret != Z_NEED_DICT); // we don't set dicts when compressing
    if(ret == Z_DATA_ERROR) {
      throw std::runtime_error("Brick compression checksum invalid.");
    }

    /* avail_out was set to 'X', and after inflate it is 'Y'.  So inflate
     * consumed X-Y bytes of the output buffer. */
    bytes += save - strm->avail_out;
  } while(ret != Z_STREAM_END);
}

/** if you call 'deflateInit' on a stream, you must call deflateEnd (even if
 * deflation fails) to cleanup internal zlib memory allocations. */
struct CleanupZlibStream {
  void operator()(z_stream* strm) const {
    deflateEnd(strm);
    delete strm;
  }
};

/** compresses data into 'out'.
 * @parameter in the data to compress
 * @parameter n  number of bytes in 'in'
 * @parameter out the output buffer created
 * @returns the number of bytes in the compressed data */
size_t zCompress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                 std::shared_ptr<uint8_t>& dst) {
  if(static_cast<uint64_t>(uncompressedBytes) >
     std::numeric_limits<uInt>::max()) {
    /* we'd have to compress this data in chunks, this mem-based interface
     * can't work.  Just bail for now. */
    dst = src;
    return uncompressedBytes;
  }
  std::shared_ptr<z_stream> strm(new z_stream, CleanupZlibStream());
  strm->zalloc = Z_NULL;
  strm->zfree = Z_NULL;
  strm->opaque = Z_NULL;
  if(deflateInit(strm.get(), Z_BEST_SPEED) != Z_OK) {
    /* zlib initialization failed, just bail with no compression. */
    dst = src;
    return uncompressedBytes;
  }
  strm->avail_in = static_cast<uInt>(uncompressedBytes);
  strm->next_in = src.get();
  dst.reset(new uint8_t[uncompressedBytes], nonstd::DeleteArray<uint8_t>());
  strm->avail_out = static_cast<uInt>(uncompressedBytes);
  strm->next_out = dst.get();

  int ret;
  do {
    ret = deflate(strm.get(), Z_FINISH);
    assert(ret != Z_STREAM_ERROR);
    if(ret != Z_STREAM_END) {
      /* compression failed.  Bail out. */
      dst = src;
      return uncompressedBytes;
    }
  } while(ret != Z_STREAM_END);

  return uncompressedBytes - strm->avail_out;
}
