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
template <class T>
T* netds_brick_request(const size_t lod, const size_t bidx, size_t* count) {
    if(typeid(T) == typeid(uint8_t))
        return (T*)netds_brick_request_ui8(lod, bidx, count);
    else if(typeid(T) == typeid(uint16_t))
        return (T*)netds_brick_request_ui16(lod, bidx, count);
    else if(typeid(T) == typeid(uint32_t))
        return (T*)netds_brick_request_ui32(lod, bidx, count);
    else {
        perror("Brick request type not yet supported");
        abort();
        return NULL;
    }
}

template <class T>
T** netds_brick_request_v(const size_t brickCount, const size_t* lods, const size_t* bidxs, size_t** dataCounts) {
    if(typeid(T) == typeid(uint8_t))
        return (T**)netds_brick_request_ui8v(brickCount, lods, bidxs, dataCounts);
    else if(typeid(T) == typeid(uint16_t))
        return (T**)netds_brick_request_ui16v(brickCount, lods, bidxs, dataCounts);
    else if(typeid(T) == typeid(uint32_t))
        return (T**)netds_brick_request_ui32v(brickCount, lods, bidxs, dataCounts);
    else {
        perror("Brick request type not yet supported");
        abort();
        return NULL;
    }
}
#endif

#endif
