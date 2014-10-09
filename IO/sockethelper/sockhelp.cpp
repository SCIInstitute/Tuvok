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
static bool shouldReencodeFloat = shouldReencode;
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
        shouldReencodeFloat = shouldReencode;
    }
    else {
        printf("Both systems have the same endianness (%d), "
               "don't need to reencode data before transfer.\n", ownEndianness);
        shouldReencode = false;
        shouldReencodeFloat = shouldReencode;
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


//Needs to be tested...
uint64_t ntoh64(const uint64_t& input)
{
    uint64_t rval;
    uint8_t *data = (uint8_t *)&rval;

    data[0] = input >> 56;
    data[1] = input >> 48;
    data[2] = input >> 40;
    data[3] = input >> 32;
    data[4] = input >> 24;
    data[5] = input >> 16;
    data[6] = input >> 8;
    data[7] = input >> 0;

    return rval;
}

uint64_t hton64(const uint64_t& input)
{
    return (ntoh64(input));
}

template<typename T> T  netEncode(const T& hostVal);
template<> uint8_t      netEncode(const uint8_t& hostInt)  {
    fprintf(stderr, "Should never be necessary!\n");
    abort();
    return hostInt;
}
template<> uint16_t     netEncode(const uint16_t& hostInt) {return htons(hostInt);}
template<> uint32_t     netEncode(const uint32_t& hostInt) {return htonl(hostInt);}
template<> uint64_t     netEncode(const uint64_t& hostInt) {return hton64(hostInt);}
template<> float        netEncode(const float& hostFloat)  {
    float retVal;
    char *floatToConvert = ( char* ) & hostFloat;
    char *returnFloat = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    size_t byteCount = sizeof(float);
    for(size_t i = 0; i < byteCount; i++) {
        returnFloat[i] = floatToConvert[byteCount-1-i];
    }

    return retVal;
}
template<> double       netEncode(const double& hostDouble){
    double retVal;
    char *floatToConvert = ( char* ) & hostDouble;
    char *returnFloat = ( char* ) & retVal;

    // swap the bytes into a temporary buffer
    size_t byteCount = sizeof(double);
    for(size_t i = 0; i < byteCount; i++) {
        returnFloat[i] = floatToConvert[byteCount-1-i];
    }

    return retVal;
}

template<typename T> void  hostEncode(T& netVal);
template<> void hostEncode(uint8_t&)         {
    fprintf(stderr, "Should never be necessary!\n");
    abort();
}
template<> void hostEncode(uint16_t& netInt) {netInt = ntohs(netInt);}
template<> void hostEncode(uint32_t& netInt) {netInt = ntohl(netInt);}
template<> void hostEncode(uint64_t& netInt) {netInt = ntoh64(netInt);}
template<> void hostEncode(char&)            {
    fprintf(stderr, "Should never be necessary!\n");
    abort();
}
template<> void hostEncode(float&)           {/*should not be necessary*/}
template<> void hostEncode(double&)          {/*should not be necessary*/}

template<typename T>
bool templatedWr_single(int fd, const T buf, bool encode) {
    if(!encode)
        return wr(fd, &buf, sizeof(T));

    const T data = netEncode(buf);
    return wr(fd, &data, sizeof(T));
}

bool wr_single(int fd, const uint8_t buf){
    return templatedWr_single(fd, buf, false);
}
bool wr_single(int fd, const uint16_t buf) {
    return templatedWr_single(fd, buf, shouldReencode);
}
bool wr_single(int fd, const uint32_t buf) {
    return templatedWr_single(fd, buf, shouldReencode);
}
bool wr_single(int fd, const uint64_t buf) {
    return templatedWr_single(fd, buf, shouldReencode);
}
bool wr_single(int fd, const float buf) {
    return templatedWr_single(fd, buf, shouldReencodeFloat);
}
bool wr_single(int fd, const double buf) {
    return templatedWr_single(fd, buf, shouldReencodeFloat);
}

bool wr_single(int fd, const NetDSCommandCode code) {
    return wr_single(fd, (uint8_t)code);
}
bool wr_single(int fd, const NetDataType code) {
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

    if(wr_sizet(fd, len) && wr(fd, cstr, len)) {
        return true;
    }
    return false;
}
bool wr_single(int fd, const string buf) {
    return wrCStr(fd, buf.c_str());
}
bool wr_sizet(int fd, const size_t buf) {
    return wr_single(fd, (uint32_t) buf);
}

template<typename T> bool
templatedWr_multiple(int fd, const T* buf, size_t count, bool announce, bool reencode) {
    if(announce)
        wr_sizet(fd, count);

    if(!reencode)
        return wr(fd, buf, sizeof(T)*count);
    else {
        T netData[count];
        for(size_t i = 0; i < count; i++) {
            netData[i] = netEncode(buf[i]);
        }
        return wr(fd, &netData[0], sizeof(T)*count);
    }
}
bool wr_multiple(int fd, const uint8_t* buf, size_t count, bool announce) {
    return templatedWr_multiple(fd, buf, count, announce, false);
}
bool wr_multiple(int fd, const uint16_t* buf, size_t count, bool announce) {
    return templatedWr_multiple(fd, buf, count, announce, shouldReencode);
}
bool wr_multiple(int fd, const uint32_t* buf, size_t count, bool announce) {
    return templatedWr_multiple(fd, buf, count, announce, shouldReencode);
}
bool wr_multiple(int fd, const uint64_t* buf, size_t count, bool announce) {
    return templatedWr_multiple(fd, buf, count, announce, shouldReencode);
}
bool wr_multiple(int fd, const float* buf, size_t count, bool announce) {
    return templatedWr_multiple(fd, buf, count, announce, shouldReencodeFloat);
}
bool wr_multiple(int fd, const double* buf, size_t count, bool announce) {
    return templatedWr_multiple(fd, buf, count, announce, shouldReencodeFloat);
}
bool wr_mult_sizet(int fd, const size_t* buf, size_t count, bool announce) {
    //if(sizeof(size_t) == 4)
    //    return wr_multiple(fd, &buf[0], count, announce);

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

template<typename T>
bool templatedR_single(int socket, T& value, bool reencode) {
    bool success = 0 < (readFromSocket(socket, &value, sizeof(T)));

    if(reencode)
        hostEncode(value);

    return success;
}
bool r_single(int socket, uint8_t& value) {
    return templatedR_single(socket, value, false);
}
bool r_single(int socket, uint16_t& value) {
    return templatedR_single(socket, value, shouldReencode);
}
bool r_single(int socket, uint32_t& value) {
    return templatedR_single(socket, value, shouldReencode);
}
bool r_single(int socket, uint64_t& value) {
    return templatedR_single(socket, value, shouldReencode);
}
bool r_single(int socket, float& value) {
    return templatedR_single(socket, value, shouldReencodeFloat);
}
bool r_single(int socket, double &value) {
    return templatedR_single(socket, value, shouldReencodeFloat);
}
bool r_single(int socket, NetDSCommandCode& value) {
    uint8_t tmp;
    bool success = r_single(socket, tmp);
    value = (NetDSCommandCode)tmp;
    return success;
}
bool r_single(int socket, NetDataType& value) {
    uint8_t tmp;
    bool success = r_single(socket, tmp);
    value = (NetDataType)tmp;
    return success;
}
bool rCStr(int socket, char **buffer, size_t *countOrNULL) {
    size_t len;
    r_sizet(socket, len);

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
bool r_sizet(int socket, size_t& value) {
    uint32_t tmp_val;
    bool success = r_single(socket, tmp_val);
    value = (size_t)tmp_val;
    return success;
}

//private
template<typename T>
size_t getCountAndAlloc(int socket, vector<T>&  buffer, bool sizeIsPredetermined) {
    size_t count;

    if(sizeIsPredetermined)
        count = buffer.size();
    else {
        r_sizet(socket, count);
        buffer.resize(count);
    }

   // if(count == 0)
     //   abort(); //are you sure, that you want this? o_O

    return count;
}

template<typename T>
bool templatedR_multiple(int socket, vector<T>&  buffer, bool sizeIsPredetermined, bool reencode) {
    size_t count = getCountAndAlloc(socket, buffer, sizeIsPredetermined);
    if(count == 0)
        return true;

    bool success = 0 < (readFromSocket(socket, &buffer[0], sizeof(T)*count));
    if(reencode) {
        for(size_t i = 0; i < count; i++) {
            hostEncode(buffer[i]);
        }
    }
    return success;
}

bool r_multiple(int socket, vector<uint8_t>&  buffer, bool sizeIsPredetermined) {
    return templatedR_multiple(socket, buffer, sizeIsPredetermined, false);
}
bool r_multiple(int socket, vector<uint16_t>&  buffer, bool sizeIsPredetermined) {
    return templatedR_multiple(socket, buffer, sizeIsPredetermined, shouldReencode);
}
bool r_multiple(int socket, vector<uint32_t>&  buffer, bool sizeIsPredetermined) {
    return templatedR_multiple(socket, buffer, sizeIsPredetermined, shouldReencode);
}
bool r_multiple(int socket, vector<uint64_t> &buffer, bool sizeIsPredetermined) {
    return templatedR_multiple(socket, buffer, sizeIsPredetermined, shouldReencode);
}
bool r_multiple(int socket, vector<float>&  buffer, bool sizeIsPredetermined) {
    return templatedR_multiple(socket, buffer, sizeIsPredetermined, shouldReencodeFloat);
}
bool r_multiple(int socket, vector<double>&  buffer, bool sizeIsPredetermined) {
    return templatedR_multiple(socket, buffer, sizeIsPredetermined, shouldReencodeFloat);
}
bool r_multiple(int socket, vector<char>& buffer, bool sizeIsPredetermined) {
    return templatedR_multiple(socket, buffer, sizeIsPredetermined, false);
}
bool r_mult_sizet(int socket, vector<size_t>&  buffer, bool sizeIsPredetermined) {
    //Does not work on osx -_-
    /*if(sizeof(size_t) == 4) {
        return r_multiple(socket, buffer, sizeIsPredetermined);
    }*/

    vector<uint32_t> newBuffer(buffer.size());
    bool success = r_multiple(socket, newBuffer, sizeIsPredetermined);

    if(success) {
        if(!sizeIsPredetermined)
            buffer.resize(newBuffer.size());

        for(size_t i = 0; i < buffer.size(); i++) {
            buffer[i] = (size_t)newBuffer[i];
        }
    }

    return success;
}

}
