/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*Brief: 连接池相关处理

*Data:2020.05.28
*Version: 1.0
=====================================*/

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_datastruct.h"
#include "ser_memory.h"
#include "ser_lock.h"

ser_connection_s::ser_connection_s() : mCurrsequence(0)
{
    pthread_mutex_init(&mLogicProcMutex, NULL);
}

ser_connection_s::~ser_connection_s()
{
    pthread_mutex_destroy(&mLogicProcMutex);
}

void ser_connection_s::GetOneToUse()
{
    ++mCurrsequence;
    mRecvStat = PKG_HD_INIT;
    mRecvLocation = mPkgHeadInfo;
    mRecvLength = sizeof(PKG_HEADER);

    mRecvPkgData = nullptr;
    mThrowSendCount = 0;
    mSendPkgData = nullptr;
    mEpollEvents = 0;
}

void ser_connection_s::PutOneToFree()
{
    ++mCurrsequence;
    auto pMemory = SerMemory::GetInstance();
    //分配过的内存需要释放
    if(mRecvPkgData != nullptr)
    {
        pMemory->FreeMemory(mRecvPkgData);
    }
    if(mSendPkgData != nullptr)
    {
        pMemory->FreeMemory(mRecvPkgData);
    }

    mThrowSendCount = 0;
}

void SerSocket::ser_init_connection()
{
    auto pMemory = SerMemory::GetInstance();
    auto connectionlength = sizeof(ser_connection_t);
    lpser_connection_t pConnection = nullptr;
    for(int i = 0; i < mWorkerConnections; ++i)
    {
        pConnection = (lpser_connection_t)pMemory->MallocMemory(connectionlength, true); //true:清内存
        pConnection = new (pConnection) ser_connection_t(); //定位new，手动调用构造函数
        pConnection->GetOneToUse();
        mConnectionList.push_back(pConnection);
        mFreeConnectionList.push_back(pConnection);
    }
    return;
}

void SerSocket::ser_clear_connection()
{
    lpser_connection_t pConnection = nullptr;
    auto pMemory = SerMemory::GetInstance();
    while(!mConnectionList.empty())
    {
        pConnection = mConnectionList.front();
        mConnectionList.pop_front();
        pConnection->~ser_connection_t();
        pMemory->FreeMemory(pConnection);
    }
}



lpser_connection_t SerSocket::ser_get_free_connection(const int& sockfd)
{
    SerLock locker(&mConnectionMutex);

    lpser_connection_t pConnection = nullptr;
    if(!mFreeConnectionList.empty())
    {
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
    mConnectionList.push_back(pConnection); //入总的连接池，不入空闲连接和延迟回收连接池

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

void SerSocket::ser_free_connection(lpser_connection_t& pConnection)
{
    // if(pConnection->mIfNewRecvMem) //如果new了内存需要释放
    // {
    //     SerMemory::GetInstance()->FreeMemory(pConnection->mPkgData);
    // }

    // pConnection->mNext = mFreeConnectionHeader;
    // ++(pConnection->mCurrsequence);
    // mFreeConnectionHeader = pConnection; //空闲连接池表头指向新地址
    // ++mFreeConnectionLegth; //空闲连接池大小加1
}

void SerSocket::ser_close_connection(lpser_connection_t connection)
{
    int fd = connection->mSockFd;
    if(close(fd) == -1)
    {
        SER_LOG(SER_LOG_ALERT, errno, "SerSocket::ser_close_accepted_connection中close tcp fd失败!");
    }
    connection->mSockFd = -1; //用于判断是否有过期事件的存在
    ser_free_connection(connection);
    return;
}

