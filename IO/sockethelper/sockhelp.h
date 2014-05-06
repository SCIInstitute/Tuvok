#ifndef SOCKHELP_H
#define SOCKHELP_H

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#ifdef __cplusplus
# define EXPORT extern "C"
#else
# define EXPORT /* no extern "C" needed. */
#endif

//Treat as uint8_t
enum NetDSCommandCode {
    nds_OPEN=0,
    nds_CLOSE,
    nds_BRICK,
    nds_LIST_FILES,
    nds_SHUTDOWN,
};

//For writing
EXPORT bool wrmsg(int fd, const void* buf, const size_t len);
EXPORT bool write2(int fd, const void* buffer_, const size_t len);
EXPORT bool wru8(int fd, const uint8_t buf);
EXPORT bool wru16(int fd, const uint16_t buf);
EXPORT bool wru32(int fd, const uint32_t buf);

typedef bool (msgsend)(int, const void*, const size_t);
static msgsend* wr = wrmsg;

//For reading
EXPORT int readFromSocket(int socket, void *buffer, size_t len);
EXPORT bool ru8(int socket, uint8_t* value);
EXPORT bool ru16(int socket, uint16_t* value);
EXPORT bool ru32(int socket, uint32_t* value);

#endif // SOCKHELP_H
