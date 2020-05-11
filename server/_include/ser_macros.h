/*===================================
*Copyright(c): huan.liu, 2019
*All rights reversed

*File name: ser_macros.h
*Brief: 宏定义

*Data:2020.05.07
*Version: 1.0
=====================================*/

#ifndef __SER_MACROS_H__
#define __SER_MACROS_H__

#include <mutex>

#pragma region[内存释放]

// 删除指针
#define DEL_PTR(ptr)\
do\
{\
    if (nullptr != ptr)\
    {\
        delete (ptr);\
        ptr = nullptr;\
    }\
} while (false)

//删除指针数组
#define DEL_ARRAY(ptr)\
do\
{\
    if (nullptr != ptr)\
    {\
        delete[] ptr;\
        ptr = nullptr;\
    }\
} while (false)

#pragma endregion

#pragma region[单例类声明与实现]

//单例类声明
#define DECLEAR_SINGLETON(className)\
private:\
className();\
public:\
static className* GetInstance();\
\
private:\
static className* mInstance;\
static std::mutex mMutex;\
\
class GarCollector\
{\
    public:\
    ~GarCollector()\
    {\
        DEL_PTR(className::mInstance);\
    }\
};

//单例类实现
//双重判断可提高效率
//最好是能在主线程中把单例类对象构建出来，这样在第一次new的时候就不会出现多线程多次new的问题
//GarCollector 用于内存释放
#define IMPLEMENT_SINGLETON(className)\
className* className::mInstance = nullptr;\
std::mutex className::mMutex;\
\
className::className() {}\
\
className* className::GetInstance()\
{\
    if (nullptr == mInstance)\
    {\
        std::unique_lock<std::mutex> locker(mMutex);\
        if (nullptr == mInstance)\
        {\
            mInstance = new className();\
            static GarCollector collector;\
        }\
    }\
\
    return mInstance;\
}

#pragma endregion

#pragma region[其他]

//适合字符拷贝，返回指向dst+n的指针
#define SER_MEMCPY(dst, src, n)\
do\
{\
   dst = (uint8_t*)memcpy(dst, src, n) + n;\
}while(false)


#pragma endregion

#endif