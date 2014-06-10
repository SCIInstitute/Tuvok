#ifndef NETDATASET_H
#define NETDATASET_H
#include <stdlib.h>
#include "sockhelp.h"

#ifdef __cplusplus
# define EXPORT extern "C"
#else
# define EXPORT /* no extern "C" needed. */
#endif

//For requesting single bricks
EXPORT uint8_t*  netds_brick_request_ui8(const size_t  lod, const size_t bidx, size_t* out_count);
EXPORT uint16_t* netds_brick_request_ui16(const size_t lod, const size_t bidx, size_t* out_count);
EXPORT uint32_t* netds_brick_request_ui32(const size_t lod, const size_t bidx, size_t* out_count);

//For requesting multiple bricks at once
EXPORT uint8_t**  netds_brick_request_ui8v(const size_t  brickCount, const size_t* lods, const size_t* bidxs, size_t** out_dataCounts);
EXPORT uint16_t** netds_brick_request_ui16v(const size_t brickCount, const size_t* lods, const size_t* bidxs, size_t** out_dataCounts);
EXPORT uint32_t** netds_brick_request_ui32v(const size_t brickCount, const size_t* lods, const size_t* bidxs, size_t** out_dataCounts);

EXPORT void netds_close(const char* filename);
EXPORT char** netds_list_files(size_t* count);
EXPORT void netds_shutdown();

//Initiates a sending of bricks that the client might need!
//call netds_setBatchSize first!!!
EXPORT void netds_rotation(const float m[16], enum NetDataType type);
//The server always sends bricks in form of batches
//This allows the sending of another rotation to restart the sending process
EXPORT void netds_setBatchSize(size_t maxBatchSize);
//EXPORT void netds_cancelBatches();

struct BatchInfo {
    size_t batchSize;
    size_t* lods;
    size_t* idxs;
    size_t* brickSizes;
    bool moreDataComing;
};
// !!! It can also happen, that the batchSize is zero, then NULL will be returned !!!
EXPORT uint8_t**  netds_readBrickBatch_ui8(struct BatchInfo* out_info);
EXPORT uint16_t** netds_readBrickBatch_ui16(struct BatchInfo* out_info);
EXPORT uint32_t** netds_readBrickBatch_ui32(struct BatchInfo* out_info);



#ifdef __cplusplus

#include "Brick.h"

struct DSMetaData {
    size_t lodCount;
    unsigned *layouts; //3 unsigned per LOD

    size_t brickCount; //total count of bricks
    tuvok::BrickKey *brickKeys;
    tuvok::BrickMD *brickMDs;

    //TODO: We are supposed to also send brick zero
    //(Which type?)
};
EXPORT void netds_open(const char* filename, DSMetaData* out_meta);

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

template<class T> T** netds_readBrickBatch(BatchInfo *out_info);
template<> uint8_t** netds_readBrickBatch(BatchInfo *out_info) {
    return netds_readBrickBatch_ui8(out_info);
}
template<> uint16_t** netds_readBrickBatch(BatchInfo *out_info) {
    return netds_readBrickBatch_ui16(out_info);
}
template<> uint32_t** netds_readBrickBatch(BatchInfo *out_info) {
    return netds_readBrickBatch_ui32(out_info);
}

/* Needs some way of defining a type
template<class T> void netds_rotation(const float m[16]);
template<uint8_t> void netds_rotation(const float m[16]) {
    netds_rotation(m, N_UINT8);
}
template<uint16_t> void netds_rotation(const float m[16]) {
    netds_rotation(m, N_UINT16);
}
template<uint32_t> void netds_rotation(const float m[16]) {
    netds_rotation(m, N_UINT32);
}*/

}
#endif

#endif
