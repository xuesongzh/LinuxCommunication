/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*Brief: 连接池相关处理

*Data:2020.05.28
*Version: 1.0
=====================================*/

#include <string.h>

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"

lpser_connection_t SerSocket::ser_get_free_connection(const int& sockfd)
{
    lpser_connection_t pFreeConnection = mFreeConnectionHeader; //把空闲连接池表头指针指向的连接返回出去
    if(nullptr == pFreeConnection)
    {
        //可能出现连接池耗尽的情况
        SER_LOG_STDERR(0,"SerSocket::ser_get_free_connection()发生错误，空闲连接池为空!");
        return nullptr;
    }

    mFreeConnectionHeader = pFreeConnection->mNext; //更新空闲连接池头指针
    --mFreeConnectionLegth; //空闲连接池长度减一

    //先把有用的东西保存起来，后面要清空变量
    unsigned instance = pFreeConnection->mInstance;
    uint64_t currsequence = pFreeConnection->mCurrsequence;
    /*
    以后增加
    */

   memset(pFreeConnection, 0, sizeof(ser_connection_t));
   pFreeConnection->mSockFd = sockfd;
   //恢复有用值
   pFreeConnection->mInstance = !instance; //初始值为1,取反为0，代表每次分配内存时有效
   pFreeConnection->mCurrsequence = currsequence;
   ++(pFreeConnection->mCurrsequence); //每次取用加1

    return pFreeConnection;
}

void SerSocket::ser_free_connection(lpser_connection_t& pConnection)
{
    pConnection->mNext = mFreeConnectionHeader;
    ++(pConnection->mCurrsequence);
    mFreeConnectionHeader = pConnection; //空闲连接池表头指向新地址
    ++mFreeConnectionLegth; //空闲连接池大小加1
}
