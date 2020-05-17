#include <signal.h> //siginfo_t
#include <stdio.h>
#include <string.h> //memset
#include <unistd.h>
#include <errno.h>

#include "ser_function.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_datastruct.h"

//定义相关数据结构
typedef struct
{
    int mSigno; //信号对应的编号
    const char* mSigName; //信号对应的名字
    void (*handler)(int signo, siginfo_t* pSiginfo, void* pUconText);  //函数指针，siginfo_t为系统定义的结构
}ser_signal_t;

//信号处理函数
static void ser_signal_handler(int signo, siginfo_t* pSiginfo, void* pUconText);

ser_signal_t signals[] = 
{
      //mSigno    //mSigName         //handler
    { SIGHUP,    "SIGHUP",           ser_signal_handler},         //终端断开信号，对于守护进程常用于reload重载配置文件通知--标识1
    { SIGINT,    "SIGINT",           ser_signal_handler },        //标识2   
	{ SIGTERM,   "SIGTERM",          ser_signal_handler },        //标识15
    { SIGCHLD,   "SIGCHLD",          ser_signal_handler },        //子进程退出时，父进程会收到这个信号--标识17
    { SIGQUIT,   "SIGQUIT",          ser_signal_handler },        //标识3
    { SIGIO,     "SIGIO",            ser_signal_handler },        //指示一个异步I/O事件【通用异步I/O信号】
    { SIGSYS,    "SIGSYS, SIG_IGN",  NULL               },        //我们想忽略这个信号，SIGSYS表示收到了一个无效系统调用，如果我们不忽略，进程会被操作系统杀死，--标识31
                                                                  //所以我们把handler设置为NULL，代表 我要求忽略这个信号，请求操作系统不要执行缺省的该信号处理动作（杀掉我）
    //...暂时先处理这么多，日后根据需要再继续增加
    { 0,         NULL,               NULL               }         //结束标记
};

int ser_init_signals()
{
    ser_signal_t* pSig;
    struct sigaction sa; //sigaction,系统定义

    for(pSig = signals; pSig->mSigno != 0; ++pSig)
    {
        memset(&sa, 0, sizeof(struct sigaction));

        if(pSig->handler)
        {
            sa.sa_sigaction = pSig->handler;
            sa.sa_flags = SA_SIGINFO; //附带参数可以传入信号处理函数，使其生效
        }
        else
        {
            //其实sa_handler和sa_sigaction都是一个函数指针用来表示信号处理程序。只不过这两个函数指针他们参数不一样， sa_sigaction带的参数多，信息量大，
            //而sa_handler带的参数少，信息量少；如果你想用sa_sigaction，那么你就需要把sa_flags设置为SA_SIGINFO；      
            sa.sa_handler = SIG_IGN; //忽略此信号
        }

        //比如咱们处理某个信号比如SIGUSR1信号时不希望收到SIGUSR2信号，那咱们就可以用诸如sigaddset(&sa.sa_mask,SIGUSR2);
        //这里表示不阻塞任何信号
        sigemptyset(&sa.sa_mask);

        if(sigaction(pSig->mSigno, &sa, NULL) == -1)
        {
            SER_LOG(SER_LOG_EMERG, errno, "sigaction(%s) register failed!", pSig->mSigName);
            return -1;
        }
        else
        {
            SER_LOG_STDERR(0, "sigaction(%s) register sucess!", pSig->mSigName);
        }
    }

    return 0;
}


static void ser_signal_handler(int signo, siginfo_t* pSiginfo, void* pUconText)
{
    //暂时没有任何逻辑处理
    printf("get a signal!\n");
}