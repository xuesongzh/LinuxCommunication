#include "ser_threadpool.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "ser_datastruct.h"
#include "ser_log.h"
#include "ser_logic_socket.h"
#include "ser_macros.h"
#include "ser_memory.h"

pthread_mutex_t SerThreadPool::mThreadMutex = PTHREAD_MUTEX_INITIALIZER;  //#define PTHREAD_MUTEX_INITIALIZER ((pthread_mutex_t) -1)
pthread_cond_t SerThreadPool::mThreadCond = PTHREAD_COND_INITIALIZER;     //#define PTHREAD_COND_INITIALIZER ((pthread_cond_t) -1)
bool SerThreadPool::mIfShutDown = false;

SerThreadPool::SerThreadPool() : mThreadPoolSize(0), mRunningThreadNumber(0), mLastEmgTime(0) {
}

SerThreadPool::~SerThreadPool() {
    //一些释放资源在StopAll()里面进行
    ClearMsgRecvQueue();
}

bool SerThreadPool::Creat(const int& threadPoolSize) {
    ThreadItem* pNewItem = nullptr;
    int err = 0;
    mThreadPoolSize = threadPoolSize;  //线程池大小

    //创建线程池
    for (int i = 0; i < mThreadPoolSize; ++i) {
        mThreads.push_back(pNewItem = new ThreadItem(this));  //new 一个新线程到容器中
        //&pNewItem->mHandle:线程句柄，NULL:线程属性，ThreadFunc：线程入口函数，pNewItem：入口函数参数
        err = pthread_create(&pNewItem->mHandle, NULL, ThreadFunc, pNewItem);
        if (0 != err) {
            SER_LOG_STDERR(err, "SerThreadPool::Creat()中pthread_creat创建线程失败!");
            return false;
        } else {
            SER_LOG(SER_LOG_INFO, 0, "SerThreadPool::Creat()中创建线程成功，id=%d!", pNewItem->mHandle);
        }
    }

    //以下代码保证每个线程都启动，并运行至pthread_cond_wait()等待
lblfor:
    for (auto& thread : mThreads) {
        if (thread->mIfRunning == false) {
            usleep(100 * 1000);  //100ms
            goto lblfor;
        }
    }

    return true;
}

void SerThreadPool::StopAll() {
    if (true == mIfShutDown) {
        return;
    }

    mIfShutDown = true;
    //唤醒等待条件[在pthread_cond_wait卡住等待]的所有线程
    int err = pthread_cond_broadcast(&mThreadCond);
    if (0 != err) {
        SER_LOG_STDERR(errno, "SerThreadPool::StopAll()中pthread_cond_broadcast()失败，错误码：%d!", err);
        return;
    }

    //等待所有线程返回
    for (auto& thread : mThreads) {
        pthread_join(thread->mHandle, NULL);
    }

    //释放互斥量，条件变量
    pthread_mutex_destroy(&mThreadMutex);
    pthread_cond_destroy(&mThreadCond);

    //释放线程池
    for (auto& thread : mThreads) {
        DEL_PTR(thread);
    }
    mThreads.clear();
    SER_LOG_STDERR(0, "SerThreadPool::StopAll()中线程池正常全部释放!");
    return;
}

void SerThreadPool::Call() {
    //唤醒一个在ptherad_cond_wait()等待的线程，用于执行任务
    int err = pthread_cond_signal(&mThreadCond);
    if (0 != err) {
        SER_LOG_STDERR(errno, "SerThreadPool::Call()中pthread_cond_signal失败，错误码：%d!", err);
        return;
    }

    //线程池不够用需要提示
    if (mThreadPoolSize == mRunningThreadNumber) {
        time_t currtime = time(NULL);
        if (currtime - mLastEmgTime > 10)  //超过10秒才报警，不然会一直提示
        {
            mLastEmgTime = currtime;
            SER_LOG_STDERR(0, "SerThreadPool::Call()中发现线程池不够用!");
        }
    }
}

//线程入口函数，当调用pthread_create()之后，立即被执行
//返回值必须为void*，否则pthread_create()会报错
void* SerThreadPool::ThreadFunc(void* pThreadData) {
    auto pThread = static_cast<ThreadItem*>(pThreadData);
    auto pThreadPool = pThread->mThis;
    auto pMemory = SerMemory::GetInstance();
    int err = 0;                     //错误码
    pthread_t tid = pthread_self();  //线程id

    while (true) {
        err = pthread_mutex_lock(&mThreadMutex);  //加锁互斥量
        if (0 != err) {
            SER_LOG_STDERR(errno, " SerThreadPool::ThreadFunc()中pthread_mutex_lock()错误!错误码:%d", err);
            return (void*)-1;
        }

        while (pThreadPool->mMsgRecvQueue.size() == 0 && !mIfShutDown) {
            if (!pThread->mIfRunning) {
                pThread->mIfRunning = true;  //线程被激活标记
            }

            //刚开始执行pthread_cond_wait()会卡在这里等待，并且会释放掉mThreadMutex，当被唤醒后，mThreadMutex又会被锁住
            SER_LOG_STDERR(0, "SerThreadPool::ThreadFunc()中线程:%d,开始等待", tid);
            pthread_cond_wait(&mThreadCond, &mThreadMutex);
            SER_LOG_STDERR(0, "SerThreadPool::ThreadFunc()中线程:%d,结束等待", tid);
        }
        //mIfShutDown == true或者某个线程拿到了一条数据才能走到这里
        if (mIfShutDown) {
            pthread_mutex_unlock(&mThreadMutex);
            break;
        }

        char* pPkgData = pThreadPool->mMsgRecvQueue.front();
        pThreadPool->mMsgRecvQueue.pop_front();
        SER_LOG_STDERR(0, "SerThreadPool::ThreadFunc()中线程:%d,拿到一条数据", tid);

        //解锁互斥量
        err = pthread_mutex_unlock(&mThreadMutex);
        if (0 != err) {
            SER_LOG_STDERR(errno, "SerThreadPool::ThreadFunc()中pthread_mutex_unlock失败，错误码：%d", err);
        }

        ++(pThreadPool->mRunningThreadNumber);

        //业务逻辑处理
        // char* temp = pPkgData;
        // LPPKG_HEADER pPkgHeader = (LPPKG_HEADER)(temp + MSG_HEADER_LENGTH);
        // SER_LOG_STDERR(0,"处理业务逻辑时包的长度:%d，消息码：%d，crc32：%d",ntohs(pPkgHeader->mPkgLength), ntohs(pPkgHeader->mMsgCode),ntohl(pPkgHeader->mCRC32));

        g_socket.ser_thread_process_message(pPkgData);

        pMemory->FreeMemory(pPkgData);
        --(pThreadPool->mRunningThreadNumber);
    }

    return (void*)0;
}

void SerThreadPool::InMsgRecvQueueAndSignal(char* const& pBuffer) {
    int err = pthread_mutex_lock(&mThreadMutex);
    if (0 != err) {
        SER_LOG_STDERR(errno, "SerThreadPool::InMsgRecvQueueAndSignal()中pthread_mutex_lock发生错误，错误码：%d", err);
        return;
    }

    mMsgRecvQueue.push_back(pBuffer);

    //取消互斥
    err = pthread_mutex_unlock(&mThreadMutex);
    if (0 != err) {
        SER_LOG_STDERR(errno, "SerThreadPool::InMsgRecvQueueAndSignal()中pthread_mutex_unlock发生错误，错误码：%d", err);
        return;
    }

    //激活一个线程来干活
    Call();
    return;
}

void SerThreadPool::ClearMsgRecvQueue() {
    auto pMemory = SerMemory::GetInstance();
    char* pTemp = nullptr;
    while (!mMsgRecvQueue.empty()) {
        pTemp = mMsgRecvQueue.front();
        mMsgRecvQueue.pop_front();
        pMemory->FreeMemory(pTemp);
    }
}
