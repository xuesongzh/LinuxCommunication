/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_datastruct.h
*Brief: 一些结构体和全局变量声明

*Data:2020.05.08
*Version: 1.0
=====================================*/

#ifndef __SER_DATASTRUCT_H__
#define __SER_DATASTRUCT_H__

//配置文件
typedef struct
{
    char mName[50];
    char mContent[500];
} ConfItem, *LPConfItem;

//日志相关
typedef struct
{
    int mLogLevel;  //日志级别
    int mFd;        //日志文件描述符
} ser_log_t;

//日志等级划分，数字越低等级越高
typedef enum {
    SER_LOG_STDERR = 0,  //控制台错误
    SER_LOG_EMERG,       //紧急
    SER_LOG_ALERT,       //警戒
    SER_LOG_CRIT,        //严重
    SER_LOG_ERR,         //错误
    SER_LOG_WARN,        //警告
    SER_LOG_NOTICE,      //注意
    SER_LOG_INFO,        //信息
    SER_LOG_DEBUG,       //调试
} SER_LOG_LEVEL;

/*-------------全局变量-------------*/
//设置进程标题相关
extern char** pArgv;          //命令行参数
extern char* pNewEnviron;     //指向新的环境变量内存
extern size_t EnvironLength;  //环境变量的长度
extern size_t ArgvLength;     //命令行参数所需长度
extern int ArgcNumber;        //命令行参数个数

//日志相关
extern pid_t ser_pid;      //进程ID
extern ser_log_t ser_log;  //日志文件描述

//进程相关
extern pid_t ser_parent_pid;   //父进程id
extern int ser_daemonized;     //标记是否为守护进程
extern int ser_process_type;   //进程类型，master或者worker
extern sig_atomic_t ser_reap;  //标记子进程状态变化[一般是子进程发来SIGCHLD信号表示退出]
extern int g_stopEvent;        //0:不退出，1:退出

class SerLogicSocket;  //前置声明
class SerThreadPool;
//网络相关
extern SerLogicSocket g_socket;
extern SerThreadPool g_threadpool;

#endif