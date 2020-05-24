/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_socket.h
*Brief: 网络通信socket类

*Data:2020.05.24
*Version: 1.0
=====================================*/

#ifndef __SER_SOCKER_H__
#define __SER_SOCKER_H__

#include<vector>

#define SER_LISTEN_BACKLOG 511 //监听套接字已完成连接队列最大个数

typedef struct ser_listening_s
{
    int mPort; //监听端口
    int mFd; //套接字句柄：socket
}ser_listening_t,*lpser_listening_t;

class SerSocket
{
public:
    SerSocket();
    virtual ~SerSocket();

public:
    virtual bool Initialize();

private:
    bool ser_open_listening_sockets(); //开启监听端口
    void ser_close_listening_sockets(); //关闭监听套接字
    bool ser_set_nonblocking(const int& sockfd); //设置非阻塞套接字

private:
    int mListenPortCount; //监听端口数目，配置文件配置
    std::vector<lpser_listening_t> mListenSocketList; //监听套接字队列
};

#endif