#include "parameterwrapper.h"
#include <mpi.h>

#define DEBUG_PARAMS 1
#define DEBUG_SYNC 0

#if DEBUG_PARAMS
#define dprintf printf
#else
#define dprintf 0 && printf
#endif

#if DEBUG_SYNC
#define dprintf2 printf
#else
#define dprintf2 0 && printf
#endif

ParameterWrapper* ParamFactory::createFrom(NetDSCommandCode cmd, int socket) {
    switch(cmd) {
    case nds_OPEN:
        return new OpenParams(socket);
        break;
    case nds_CLOSE:
        return new CloseParams(socket);
        break;
    case nds_BRICK:
        return new BrickParams(socket);
        break;
    case nds_LIST_FILES:
        return new ListFilesParams(cmd);
        break;
    case nds_SHUTDOWN:
        return new ShutdownParams(cmd);
        break;
    default:
        printf("Unknown command received.\n");
        break;
    }

    return NULL;
}


/*#################################*/
/*#######   Constructors    #######*/
/*#################################*/

ParameterWrapper::ParameterWrapper(NetDSCommandCode code)
    :code(code)
{}

ParameterWrapper::~ParameterWrapper() {}

OpenParams::OpenParams(int socket)
    :ParameterWrapper(nds_OPEN){
    if (socket != -1)
        initFromSocket(socket);
}

CloseParams::CloseParams(int socket)
    :ParameterWrapper(nds_CLOSE){
    if (socket != -1)
        initFromSocket(socket);
}

BrickParams::BrickParams(int socket)
    :ParameterWrapper(nds_BRICK){
    if (socket != -1)
        initFromSocket(socket);
}

SimpleParams::SimpleParams(NetDSCommandCode code)
    :ParameterWrapper(code) {}

ListFilesParams::ListFilesParams(NetDSCommandCode code)
    :SimpleParams(code) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank == 0)
        dprintf("LIST\n");
}

ShutdownParams::ShutdownParams(NetDSCommandCode code)
    :SimpleParams(code) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank == 0)
        dprintf("SHUTDOWN\n");
}


/*#################################*/
/*#######   Initialisators  #######*/
/*#################################*/

void OpenParams::initFromSocket(int socket) {
    ru16(socket, &len);

    filename = new char[len];
    readFromSocket(socket, filename, len*sizeof(char));
    dprintf("OPEN (%d) %s\n", len, filename);
}

void CloseParams::initFromSocket(int socket) {
    ru16(socket, &len);

    filename = new char[len];
    readFromSocket(socket, filename, len*sizeof(char));
    dprintf("CLOSE (%d) %s\n", len, filename);
}

void BrickParams::initFromSocket(int socket) {
    ru32(socket, &lod);
    ru32(socket, &bidx);
    dprintf("BRICK lod=%d, bidx=%d\n", lod, bidx);
}

void SimpleParams::initFromSocket(int socket) {}


/*#################################*/
/*######  Writing to socket  ######*/
/*#################################*/

void OpenParams::writeToSocket(int socket) {
    wru8(socket, code);
    wru16(socket, len);
    wr(socket, filename, len);
}

void CloseParams::writeToSocket(int socket) {
    wru8(socket, code);
    wru16(socket, len);
    wr(socket, filename, len);
}

void BrickParams::writeToSocket(int socket) {
    wru8(socket, code);
    wru32(socket, lod);
    wru32(socket, bidx);
}

void SimpleParams::writeToSocket(int socket) {
    wru8(socket, code);
}


/*#################################*/
/*######     MPI Syncing     ######*/
/*#################################*/

void OpenParams::mpi_sync(int rank, int srcRank) {
    MPI_Bcast(&len, 1, MPI_INT16_T, srcRank, MPI_COMM_WORLD);
    if (rank != srcRank)
        filename = new char[len];
    MPI_Bcast(&filename[0], len, MPI_CHAR, srcRank, MPI_COMM_WORLD);

    if(rank != srcRank)
        dprintf2("Hi there from proc %d! Open Received: %s (%d)\n", rank, filename, len);
}

void CloseParams::mpi_sync(int rank, int srcRank) {
    MPI_Bcast(&len, 1, MPI_INT16_T, srcRank, MPI_COMM_WORLD);
    if (rank != srcRank)
        filename = new char[len];
    MPI_Bcast(&filename[0], len, MPI_CHAR, srcRank, MPI_COMM_WORLD);

    if(rank != srcRank)
        dprintf2("Hi there from proc %d! Close Received: %s (%d)\n", rank, filename, len);
}

void BrickParams::mpi_sync(int rank, int srcRank) {
    MPI_Bcast(&lod, 1, MPI_INT32_T, srcRank, MPI_COMM_WORLD);
    MPI_Bcast(&bidx, 1, MPI_INT32_T, srcRank, MPI_COMM_WORLD);

    if(rank != srcRank)
        dprintf2("Hi there from proc %d! Brick Received: lod %d & bidx: %d)\n", rank, lod, bidx);
}

void SimpleParams::mpi_sync(int rank, int srcRank) {}


/*#################################*/
/*######     Executing       ######*/
/*#################################*/

void OpenParams::perform(int socket, void* object) {
    //TODO
}

void CloseParams::perform(int socket, void* object) {
    //TODO
}

void BrickParams::perform(int socket, void* object) {
    //TODO
}

void ListFilesParams::perform(int socket, void* object) {
    //TODO

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank == 0) {
        const char* name1 = "Test1";
        const char* name2 = "Test2";

        wru16(socket, 2);
        size_t len = strlen(name1);
        wru16(socket, (uint16_t)len);
        wr(socket, name1, len);

        len = strlen(name2);
        wru16(socket, (uint16_t)len);
        wr(socket, name2, len);
    }
}

void ShutdownParams::perform(int socket, void* object) {
    //Not necessary
}

