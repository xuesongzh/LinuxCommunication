/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_memory.h
*Brief: 内存分配类，封装new

*Data:2020.06.05
*Version: 1.0
=====================================*/

#ifndef __SER_MEMORY_H__
#define __SER_MEMORY_H__

#include <mutex>

#include "ser_macros.h"

class SerMemory {
    DECLEAR_SINGLETON(SerMemory);

 public:
    ~SerMemory();

 public:
    void* MallocMemory(const int& count, bool ifmemset = false);
    void FreeMemory(void* pMemory);
};

#endif