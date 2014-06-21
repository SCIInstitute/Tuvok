//
//  main.cpp
//  TuvokClientTest
//
//  Created by Rainer Schlönvoigt on 05.05.14.
//  Copyright (c) 2014 Rainer Schlönvoigt. All rights reserved.
//

#include <iostream>
#include "netds.h"
#include <inttypes.h>
#include <vector>

#define DEBUG_BRICK 0
#define DEBUG_MBRICK 0

void printVar(uint8_t var) {
    printf("%" PRIu8 "\n", var);
}
void printVar(uint16_t var) {
    printf("%" PRIu16 "\n", var);
}
void printVar(uint32_t var) {
    printf("%" PRIu32 "\n", var);
}

template <typename T>
void typedSingleTest(NetDataType dType, DSMetaData metaData) {
    size_t dataCount;
    size_t lod = 0;
    size_t bidx = 0;
    
    T* data = netds_brick_request<T>(0, 0, &dataCount);
    printf("\nSingle brick (lod: %zu, bidx: %zu): Received brick data (%zu values);\n", lod, bidx, dataCount);
    
#if DEBUG_BRICK
    for (size_t i = 0; i < dataCount; i++) {
        printVar(data[i]);
    }
    printf("End of list.\n");
#else
    (void)data;
#endif
}

template <typename T>
void typedMultiTest(NetDataType dType, DSMetaData metaData) {
    srand(5000);
    
    size_t brickCount = std::min((size_t)2, metaData.brickCount);
    std::vector<size_t> lods(brickCount);
    std::vector<size_t> bidxs(brickCount);
    for (size_t i = 0; i < brickCount; i++) {
        size_t rand_index = rand() % metaData.brickCount;
        lods[i]     = metaData.lods[rand_index];
        bidxs[i]    = metaData.idxs[rand_index];
    }
    
    size_t *dataCounts = NULL;
    T** data2 = netds_brick_request_v<T>(brickCount, &lods[0], &bidxs[0], &dataCounts);
    printf("Multi-Brick: Received bricks:\n");
    for (size_t i = 0; i < brickCount; i++) {
        printf("Brick %zu: has %zu values!\n", i, dataCounts[i]);
#if DEBUG_MBRICK
        for (size_t j = 0; j < dataCounts[i]; j++) {
            printVar(data2[i][j]);
        }
        printf("Brick %zu: End of list.\n", i);
#else
        (void)data2;
#endif
    }
    printf("End of brick-list!\n");
}

template <typename T>
void typedRotationTest(NetDataType dType, DSMetaData metaData) {
    printf("\nRequesting rotation with identity matrix.\n");
    netds_setBatchSize(20);
    
    float identityMatrix[16] = {1, 0, 0, 0,
                                0, 1, 0, 0,
                                0, 0, 1, 0,
                                0, 0, 0, 1};
    netds_rotation(identityMatrix);
    
    bool done = false;
    while (!done) {
        BatchInfo bInfo;
        T** data = netds_readBrickBatch<T>(&bInfo);
        printf("Received a batch of size %zu\n", bInfo.batchSize);
        for (size_t i = 0; i < bInfo.batchSize; i++) {
            printf("Brick %zu has size: %zu\n", i, bInfo.brickSizes[i]);
            delete data[i];
        }
        delete data;
        printf("End of batch!\n");
        
        done = !bInfo.moreDataComing;
        freeBatchInfo(&bInfo);
    }
}

int main(int argc, const char * argv[])
{
    size_t fileCount;
    char** filenames = netds_list_files(&fileCount);
    printf("Received the following file names:\n");
    for (size_t i = 0; i < fileCount; i++) {
        printf("%s\n", filenames[i]);
    }
    printf("End of list.\n");
    
    if (fileCount > 0) {
        printf("\nRequesting OPEN file with name: %s\n", filenames[0]);
        
        DSMetaData metaData;
        netds_open(filenames[0], &metaData);
        
        if (metaData.lodCount == 0) {
            abort();
            return 0;
        }
        
        NetDataType dType = netTypeForPlainT(metaData.typeInfo);
        
        if (dType == N_UINT8) {
            typedSingleTest<uint8_t>(dType, metaData);
            typedMultiTest<uint8_t>(dType, metaData);
            typedRotationTest<uint8_t>(dType, metaData);
        }
        else if(dType == N_UINT16) {
            typedSingleTest<uint16_t>(dType, metaData);
            typedMultiTest<uint16_t>(dType, metaData);
            typedRotationTest<uint16_t>(dType, metaData);
        }
        else if(dType == N_UINT32) {
            typedSingleTest<uint32_t>(dType, metaData);
            typedMultiTest<uint32_t>(dType, metaData);
            typedRotationTest<uint32_t>(dType, metaData);
        }
        else
            abort(); //Not implemented
        
        printf("\nRequesting CLOSE file with name: %s\n", filenames[0]);
        netds_close(filenames[0]);
    }
    
    //netds_shutdown();
    return 0;
}

