/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_crc32.h
*Brief: crc32数据校验，防止数据包被篡改

*Data:2020.06.13
*Version: 1.0
=====================================*/

#ifndef __SER_CRC32_H__
#define __SER_CRC32_H__

#include <mutex>
#include "ser_macros.h"

class SerCRC32
{
DECLEAR_SINGLETON(SerCRC32);

public:
    ~SerCRC32();

public:
    int Get_CRC(unsigned char* buffer, unsigned int dwSize);

private:
    void Init_CRC32_Table();
    unsigned int Reflect(unsigned int ref, char ch);

    unsigned int crc32_table[256];
};

#endif