#include "sockhelp.h"
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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "order32.h"
#include <limits>

#define DEBUG_BYTES 0

/** if endianness needs to be fixed up between client/server */
static bool shouldReencode = true;
typedef bool (msgsend)(int, const void*, const size_t);

namespace SOCK {
    bool wrmsg(int  fd, const void* buffer, const size_t len);
    bool write2(int fd, const void* buffer, const size_t len);
    int readFromSocket(int socket, void *buffer, size_t len);
    static msgsend* wr = wrmsg;

/* returns file descriptor of connected socket. */
int connect_server(unsigned short port) {
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
    SOCK::checkEndianness(sfd);
    return sfd;
}

void checkEndianness(int socket) {
    //Get own endianness
    uint8_t ownEndianness;
    if(O32_HOST_ORDER == O32_LITTLE_ENDIAN) {
        ownEndianness = 0;
    }
    else if (O32_HOST_ORDER == O32_BIG_ENDIAN) {
        ownEndianness = 1;
    }
    else if (O32_HOST_ORDER == O32_PDP_ENDIAN) {
        ownEndianness = 2;
    }
    else {
        printf("What kind of system is this? cant determine byte order");
        ownEndianness = 3;
    }

    wr_single(socket, ownEndianness);

    uint8_t otherEndianness;
    r_single(socket, otherEndianness);

    if(ownEndianness != otherEndianness
            || ownEndianness == 3
            || otherEndianness == 3) {
        printf("Different byte order between systems, have to reencode data!\n");
        shouldReencode = true;
    }
    else {
        printf("Both systems have the same endianness (%d), "
               "don't need to reencode data before transfer.\n", ownEndianness);
        shouldReencode = false;
    }
}

enum NetDataType netTypeForPlainT(struct PlainTypeInfo info) {
    return netTypeForBitWidth(info.bitwidth, info.is_signed, info.is_float);
}

enum NetDataType netTypeForBitWidth(size_t width, bool is_signed, bool is_float) {
    if(is_float && width == 32)
        return N_FL32;
    else if(!is_float && !is_signed) {
        if(width == 8)
            return N_UINT8;
        else if(width == 16)
            return N_UINT16;
        else if(width == 32)
            return N_UINT32;
    }

    return N_NOT_SUPPORTED;
}

struct PlainTypeInfo bitWidthFromNType(enum NetDataType type) {
    struct PlainTypeInfo retVal;
    if(type == N_FL32) {
        retVal.bitwidth = 32;
        retVal.is_float = true;
        retVal.is_signed = true;
        return retVal;
    }

    retVal.is_float     = false;
    retVal.is_signed    = false;

    if(type == N_UINT8)
        retVal.bitwidth = 8;
    else if(type == N_UINT16)
        retVal.bitwidth = 16;
    else if(type == N_UINT32)
        retVal.bitwidth = 32;
    else
        retVal.bitwidth = 0; // Not supported

    return retVal;
}

/*#################################*/
/*#######       Write       #######*/
/*#################################*/

/* a msgsend for arbitrary data. */
bool wrmsg(int fd, const void* buf, const size_t len) {
  if(fd == -1) { return false; }
  assert(len > 0);
  struct iovec vec[1];
  vec[0].iov_base = (void*)buf;
  vec[0].iov_len = len;
  struct msghdr msg;
  memset(&msg, 0, sizeof(struct msghdr));
  msg.msg_iov = vec;
  msg.msg_iovlen = 1;

#ifdef __APPLE__
  if(sendmsg(fd, &msg, 0) != (ssize_t)len) {
#else //Only available on linux systems
  if(sendmsg(fd, &msg, MSG_NOSIGNAL) != (ssize_t)len) {
#endif

    fprintf(stderr, "error writing %zu-byte buffer at %p: %d\n", len, buf,
            errno);
    return false;
  }
  return true;
}
/* same as write(2), but never reports partial writes. */
bool write2(int fd, const void* buffer_, const size_t len) {
  if(fd == -1) { return false; }
  assert(len > 0);
  const char* buf = (const char*)buffer_;
  struct pollfd pll;

  pll.fd = fd;
  pll.events = POLLOUT;
  size_t n = 0;
  while(len > n) {
    const ssize_t bytes = write(fd, buf+n, len-n);
    switch(bytes) {
      case -1:
        if(errno == EAGAIN) {
          continue;
        } else if(errno == EWOULDBLOCK) {
          poll(&pll, 1, -1); /* wait until we can send data. */
          continue;
        }
        return false;
      case 0:
        return false;
      default:
        n += bytes;
    }
  }
  return true;
}


bool wr_single(int fd, const uint8_t buf){
  return wr(fd, &buf, sizeof(uint8_t));
}
bool wr_single(int fd, const uint16_t buf) {
    if(!shouldReencode)
        return wr(fd, &buf, sizeof(uint16_t));

    const uint16_t data = htons(buf);
    return wr(fd, &data, sizeof(uint16_t));
}
bool wr_single(int fd, const uint32_t buf) {
    if(!shouldReencode)
        return wr(fd, &buf, sizeof(uint32_t));

    const uint32_t data = htonl(buf);
    return wr(fd, &data, sizeof(uint32_t));
}
bool wr_single(int fd, const size_t buf) {
    return wr_single(fd, (uint32_t) buf);
}
bool wr_single(int fd, const NetDSCommandCode code) {
    return wr_single(fd, (uint8_t)code);
}
bool wrCStr(int fd, const char *cstr) {
    size_t len = strlen(cstr);
    if(len == 0) {
        fprintf(stderr, "no filename, ignoring (not sending) close notification\n");
        abort();
    }
    if(len > std::numeric_limits<uint16_t>::max()) {
        fprintf(stderr, "error, ridiculously long (%zu-byte) filename\n", len);
        abort();
    }

    len++; //For nulltermination

    if(wr_single(fd, len) && wr(fd, cstr, len)) {
        return true;
    }
    return false;
}
bool wr_single(int fd, const string buf) {
    return wrCStr(fd, buf.c_str());
}


bool wr_multiple(int fd, const uint8_t* buf, size_t count, bool announce) {
    if(announce)
        wr_single(fd, count);

    return wr(fd, buf, sizeof(uint8_t)*count);
}
bool wr_multiple(int fd, const uint16_t* buf, size_t count, bool announce) {
    if(announce)
        wr_single(fd, count);

    if(!shouldReencode) {
        return wr(fd, buf, sizeof(uint16_t)*count);
    }
    else {
        uint16_t netData[count];
        for(size_t i = 0; i < count; i++) {
            netData[i] = htons(buf[i]);
        }
        bool retValue = wr(fd, &netData[0], sizeof(uint16_t)*count);
        return retValue;
    }
}
bool wr_multiple(int fd, const uint32_t* buf, size_t count, bool announce) {
    if(announce)
        wr_single(fd, count);

    if(!shouldReencode) {
        return wr(fd, buf, sizeof(uint32_t)*count);
    }
    else {
        uint32_t netData[count];
        for(size_t i = 0; i < count; i++) {
            netData[i] = htonl(buf[i]);
        }
        bool retValue = wr(fd, &netData[0], sizeof(uint32_t)*count);
        return retValue;
    }
}
bool wr_multiple(int fd, const float* buf, size_t count, bool announce) {
    if(announce)
        wr_single(fd, count);

    if(!shouldReencode) {
        return wr(fd, buf, sizeof(float)*count);
    }

    float* swapped = new float[count];
    for(float* b = swapped; b < swapped+count; ++b) {
        *b = htonl(*b);
    }
    const bool rv = wr(fd, swapped, sizeof(float)*count);
    free(swapped);
    return rv;
}
bool wr_multiple(int fd, const size_t* buf, size_t count, bool announce) {
    uint32_t newBuffer[count];
    for(size_t i = 0; i < count; i++) {
        newBuffer[i] = (uint32_t)buf[i];
    }
    return wr_multiple(fd, &newBuffer[0], count, announce);
}


/*#################################*/
/*#######       Read        #######*/
/*#################################*/
bool newDataOnSocket(int socket) {
    fd_set rfds;
    struct timeval tv;
    int retval;

    /* Watch socket to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(socket, &rfds);

    /* Don't wait... */
    tv.tv_sec   = 0;
    tv.tv_usec  = 0;

    retval = select(socket+1, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */

    if (retval == -1) {
        perror("select()");
        return false;
    }
    else if (retval)
        return true;
    else
        return false;
}

int readFromSocket(int socket, void *buffer, size_t len) {
#if DEBUG_BYTES
    printf("Waiting on data from client...(len: %d)\n", len);
#endif

    int byteCount = (int)recv(socket, buffer, len, MSG_WAITALL);
    if (byteCount < 0) {
        fprintf(stderr, "Failed to receive message! (errno %d)\n", errno);
        close(socket);
        exit(EXIT_FAILURE);
    }

#if DEBUG_BYTES
    printf("Received %d bytes from client.\n", byteCount);
#endif

    return byteCount;
}

bool r_single(int socket, uint8_t& value) {
    return 0 < (readFromSocket(socket, &value, sizeof(uint8_t)));
}
bool r_single(int socket, uint16_t& value) {
    bool success = 0 < (readFromSocket(socket, &value, sizeof(uint16_t)));

    if (shouldReencode)
        value = ntohs(value);

    return success;
}
bool r_single(int socket, uint32_t& value) {
    bool success = 0 < (readFromSocket(socket, &value, sizeof(uint32_t)));

    if (shouldReencode)
        value = ntohl(value);

    return success;
}
bool r_single(int socket, float& value) {
    bool success = 0 < (readFromSocket(socket, &value, sizeof(float)));

    if (shouldReencode)
        value = ntohl(value);

    return success;
}
bool r_single(int socket, size_t& value) {
    uint32_t tmp_val;
    bool success = r_single(socket, tmp_val);
    value = (size_t)tmp_val;
    return success;
}
bool r_single(int socket, NetDSCommandCode& value) {
    uint8_t tmp;
    bool success = r_single(socket, tmp);
    value = (NetDSCommandCode)tmp;
    return success;
}
bool rCStr(int socket, char **buffer, size_t *countOrNULL) {
    size_t len;
    r_single(socket, len);

    if (countOrNULL != NULL)
        *countOrNULL = len;

    *buffer = new char[len];
    return readFromSocket(socket, *buffer, len);
}
bool r_single(int socket, string& value) {
    char* tmp;
    bool success = rCStr(socket, &tmp, NULL);
    if(!success) {
        delete tmp;
        fprintf(stderr, "Could not read string from stream!\n");
        return false;
    }
    value = tmp;
    delete tmp;
    return success;
}

//private
template<typename T>
size_t getCountAndAlloc(int socket, vector<T>&  buffer, bool sizeIsPredetermined) {
    size_t count;

    if(sizeIsPredetermined)
        count = buffer.size();
    else {
        r_single(socket, count);
        buffer.resize(count);
    }

   // if(count == 0)
     //   abort(); //are you sure, that you want this? o_O

    return count;
}

bool r_multiple(int socket, vector<uint8_t>&  buffer, bool sizeIsPredetermined) {
    size_t count = getCountAndAlloc(socket, buffer, sizeIsPredetermined);
    if(count == 0)
        return true;

    return 0 < (readFromSocket(socket, &buffer[0], sizeof(uint8_t)*count));
}
bool r_multiple(int socket, vector<uint16_t>&  buffer, bool sizeIsPredetermined) {
    size_t count = getCountAndAlloc(socket, buffer, sizeIsPredetermined);
    if(count == 0)
        return true;

    bool success = 0 < (readFromSocket(socket, &buffer[0], sizeof(uint16_t)*count));
    if (shouldReencode) {
        for(size_t i = 0; i < count; i++) {
            buffer[i] = ntohs(buffer[i]);
        }
    }
    return success;
}
bool r_multiple(int socket, vector<uint32_t>&  buffer, bool sizeIsPredetermined) {
    size_t count = getCountAndAlloc(socket, buffer, sizeIsPredetermined);
    if(count == 0)
        return true;

    bool success = 0 < (readFromSocket(socket, &buffer[0], sizeof(uint32_t)*count));
    if (shouldReencode) {
        for(size_t i = 0; i < count; i++) {
            buffer[i] = ntohl(buffer[i]);
        }
    }
    return success;
}
bool r_multiple(int socket, vector<float>&  buffer, bool sizeIsPredetermined) {
    size_t count = getCountAndAlloc(socket, buffer, sizeIsPredetermined);
    if(count == 0)
        return true;

    bool success = 0 < (readFromSocket(socket, &buffer[0], sizeof(float)*count));
    if (shouldReencode) {
        for(size_t i = 0; i < count; i++) {
            buffer[i] = ntohl(buffer[i]);
        }
    }
    return success;
}
bool r_multiple(int socket, vector<size_t>&  buffer, bool sizeIsPredetermined) {
    size_t count = getCountAndAlloc(socket, buffer, sizeIsPredetermined);
    if(count == 0)
        return true;

    vector<uint32_t> newBuffer(count);
    bool success = r_multiple(socket, newBuffer, sizeIsPredetermined);

    if(success) {
        for(size_t i = 0; i < count; i++) {
            buffer[i] = (size_t)newBuffer[i];
        }
    }

    return success;
}
bool r_multiple(int socket, vector<char>& buffer, bool sizeIsPredetermined) {
    size_t count = getCountAndAlloc(socket, buffer, sizeIsPredetermined);
    if(count == 0)
        return true;

    return 0 < (readFromSocket(socket, &buffer[0], sizeof(uint8_t)*count));
}

}
