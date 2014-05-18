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

#define DEBUG_BRICK 0
#define DEBUG_MBRICK 0

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
        netds_open(filenames[0]);
        
        size_t dataCount;
        size_t lod = 0;
        size_t bidx = 0;
        uint8_t* data = netds_brick_request_ui8(0, 0, &dataCount);
        printf("\nSingle brick uint8_t (lod: %zu, bidx: %zu): Received brick data (%zu values);\n", lod, bidx, dataCount);
        
#if DEBUG_BRICK
        for (size_t i = 0; i < dataCount; i++) {
            printf("%" PRIu8 "\n", data[i]);
        }
        printf("End of list.\n");
#else
        (void)data;
#endif
        
        size_t brickCount = 2;
        size_t lods[2] = {0, 1};
        size_t bidxs[2] = {0, 4};
        
        size_t *dataCounts = NULL;
        uint32_t** data2 = netds_brick_request_ui32v(brickCount, lods, bidxs, &dataCounts);
        printf("Multi-Brick uint32_t: Received bricks:\n");
        for (size_t i = 0; i < brickCount; i++) {
            printf("Brick %zu: has %zu values!\n", i, dataCounts[i]);
#if DEBUG_MBRICK
            for (size_t j = 0; j < dataCounts[i]; j++) {
                printf("%" PRIu32 "\n", data2[i][j]);
            }
            printf("Brick %zu: End of list.\n", i);
#else
            (void)data2;
#endif
        }
        printf("End of brick-list!\n");
        
        printf("\nRequesting CLOSE file with name: %s\n", filenames[0]);
        netds_close(filenames[0]);
    }
    
    //netds_shutdown();
    return 0;
}

