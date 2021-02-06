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
#include <map>
#include <vector>

#include "ser_pkg_defs.h"

//前向声明
class SerSocket;

#define SER_LISTEN_BACKLOG 511  //监听套接字已完成连接队列最大个数
#define SER_EVENTS_MAX 512      //epoll_wait一次最多接受的事件个数

#define MSG_HEADER_LENGTH (sizeof(struct MSG_HEADER))  //消息头长度
#define PKG_HEADER_LENGTH (sizeof(struct PKG_HEADER))  //包头长度

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

    //epoll事件
    uint32_t mEpollEvents;

    // uint8_t mWReady;             //写准备好标记
    ser_event_handler mRHandler;  //读事件相关处理
    ser_event_handler mWHandler;  //写事件相关处理

    pthread_mutex_t mLogicProcMutex;  //业务逻辑处理锁

    //收数据包相关
    int mRecvStat;                          //收数据包状态：PKG_HD_INIT...
    char mPkgHeadInfo[PKG_HEAD_INFO_SIZE];  //接收数据包头数据，长度比数据包头结构体大
    char* mRecvLocation;                    //接收数据缓冲区的位置，包头+包体
    uint32_t mRecvLength;                   //接收数据长度

    // bool mIfNewRecvMem; //接收数据的时候是否new了内存
    char* mRecvPkgData;  //和mIfNewRecvMem搭配使用，包含消息头，包头，包体的数据

    //发包相关
    std::atomic<int> mThrowSendCount;  //发送消息，如果发送缓冲区满了，需要通过epoll事件来驱动消息继续发送
    char* mSendLocation;               //发送数据缓冲区的位置，包头+包体
    uint32_t mSendLength;              //发送数据大小
    char* mSendPkgData;                //发送数据包：消息头+包头+包体

    //进入延迟回收池时的时间
    time_t mInRecyTime;
    //上次发送心跳包的时间
    time_t mLastPingTime;

    lpser_connection_t mNext;
} ser_connection_t, *lpser_connection_t;

//引入消息头记录一下额外的需要使用的东西
typedef struct MSG_HEADER {
    lpser_connection_t mConnection;  //记录对应的连接池对象
    uint64_t mCurrsequence;          //收到数据时对应的序列号，将来用于判断连接是否过期
} MSG_HEADER, *LPMSG_HEADER;

class SerSocket {
 public:
    SerSocket();
    virtual ~SerSocket();

 public:
    virtual bool Initialize();  //父进程中执行，初始化函数
    virtual void ser_thread_process_message(char* pPkgData);
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
    int ser_epoll_oper_event(const int& fd,                    //socket描述符
                             const uint32_t& op,               //事件类型:EPOLL_CTL_ADD,EPOLL_CTL_MOD,EPOLL_CTL_DEL
                             const uint32_t& events,           //关心的事件：EPOLLIN等
                             const int action,                 //补充标记，当EPOLL_CTL_ADD时不需要这个参数,EPOLL_CTL_MOD有效，0：增加，1：去掉，2：覆盖
                             lpser_connection_t pConnection);  //连接池对象
    int ser_epoll_process_events(const int& timer);

 protected:
    //发消息队列
    void ser_in_send_queue(char* pSendBuffer);

 protected:
    int mWaitTime;  //心跳检查时间

 private:
    void ReadConf();
    bool ser_open_listening_sockets();            //开启监听端口
    void ser_close_listening_sockets();           //关闭监听套接字
    bool ser_set_nonblocking(const int& sockfd);  //设置非阻塞套接字

    //发消息相关
    void ser_clear_send_queue();
    ssize_t ser_send_pkg(const int& sockFd, char* buffer, ssize_t len);

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
    ssize_t ser_recv_pkg(lpser_connection_t pConnection, char* pBuffer, const ssize_t& bufferLength);
    void ser_wait_request_process_pkg(lpser_connection_t pConnection);
    void ser_wait_request_in_msgqueue(lpser_connection_t pConnection);
    // void ser_in_msgqueue(char* const& pBuffer);
    // void ser_temp_out_msgqueue();
    // void ser_clear_msgqueue();

    //和时间相关的函数
    void addToTimerQueue(lpser_connection_t pConnection);  //设置踢出时钟(向map表中增加内容)
    time_t getEarliestTime();                              //从multimap中取得最早的时间
    LPMSG_HEADER removeFirstTimer();                       //从multimap移除最早的时间，并把最早这个时间所在的项的值所对应的指针 返回，调用者负责互斥，所以本函数不用互斥
    LPMSG_HEADER getOverTimeTimer(time_t time);            //根据给的当前时间，从m_timeQueuemap找到比这个时间更老（更早）的节点【1个】返回去，这些节点都是时间超过了，要处理的节点
    void deleteFromTimerQueue(lpser_connection_t pConnection);
    void clearTimerQueue();

    //和网络安全有关
    bool testFlood(lpser_connection_t pConnection);  //测试是否flood攻击成立，成立则返回true，否则返回false

    //线程相关
    static void* ser_send_msg_queue_thread(void* pThreadData);
    static void* ser_recy_connection_thread(void* pThreadData);      //用于延迟回收连接池
    static void* ser_timer_queue_monitor_thread(void* pThreadData);  //时间队列监视线程，处理到期不发心跳包的用户踢出的线程

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

    std::list<lpser_connection_t> mConnectionList;      //总的连接池
    std::list<lpser_connection_t> mFreeConnectionList;  //空闲连接池
    std::list<lpser_connection_t> mRecyConnectionList;  //延迟回收连接池
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

    struct epoll_event mEvents[SER_EVENTS_MAX];  //事件数组，最多处理SER_EVENTS_MAX个事件

    //时间相关
    int mIfKickTimer;                                    //是否开启踢人时钟，1：开启   0：不开启
    pthread_mutex_t mTimerQueueMutex;                    //和时间队列有关的互斥量
    std::multimap<time_t, LPMSG_HEADER> mTimerQueuemap;  //时间队列
    size_t mTimerQueueSize;                              //时间队列的尺寸
    time_t mTimerValue;                                  //当前计时队列头部时间值

    //在线用户相关
    std::atomic<int> mOnlineUserCount;  //当前在线用户数统计
    //网络安全相关
    int mFloodAkEnable;               //Flood攻击检测是否开启,1：开启   0：不开启
    unsigned int mFloodTimeInterval;  //表示每次收到数据包的时间间隔是100(毫秒)
    int mFloodKickCount;              //累积多少次踢出此人

    //统计用途
    time_t mLastPrintTime;     //上次打印统计信息的时间(10秒钟打印一次)
    int mDiscardSendPkgCount;  //丢弃的发送数据包数量
};

#endif