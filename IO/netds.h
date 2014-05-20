#ifndef NETDATASET_H
#define NETDATASET_H
#include <stdlib.h>

#ifdef __cplusplus
# define EXPORT extern "C"
#else
# define EXPORT /* no extern "C" needed. */
#endif

//For requesting single bricks
EXPORT uint8_t* netds_brick_request_ui8(const size_t lod, const size_t bidx, size_t* count);
EXPORT uint16_t* netds_brick_request_ui16(const size_t lod, const size_t bidx, size_t* count);
EXPORT uint32_t* netds_brick_request_ui32(const size_t lod, const size_t bidx, size_t* count);

//For requesting multiple bricks at once
EXPORT uint8_t** netds_brick_request_ui8v(const size_t brickCount, const size_t* lods, const size_t* bidxs, size_t** dataCounts);
EXPORT uint16_t** netds_brick_request_ui16v(const size_t brickCount, const size_t* lods, const size_t* bidxs, size_t** dataCounts);
EXPORT uint32_t** netds_brick_request_ui32v(const size_t brickCount, const size_t* lods, const size_t* bidxs, size_t** dataCounts);

EXPORT void netds_open(const char*);
EXPORT void netds_close(const char*);
EXPORT char** netds_list_files(size_t* count);
EXPORT void netds_shutdown();

#ifdef __cplusplus
namespace {
template<typename T> T*
netds_brick_request(const size_t, const size_t, size_t*);

template<> uint8_t*
netds_brick_request(const size_t lod, const size_t bidx,
                                     size_t* count) {
  return netds_brick_request_ui8(lod, bidx, count);
}
template<> uint16_t*
netds_brick_request(const size_t lod, const size_t bidx,
                                       size_t* count) {
  return netds_brick_request_ui16(lod, bidx, count);
}
template<> uint32_t*
netds_brick_request(const size_t lod, const size_t bidx,
                                       size_t* count) {
  return netds_brick_request_ui32(lod, bidx, count);
}

template<class T> T**
netds_brick_request_v(const size_t, const size_t* LoDs, const size_t* bidxs,
                      size_t** counts);

template<> uint8_t**
netds_brick_request_v(const size_t brickCount, const size_t* lods,
                      const size_t* bidxs, size_t** dataCounts) {
  return netds_brick_request_ui8v(brickCount, lods, bidxs, dataCounts);
}
template<> uint16_t**
netds_brick_request_v(const size_t brickCount, const size_t* lods,
                      const size_t* bidxs, size_t** dataCounts) {
  return netds_brick_request_ui16v(brickCount, lods, bidxs, dataCounts);
}
template<> uint32_t**
netds_brick_request_v(const size_t brickCount, const size_t* lods,
                      const size_t* bidxs, size_t** dataCounts) {
  return netds_brick_request_ui32v(brickCount, lods, bidxs, dataCounts);
}
}
#endif

#endif
