/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_threadpool.h
*Brief: 线程池

*Data:2020.06.10
*Version: 1.0
=====================================*/

#ifndef __SER_THREADPOOL_H__
#define __SER_THREADPOOL_H__

#include <pthread.h>

#include <atomic>
#include <list>
#include <vector>

class SerThreadPool {
 private:
    //定义线程结构
    struct ThreadItem {
        ThreadItem(SerThreadPool* pThis) : mThis(pThis), mIfRunning(false) {}
        ~ThreadItem(){};

        pthread_t mHandle;     //线程句柄
        SerThreadPool* mThis;  //记录线程池指针
        bool mIfRunning;       //是否启动，只有启动起来了才能调用StopAll()
    };

 public:
    SerThreadPool();
    virtual ~SerThreadPool();

 public:
    void InMsgRecvQueueAndSignal(char* pBuffer);
    bool Create(const int& threadPoolSize);  //创建线程池
    void StopAll();                          //停止所有线程
    void Call();                             //唤醒一个线程来干活

 private:
    static void* ThreadFunc(void* pThreadData);  //线程回调函数
    void ClearMsgRecvQueue();

 private:
    static pthread_mutex_t mThreadMutex;  //线程同步互斥量
    static pthread_cond_t mThreadCond;    //线程同步条件变量
    static bool mIfShutDown;              //线程是否退出

    int mThreadPoolSize;                    //线程池大小/线程池中线程的数目
    std::atomic<int> mRunningThreadNumber;  //正在运行的线程数目
    time_t mLastEmgTime;                    //上次发生线程池不够用的时间，避免过于频繁的打印日志

    std::vector<ThreadItem*> mThreads;  //线程容器
    std::list<char*> mMsgRecvQueue;     //接收数据消息队列
};

#endif