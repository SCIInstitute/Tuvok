#include <cassert>
#include <stdexcept>
#include <string>
#include "Basics/nonstd.h"
#include "LzmaCompression.h"

extern "C" {
#include "LzmaEnc.h"
#include "LzmaDec.h"
}

static void* AllocForLzma(void* /*p*/, size_t size) { return malloc(size); }
static void FreeForLzma(void* /*p*/, void* address) { free(address); }
static ISzAlloc g_AllocForLzma = { &AllocForLzma, &FreeForLzma };

class LzmaError : public std::runtime_error {
public:
  static char const* ErrorCodeToStr(SRes errorCode);
  static char const* StatusToStr(ELzmaStatus status);
  LzmaError(std::string const& msg, int errorCode);
  LzmaError(std::string const& msg, ELzmaStatus status);
};

char const* LzmaError::ErrorCodeToStr(SRes errorCode)
{
  switch (errorCode) {
  case SZ_ERROR_DATA:        return "Data";
  case SZ_ERROR_MEM:         return "Mem";
  case SZ_ERROR_CRC:         return "CRC";
  case SZ_ERROR_UNSUPPORTED: return "Unsupported";
  case SZ_ERROR_PARAM:       return "Param";
  case SZ_ERROR_INPUT_EOF:   return "Input EOF";
  case SZ_ERROR_OUTPUT_EOF:  return "Output EOF";
  case SZ_ERROR_READ:        return "Read";
  case SZ_ERROR_WRITE:       return "Write";
  case SZ_ERROR_PROGRESS:    return "Progress";
  case SZ_ERROR_FAIL:        return "Fail";
  case SZ_ERROR_THREAD:      return "Thread";
  case SZ_ERROR_ARCHIVE:     return "Archive";
  case SZ_ERROR_NO_ARCHIVE:  return "No archive";
  default: return "Unknown";
  }
}

char const* LzmaError::StatusToStr(ELzmaStatus status)
{
  switch (status) {
  case LZMA_STATUS_NOT_SPECIFIED:               return "Not specified";
  case LZMA_STATUS_FINISHED_WITH_MARK:          return "Finished with mark";
  case LZMA_STATUS_NOT_FINISHED:                return "Not finished";
  case LZMA_STATUS_NEEDS_MORE_INPUT:            return "Needs more input";
  case LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK: return "Maybe finished without mark";
  default: return "Unknown";
  }
}

LzmaError::LzmaError(std::string const& msg, int errorCode)
  : std::runtime_error(msg + std::string(ErrorCodeToStr(errorCode)))
{}

LzmaError::LzmaError(std::string const& msg, ELzmaStatus status)
  : std::runtime_error(msg + std::string(StatusToStr(status)))
{}

size_t lzmaCompress(std::shared_ptr<uint8_t> src, size_t uncompressedBytes,
                    std::shared_ptr<uint8_t>& dst,
                    std::array<uint8_t, 5>& encodedProps,
                    uint32_t compressionLevel)
{
  assert(compressionLevel <= 9);
  if (compressionLevel > 9)
    compressionLevel = 9;
  assert(encodedProps.size() == LZMA_PROPS_SIZE);
  SizeT encodedPropsSize = LZMA_PROPS_SIZE;

  CLzmaEncProps props;
  LzmaEncProps_Init(&props);
  props.writeEndMark = 0;
  props.level = compressionLevel;

  dst.reset(new uint8_t[uncompressedBytes], nonstd::DeleteArray<uint8_t>());
  SizeT compressedBytes = uncompressedBytes;
  SRes res = LzmaEncode(dst.get(), &compressedBytes,
                        src.get(), uncompressedBytes,
                        &props, &encodedProps[0], &encodedPropsSize,
                        props.writeEndMark,
                        NULL, &g_AllocForLzma, &g_AllocForLzma);

  assert(encodedPropsSize == LZMA_PROPS_SIZE);
  if (res != SZ_OK)
    throw LzmaError("LzmaEncode failed: ", res);

  return compressedBytes;
}

void lzmaDecompress(std::shared_ptr<uint8_t> src, std::shared_ptr<uint8_t>& dst,
                    size_t uncompressedBytes,
                    std::array<uint8_t, 5> const& encodedProps)
{
  ELzmaStatus status;
  SizeT bytes = uncompressedBytes;
  SRes res = LzmaDecode(dst.get(), &bytes,
                        src.get(), &bytes,
                        &encodedProps[0], LZMA_PROPS_SIZE,
                        LZMA_FINISH_END,
                        &status,
                        &g_AllocForLzma);

  assert(bytes == uncompressedBytes);
  if (res != SZ_OK)
    throw LzmaError("LzmaDecode failed: ", res);

  if (status != LZMA_STATUS_FINISHED_WITH_MARK &&
      status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)
    throw LzmaError("LzmaDecode returned invalid status: ", status);
}
