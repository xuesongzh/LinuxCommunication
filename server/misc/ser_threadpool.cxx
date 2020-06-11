#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "ser_threadpool.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_datastruct.h"
#include "ser_memory.h"
#include "ser_socket.h"

pthread_mutex_t SerThreadPool::mThreadMutex = PTHREAD_MUTEX_INITIALIZER; //#define PTHREAD_MUTEX_INITIALIZER ((pthread_mutex_t) -1)
pthread_cond_t SerThreadPool::mThreadCond = PTHREAD_COND_INITIALIZER; //#define PTHREAD_COND_INITIALIZER ((pthread_cond_t) -1)
bool SerThreadPool::mIfShutDown = false;

SerThreadPool::SerThreadPool() : mThreadPoolSize(0), mRunningThreadNumber(0), mLastEmgTime(0)
{

}

SerThreadPool::~SerThreadPool()
{
    //释放资源在StopAll()里面进行
}

bool SerThreadPool::Creat(const int& threadPoolSize)
{
    ThreadItem* pNewItem =nullptr;
    int err = 0;
    mThreadPoolSize = threadPoolSize; //线程池大小

    //创建线程池
    for(int i = 0; i < mThreadPoolSize; ++i)
    {
        mThreads.push_back(pNewItem = new ThreadItem(this)); //new 一个新线程到容器中
        //&pNewItem->mHandle:线程句柄，NULL:线程属性，ThreadFunc：线程入口函数，pNewItem：入口函数参数
        err = pthread_create(&pNewItem->mHandle, NULL, ThreadFunc, pNewItem);
        if(0 != err)
        {
            SER_LOG_STDERR(err, "SerThreadPool::Creat()中pthread_creat创建线程失败!");
            return false;
        }
        else
        {
            SER_LOG(SER_LOG_INFO, 0,  "SerThreadPool::Creat()中创建线程成功，id=%d!", pNewItem->mHandle);
        }
    }

    //以下代码保证每个线程都启动，并运行至pthread_cond_wait()等待
lblfor:
    for(auto& thread : mThreads)
    {
        if(thread->mIfRunning == false)
        {
            usleep(100*1000); //100ms
            goto lblfor;
        }
    }

    return true;
}

//线程入口函数，当调用pthread_create()之后，立即被执行
//返回值必须为void*，否则pthread_create()会报错
void* SerThreadPool::ThreadFunc(void* pThreadData)
{
    auto pThread = static_cast<ThreadItem*>(pThreadData);
    auto pThreadPool = pThread->mThis;
    char* pPkgData = nullptr; //消息队列中的数据，就是数据包的数据
    auto pMemory = SerMemory::GetInstance();
    int err = 0; //错误码
    pthread_t tid = pthread_self(); //线程id

    while(true)
    {
        err = pthread_mutex_lock(&mThreadMutex); //加锁互斥量
        if(0 != err)
        {
            SER_LOG_STDERR(errno, " SerThreadPool::ThreadFunc()中pthread_mutex_lock()错误!错误码:%d", err);
            return (void*)-1;
        }

        while((pPkgData = g_socket.ser_get_one_message()) == nullptr && !mIfShutDown)
        {
            if(!pThread->mIfRunning)
            {
                pThread->mIfRunning = true; //线程被激活标记
            }

            //刚开始执行pthread_cond_wait()会卡在这里等待，并且会释放掉mThreadMutex，当被唤醒后，mThreadMutex又会被锁住
            SER_LOG_STDERR(0, "SerThreadPool::ThreadFunc()中线程:%d,开始等待",tid);
            pthread_cond_wait(&mThreadCond, &mThreadMutex);
            SER_LOG_STDERR(0, "SerThreadPool::ThreadFunc()中线程:%d,结束等待",tid);
        }
        //mIfShutDown == true或者某个线程拿到了一条数据才能走到这里
        err = pthread_mutex_unlock(&mThreadMutex);
        if(mIfShutDown)
        {
            if(pPkgData != nullptr)
            {
                pMemory->FreeMemory(pPkgData);
            }
            break;
        }

        SER_LOG_STDERR(0, "SerThreadPool::ThreadFunc()中线程:%d,拿到一条数据",tid);
        ++(pThreadPool->mRunningThreadNumber);

        //业务逻辑处理
        g_socket.ser_thread_process_message(pPkgData);

        pMemory->FreeMemory(pPkgData);
        --(pThreadPool->mRunningThreadNumber);
    }

    return (void*)0;
}
