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
    nds_ROTATION,
};

//treat as uint8_t
enum NetDataType {
    N_UINT8 =0,
    N_UINT16,
    N_UINT32
};

EXPORT void checkEndianness(int socket);

//For writing
EXPORT bool wrmsg(int fd, const void* buf, const size_t len);
EXPORT bool write2(int fd, const void* buffer_, const size_t len);

EXPORT bool wru8(int fd, const uint8_t buf);
EXPORT bool wru16(int fd, const uint16_t buf);
EXPORT bool wru32(int fd, const uint32_t buf);

EXPORT bool wru8v(int fd, const uint8_t* buf, size_t count);
EXPORT bool wru16v(int fd, const uint16_t* buf, size_t count);
EXPORT bool wru32v(int fd, const uint32_t* buf, size_t count);
EXPORT bool wrf32v(int fd, const float* buf, size_t count);

EXPORT bool wrCStr(int fd, const char* cstr);

typedef bool (msgsend)(int, const void*, const size_t);
static msgsend* wr = wrmsg;

//For reading
EXPORT int readFromSocket(int socket, void *buffer, size_t len);
EXPORT bool ru8(int socket, uint8_t* value);
EXPORT bool ru16(int socket, uint16_t* value);
EXPORT bool ru32(int socket, uint32_t* value);

EXPORT bool ru8v(int socket, uint8_t** buffer, size_t* count); //count only acts as output parameter, size is being read from the stream
EXPORT bool ru16v(int socket, uint16_t** buffer, size_t* count);
EXPORT bool ru32v(int socket, uint32_t** buffer, size_t* count);

EXPORT bool rCStr(int socket, char** buffer, size_t* countOrNULL); //since string lengths can always be recalculated, you can also just pass NULL. count will include \0

#endif // SOCKHELP_H
