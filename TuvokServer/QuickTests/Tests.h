//
//  main.cpp
//  TuvokClientTest
//
//  Created by Rainer Schlönvoigt on 05.05.14.
//  Copyright (c) 2014 Rainer Schlönvoigt. All rights reserved.
//

#define DEBUG_BRICK 0
#define DEBUG_MBRICK 0
#include <iostream>
#include "netds.h"
#include <inttypes.h>
#include <vector>

using namespace NETDS;
using namespace SOCK;

void printVar(uint8_t var) {
    std::cout << var << "\n";
    //printf("%" PRIu8 "\n", var);
}
void printVar(uint16_t var) {
    std::cout << var << "\n";
    //printf("%" PRIu16 "\n", var);
}
void printVar(uint32_t var) {
    std::cout << var << "\n";
    //printf("%" PRIu32 "\n", var);
}

template <typename T>
void typedSingleTest(DSMetaData metaData) {
    (void)metaData;

    size_t lod = 0;
    size_t bidx = 0;

    vector<T> buffer;
    if(!NETDS::getBrick(lod, bidx, buffer))
        abort();

    printf("\nSingle brick (lod: %zu, bidx: %zu): Received brick data (%zu values);\n", lod, bidx, buffer.size());
    
#if DEBUG_BRICK
    for (size_t i = 0; i < buffer.size(); i++) {
        printVar(buffer[i]);
    }
    printf("End of list.\n");
#endif
}

template <typename T>
void typedMultiTest(DSMetaData metaData) {
    srand(5000);
    
    size_t brickCount = std::min((size_t)2, metaData.brickCount);
    std::vector<size_t> lods(brickCount);
    std::vector<size_t> bidxs(brickCount);
    for (size_t i = 0; i < brickCount; i++) {
        size_t rand_index = rand() % metaData.brickCount;
        lods[i]     = metaData.lods[rand_index];
        bidxs[i]    = metaData.idxs[rand_index];
    }
    
    vector<vector<T>> result;
    if(!NETDS::getBricks(brickCount, lods, bidxs, result))
        abort();

    printf("Multi-Brick: Received bricks:\n");
    for (size_t i = 0; i < result.size(); i++) {
        printf("Brick %zu: has %zu values!\n", i, result[i].size());
#if DEBUG_MBRICK
        for (size_t j = 0; j < result[i].size(); j++) {
            printVar(result[i][j]);
        }
        printf("Brick %zu: End of list.\n", i);
#endif
    }
    printf("End of brick-list!\n");
}

template <typename T>
void typedRotationTest(DSMetaData metaData) {
    (void)metaData;

    printf("\nRequesting rotation with identity matrix.\n");
    NETDS::setBatchSize(4);
    
    float identityMatrix[16] = {1, 0, 0, 0,
                                0, 1, 0, 0,
                                0, 0, 1, 0,
                                0, 0, 0, 1};
    NETDS::rotate(identityMatrix);
    
    shared_ptr<NETDS::RotateInfo> rotInfo = NETDS::getLastRotationKeys();
    printf("We should be receiving the following %zu bricks:\n", rotInfo->brickCount);
    for(size_t i = 0; i < rotInfo->brickCount; i++) {
        printf("lod: %zu, idx: %zu\n", rotInfo->lods[i], rotInfo->idxs[i]);
    }
    printf("End of list\n");

    bool done = false;

    vector<vector<T>> batchData;
    BatchInfo bInfo;

    while (!done) {
        if(!NETDS::readBrickBatch(bInfo, batchData))
            abort();

        printf("\nReceived a batch of size %zu\n", bInfo.batchSize);
        for (size_t i = 0; i < bInfo.batchSize; i++) {
            printf("Brick %zu (lod: %zu, idx: %zu) has size: %zu\n", i, bInfo.lods[i], bInfo.idxs[i], bInfo.brickSizes[i]);
        }
        printf("End of batch!\n");
        
        done = !bInfo.moreDataComing;
    }
}

void performTests()
{
    vector<string> filenames;
    if(!NETDS::listFiles(filenames))
        abort();

    printf("Received the following file names:\n");
    for (size_t i = 0; i < filenames.size(); i++) {
        printf("%s\n", filenames[i].c_str());
    }
    printf("End of list.\n");
    
    if (filenames.size() > 0) {
        printf("\nRequesting OPEN file with name: %s\n", filenames[0].c_str());
        
        DSMetaData metaData;
        std::array<size_t, 3> bSize = {{1024, 1024, 1024}};
        NETDS::openFile(filenames[0], metaData, 2, bSize, 1920, 1080);
        
        if (metaData.lodCount == 0) {
            abort();
            return;
        }
        
        NetDataType dType = netTypeForPlainT(metaData.typeInfo);
        
        if (dType == N_UINT8) {
            typedSingleTest<uint8_t>(metaData);
            typedMultiTest<uint8_t>(metaData);
            typedRotationTest<uint8_t>(metaData);
        }
        else if(dType == N_UINT16) {
            typedSingleTest<uint16_t>(metaData);
            typedMultiTest<uint16_t>(metaData);
            typedRotationTest<uint16_t>(metaData);
        }
        else if(dType == N_UINT32) {
            typedSingleTest<uint32_t>(metaData);
            typedMultiTest<uint32_t>(metaData);
            typedRotationTest<uint32_t>(metaData);
        }
        else
            abort(); //Not implemented
        
        printf("\nRequesting CLOSE file with name: %s\n", filenames[0].c_str());
        NETDS::closeFile(filenames[0]);
    }

    //NETDS::shutdownServer();
}

