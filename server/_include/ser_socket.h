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
#include<sys/socket.h> //sockaddr
#include<stdint.h>

class SerSocket;

#define SER_LISTEN_BACKLOG 511 //监听套接字已完成连接队列最大个数
#define SER_EVENTS_MAX 512 //epoll_wait一次最多接受的事件个数

typedef struct ser_listening_s ser_listening_t,*lpser_listening_t;
typedef struct ser_connection_s ser_connection_t,*lpser_connection_t;
//定义成员函数指针
typedef void (SerSocket::*ser_event_handler)(lpser_connection_t connect);

struct ser_listening_s
{
    int mPort; //监听端口
    int mFd; //套接字句柄：socket
    lpser_connection_t mConnection; //指向对应的连接池中的连接
};

//连接池格式，与TCP连接绑定
struct ser_connection_s
{
    int mSockFd; //套接字描述符
    lpser_listening_t mListening; //如果这个连接与监听套接字绑定，指向监听套接字

    unsigned mInstance:1; //位域，失效标志物，0：有效，1：失效
    uint64_t mCurrsequence; //序号，每次分配出去+1，用于检测过期包，废包
    struct sockaddr mSockAddr; //保存地址

    uint8_t mWReady; //写准备好标记
    ser_event_handler mRHandler; //读事件相关处理
    ser_event_handler mWHandler; //写事件相关处理

    lpser_connection_t mNext;
};

class SerSocket
{
public:
    SerSocket();
    virtual ~SerSocket();

public:
    virtual bool Initialize();

    //epoll
    int ser_epoll_init();
    int ser_epoll_add_event(
        const int& fd, 
        const int& readEvent,
        const int& writeEvent,
        const uint32_t& otherFlag,
        const uint32_t& eventType,
        lpser_connection_t& pConnection);

private:
    void ReadConf(); 
    bool ser_open_listening_sockets(); //开启监听端口
    void ser_close_listening_sockets(); //关闭监听套接字
    bool ser_set_nonblocking(const int& sockfd); //设置非阻塞套接字

    //连接池
    lpser_connection_t ser_get_free_connection(const int& sockfd);
    void ser_free_connection(lpser_connection_t& pConnection);

    //event
    void ser_event_accept(lpser_connection_t listenConnection);
private:
    int mListenPortCount; //监听端口数目，配置文件配置
    int mWorkerConnections; //epoll连接的最大数目，配置文件配置
    int mEpollFd; //epoll对象描述符

    lpser_connection_t mConnectionHeader; //指向连接池头
    lpser_connection_t mFreeConnectionHeader; //指向空闲连接池头,用于快速分配连接对象

    int mConnectionLength; //连接池大小
    int mFreeConnectionLegth; //空闲连接池大小

    std::vector<lpser_listening_t> mListenSocketList; //监听套接字队列
};

#endif