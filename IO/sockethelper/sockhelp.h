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
    nds_BATCHSIZE,
    nds_CANCEL_BATCHES
};

//treat as uint8_t
enum NetDataType {
    N_UINT8 = 0,
    N_UINT16,
    N_UINT32
};

EXPORT void checkEndianness(int socket);

//For writing
EXPORT bool wrmsg(int  fd, const void* buffer, const size_t len);
EXPORT bool write2(int fd, const void* buffer, const size_t len);

EXPORT bool wru8(int  fd, const uint8_t  buf);
EXPORT bool wru16(int fd, const uint16_t buf);
EXPORT bool wru32(int fd, const uint32_t buf);
EXPORT bool wrsizet(int fd, const size_t buf);

//tells the other side also how many values will be send.
EXPORT bool wru8v(int    fd, const uint8_t*  buf, size_t count);
EXPORT bool wru16v(int   fd, const uint16_t* buf, size_t count);
EXPORT bool wru32v(int   fd, const uint32_t* buf, size_t count);
EXPORT bool wrf32v(int   fd, const float*    buf, size_t count);
EXPORT bool wrsizetv(int fd, const size_t*   buf, size_t count);

//Does NOT tell the other side how many values will be send.
//Usefull, if amount already known
EXPORT bool wru8v_d(int    fd, const uint8_t*  buf, size_t count);
EXPORT bool wru16v_d(int   fd, const uint16_t* buf, size_t count);
EXPORT bool wru32v_d(int   fd, const uint32_t* buf, size_t count);
EXPORT bool wrf32v_d(int   fd, const float*    buf, size_t count);
EXPORT bool wrsizetv_d(int fd, const size_t*   buf, size_t count);

EXPORT bool wrCStr(int fd, const char* cstr);

typedef bool (msgsend)(int, const void*, const size_t);
static msgsend* wr = wrmsg;

//For reading
EXPORT int readFromSocket(int socket, void *buffer, size_t len);
EXPORT bool ru8(int socket, uint8_t* value);
EXPORT bool ru16(int socket, uint16_t* value);
EXPORT bool ru32(int socket, uint32_t* value);
EXPORT bool rf32(int socket, float* value);
EXPORT bool rsizet(int socket, size_t* value);

//the amount of values to be read is also read from the stream.
//for defining the amount of values to read yourself, use the r???v_d functions
//buffer is being newly allocated based on the read size_t
EXPORT bool ru8v(int    socket, uint8_t**  buffer, size_t* out_count);
EXPORT bool ru16v(int   socket, uint16_t** buffer, size_t* out_count);
EXPORT bool ru32v(int   socket, uint32_t** buffer, size_t* out_count);
EXPORT bool rf32v(int   socket, float**    buffer, size_t* out_count);
EXPORT bool rsizetv(int socket, size_t**   buffer, size_t* out_count);

//reads defined amount of values from the stream
//buffer must already be initialized
EXPORT bool ru8v_d(int    socket, uint8_t*  buffer, size_t count);
EXPORT bool ru16v_d(int   socket, uint16_t* buffer, size_t count);
EXPORT bool ru32v_d(int   socket, uint32_t* buffer, size_t count);
EXPORT bool rf32v_d(int   socket, float*    buffer, size_t count);
EXPORT bool rsizetv_d(int socket, size_t*   buffer, size_t count);

EXPORT bool rCStr(int socket, char** buffer, size_t* countOrNULL); //since string lengths can always be recalculated, you can also just pass NULL. count will include \0

#ifdef __cplusplus
namespace {
template<typename T> bool wr_single(int  fd, const T buf);
template<> bool wr_single(int fd, const uint8_t buf) {
    return wru8(fd, buf);
}
template<> bool wr_single(int fd, const uint16_t buf) {
    return wru16(fd, buf);
}
template<> bool wr_single(int fd, const uint32_t buf) {
    return wru32(fd, buf);
}
template<> bool wr_single(int fd, const size_t buf) {
    return wrsizet(fd, buf);
}

template<typename T> bool wr_multiple(int fd, const T*  buf, size_t count, bool announce);
template<> bool wr_multiple(int fd, const uint8_t* buf, size_t count, bool announce) {
    if(announce)    return wru8v(fd, buf, count);
    else            return wru8v_d(fd, buf, count);
}
template<> bool wr_multiple(int fd, const uint16_t* buf, size_t count, bool announce) {
    if(announce)    return wru16v(fd, buf, count);
    else            return wru16v_d(fd, buf, count);
}
template<> bool wr_multiple(int fd, const uint32_t* buf, size_t count, bool announce) {
    if(announce)    return wru32v(fd, buf, count);
    else            return wru32v_d(fd, buf, count);
}
template<> bool wr_multiple(int fd, const float* buf, size_t count, bool announce) {
    if(announce)    return wrf32v(fd, buf, count);
    else            return wrf32v_d(fd, buf, count);
}
}
#endif

#endif // SOCKHELP_H
