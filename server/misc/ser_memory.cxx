#include <string.h>

#include "ser_memory.h"
#include "ser_macros.h"

IMPLEMENT_SINGLETON(SerMemory);

SerMemory::~SerMemory()
{

}

//分配内存
//count:字节大小
//ifmemset:是否初始化为0
void* SerMemory::MallocMemory(const int& count, bool ifmemset)
{
    void* pTempData = (void*)new char[count];
    if(ifmemset)
    {
        memset(pTempData, 0, count);
    }

    return pTempData;
}

//内存释放
void SerMemory::FreeMemory(void* pMemory)
{
    delete []((char*)pMemory); //分配的时候使用char*,释放的时候转为char*
    pMemory = nullptr;
}