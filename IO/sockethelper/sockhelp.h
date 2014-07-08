#ifndef SOCKHELP_H
#define SOCKHELP_H

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <vector>
#include <string>
using std::vector;
using std::string;

namespace SOCK {

    //Treat as uint8_t
    enum NetDSCommandCode {
        nds_OPEN=0,
        nds_CLOSE,
        nds_BRICK,
        nds_LIST_FILES,
        nds_SHUTDOWN,
        nds_ROTATION,
        nds_BATCHSIZE,
        nds_CANCEL_BATCHES,
        nds_CALC_MINMAX
    };

    //treat as uint8_t
    enum NetDataType {
        N_UINT8 = 0,
        N_UINT16,
        N_UINT32,
        N_FL32,
        N_NOT_SUPPORTED
    };

    struct PlainTypeInfo {
        size_t bitwidth;
        bool is_signed;
        bool is_float;
    };

    int connect_server(unsigned short port);

    void checkEndianness(int socket);
    enum NetDataType netTypeForPlainT(struct PlainTypeInfo info);
    enum NetDataType netTypeForBitWidth(size_t width, bool is_signed, bool is_float);
    PlainTypeInfo bitWidthFromNType(enum NetDataType type);

    //For writing
    bool wr_single(int fd, const uint8_t buf);
    bool wr_single(int fd, const uint16_t buf);
    bool wr_single(int fd, const uint32_t buf);
    bool wr_single(int fd, const size_t buf);
    bool wr_single(int fd, const NetDSCommandCode code);
    bool wr_single(int fd, const std::string buf);

    bool wr_multiple(int fd, const uint8_t* buf, size_t count, bool announce);
    bool wr_multiple(int fd, const uint16_t* buf, size_t count, bool announce);
    bool wr_multiple(int fd, const uint32_t* buf, size_t count, bool announce);
    bool wr_multiple(int fd, const float* buf, size_t count, bool announce);
    bool wr_multiple(int fd, const double* buf, size_t count, bool announce);
    bool wr_multiple(int fd, const size_t* buf, size_t count, bool announce);

 //   bool wrCStr(int fd, const char* cstr);

    //For reading
    bool newDataOnSocket(int socket);

    bool r_single(int socket, uint8_t& value);
    bool r_single(int socket, uint16_t& value);
    bool r_single(int socket, uint32_t& value);
    bool r_single(int socket, float& value);
    bool r_single(int socket, size_t& value);
    bool r_single(int socket, NetDSCommandCode& value);
    bool r_single(int socket, string& value);

    //If sizeIsPredetermined == true, reads as many elements from the socket as the buffers size.
    //if sizeIsPredetermined == false, reads the size from stream and initializes based on that
    bool r_multiple(int socket, vector<uint8_t>&  buffer, bool sizeIsPredetermined);
    bool r_multiple(int socket, vector<uint16_t>&  buffer, bool sizeIsPredetermined);
    bool r_multiple(int socket, vector<uint32_t>&  buffer, bool sizeIsPredetermined);
    bool r_multiple(int socket, vector<float>&  buffer, bool sizeIsPredetermined);
    bool r_multiple(int socket, vector<double>&  buffer, bool sizeIsPredetermined);
    bool r_multiple(int socket, vector<size_t>&  buffer, bool sizeIsPredetermined);
    bool r_multiple(int socket, vector<char>& buffer, bool sizeIsPredetermined);

   // bool rCStr(int socket, char** buffer, size_t* countOrNULL); //since string lengths can always be recalculated, you can also just pass NULL. count will include \0
}

#endif // SOCKHELP_H
