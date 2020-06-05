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
#include<list>
#include<sys/socket.h> //sockaddr
#include<sys/epoll.h>
#include<stdint.h>

#include "ser_pkg_defs.h"

//前向声明
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

    //收数据包相关
    int mRecvStat; //收数据包状态：PKG_HD_INIT...
    char mPkgHeadInfo[PKG_HEAD_INFO_SIZE]; //接收数据包头数据，长度比数据包结构体大
    char* mRecvLocation; //接收数据的位置
    unsigned int mRecvLength; //接收数据长度

    bool mIfNewRecvMem; //接收数据的时候是否new了内存
    char* mPkgData; //和mIfNewRecvMem搭配使用，包含消息头，包头，消息体的数据

    lpser_connection_t mNext;
};

//引入消息头记录一下额外的需要使用的东西
typedef struct NSG_HEADER
{
    lpser_connection_t mConnection; //记录对应的连接池对象
    uint64_t  mCurrsequence; //收到数据时对应的序列号，将来用于判断连接是否过期
}MSG_HEADER, *LPMSG_HEADER;

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
    int ser_epoll_process_events(const int& timer);

private:
    void ReadConf(); 
    bool ser_open_listening_sockets(); //开启监听端口
    void ser_close_listening_sockets(); //关闭监听套接字
    bool ser_set_nonblocking(const int& sockfd); //设置非阻塞套接字

    //连接池
    lpser_connection_t ser_get_free_connection(const int& sockfd);
    void ser_free_connection(lpser_connection_t& pConnection);
    void ser_close_connection(lpser_connection_t connection); //关闭连接池对象并释放对应的套接字对象

    //event
    void ser_event_accept(lpser_connection_t listenConnection);
    void ser_wait_request_handler(lpser_connection_t tcpConnection);

    //pkg
    void ser_clear_msgqueue();
    ssize_t ser_recv_pkg(lpser_connection_t const& pConnection, char* const& pBuffer, const ssize_t& bufferLength);
    
private:
    int mListenPortCount; //监听端口数目，配置文件配置
    int mWorkerConnections; //epoll连接的最大数目，配置文件配置
    int mEpollFd; //epoll对象描述符

    lpser_connection_t mConnectionHeader; //指向连接池头
    lpser_connection_t mFreeConnectionHeader; //指向空闲连接池头,用于快速分配连接对象

    int mConnectionLength; //连接池大小
    int mFreeConnectionLegth; //空闲连接池大小

    std::vector<lpser_listening_t> mListenSocketList; //监听套接字队列

    struct epoll_event mEvents[SER_EVENTS_MAX]; //时间数组，最多处理SER_EVENTS_MAX个事件

    std::list<char*> mMsgRecvQueue; //接收数据消息队列
};

#endif