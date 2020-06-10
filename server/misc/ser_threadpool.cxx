#include <signal.h>
#include <unistd.h>

#include "ser_threadpool.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_datastruct.h"

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
void* SerThreadPool::ThreadFunc(void* pPkgData)
{
    return nullptr;
}
