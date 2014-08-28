#define _POSIX_C_SOURCE 200812L
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "netds.h"
#include "DebugOut/debug.h"
#include <array>
#include <limits>
#include <cstdint>

DECLARE_CHANNEL(net);

/* file descriptor for server we're pushing requests to. */
static int remote   = -1;
static int remote2  = -1;

/* port number we'll connect to on the server */
static const unsigned short portA = 4445;
static const unsigned short portB = 4446; //For batches

static shared_ptr<NETDS::RotateInfo> lastKeys(nullptr);
static shared_ptr<NETDS::DSMetaData> dsmd(nullptr);
static shared_ptr<NETDS::MMInfo> minMaxInfo(nullptr);

using namespace SOCK;

static void
force_connect() {
    if(remote == -1) {
        if((remote = connect_server(portA))== -1) {
            fprintf(stderr, "Bailing due to error.\n");
            abort();
        }
    }
    if(remote2 == -1) {
        if((remote2 = connect_server(portB))== -1) {
            fprintf(stderr, "Bailing due to error.\n");
            abort();
        }
    }
    assert(remote > 0);
}

namespace NETDS {

//Requesting a single brick
template<typename T>
bool sharedSingleBrickStuff(const size_t LoD, const size_t brickidx, vector<T>& resultBuffer) {
    force_connect();
    if(LoD > std::numeric_limits<uint32_t>::max()) {
        ERR(net, "LoD is absurd (%zu).  Bug elsewhere.\n", LoD);
        abort();
    }
    if(brickidx > std::numeric_limits<uint32_t>::max()) {
        ERR(net, "brick index is absurd (%zu).  Bug elsewhere.\n", brickidx);
        abort();
    }

    if(!wr_single(remote, (uint8_t)nds_BRICK)) {
        ERR(net, "Could not send brick-requestCode to server!\n");
        return false;
    }

    //Tell the other side which data type to use
    if(!wr_sizet(remote, LoD)) {
        ERR(net, "Could not send brick-LOD to server!\n");
        return false;
    }
    if(!wr_sizet(remote, brickidx)) {
        ERR(net, "Could not send brick-index to server!\n");
        return false;
    }

    if(!r_multiple(remote, resultBuffer, false)) {
        ERR(net, "Could not read brick-data from server!\n");
        return false;
    }

    return true;
}
bool getBrick(const size_t lod, const size_t bidx, vector<uint8_t>& resultBuffer) {
    return sharedSingleBrickStuff(lod, bidx, resultBuffer);
}
bool getBrick(const size_t lod, const size_t bidx, vector<uint16_t>& resultBuffer) {
    return sharedSingleBrickStuff(lod, bidx, resultBuffer);
}
bool getBrick(const size_t lod, const size_t bidx, vector<uint32_t>& resultBuffer) {
    return sharedSingleBrickStuff(lod, bidx, resultBuffer);
}


//Requesting multiple bricks
template<typename T>
bool sharedMultiBrickStuff(const size_t  brickCount, const vector<size_t>& lods, const vector<size_t>& bidxs, vector<vector<T>>& resultBuffer) {
    //Making sure the buffer has the correct size
    resultBuffer.resize(brickCount);

    //TODO just a temporary solution, will rewrite it to a single request
    for(size_t i = 0; i < brickCount; i++) {
        if(!getBrick(lods[i], bidxs[i], resultBuffer[i])) {
            return false; //Dont need to print error, since netds_brick_request already does
        }
    }

    return true;
}
bool getBricks(const size_t  brickCount, const vector<size_t>& lods, const vector<size_t>& bidxs, vector<vector<uint8_t>>& resultBuffer) {
    return sharedMultiBrickStuff(brickCount, lods, bidxs, resultBuffer);
}
bool getBricks(const size_t  brickCount, const vector<size_t>& lods, const vector<size_t>& bidxs, vector<vector<uint16_t>>& resultBuffer) {
    return sharedMultiBrickStuff(brickCount, lods, bidxs, resultBuffer);
}
bool getBricks(const size_t  brickCount, const vector<size_t>& lods, const vector<size_t>& bidxs, vector<vector<uint32_t>>& resultBuffer) {
    return sharedMultiBrickStuff(brickCount, lods, bidxs, resultBuffer);
}

//Reading batches from socket
template<typename T>
bool sharedBatchReadStuff(BatchInfo& out_info, vector<vector<T>>& buffer) {
    force_connect();
    size_t batchSize;
    if(!r_sizet(remote2, batchSize)) {
        ERR(net, "Could not read batchsize from socket!\n");
        return false;
    }

    out_info.batchSize = batchSize;

    uint8_t moreNet;
    if(!r_single(remote2, moreNet)) {
        ERR(net, "Could not read moreDataComing-flag from socket!\n");
        return false;
    }
    out_info.moreDataComing = (moreNet == 1);

    buffer.resize(batchSize);
    out_info.lods.resize(batchSize);
    out_info.idxs.resize(batchSize);
    out_info.brickSizes.resize(batchSize);

    if(batchSize <= 0)
        return true;

    if(!r_mult_sizet(remote2, out_info.lods, true)) {
        ERR(net, "Could not read batch-lods from socket!\n");
        return false;
    }
    if(!r_mult_sizet(remote2, out_info.idxs, true)) {
        ERR(net, "Could not read batch-idxs from socket!\n");
        return false;
    }
    if(!r_mult_sizet(remote2, out_info.brickSizes, true)) {
        ERR(net, "Could not read batch-brickSizes from socket!\n");
        return false;
    }

    for(size_t i = 0; i < out_info.batchSize; i++) {
        buffer[i].resize(out_info.brickSizes[i]);
        if(!r_multiple(remote2, buffer[i], true)) {
            ERR(net, "Could not read brickData for batchIndex %zu of %zu!\n", i, out_info.batchSize);
            return false;
        }
    }

    return true;
}

bool readBrickBatch(BatchInfo &out_info, vector<vector<uint8_t>>& buffer) {
    return sharedBatchReadStuff(out_info, buffer);
}
bool readBrickBatch(BatchInfo &out_info, vector<vector<uint16_t>>& buffer){
    return sharedBatchReadStuff(out_info, buffer);
}
bool readBrickBatch(BatchInfo &out_info, vector<vector<uint32_t>>& buffer){
    return sharedBatchReadStuff(out_info, buffer);
}


void setClientMetaData(struct DSMetaData d) {
    dsmd = std::make_shared<DSMetaData>(d);
}
struct DSMetaData clientMetaData() {
    if(!dsmd) {
        FIXME(net, "Can't read metaData: openFile must be called first!");
        assert(false);
    }
    return *dsmd;
}


void setMinMaxInfo(const MMInfo& info) {
    minMaxInfo = std::make_shared<MMInfo>(info);
}
bool calcMinMax(MMInfo& result) {
    force_connect();

    wr_single(remote, nds_CALC_MINMAX);

    if(!r_sizet(remote, result.brickCount)) {
        ERR(net, "Could not read amount of bricks from socket during minMax call!\n");
        return false;
    }

    result.lods.resize(result.brickCount);
    result.idxs.resize(result.brickCount);
    result.minScalars.resize(result.brickCount);
    result.maxScalars.resize(result.brickCount);
    result.minGradients.resize(result.brickCount);
    result.maxGradients.resize(result.brickCount);

    bool success = (r_mult_sizet(remote, result.lods, true)
            && r_mult_sizet(remote, result.idxs, true)
            && r_multiple(remote, result.minScalars, true)
            && r_multiple(remote, result.maxScalars, true)
            && r_multiple(remote, result.minGradients, true)
            && r_multiple(remote, result.maxGradients, true));

    if(success)
        setMinMaxInfo(result);
    else {
        ERR(net, "Could not read minMax result from socket!\n");
    }

    return success;
}
shared_ptr<MMInfo> getMinMaxInfo() {
    if(!minMaxInfo) {
        FIXME(net, "MinMaxInfo not yet set!!! call calcMinMax first!");
        assert(false);
    }
    return minMaxInfo;
}
void clearMinMaxValues() {
    minMaxInfo.reset();
}

bool openFile(const string& filename, DSMetaData& out_meta, size_t minmaxMode, std::array<size_t, 3> bSize, uint32_t width, uint32_t height)
{
    force_connect();

    wr_single(remote, nds_OPEN);

    wr_mult_sizet(remote, &bSize[0], 3, false);
    wr_sizet(remote, minmaxMode);
    wr_single(remote, width);
    wr_single(remote, height);
    wr_single(remote, filename);

    //Read meta-data from server
    out_meta.filename = filename;
    r_sizet(remote, out_meta.lodCount);
    if (out_meta.lodCount == 0) {
        return false;
    }

    uint8_t ntype;
    r_single(remote, ntype);
    out_meta.typeInfo = bitWidthFromNType((NetDataType)ntype);

    r_multiple(remote, out_meta.layouts, false);
    r_multiple(remote, out_meta.domainSizes, false);
    assert(out_meta.layouts.size() == out_meta.lodCount*3);

    out_meta.overlap.resize(3);
    r_multiple(remote, out_meta.overlap, true);

    r_single(remote, out_meta.range1);
    r_single(remote, out_meta.range2);

    size_t brickCount;
    r_sizet(remote, brickCount);
    out_meta.brickCount    = brickCount;
    out_meta.lods.resize(brickCount);
    out_meta.idxs.resize(brickCount);
    out_meta.md_centers.resize(brickCount * 3);
    out_meta.md_extents.resize(brickCount * 3);
    out_meta.md_n_voxels.resize(brickCount * 3);

    //Retrieve key-data
    r_mult_sizet(remote, out_meta.lods, true);
    r_mult_sizet(remote, out_meta.idxs, true);

    //Retrieve BrickMDs
    r_multiple(remote, out_meta.md_centers, true);
    r_multiple(remote, out_meta.md_extents, true);
    r_multiple(remote, out_meta.md_n_voxels, true);

    r_single(remote, out_meta.maxGradientMagnitude);

    //TODO: Receive brick zero?

    setClientMetaData(out_meta);

    return true;
}

void closeFile(const string& filename)
{
    if(remote == -1) {
        return;
    }
    wr_single(remote, nds_CLOSE);
    wr_single(remote, filename);

    lastKeys.reset();
    dsmd.reset();
    minMaxInfo.reset();
}

void shutdownServer() {
    if(remote == -1) { return; }
    wr_single(remote, nds_SHUTDOWN);
}

void rotate(const float m[16]) {
    if(remote == -1) { /* ignore rotations if not connected. */
        return;
    }
    /* we might want to start thinking about cork/uncorking our sends .. */
    wr_single(remote, nds_ROTATION);
    wr_multiple(remote, &m[0], 16, false);

    //Answer represents all keys of bricks needed to be rendered
    if(!lastKeys) {
        lastKeys = std::make_shared<RotateInfo>();
    }
    r_sizet(remote, lastKeys->brickCount);
    lastKeys->lods.resize(lastKeys->brickCount);
    lastKeys->idxs.resize(lastKeys->brickCount);
    r_mult_sizet(remote, lastKeys->lods, true);
    r_mult_sizet(remote, lastKeys->idxs, true);
}

shared_ptr<RotateInfo> getLastRotationKeys() {
    return lastKeys;
}

void clearRotationKeys() {
    lastKeys.reset();
}

bool listFiles(vector<string>& resultBuffer)
{
    force_connect();

    wr_single(remote, nds_LIST_FILES);

    uint16_t tmp_count;
    r_single(remote, tmp_count);

    resultBuffer.resize(tmp_count);
    for (size_t i = 0; i < tmp_count; i++) {
        r_single(remote, resultBuffer[i]);
    }

    return true;
}

void setBatchSize(size_t maxBatchSize) {
    wr_single(remote, nds_BATCHSIZE);
    wr_sizet(remote, maxBatchSize);
}

/*
    void netds_cancelBatches() {
        wru8(remote, (uint8_t)nds_CANCEL_BATCHES);

        //still have to flush but don't have a proper type... would have to template it
    }*/
}
