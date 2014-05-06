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
    virtual void perform(int socket, void* object)  = 0;
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
    void perform(int socket, void* object);
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
    void perform(int socket, void* object);
};

class BrickParams : public ParameterWrapper
{
public:
    uint32_t lod;
    uint32_t bidx;

    BrickParams(int socket = -1);
    void initFromSocket(int socket);
    void writeToSocket(int socket);
    void mpi_sync(int rank, int srcRank);
    void perform(int socket, void* object);
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
    void perform(int socket, void* object);
};

class ShutdownParams : public SimpleParams
{
public:
    ShutdownParams(NetDSCommandCode code);
    void perform(int socket, void* object);
};


class ParamFactory
{
public:
    static ParameterWrapper* createFrom(NetDSCommandCode code, int socket);
};

#endif // PARAMETERWRAPPER_H
