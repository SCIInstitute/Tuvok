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
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DEBUG_BYTES 0

/*#################################*/
/*#######       Write       #######*/
/*#################################*/

/* a msgsend for arbitrary data. */
bool wrmsg(int fd, const void* buf, const size_t len) {
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
  assert(len > 0);
  const char* buf = buffer_;
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

bool wru8(int fd, const uint8_t buf) {
  return wrmsg(fd, &buf, 1);
}

bool wru16(int fd, const uint16_t buf) {
  const uint16_t data = htons(buf);
  return wrmsg(fd, &data, sizeof(uint16_t));
}

bool wru32(int fd, const uint32_t buf) {
  const uint32_t data = htonl(buf);
  return wrmsg(fd, &data, sizeof(uint32_t));
}

/*#################################*/
/*#######       Read        #######*/
/*#################################*/
int readFromSocket(int socket, void *buffer, size_t len) {
#if DEBUG_BYTES
    printf("Waiting on data from client...(len: %d)\n", len);
#endif

    int byteCount = recv(socket, buffer, len, MSG_WAITALL);
    if (byteCount < 0) {
        fprintf(stderr, "Failed to receive message from client!\n");
        close(socket);
        exit(EXIT_FAILURE);
    }

#if DEBUG_BYTES
    printf("Received %d bytes from client.\n", byteCount);
#endif

    return byteCount;
}

bool ru8(int socket, uint8_t* value) {
    return 0 < (readFromSocket(socket, value, 1));
}

bool ru16(int socket, uint16_t* value) {
    bool success = 0 < (readFromSocket(socket, value, sizeof(uint16_t)));
    *value = ntohs(*value);
    return success;
}

bool ru32(int socket, uint32_t* value) {
    bool success = 0 < (readFromSocket(socket, value, sizeof(uint32_t)));
    *value = ntohl(*value);
    return success;
}
