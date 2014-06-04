#include <mpi.h>
#include "parameterwrapper.h"
#include "DebugOut/debug.h"

DECLARE_CHANNEL(params);
DECLARE_CHANNEL(sync);

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
    if(rank == 0) {
        TRACE(params, "LIST");
    }
}

ShutdownParams::ShutdownParams(NetDSCommandCode code)
    :SimpleParams(code) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank == 0) {
        TRACE(params, "SHUTDOWN");
    }
}


/*#################################*/
/*#######   Initialisators  #######*/
/*#################################*/

void OpenParams::initFromSocket(int socket) {
    ru16(socket, &len);

    filename = new char[len];
    readFromSocket(socket, filename, len*sizeof(char));
    TRACE(params, "OPEN (%d) %s", len, filename);
}

void CloseParams::initFromSocket(int socket) {
    ru16(socket, &len);

    filename = new char[len];
    readFromSocket(socket, filename, len*sizeof(char));
    TRACE(params, "CLOSE (%d) %s", len, filename);
}

void BrickParams::initFromSocket(int socket) {
    ru8(socket, &type);
    ru32(socket, &lod);
    ru32(socket, &bidx);
    TRACE(params, "BRICK lod=%u, bidx=%u", lod, bidx);
}

void SimpleParams::initFromSocket(int socket) {(void)socket;}


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
    MPI_Bcast(&len, 1, MPI_UNSIGNED_SHORT, srcRank, MPI_COMM_WORLD);

    if (rank != srcRank)
        filename = new char[len];
    MPI_Bcast(&filename[0], len, MPI_UNSIGNED_CHAR, srcRank, MPI_COMM_WORLD);

    if(rank != srcRank) {
        TRACE(sync, "proc %d open received %s (%d)", rank, filename, len);
    }
}

void CloseParams::mpi_sync(int rank, int srcRank) {
    MPI_Bcast(&len, 1, MPI_UNSIGNED_SHORT, srcRank, MPI_COMM_WORLD);
    if (rank != srcRank)
        filename = new char[len];
    MPI_Bcast(&filename[0], len, MPI_CHAR, srcRank, MPI_COMM_WORLD);

    if(rank != srcRank) {
        TRACE(sync, "proc %d close received %s (%d)", rank, filename, len);
    }
}

void BrickParams::mpi_sync(int rank, int srcRank) {
    MPI_Bcast(&type, 1, MPI_UNSIGNED_CHAR, srcRank, MPI_COMM_WORLD);
    MPI_Bcast(&lod, 1, MPI_UNSIGNED, srcRank, MPI_COMM_WORLD);
    MPI_Bcast(&bidx, 1, MPI_UNSIGNED, srcRank, MPI_COMM_WORLD);

    if(rank != srcRank) {
        TRACE(sync, "proc %d brick received: lod %u & bidx: %u", rank, lod,
              bidx);
    }
}

void SimpleParams::mpi_sync(int rank, int srcRank) {(void)rank; (void)srcRank;}


/*#################################*/
/*######     Executing       ######*/
/*#################################*/

void OpenParams::perform(int socket, CallPerformer* object) {
    object->openFile(filename);
    (void)socket; //currently no answer
}

void CloseParams::perform(int socket, CallPerformer* object) {
    object->closeFile(filename);
    (void)socket; //currently no answer
}

void BrickParams::perform(int socket, CallPerformer* object) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank == 0) {
        NetDataType dType = (NetDataType)type;

        if(dType == N_UINT8)
            internal_brickPerform<uint8_t>(socket, object);
        else if(dType == N_UINT16)
            internal_brickPerform<uint16_t>(socket, object);
        else if(dType == N_UINT32)
            internal_brickPerform<uint32_t>(socket, object);
    }
}

void ListFilesParams::perform(int socket, CallPerformer* object) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank == 0) {
        vector<std::string> filenames = object->listFiles();

        wru16(socket, (uint16_t)filenames.size());
        for(std::string name : filenames) {
            const char* cstr = name.c_str();
            wrCStr(socket, cstr);
        }
    }
}

void ShutdownParams::perform(int socket, CallPerformer* object) {
    //Not necessary
    (void)socket; //currently no answer
    (void)object;
}
