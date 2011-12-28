#pragma once

#ifndef MD5_H
#define MD5_H

#include <vector>
#include "../StdDefines.h"

class MD5
{
public:
        MD5();
        virtual ~MD5() {};

        //RSA MD5 Implementation
        void Transform(const uint8_t Block[64], int& error);
        void Update(const uint8_t* Input, uint32_t nInputLen, int& error);
        std::vector<uint8_t> Final(int& iErrorCalculate);

protected:
        inline uint32_t RotateLeft(uint32_t x, int n);
        inline void FF( uint32_t& A, uint32_t B, uint32_t C, uint32_t D, uint32_t X, uint32_t S, uint32_t T);
        inline void GG( uint32_t& A, uint32_t B, uint32_t C, uint32_t D, uint32_t X, uint32_t S, uint32_t T);
        inline void HH( uint32_t& A, uint32_t B, uint32_t C, uint32_t D, uint32_t X, uint32_t S, uint32_t T);
        inline void II( uint32_t& A, uint32_t B, uint32_t C, uint32_t D, uint32_t X, uint32_t S, uint32_t T);

        // Helpers
        inline void UINTToByte(uint8_t* Output, const uint32_t* Input, uint32_t nLength, int& error);
        inline void ByteToUINT(uint32_t* Output, const uint8_t* Input, uint32_t nLength, int& error);
        void MemoryMove(uint8_t* from, uint8_t* to, uint32_t size);

private:
        uint8_t  m_lpszBuffer[64];    // InputBuffer
        uint32_t m_nCount[2] ;        // bitcount, modulo 2^64 (lsb first)
        uint32_t m_lMD5[4] ;          // MD5 sum
};
#endif // MD5_H
