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

#pragma region[日志相关]
//日志打印
//日志初始化
#define SER_LOG_INIT()\
do\
{\
    ser_log_init();\
}while(false)

//标准错误输出到屏幕
#define SER_LOG_STDERR(errNum, pfmt, ...)\
do\
{\
    ser_log_stderr(errNum, pfmt, ##__VA_ARGS__);\
}while(false)

//将日志输出到日志文件
#define SER_LOG(logLevel, errNum, pfmt, ...)\
do\
{\
    ser_log_error_core(logLevel, errNum, pfmt, ##__VA_ARGS__);\
}while(false)

//日志信息的最大长度
#define SER_MAX_ERROR_STR 2048

#define SER_LOG_DEFAULT_PATH "logs/error.log"

#pragma endregion

#pragma region[其它]

//适合字符拷贝，返回指向dst+n的指针
#define SER_MEMCPY(dst, src, n)\
do\
{\
   dst = (uint8_t*)memcpy(dst, src, n) + n;\
}while(false)

//64位有符号数需要的最大字符串长度
#define SER_INT64_STR_LEN_MAXA (sizeof("-9223372036854775808") - 1)

//32位无符号最大数字
#define SER_MAX_UINT32_VALUE (uint32_t) 0xffffffff

#pragma endregion

#pragma region[进程相关]

#define SER_PROCESS_MASTER 0 //管理进程
#define SER_PROCESS_WORKER 1 //工作进程

#pragma endregion

#endif