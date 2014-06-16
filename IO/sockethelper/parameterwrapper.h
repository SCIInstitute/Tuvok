#ifndef PARAMETERWRAPPER_H
#define PARAMETERWRAPPER_H

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include "sockhelp.h"
#include "../../TuvokServer/callperformer.h"

class ParameterWrapper
{
public:
    NetDSCommandCode code;
    ParameterWrapper(NetDSCommandCode code);
    virtual ~ParameterWrapper();

    //To overwrite
    virtual void initFromSocket(int socket)         = 0;
    virtual void writeToSocket(int socket)          = 0;
    virtual void mpi_sync(int rank, int srcRank)    = 0;
    virtual void perform(int socket, CallPerformer* object)  = 0;
};

class OpenParams : public ParameterWrapper
{
public:
    uint16_t len;
    char *filename;

    OpenParams(int socket = -1);
    void initFromSocket(int socket);
    void writeToSocket(int socket);
    void mpi_sync(int rank, int srcRank);
    void perform(int socket, CallPerformer* object);
};

class CloseParams : public ParameterWrapper
{
public:
    uint16_t len;
    char *filename;

    CloseParams(int socket = -1);
    void initFromSocket(int socket);
    void writeToSocket(int socket);
    void mpi_sync(int rank, int srcRank);
    void perform(int socket, CallPerformer* object);
};

class BatchSizeParams : public ParameterWrapper
{
public:
    size_t newBatchSize;

    BatchSizeParams(int socket = -1);
    void initFromSocket(int socket);
    void writeToSocket(int socket);
    void mpi_sync(int rank, int srcRank);
    void perform(int socket, CallPerformer* object);
};

class RotateParams : public ParameterWrapper
{
    bool newDataOnSocket(int socket) {
        //TODO: implement correct
        return false;
    }

    template <class T>
    void startBrickSendLoop(int socket, CallPerformer *object, std::vector<tuvok::BrickKey>& allKeys) {
        uint8_t moreDataComing = 1;
        size_t offset = 0;

        size_t maxBatchSize = object->maxBatchSize;
        std::vector<size_t> lods(maxBatchSize);
        std::vector<size_t> idxs(maxBatchSize);
        std::vector<size_t> brickSizes(maxBatchSize);
        std::vector<std::vector<T>> batchBricks(maxBatchSize);
        while(moreDataComing && !newDataOnSocket(socket)) {
            lods.resize(0);
            idxs.resize(0);
            brickSizes.resize(0);
            for(size_t i = 0; i < batchBricks.size(); i++)
                batchBricks[i].resize(0);
            batchBricks.resize(0);

            for(size_t i = 0; i < maxBatchSize && (i + offset) < allKeys.size(); i++) {
                const tuvok::BrickKey& bk = allKeys[i + offset];
                lods.push_back(std::get<1>(bk));
                idxs.push_back(std::get<2>(bk));
            }

            size_t actualBatchSize = lods.size();
            offset += actualBatchSize;
            moreDataComing = (offset == (allKeys.size() - 1)) ? 0 : 1;

            wrsizet(socket, actualBatchSize);
            wru8(socket, moreDataComing);

            if(actualBatchSize > 0) {
                for(size_t i = 0; i < actualBatchSize; i++) {
                    object->brick_request(lods[i], idxs[i], batchBricks[i]);
                    brickSizes[i] = batchBricks[i].size();
                }

                wrsizetv_d(socket, &lods[0], actualBatchSize);
                wrsizetv_d(socket, &idxs[0], actualBatchSize);
                wrsizetv_d(socket, &brickSizes[0], actualBatchSize);

                for(size_t i = 0; i < actualBatchSize; i++) {
                    if(brickSizes[i] > 0)
                        wr_multiple(socket, &batchBricks[i][0], brickSizes[i], false);
                }
            }
        }
    }

public:
    uint8_t type; //When using, cast to NetDataType first... here we leave it as uint8_t for easier MPI-Syncing
    size_t matSize;
    float *matrix;

    RotateParams(int socket = -1);
    void initFromSocket(int socket);
    void writeToSocket(int socket);
    void mpi_sync(int rank, int srcRank);
    void perform(int socket, CallPerformer* object);
};

class BrickParams : public ParameterWrapper
{
    template <class T>
    void internal_brickPerform(int socket, CallPerformer* object) {
        vector<T> returnData;
        object->brick_request(lod, bidx, returnData);

        printf("There are %zu values in the brick.\n", returnData.size());
        wr_multiple(socket, &returnData[0], returnData.size(), true);
    }

public:
    uint8_t type; //When using, cast to NetDataType first... here we leave it as uint8_t for easier MPI-Syncing
    uint32_t lod;
    uint32_t bidx;

    BrickParams(int socket = -1);
    void initFromSocket(int socket);
    void writeToSocket(int socket);
    void mpi_sync(int rank, int srcRank);
    void perform(int socket, CallPerformer* object);
};

//For functions that don't need parameters
class SimpleParams : public ParameterWrapper
{
public:
    SimpleParams(NetDSCommandCode code);
    void initFromSocket(int socket);
    void writeToSocket(int socket);
    void mpi_sync(int rank, int srcRank);
};

class ListFilesParams : public SimpleParams
{
public:
    ListFilesParams(NetDSCommandCode code);
    void perform(int socket, CallPerformer* object);
};

class ShutdownParams : public SimpleParams
{
public:
    ShutdownParams(NetDSCommandCode code);
    void perform(int socket, CallPerformer* object);
};


class ParamFactory
{
public:
    static ParameterWrapper* createFrom(NetDSCommandCode code, int socket);
};

#endif // PARAMETERWRAPPER_H
