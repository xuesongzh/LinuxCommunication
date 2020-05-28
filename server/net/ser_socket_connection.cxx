/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*Brief: 连接池相关处理

*Data:2020.05.28
*Version: 1.0
=====================================*/

#include "ser_socket.h"

lpser_connection_t SerSocket::ser_get_free_connection(const int& sockfd)
{
    lpser_connection_t pFreeConnection = mFreeConnectionHeader; //把空闲连接池表头指针指向的连接返回出去
    if(nullptr == pFreeConnection)
    {

    }

    return pFreeConnection;
}

void SerSocket::ser_free_connection(lpser_connection_t& pConnection)
{
    pConnection->mNext = mFreeConnectionHeader;
    ++(pConnection->mCurrsequence);
    mFreeConnectionHeader = pConnection; //空闲连接池表头指向新地址
    ++mFreeConnectionLegth; //空闲连接池大小加1
}

