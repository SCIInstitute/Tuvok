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

/* file descriptor for server we're pushing requests to. */
static int remote = -1;
/* port number we'll connect to on the server */
static const unsigned short port = 4445;

/* returns file descriptor of connected socket. */
static int
connect_server() {
  const char* host = getenv("IV3D_SERVER");
  if(host == NULL) {
    fprintf(stderr, "You need to set the IV3D_SERVER environment variable to "
                    "the host name or IP address of the server.\n");
    return -1;
  }
  if(getenv("IV3D_USE_WRITE2") != NULL) {
    printf("USE_WRITE2 set; using write(2) for socket comm.\n");
    wr = write2;
  }
  char portc[16];
  if(snprintf(portc, 15, "%hu", port) != 4) {
    fprintf(stderr, "port conversion to string failed\n");
    return -1;
  }

  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* servlist;
  int addrerr;
  if((addrerr = getaddrinfo(host, portc, &hints, &servlist)) != 0) {
    fprintf(stderr, "error getting address info for '%s': %d\n", host,
            addrerr);
    return -1;
  }

  int sfd = -1;
  for(struct addrinfo* addr=servlist; addr != NULL; addr = addr->ai_next) {
    sfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if(sfd == -1) {
      continue;
    }
    if(connect(sfd, addr->ai_addr, addr->ai_addrlen) == -1) {
      close(sfd);
      sfd = -1;
      continue;
    }
    break;
  }
  freeaddrinfo(servlist);
  if(sfd == -1) {
    fprintf(stderr, "could not connect to server '%s'\n", host);
    return -1;
  }
  if(wr(sfd, "IV3D", (size_t)4) == false) {
    fprintf(stderr, "error sending protocol header to server\n");
    close(sfd);
    return -1;
  }
  checkEndianness(sfd);
  return sfd;
}

static void
force_connect() {
  if(remote == -1) {
    if((remote = connect_server())== -1) {
      fprintf(stderr, "Bailing due to error.\n");
      abort();
    }
  }
  assert(remote > 0);
}

void sharedBrickStuff(const size_t LoD, const size_t brickidx, enum NetDataType type) {
    force_connect();
    if(LoD > UINT32_MAX) {
        fprintf(stderr, "LoD is absurd (%zu).  Bug elsewhere.\n", LoD);
        abort();
    }
    if(brickidx > UINT32_MAX) {
        fprintf(stderr, "brick index is absurd (%zu).  Bug elsewhere.\n", brickidx);
        abort();
    }
    wru8(remote, (uint8_t)nds_BRICK);

    //Tell the other side which data type to use
    const uint8_t ntype = (uint8_t)type;
    const uint32_t lod = (uint32_t)LoD;
    const uint32_t bidx = (uint32_t)brickidx;
    wru8(remote, ntype);
    wru32(remote, lod);
    wru32(remote, bidx);
}

//Single bricks
uint8_t*
netds_brick_request_ui8(const size_t lod, const size_t brickidx, size_t* count) {
    sharedBrickStuff(lod, brickidx, N_UINT8);
    
    //wait for answer
    uint8_t* retValue = NULL;
    ru8v(remote, &retValue, count);
    return retValue;
}

uint16_t*
netds_brick_request_ui16(const size_t lod, const size_t brickidx, size_t *count) {
    sharedBrickStuff(lod, brickidx, N_UINT16);

    //wait for answer
    uint16_t* retValue = NULL;
    ru16v(remote, &retValue, count);
    return retValue;
}

uint32_t*
netds_brick_request_ui32(const size_t lod, const size_t brickidx, size_t *count) {
    sharedBrickStuff(lod, brickidx, N_UINT32);

    //wait for answer
    uint32_t* retValue = NULL;
    ru32v(remote, &retValue, count);
    return retValue;
}

//Multiple bricks
uint8_t**
netds_brick_request_ui8v(const size_t brickCount, const size_t* lods, const size_t* bidxs, size_t** dataCounts) {
    if(brickCount == 0)
        return NULL;
    
    //Tell the other side which data type to use
    //wru8(remote, N_UINT8);

    //TODO just a temporary solution, will rewrite it to a single request
    //REMEMBER TO SEND THE NetDataType AGAIN, when switching to a single request!
    uint8_t** retArray = malloc(sizeof(uint8_t*)*brickCount);
    
    if(*dataCounts == NULL) {
        *dataCounts = malloc(sizeof(size_t)*brickCount);
    }
    
    for(size_t i = 0; i < brickCount; i++) {
        retArray[i] = netds_brick_request_ui8(lods[i], bidxs[i], &(*dataCounts)[i]);
    }

    return retArray;
}

uint16_t**
netds_brick_request_ui16v(const size_t brickCount, const size_t* lods, const size_t* bidxs, size_t** dataCounts) {
    if(brickCount == 0)
        return NULL;
    
    //Tell the other side which data type to use
    //wru8(remote, N_UINT16);
    
    //TODO just a temporary solution, will rewrite it to a single request
    //REMEMBER TO SEND THE NetDataType AGAIN, when switching to a single request!
    uint16_t** retArray = malloc(sizeof(uint16_t*)*brickCount);
    
    if(*dataCounts == NULL) {
        *dataCounts = malloc(sizeof(size_t)*brickCount);
    }
    
    for(size_t i = 0; i < brickCount; i++) {
        retArray[i] = netds_brick_request_ui16(lods[i], bidxs[i], &(*dataCounts)[i]);
    }

    return retArray;
}

uint32_t**
netds_brick_request_ui32v(const size_t brickCount, const size_t* lods, const size_t* bidxs, size_t** dataCounts) {
    if(brickCount == 0)
        return NULL;

    //Tell the other side which data type to use
    //wru8(remote, N_UINT32);
    
    //TODO just a temporary solution, will rewrite it to a single request
    //REMEMBER TO SEND THE NetDataType AGAIN, when switching to a single request!
    uint32_t** retArray = malloc(sizeof(uint32_t*)*brickCount);
    
    if(*dataCounts == NULL) {
        *dataCounts = malloc(sizeof(size_t)*brickCount);
    }
    
    for(size_t i = 0; i < brickCount; i++) {
        retArray[i] = netds_brick_request_ui32(lods[i], bidxs[i], &(*dataCounts)[i]);
    }

    return retArray;
}

void sharedBatchReadStuff(struct BatchInfo* out_info) {
    size_t batchSize;
    rsizet(remote, &batchSize);
    out_info->batchSize = batchSize;

    uint8_t moreNet;
    ru8(remote, &moreNet);
    out_info->moreDataComing = (moreNet == 1);

    if(batchSize <= 0)
        return;

    out_info->lods          = malloc(sizeof(size_t) * batchSize);
    out_info->idxs          = malloc(sizeof(size_t) * batchSize);
    out_info->brickSizes    = malloc(sizeof(size_t) * batchSize);

    rsizetv_d(remote, &out_info->lods[0],       batchSize);
    rsizetv_d(remote, &out_info->idxs[0],       batchSize);
    rsizetv_d(remote, &out_info->brickSizes[0], batchSize);
}

uint8_t**  netds_readBrickBatch_ui8(struct BatchInfo* out_info) {
    sharedBatchReadStuff(out_info);

    if(out_info->batchSize <= 0)
        return NULL;

    uint8_t** retArray = malloc(sizeof(uint8_t*) * out_info->batchSize);
    for(size_t i = 0; i < out_info->batchSize; i++) {
        retArray[i] = malloc(sizeof(uint8_t) * out_info->brickSizes[i]);
        ru8v_d(remote, retArray[i], out_info->brickSizes[i]);
    }
    return retArray;
}
uint16_t** netds_readBrickBatch_ui16(struct BatchInfo* out_info) {
    sharedBatchReadStuff(out_info);

    if(out_info->batchSize <= 0)
        return NULL;

    uint16_t** retArray = malloc(sizeof(uint16_t*) * out_info->batchSize);
    for(size_t i = 0; i < out_info->batchSize; i++) {
        retArray[i] = malloc(sizeof(uint16_t) * out_info->brickSizes[i]);
        ru16v_d(remote, retArray[i], out_info->brickSizes[i]);
    }
    return retArray;
}
uint32_t** netds_readBrickBatch_ui32(struct BatchInfo* out_info) {
    sharedBatchReadStuff(out_info);

    if(out_info->batchSize <= 0)
        return NULL;

    uint32_t** retArray = malloc(sizeof(uint32_t*) * out_info->batchSize);
    for(size_t i = 0; i < out_info->batchSize; i++) {
        retArray[i] = malloc(sizeof(uint32_t) * out_info->brickSizes[i]);
        ru32v_d(remote, retArray[i], out_info->brickSizes[i]);
    }
    return retArray;
}

void
netds_close(const char* filename)
{
  if(remote == -1) { return; }
  const size_t len = strlen(filename);
  if(len == 0) {
    fprintf(stderr, "no filename, ignoring (not sending) close notification\n");
    close(remote);
    return;
  }
  if(len > UINT16_MAX) {
    fprintf(stderr, "error, ridiculously long (%zu-byte) filename\n", len);
    abort();
  }
  wru8(remote, (uint8_t)nds_CLOSE);
  wrCStr(remote, filename);
}
    
void netds_shutdown() {
    force_connect();
    wru8(remote, nds_SHUTDOWN);
}

void netds_rotation(const float m[16], enum NetDataType type) {
    force_connect();
    /* we might want to start thinking about cork/uncorking our sends .. */
    wru8(remote, (uint8_t)nds_ROTATION);
    wrf32v(remote, m, 16);
    wru8(remote, (uint8_t)type);
}
    
char** netds_list_files(size_t* count)
{
    force_connect();
    wru8(remote, nds_LIST_FILES);
    uint16_t tmp_count;
    ru16(remote, &tmp_count);
    *count = tmp_count;
    
    char** retValue = malloc(sizeof(char*) * tmp_count);
    for (size_t i = 0; i < *count; i++) {
        rCStr(remote, &retValue[i], NULL);
    }
    return retValue;
}

void netds_setBatchSize(size_t maxBatchSize) {
    wru8(remote, (uint8_t)nds_BATCHSIZE);
    wrsizet(remote, maxBatchSize);
}

/*
void netds_cancelBatches() {
    wru8(remote, (uint8_t)nds_CANCEL_BATCHES);

    //still have to flush but don't have a proper type... would have to template it
}*/

#ifdef __cplusplus
void netds_open(const char* filename, DSMetaData* out_meta)
{
    force_connect();
    const size_t len = strlen(filename)+1;
    if(len > UINT16_MAX) {
        fprintf(stderr, "error, ridiculously long (%zu-byte) filename\n", len);
        abort();
    }
    if(len == 0) {
        fprintf(stderr, "open of blank filename?  ignoring.\n");
        return;
    }
    wru8(remote, (uint8_t)nds_OPEN);
    wru16(remote, (uint16_t)len);
    wr(remote, filename, len);

    //Read meta-data from server
    rsizet(remote, &out_meta->lodCount);

    size_t layoutsCount;
    ru32v(remote, &out_meta->layouts, &layoutsCount);
    assert(layoutsCount == out_meta->lodCount*3);

    size_t brickCount;
    rsizet(remote, &brickCount);
    out_meta->brickCount = brickCount;
    out_meta->brickKeys = new tuvok::BrickKey[brickCount];
    out_meta->brickMDs = new tuvok::BrickMD[brickCount];

    //Retrieve key-data
    size_t lods[brickCount];
    size_t idxs[brickCount];
    rsizetv_d(remote, &lods[0], brickCount);
    rsizetv_d(remote, &idxs[0], brickCount);

    //Retrieve BrickMDs
    float md_centers[brickCount * 3];
    float md_extents[brickCount * 3];
    uint32_t md_n_voxels[brickCount * 3];
    rf32v_d(remote, &md_centers[0], brickCount * 3);
    rf32v_d(remote, &md_extents[0], brickCount * 3);
    ru32v_d(remote, &md_n_voxels[0], brickCount * 3);

    //build keys and MDs
    for(size_t i = 0; i < out_meta->brickCount; i++) {
        out_meta->brickKeys[i] = BrickKey(0, lods[i], idxs[i]);

        out_meta->brickMDs[i].center.x = md_centers[i*3 + 0];
        out_meta->brickMDs[i].center.y = md_centers[i*3 + 1];
        out_meta->brickMDs[i].center.z = md_centers[i*3 + 2];

        out_meta->brickMDs[i].extents.x = md_extents[i*3 + 0];
        out_meta->brickMDs[i].extents.y = md_extents[i*3 + 1];
        out_meta->brickMDs[i].extents.z = md_extents[i*3 + 2];

        out_meta->brickMDs[i].n_voxels.x = md_n_voxels[i*3 + 0];
        out_meta->brickMDs[i].n_voxels.y = md_n_voxels[i*3 + 1];
        out_meta->brickMDs[i].n_voxels.z = md_n_voxels[i*3 + 2];
    }

    //Receive brick zero?
}

#endif
