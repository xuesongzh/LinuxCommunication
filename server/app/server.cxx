#include <signal.h>
#include <stdio.h>
#include <stdlib.h>  //exit()
#include <string.h>
#include <unistd.h>  //sleep()

#include "ser_configer.h"
#include "ser_function.h"
#include "ser_log.h"
#include "ser_logic_socket.h"
#include "ser_macros.h"
#include "ser_memory.h"
#include "ser_threadpool.h"

//设置进程标题相关全局变量
char** pArgv = nullptr;
char* pNewEnviron = nullptr;
size_t EnvironLength = 0;
size_t ArgvLength = 0;
int ArgcNumber = 0;

pid_t ser_pid;
pid_t ser_parent_pid;
int ser_daemonized = 0;
int ser_process_type = SER_PROCESS_MASTER;
sig_atomic_t ser_reap = 0;
int g_stopEvent;

//全局对象socket
SerLogicSocket g_socket;
SerThreadPool g_threadpool;

static void FreeSource();

int main(int argc, char** argv) {
    /**************一些变量的初始化和赋值*******************/
    g_stopEvent = 0;
    int exitCode = 0;
    ser_pid = getpid();          //获取进程ID
    ser_parent_pid = getppid();  //父进程ID
    pArgv = const_cast<char**>(argv);
    const char* masterProcessTitle = nullptr;  //主进程标题

    //取得命令行参数内存长度
    for (int i = 0; i < argc; ++i) {  //argv: ./server -d -e -t...
        ArgvLength += strlen(argv[i]) + 1;
    }

    //取得环境变量内存长度
    for (int i = 0; environ[i]; ++i) {
        EnvironLength += strlen(environ[i]) + 1;  //环境变量长度，+1是因为'\0'的存在
    }
    ArgcNumber = argc;

    ser_log.mFd = -1;                       //日志文件标记为失效状态
    ser_process_type = SER_PROCESS_MASTER;  //标记为master进程
    ser_reap = 0;                           //标记子进程状态没有发生变化

    /**************配置文件加载*******************/
    SerConfiger* pConfiger = SerConfiger::GetInstance();
    if (nullptr == pConfiger) {
        SER_LOG_INIT();
        SER_LOG_STDERR(errno, "create configer failed!");
        exitCode = 1;
        goto lblexit;
    }

    if (!pConfiger->Load("server.conf")) {
        SER_LOG_STDERR(errno, "load configer content failed!");
        exitCode = 2;  //找不到文件
        goto lblexit;
    }

    /**************初始化工作*******************/
    //单例类初始化
    if (nullptr == SerMemory::GetInstance()) {
        SER_LOG_STDERR(0, "ser memory init failed!");
        exitCode = 1;
        goto lblexit;
    }

    //日志系统初始化
    SER_LOG_INIT();
    //初始化信号，注册相关信号处理函数
    if (ser_init_signals() != 0) {
        SER_LOG_STDERR(0, "signals init failed!");
        exitCode = 1;
        goto lblexit;
    }

    //初始化socket
    if (!g_socket.Initialize()) {
        SER_LOG_STDERR(0, "socket init failed!");
        exitCode = 1;
        goto lblexit;
    }

    /**************进程相关*******************/
    //将环境变量搬走：为了设置进程标题
    pNewEnviron = new char[EnvironLength];
    memset(pNewEnviron, 0, EnvironLength);
    MoveEnviron(pNewEnviron);

    //创建守护进程
    if (pConfiger->GetIntDefault("Daemon", 0) == 1) {
        int result = ser_daemon();
        if (-1 == result) {
            exitCode = 1;
            goto lblexit;
        }
        if (1 == result) {
            //父进程退出，但不打印
            exitCode = 0;
            FreeSource();
            return exitCode;
        }

        ser_daemonized = 1;  //标记为守护进程
    }

    //如果配置了守护进程，fork()出来的子进程才会走到这里，也是守护进程作为我们的主进程
    //主进程和子进程在里面循环，干活
    ser_master_process_cycle();

lblexit:
    SER_LOG_STDERR(0, "程序退出!\n");
    FreeSource();
    return exitCode;
}

static void FreeSource() {
    //释放新的环境变量所在内存
    DEL_ARRAY(pNewEnviron);

    //关闭日志文件
    if (ser_log.mFd != STDERR_FILENO && ser_log.mFd != -1) {
        close(ser_log.mFd);
        ser_log.mFd = -1;  //标记下，防止再次被close
    }
}