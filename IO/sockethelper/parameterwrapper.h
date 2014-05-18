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

class BrickParams : public ParameterWrapper
{
    template <class T>
    void internal_brickPerform(int socket, CallPerformer* object) {
        vector<T> returnData;
        object->brick_request(lod, bidx, returnData);

        printf("There are %zu values in the brick.\n", returnData.size());

        //Return the data
        if(typeid(T) == typeid(uint8_t)) {
            printf("Test 0\n");
            uint8_t* data = (uint8_t*)&returnData[0];
            wru8v(socket, data, returnData.size());
        }
        else if(typeid(T) == typeid(uint16_t)) {
            printf("Test 1\n");
            uint16_t* data = (uint16_t*)&returnData[0];
            wru16v(socket, data, returnData.size());
        }
        else if(typeid(T) == typeid(uint32_t)) {
            printf("Test 2\n");
            uint32_t* data = (uint32_t*)&returnData[0];
            wru32v(socket, data, returnData.size());
        }
        else {
            printf("Brick return type invalid!\n");
            abort();
        }
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
