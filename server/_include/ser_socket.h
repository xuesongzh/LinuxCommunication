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

#include <semaphore.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/socket.h>  //sockaddr

#include <atomic>
#include <list>
#include <vector>

#include "ser_pkg_defs.h"

//前向声明
class SerSocket;

#define SER_LISTEN_BACKLOG 511  //监听套接字已完成连接队列最大个数
#define SER_EVENTS_MAX 512      //epoll_wait一次最多接受的事件个数

#define PKG_HEADER_LENGTH (sizeof(struct PKG_HEADER))  //包头长度
#define MSG_HEADER_LENGTH (sizeof(struct NSG_HEADER))  //消息头长度

//定义成员函数指针
typedef void (SerSocket::*ser_event_handler)(lpser_connection_t connect);

typedef struct ser_listening_s {
    int mPort;                       //监听端口
    int mFd;                         //套接字句柄：socket
    lpser_connection_t mConnection;  //指向对应的连接池中的连接
} ser_listening_t, *lpser_listening_t;

//连接池格式，与TCP连接绑定
typedef struct ser_connection_s {
    ser_connection_s();
    ~ser_connection_s();
    void GetOneToUse();   //取一个连接时需要更新的一些状态
    void PutOneToFree();  //释放连接池对象时需要更新的状态

    int mSockFd;                   //套接字描述符
    lpser_listening_t mListening;  //如果这个连接与监听套接字绑定，指向监听套接字

    // unsigned mInstance : 1;    //位域，失效标志物，0：有效，1：失效
    uint64_t mCurrsequence;     //序号，每次分配出去+1，用于检测过期包，废包
    struct sockaddr mSockAddr;  //保存地址

    // uint8_t mWReady;             //写准备好标记
    ser_event_handler mRHandler;  //读事件相关处理
    ser_event_handler mWHandler;  //写事件相关处理

    pthread_mutex_t mLogicProcMutex;  //业务逻辑处理锁

    //收数据包相关
    int mRecvStat;                          //收数据包状态：PKG_HD_INIT...
    char mPkgHeadInfo[PKG_HEAD_INFO_SIZE];  //接收数据包头数据，长度比数据包结构体大
    char* mRecvLocation;                    //接收数据缓冲区的位置，包头+包体
    unsigned int mRecvLength;               //接收数据长度

    // bool mIfNewRecvMem; //接收数据的时候是否new了内存
    char* mRecvPkgData;  //和mIfNewRecvMem搭配使用，包含消息头，包头，消息体的数据

    //epoll事件
    uint32_t mEpollEvents;

    //发包相关
    std::atomic<int> mThrowSendCount;  //发送消息，如果发送缓冲区满了，需要通过epoll事件来驱动消息继续发送
    char* mSendLocation;               //发送数据缓冲区的位置，包头+包体
    unsigned int mSendLength;          //发送数据大小
    char* mSendPkgData;                //发送数据包：消息头+包头+包体

    //进入延迟回收池时的时间
    time_t mInRecyTime;

    lpser_connection_t mNext;
} ser_connection_t, *lpser_connection_t;

//引入消息头记录一下额外的需要使用的东西
typedef struct NSG_HEADER {
    lpser_connection_t mConnection;  //记录对应的连接池对象
    uint64_t mCurrsequence;          //收到数据时对应的序列号，将来用于判断连接是否过期
} MSG_HEADER, *LPMSG_HEADER;

class SerSocket {
 public:
    SerSocket();
    virtual ~SerSocket();

 public:
    virtual bool Initialize();  //父进程中执行，初始化函数
    virtual void ser_thread_process_message(char* const& pPkgData);
    bool ser_init_subproc();      //子进程中执行，初始化函数
    void ser_shutdown_subproc();  //子进程中执行，程序退出时要执行的函数
    // char* ser_get_one_message();

    //epoll
    int ser_epoll_init();
    // int ser_epoll_add_event(
    //     const int &fd,
    //     const int &readEvent,
    //     const int &writeEvent,
    //     const uint32_t &otherFlag,
    //     const uint32_t &eventType,
    //     lpser_connection_t &pConnection);
    int ser_epoll_oper_event(
        const int& fd,                    //socket描述符
        const uint32_t& eventType,        //时间类型:EPOLL_CTL_ADD,EPOLL_CTL_MOD,EPOLL_CTL_DEL
        const uint32_t& events,           //关心的事件：EPOLLIN等
        const int supAction,              //补充标记，当EPOLL_CTL_ADD时不需要这个参数,EPOLL_CTL_MOD有效，0：增加，1：去掉，2：覆盖
        lpser_connection_t pConnection);  //连接池对象
    int ser_epoll_process_events(const int& timer);

 protected:
    //发消息队列
    void ser_in_send_queue(char*& pSendBuffer);

 private:
    void ReadConf();
    bool ser_open_listening_sockets();            //开启监听端口
    void ser_close_listening_sockets();           //关闭监听套接字
    bool ser_set_nonblocking(const int& sockfd);  //设置非阻塞套接字

    //发消息相关
    void ser_clear_send_queue();
    ssize_t ser_send_pkg(const int& sockFd, char* const& pBuffer, const ssize_t& bufferLength);

    //连接池
    void ser_init_connection();   //初始化连接池
    void ser_clear_connection();  //回收连接池
    lpser_connection_t ser_get_free_connection(const int& sockfd);
    void ser_free_connection(lpser_connection_t pConnection);
    void ser_close_connection(lpser_connection_t connection);  //关闭连接池对象并释放对应的套接字对象
    void ser_in_recy_connection(lpser_connection_t const& pConnection);

    //event
    void ser_event_accept(lpser_connection_t listenConnection);
    void ser_read_request_handler(lpser_connection_t tcpConnection);
    void ser_write_request_handler(lpser_connection_t tcpConnection);

    //pkg
    ssize_t ser_recv_pkg(lpser_connection_t const& pConnection, char* const& pBuffer, const ssize_t& bufferLength);
    void ser_wait_request_process_pkg(lpser_connection_t const& pConnection);
    void ser_wait_request_in_msgqueue(lpser_connection_t const& pConnection);
    // void ser_in_msgqueue(char* const& pBuffer);
    // void ser_temp_out_msgqueue();
    // void ser_clear_msgqueue();

    //线程相关
    static void* ser_send_msg_queue_thread(void* pThreadData);
    static void* ser_recy_connection_thread(void* pThreadData);  //用于延迟回收连接池

 private:
    struct ThreadItem {  //延迟回收线程结构
        pthread_t mHandle;
        SerSocket* mThis;
        bool mIfRunning;

        ThreadItem(SerSocket* pThis) : mThis(pThis), mIfRunning(false) {}
        ~ThreadItem() {}
    };

    int mListenPortCount;    //监听端口数目，配置文件配置
    int mWorkerConnections;  //epoll连接的最大数目，配置文件配置
    int mEpollFd;            //epoll对象描述符

    std::list<lpser_connection_t> mConnectionList;      //接池头
    std::list<lpser_connection_t> mFreeConnectionList;  //空闲连接池
    std::list<lpser_connection_t> mRecyConnectionList;  //延迟回收队列
    pthread_mutex_t mConnectionMutex;
    pthread_mutex_t mRecyConnectionMutex;
    int mRecyWaiteTime;  //延迟回收等待时间(s)

    //发消息队列
    std::list<char*> mMsgSendQueue;
    pthread_mutex_t mSendQueueMutex;
    sem_t mSendQueueSem;  //发消息线程信号量

    //线程容器，目前只有一个用于将延迟回收队列中的连接对象放入空闲连接
    std::vector<ThreadItem*> mThreads;

    std::vector<lpser_listening_t> mListenSocketList;  //监听套接字队列

    struct epoll_event mEvents[SER_EVENTS_MAX];  //时间数组，最多处理SER_EVENTS_MAX个事件
};

#endif