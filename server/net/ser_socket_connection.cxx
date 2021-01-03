/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*Brief: 连接池相关处理

*Data:2020.05.28
*Version: 1.0
=====================================*/

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "ser_datastruct.h"
#include "ser_lock.h"
#include "ser_log.h"
#include "ser_macros.h"
#include "ser_memory.h"
#include "ser_socket.h"

ser_connection_s::ser_connection_s() : mCurrsequence(0) {
    pthread_mutex_init(&mLogicProcMutex, NULL);
}

ser_connection_s::~ser_connection_s() {
    pthread_mutex_destroy(&mLogicProcMutex);
}

void ser_connection_s::GetOneToUse() {
    ++mCurrsequence;
    mRecvStat = PKG_HD_INIT;
    mRecvLocation = mPkgHeadInfo;
    mRecvLength = sizeof(PKG_HEADER);

    mRecvPkgData = nullptr;
    mThrowSendCount = 0;
    mSendPkgData = nullptr;
    mEpollEvents = 0;
}

void ser_connection_s::PutOneToFree() {
    ++mCurrsequence;
    auto pMemory = SerMemory::GetInstance();
    //分配过的内存需要释放
    if (mRecvPkgData != nullptr) {
        pMemory->FreeMemory(mRecvPkgData);
    }
    if (mSendPkgData != nullptr) {
        pMemory->FreeMemory(mRecvPkgData);
    }

    mThrowSendCount = 0;
}

void SerSocket::ser_init_connection() {
    auto pMemory = SerMemory::GetInstance();
    auto connectionlength = sizeof(ser_connection_t);
    lpser_connection_t pConnection = nullptr;
    for (int i = 0; i < mWorkerConnections; ++i) {
        pConnection = (lpser_connection_t)pMemory->MallocMemory(connectionlength, true);  //true:清内存
        pConnection = new (pConnection) ser_connection_t();                               //定位new，手动调用构造函数
        pConnection->GetOneToUse();
        mConnectionList.push_back(pConnection);
        mFreeConnectionList.push_back(pConnection);
    }
    return;
}

void SerSocket::ser_clear_connection() {
    lpser_connection_t pConnection = nullptr;
    auto pMemory = SerMemory::GetInstance();
    while (!mConnectionList.empty()) {
        pConnection = mConnectionList.front();
        mConnectionList.pop_front();
        pConnection->~ser_connection_t();
        pMemory->FreeMemory(pConnection);
    }
}

void SerSocket::ser_in_recy_connection(lpser_connection_t const& pConnection) {
    SER_LOG(SER_LOG_INFO, 0, "一个连接入延迟回收队列!");

    SerLock locker(&mRecyConnectionMutex);

    pConnection->mInRecyTime = time(NULL);  //记录回收时间
    SER_LOG(SER_LOG_INFO, 0, "入延迟回收队列的时间为:%d", pConnection->mInRecyTime);
    ++pConnection->mCurrsequence;
    mRecyConnectionList.push_back(pConnection);
    return;
}

void* SerSocket::ser_recy_connection_thread(void* pThreadData) {
    SER_LOG(SER_LOG_INFO, 0, "延迟回收线程启动!");
    auto pThread = static_cast<ThreadItem*>(pThreadData);
    auto pSocket = pThread->mThis;
    std::list<lpser_connection_t>::iterator iter, iterEnd;
    lpser_connection_t pConnection = nullptr;
    time_t currentTime;
    int err;

    while (true) {
        usleep(200 * 1000);  //每隔200ms检查一次是否有满足条件的连接池对象

        if (!pSocket->mRecyConnectionList.empty()) {
            // SER_LOG(SER_LOG_INFO, 0, "延迟回收队列大小：%d", pSocket->mRecyConnectionList.size());
            currentTime = time(NULL);
            err = pthread_mutex_lock(&pSocket->mRecyConnectionMutex);
            if (0 != err) {
                SER_LOG(SER_LOG_STDERR, errno, "SerSocket::ser_recy_connection_thread()中pthread_mutex_lock执行失败，错误码：%d", err);
            }
        lblRRTD:
            iter = pSocket->mRecyConnectionList.begin();
            iterEnd = pSocket->mRecyConnectionList.end();
            for (; iter != iterEnd; ++iter) {
                pConnection = *iter;
                if ((pConnection->mInRecyTime + pSocket->mRecyWaiteTime) > currentTime && g_stopEvent == 0) {
                    //如果系不退出，并且没有达到延迟回收的时间，继续。如果系统退出，则需要强制释放资源
                    continue;
                }

                //走到这里表示达到延迟回收的时间
                pSocket->mRecyConnectionList.erase(iter);  //迭代器失效goto重新开始
                pSocket->ser_free_connection(pConnection);
                SER_LOG(SER_LOG_INFO, 0, "一个延迟回收队列中的连接对象被回收，sockfd:%d", pConnection->mSockFd);
                goto lblRRTD;
            }
            err = pthread_mutex_unlock(&pSocket->mRecyConnectionMutex);
            if (0 != err) {
                SER_LOG(SER_LOG_STDERR, errno, "SerSocket::ser_recy_connection_thread()中pthread_mutex_unlock执行失败，错误码：%d", err);
            }
        }

        if (g_stopEvent == 1)  //整个程序要退出,释放资源
        {
            SER_LOG(SER_LOG_INFO, 0, "程序要退出，释放连接池资源!");

            if (!pSocket->mRecyConnectionList.empty()) {
                err = pthread_mutex_lock(&pSocket->mRecyConnectionMutex);
                if (0 != err) {
                    SER_LOG(SER_LOG_STDERR, errno, "SerSocket::ser_recy_connection_thread()中pthread_mutex_lock执行失败，错误码：%d", err);
                }
            lblRRTD2:
                iter = pSocket->mRecyConnectionList.begin();
                iterEnd = pSocket->mRecyConnectionList.end();
                for (; iter != iterEnd; ++iter) {
                    pConnection = *iter;
                    pSocket->mRecyConnectionList.erase(iter);
                    pSocket->ser_free_connection(pConnection);
                    goto lblRRTD2;
                }
                err = pthread_mutex_unlock(&pSocket->mRecyConnectionMutex);
                if (0 != err) {
                    SER_LOG(SER_LOG_STDERR, errno, "SerSocket::ser_recy_connection_thread()中pthread_mutex_unlock执行失败，错误码：%d", err);
                }
            }
        }
    }

    return (void*)0;
}

lpser_connection_t SerSocket::ser_get_free_connection(const int& sockfd) {
    SerLock locker(&mConnectionMutex);

    lpser_connection_t pConnection = nullptr;
    if (!mFreeConnectionList.empty()) {
        //如果空闲链表不为空，则从链表中返回一个连接对象
        pConnection = mFreeConnectionList.front();
        mFreeConnectionList.pop_front();
        pConnection->GetOneToUse();
        pConnection->mSockFd = sockfd;
        return pConnection;
    }

    //如果走到这里表示空闲链表没有元素，考虑重新创建一个连接对象
    auto pMemory = SerMemory::GetInstance();
    pConnection = (lpser_connection_t)pMemory->MallocMemory(sizeof(ser_connection_t), true);
    pConnection = new (pConnection) ser_connection_t();
    pConnection->GetOneToUse();
    pConnection->mSockFd = sockfd;
    mConnectionList.push_back(pConnection);  //入总的连接池，不入空闲连接和延迟回收连接池

    return pConnection;

    //     lpser_connection_t pFreeConnection = mFreeConnectionHeader; //把空闲连接池表头指针指向的连接返回出去
    //     if(nullptr == pFreeConnection)
    //     {
    //         //可能出现连接池耗尽的情况
    //         SER_LOG_STDERR(0,"SerSocket::ser_get_free_connection()发生错误，空闲连接池为空!");
    //         return nullptr;
    //     }

    //     mFreeConnectionHeader = pFreeConnection->mNext; //更新空闲连接池头指针
    //     --mFreeConnectionLegth; //空闲连接池长度减一

    //     //先把有用的东西保存起来，后面要清空变量
    //     unsigned instance = pFreeConnection->mInstance;
    //     uint64_t currsequence = pFreeConnection->mCurrsequence;
    //     /*
    //     以后增加
    //     */

    //    memset(pFreeConnection, 0, sizeof(ser_connection_t));
    //    pFreeConnection->mSockFd = sockfd;
    //    pFreeConnection->mRecvStat = PKG_HD_INIT; //刚开始处于接收包头状态
    //    pFreeConnection->mRecvLocation = pFreeConnection->mPkgHeadInfo; //接收位置指向包头数组
    //    pFreeConnection->mRecvLength = sizeof(PKG_HEADER);
    //    pFreeConnection->mIfNewRecvMem = false; //没有分配内存
    //    pFreeConnection->mRecvPkgData = nullptr; //入消息队列的包

    //    //恢复有用值
    //    pFreeConnection->mInstance = !instance; //初始值为1,取反为0，代表每次分配内存时有效
    //    pFreeConnection->mCurrsequence = currsequence;
    //    ++(pFreeConnection->mCurrsequence); //每次取用加1

    //     return pFreeConnection;
}

void SerSocket::ser_free_connection(lpser_connection_t pConnection) {
    SerLock locker(&mConnectionMutex);

    pConnection->PutOneToFree();

    mFreeConnectionList.push_back(pConnection);

    return;

    // if(pConnection->mIfNewRecvMem) //如果new了内存需要释放
    // {
    //     SerMemory::GetInstance()->FreeMemory(pConnection->mPkgData);
    // }

    // pConnection->mNext = mFreeConnectionHeader;
    // ++(pConnection->mCurrsequence);
    // mFreeConnectionHeader = pConnection; //空闲连接池表头指向新地址
    // ++mFreeConnectionLegth; //空闲连接池大小加1
}

void SerSocket::ser_close_connection(lpser_connection_t connection) {
    ser_free_connection(connection);
    int fd = connection->mSockFd;
    if (close(fd) == -1) {
        SER_LOG(SER_LOG_ALERT, errno, "SerSocket::ser_close_accepted_connection中close tcp fd失败!");
    }
    return;
}
