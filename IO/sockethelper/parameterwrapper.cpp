#if MPI_ACTIVE
#include <mpi.h>
#endif
#include "parameterwrapper.h"
#include "DebugOut/debug.h"
using std::vector;

DECLARE_CHANNEL(params);
DECLARE_CHANNEL(bricks);
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
    case nds_ROTATION:
        return new RotateParams(socket);
        break;
    case nds_BATCHSIZE:
        return new BatchSizeParams(socket);
        break;
    default:
        printf("Unknown command received.\n");
        break;
    }

    return NULL;
}

NetDataType netTypeForDataset(tuvok::Dataset* ds) {
    size_t width = static_cast<size_t>(ds->GetBitWidth());
    bool is_signed = ds->GetIsSigned();
    bool is_float = ds->GetIsFloat();
    return netTypeForBitWidth(width, is_signed, is_float);
}

/*#################################*/
/*#######   Constructors    #######*/
/*#################################*/

ParameterWrapper::ParameterWrapper(NetDSCommandCode code)
    :code(code)
{}

ParameterWrapper::~ParameterWrapper() {}

OpenParams::OpenParams(int socket)
    :ParameterWrapper(nds_OPEN), bSize(3){
    if (socket != -1)
        initFromSocket(socket);
}

CloseParams::CloseParams(int socket)
    :ParameterWrapper(nds_CLOSE){
    if (socket != -1)
        initFromSocket(socket);
}

BatchSizeParams::BatchSizeParams(int socket)
    :ParameterWrapper(nds_BATCHSIZE){
    if (socket != -1)
        initFromSocket(socket);
}

BrickParams::BrickParams(int socket)
    :ParameterWrapper(nds_BRICK){
    if (socket != -1)
        initFromSocket(socket);
}

RotateParams::RotateParams(int socket)
    :ParameterWrapper(nds_ROTATION), rotMatrix(16) {
    if (socket != -1)
        initFromSocket(socket);
}

SimpleParams::SimpleParams(NetDSCommandCode code)
    :ParameterWrapper(code) {}

ListFilesParams::ListFilesParams(NetDSCommandCode code)
    :SimpleParams(code) {

#if MPI_ACTIVE
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank == 0) {
        TRACE(params, "LIST");
    }
#else
    TRACE(params, "LIST");
#endif
}

MinMaxParams::MinMaxParams(NetDSCommandCode code)
    :SimpleParams(code) {

#if MPI_ACTIVE
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank == 0) {
        TRACE(params, "MinMaxCalc");
    }
#else
    TRACE(params, "MinMaxCalc");
#endif
}

ShutdownParams::ShutdownParams(NetDSCommandCode code)
    :SimpleParams(code) {
#if MPI_ACTIVE
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank == 0) {
        TRACE(params, "SHUTDOWN");
    }
#else
    TRACE(params, "SHUTDOWN");
#endif
}


/*#################################*/
/*#######   Initialisators  #######*/
/*#################################*/

void OpenParams::initFromSocket(int socket) {
    r_mult_sizet(socket, bSize, true);
    r_sizet(socket, minmaxMode);
    r_single(socket, width);
    r_single(socket, height);

    r_single(socket, filename);

    TRACE(params, "OPEN (%zu) %s", strlen(filename.c_str()), filename.c_str());
}

void CloseParams::initFromSocket(int socket) {
    r_single(socket, filename);

    TRACE(params, "CLOSE (%zu) %s", strlen(filename.c_str()), filename.c_str());
}

void BatchSizeParams::initFromSocket(int socket) {
    r_sizet(socket, newBatchSize);

    TRACE(params, "BATCHSIZE %zu", newBatchSize);
}

void RotateParams::initFromSocket(int socket) {
    r_multiple(socket, rotMatrix, true);
    TRACE(params, "ROTATE");
}

void BrickParams::initFromSocket(int socket) {
    r_sizet(socket, lod);
    r_sizet(socket, bidx);
    TRACE(params, "BRICK lod=%zu, bidx=%zu", lod, bidx);
}

void SimpleParams::initFromSocket(int socket) {(void)socket;}


/*#################################*/
/*######  Writing to socket  ######*/
/*#################################*/
//Not up to date
/*
void OpenParams::writeToSocket(int socket) {
    wr_single(socket, code);
    wr_single(socket, len);
    wr(socket, filename, len);
}

void CloseParams::writeToSocket(int socket) {
    wr_single(socket, code);
    wr_single(socket, len);
    wr(socket, filename, len);
}

void BatchSizeParams::writeToSocket(int socket) {
    wr_single(socket, code);
    wr_single(socket, newBatchSize);
}

void RotateParams::writeToSocket(int socket) {
    wr_single(socket, code);
    wrf32v(socket, rotMatrix, 16);
}

void BrickParams::writeToSocket(int socket) {
    wr_single(socket, code);
    wr_single(socket, lod);
    wr_single(socket, bidx);
}

void SimpleParams::writeToSocket(int socket) {
    wr_single(socket, code);
}*/


/*#################################*/
/*######     MPI Syncing     ######*/
/*#################################*/

#if MPI_ACTIVE
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

void BatchSizeParams::mpi_sync(int rank, int srcRank) {
    uint32_t tmp = (uint32_t)newBatchSize;
    MPI_Bcast(&tmp, 1, MPI_UNSIGNED, srcRank, MPI_COMM_WORLD);
    newBatchSize = (size_t)tmp;

    if(rank != srcRank) {
        TRACE(sync, "proc %d setBatchSize received with %zu", rank, newBatchSize);
    }
}

void RotateParams::mpi_sync(int rank, int srcRank) {
    MPI_Bcast(&matrix[0], 16, MPI_FLOAT, srcRank, MPI_COMM_WORLD);

    if(rank != srcRank) {
        TRACE(sync, "proc %d rotate received", rank);
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
#else
void OpenParams::mpi_sync(int rank, int srcRank)  {(void)rank; (void)srcRank;}
void CloseParams::mpi_sync(int rank, int srcRank)  {(void)rank; (void)srcRank;}
void BatchSizeParams::mpi_sync(int rank, int srcRank)  {(void)rank; (void)srcRank;}
void RotateParams::mpi_sync(int rank, int srcRank)  {(void)rank; (void)srcRank;}
void BrickParams::mpi_sync(int rank, int srcRank)  {(void)rank; (void)srcRank;}
#endif

void SimpleParams::mpi_sync(int rank, int srcRank) {(void)rank; (void)srcRank;}


/*#################################*/
/*######     Executing       ######*/
/*#################################*/

void OpenParams::perform(int socket, int socketB, CallPerformer* object) {
    (void)socketB;

    object->width   = width;
    object->height  = height;

    bool opened = object->openFile(filename, bSize, minmaxMode);

#if MPI_ACTIVE
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank != 0)
        return; //mpi currently not supported
#endif

    if(!opened) {
        wr_sizet(socket, (size_t)0);
        return;
    }

    //send LODs
    size_t lodCount = object->getDataSet()->GetLODLevelCount();
    wr_sizet(socket, lodCount);

    if(lodCount == 0)
        return;

    const uint8_t ntype = (uint8_t)netTypeForDataset(object->getDataSet());
    wr_single(socket, ntype);

    //Send layouts
    size_t layoutsCount = lodCount * 3;
    uint32_t layouts[layoutsCount];
    uint64_t domainSizes[layoutsCount];
    for(size_t lod=0; lod < lodCount; ++lod) {
        UINTVECTOR3 layout = object->getDataSet()->GetBrickLayout(lod, 0);
        layouts[lod * 3 + 0] = layout.x;
        layouts[lod * 3 + 1] = layout.y;
        layouts[lod * 3 + 2] = layout.z;

        UINT64VECTOR3 domainSize = object->getDataSet()->GetDomainSize(lod, 0);
        domainSizes[lod * 3 + 0] = domainSize.x;
        domainSizes[lod * 3 + 1] = domainSize.y;
        domainSizes[lod * 3 + 2] = domainSize.z;

    }
    wr_multiple(socket, &layouts[0], layoutsCount, true);
    wr_multiple(socket, &domainSizes[0], layoutsCount, true);

    UINTVECTOR3 ovl = object->getDataSet()->GetBrickOverlapSize();
    uint32_t overlap[3] = {ovl.x, ovl.y, ovl.z};
    wr_multiple(socket, &overlap[0], 3, false);

    //write total count of bricks out
    size_t brickCount = object->getDataSet()->GetTotalBrickCount();
    wr_sizet(socket, brickCount);

    //We need the brick Keys
    size_t lods[brickCount];
    size_t idxs[brickCount];
    //And the brickMD
    float md_centers[brickCount * 3];
    float md_extents[brickCount * 3];
    uint32_t md_n_voxels[brickCount * 3];

    //Actually read the data
    size_t i = 0;
    for(auto brick = object->getDataSet()->BricksBegin(); brick != object->getDataSet()->BricksEnd(); brick++, i++) {
        tuvok::BrickKey key    = brick->first;
        tuvok::BrickMD md      = brick->second;

        lods[i] = std::get<1>(key);
        idxs[i] = std::get<2>(key);

        md_centers[i*3 + 0] = md.center.x;
        md_centers[i*3 + 1] = md.center.y;
        md_centers[i*3 + 2] = md.center.z;

        md_extents[i*3 + 0] = md.extents.x;
        md_extents[i*3 + 1] = md.extents.y;
        md_extents[i*3 + 2] = md.extents.z;

        md_n_voxels[i*3 + 0] = md.n_voxels.x;
        md_n_voxels[i*3 + 1] = md.n_voxels.y;
        md_n_voxels[i*3 + 2] = md.n_voxels.z;
    }

    wr_mult_sizet(socket, &lods[0], brickCount, false);
    wr_mult_sizet(socket, &idxs[0], brickCount, false);
    wr_multiple(socket, &md_centers[0], brickCount * 3, false);
    wr_multiple(socket, &md_extents[0], brickCount * 3, false);
    wr_multiple(socket, &md_n_voxels[0], brickCount * 3, false);
}

void CloseParams::perform(int socket, int socketB, CallPerformer* object) {
    object->closeFile(filename);
    (void)socketB;
    (void)socket; //currently no answer
}

void BatchSizeParams::perform(int socket, int socketB, CallPerformer *object) {
    object->maxBatchSize = newBatchSize;
    (void)socketB;
    (void)socket;
}

template <class T>
void startBrickSendLoop(int socket, int socketB, CallPerformer *object, vector<tuvok::BrickKey>& allKeys) {
    uint8_t moreDataComing = 1;
    size_t offset = 0;

    vector<size_t> lods(allKeys.size());
    vector<size_t> idxs(allKeys.size());
    for(size_t i = 0; i < allKeys.size(); i++) {
        const tuvok::BrickKey& bk = allKeys[i];
        lods[i] = std::get<1>(bk);
        idxs[i] = std::get<2>(bk);
    }

    //Tell the client all the keys of bricks to be expected
    size_t brickCount = allKeys.size();
    wr_sizet(socket, brickCount);
    wr_mult_sizet(socket, &lods[0], lods.size(), false);
    wr_mult_sizet(socket, &idxs[0], lods.size(), false);


    size_t maxBatchSize = object->maxBatchSize;
    //printf("max batch size: %zu\n", maxBatchSize);
    vector<size_t> brickSizes(allKeys.size());
    vector<vector<T>> batchBricks(maxBatchSize);
    while(moreDataComing) {
        //In case there is a new request from the client arriving,
        //we have to stop sending the current bricks.
        // !!!! BUT: Due to the asynchronous nature of this sending loop
        // an outdated "end of bricks"-batch might still arrive at a client.
        // Therefore we always send a last "empty batch" that the client can handle.
        if(newDataOnSocket(socket)) {
            TRACE(bricks, "Received new request. Interrupting current brick-batch-sending.");
            wr_sizet(socketB, (size_t)0);
            wr_single(socketB, (uint8_t)0);
            break;
        }

        //Retrieve amount of bricks in this batch and increment the offset
        size_t actualBatchSize = std::min(maxBatchSize, allKeys.size() - offset);
        moreDataComing = ((offset + actualBatchSize) == allKeys.size()) ? 0 : 1;

        //Tell client how many bricks to expect
        wr_sizet(socketB, actualBatchSize);
        wr_single(socketB, moreDataComing);

        if(actualBatchSize > 0) {
            for(size_t i = 0; i < actualBatchSize; i++) {
                size_t in_index = offset + i;
                object->brick_request(lods[in_index], idxs[in_index], batchBricks[i]);
                brickSizes[i] = batchBricks[i].size();
            }

            wr_mult_sizet(socketB, &lods[offset], actualBatchSize, false);
            wr_mult_sizet(socketB, &idxs[offset], actualBatchSize, false);
            wr_mult_sizet(socketB, &brickSizes[0], actualBatchSize, false);

            for(size_t i = 0; i < actualBatchSize; i++) {
                if(brickSizes[i] > 0)
                    wr_multiple(socketB, &batchBricks[i][0], brickSizes[i], false);
            }
        }

        offset += actualBatchSize;
    }
}

void RotateParams::perform(int socket, int socketB, CallPerformer *object) {

#if MPI_ACTIVE
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank != 0)
        return; //mpi currently not supported
#endif

    //renders the scene
    object->rotate(&rotMatrix[0]);
    vector<tuvok::BrickKey> allKeys = object->getRenderedBrickKeys();

    NetDataType dType = netTypeForDataset(object->getDataSet());
    if(dType == N_UINT8)
        startBrickSendLoop<uint8_t>(socket, socketB, object, allKeys);
    else if(dType == N_UINT16)
        startBrickSendLoop<uint16_t>(socket, socketB, object, allKeys);
    else if(dType == N_UINT32)
        startBrickSendLoop<uint32_t>(socket, socketB, object, allKeys);

    printf("Done sending bricks!\n");
}

void BrickParams::perform(int socket, int socketB, CallPerformer* object) {
    (void)socketB;
#if MPI_ACTIVE
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank != 0)
        return;
    //TODO: Would be useful to implement a behavior of:
    // - Do i have brick in cash? no?
    // => get from another MPI process that has brick in cash
#endif

    NetDataType dType = netTypeForDataset(object->getDataSet());

    if(dType == N_UINT8)
        internal_brickPerform<uint8_t>(socket, object);
    else if(dType == N_UINT16)
        internal_brickPerform<uint16_t>(socket, object);
    else if(dType == N_UINT32)
        internal_brickPerform<uint32_t>(socket, object);
}

void ListFilesParams::perform(int socket, int socketB, CallPerformer* object) {
    (void)socketB;
#if MPI_ACTIVE
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(rank != 0)
        return;
#endif
    vector<std::string> filenames = object->listFiles();

    wr_single(socket, (uint16_t)filenames.size());
    for(std::string name : filenames) {
        wr_single(socket, name);
    }
}

void MinMaxParams::perform(int socket, int socketB, CallPerformer *object) {
    (void)socketB;

    size_t brickCount = object->getDataSet()->GetTotalBrickCount();
    vector<size_t> lods (brickCount);
    vector<size_t> idxs (brickCount);
    vector<double> minScalars (brickCount);
    vector<double> maxScalars (brickCount);
    vector<double> minGradients (brickCount);
    vector<double> maxGradients (brickCount);

    //TODO: Retrieve values
    size_t i = 0;
    for(auto brick = object->getDataSet()->BricksBegin(); brick != object->getDataSet()->BricksEnd(); brick++, i++) {
        tuvok::BrickKey key    = brick->first;
        lods[i] = std::get<1>(key);
        idxs[i] = std::get<2>(key);

        tuvok::MinMaxBlock mm = object->getDataSet()->MaxMinForKey(key);
        minScalars[i]   = mm.minScalar;
        maxScalars[i]   = mm.maxScalar;
        minGradients[i] = mm.minGradient;
        maxGradients[i] = mm.maxGradient;
    }

    wr_sizet(socket, brickCount);
    wr_mult_sizet(socket, &lods[0], lods.size(), false);
    wr_mult_sizet(socket, &idxs[0], idxs.size(), false);
    wr_multiple(socket, &minScalars[0], minScalars.size(), false);
    wr_multiple(socket, &maxScalars[0], maxScalars.size(), false);
    wr_multiple(socket, &minGradients[0], minGradients.size(), false);
    wr_multiple(socket, &maxGradients[0], maxGradients.size(), false);
}

void ShutdownParams::perform(int socket, int socketB, CallPerformer* object) {
    (void)socketB;
    //Not necessary
    (void)socket; //currently no answer
    (void)object;
}
