#include<signal.h>
#include<unistd.h> //close
#include<errno.h>
#include<sys/socket.h>
#include<sys/ioctl.h> //ioctl
#include<arpa/inet.h> //sockaddr_in
#include<string.h>

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_configer.h"

SerSocket::SerSocket():mListenPortCount(1), mWorkerConnections(1),mEpollFd(-1),mConnectionHeader(nullptr), 
    mFreeConnectionHeader(nullptr),mConnectionLength(1),mFreeConnectionLegth(1)
{
    return;
}

SerSocket::~SerSocket()
{
    for(auto& pSocketItem : mListenSocketList)
    {
        DEL_PTR(pSocketItem);
    }

    mListenSocketList.clear();

    DEL_ARRAY(mConnectionHeader); //释放连接池

    return;
}

bool SerSocket::Initialize() //在fork子进程之前调用
{
    ReadConf();
    bool ret = ser_open_listening_sockets();
    return ret;
}

void SerSocket::ReadConf()
{
    auto pConfiger = SerConfiger::GetInstance();
    mListenPortCount = pConfiger->GetIntDefault("ListenPortCount", mListenPortCount);
    mWorkerConnections = pConfiger->GetIntDefault("WorkerConnections", mWorkerConnections);
    return;
}

bool SerSocket::ser_open_listening_sockets()
{
    int listenSockFd; //监听套接字
    struct sockaddr_in serv_addr; //服务器地址结构
    int listenPort; //监听端口
    char listenPortStr[100] = {0}; //监听端口如ListenPort0

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET; //ipv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);//监听所有本地IP

    auto pConfiger = SerConfiger::GetInstance();

    for(int i = 0; i < mListenPortCount; ++i)
    {
        listenSockFd = socket(AF_INET, SOCK_STREAM, 0); //SOCK_STREAM:tcp
        if(-1 == listenSockFd)
        {
            SER_LOG_STDERR(errno, "SerSocket::ser_open_listening_sockets中socket()执行失败!");
            return false;
        }

        //setsockopt:SO_REUSEADDR:主要解决TIME_WAIT状态bind失败的问题
        //SO_REUSEADDR和SOL_SOCKET一起使用
        int reuseaddr = 1; //1:打开对应的设置
        if(-1 == setsockopt(listenSockFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&reuseaddr, sizeof(reuseaddr)))
        {
            SER_LOG_STDERR(errno, "SerSocket::ser_open_listening_sockets中setsocketopt()执行失败!");
            close(listenSockFd);
            return false;
        }

        //设置监听套接字为非阻塞
        if(false == ser_set_nonblocking(listenSockFd))
        {
            SER_LOG_STDERR(errno, "SerSocket::ser_set_nonblocking()执行失败!");
            close(listenSockFd);
            return false;
        }

        //设置监听端口
        memset(listenPortStr, 0, sizeof(listenPortStr));
        sprintf(listenPortStr, "ListenPort%d", i);//把格式化字符串写入到变量
        listenPort = pConfiger->GetIntDefault(listenPortStr, 10000);
        serv_addr.sin_port = htons((in_port_t)listenPort);

        //绑定服务器地址结构
        if(-1 == bind(listenSockFd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)))
        {
            SER_LOG_STDERR(errno, "SerSocket::ser_open_listening_sockets中bind()执行失败!");
            close(listenSockFd);
            return false;      
        }

        //开始监听
        if(-1 == listen(listenSockFd, SER_LISTEN_BACKLOG))
        {
            SER_LOG_STDERR(errno, "SerSocket::ser_open_listening_sockets中listen()执行失败!");
            close(listenSockFd);
            return false;      
        }

        //监听成功放入列表中
        auto pListenSocketItem = new ser_listening_t();
        memset(pListenSocketItem, 0, sizeof(ser_listening_t));
        pListenSocketItem->mPort = listenPort;
        pListenSocketItem->mFd = listenSockFd;
        SER_LOG(SER_LOG_INFO, 0, "监听端口%d成功", listenPort);
        mListenSocketList.push_back(pListenSocketItem);
    }

    return true;
}

int SerSocket::ser_epoll_init()
{
    //创建epoll对象，红黑树+双向链表
    mEpollFd = epoll_create(mWorkerConnections); //参数大于0即可，这里串epoll能处理的最大连接数
    if(-1 == mEpollFd)
    {
        SER_LOG_STDERR(0,"SerSocket::ser_epoll_init()中epoll_create失败!");
        exit(2); //致命问题直接退出
    }

    //创建连接池
    mConnectionLength = mWorkerConnections; //连接池大小等于能处理的TCP连接最大数目
    mConnectionHeader = new ser_connection_t[mConnectionLength];
    mFreeConnectionHeader = nullptr;
    int i = mConnectionLength;

    do
    {
        --i;
        mConnectionHeader[i].mNext = mFreeConnectionHeader; //最后一个元素指向空
        mConnectionHeader[i].mSockFd = -1; //初始化TCP连接，无socket绑定
        mConnectionHeader[i].mInstance = 1; //失效标志位：失效
        mConnectionHeader[i].mCurrsequence = 0; //序号从0开始

        //更新空闲链头指针
        mFreeConnectionHeader = &mConnectionHeader[i];

    }while(i);
    mFreeConnectionLegth = mConnectionLength; //刚开始空闲链大小等于连接池大小

    //为监听套接字增加一个连接池中的连接
    for(auto& sockListen : mListenSocketList)
    {
        auto pFreeConnection = ser_get_free_connection(sockListen->mFd); //取出一个空闲链接
        if(nullptr == pFreeConnection)
        {
            //说明空闲连接没有了，链接池被分配完了
            SER_LOG_STDERR(0,"ser_get_free_connection()取空闲链接失败!");
            exit(2);
        }
        pFreeConnection->mListening = sockListen; //连接对象关联监听套接字对象
        sockListen->mConnection = pFreeConnection; //监听对象关联连接池对象
        pFreeConnection->mRHandler = &SerSocket::ser_event_accept; //对监听端口读事件设置处理方法
        //往监听套接字上增加监听事件
        if(-1 == ser_epoll_add_event(sockListen->mFd, 1, 0, 0, EPOLL_CTL_ADD, pFreeConnection))
        {
            exit(2);
        }

    }

    return 0;
}

//timer表示epoll_waiter要等待的时间，单位ms
//返回值1：正常返回，0：有问题返回。两者出现都需要程序正常继续执行
int SerSocket::ser_epoll_process_events(const int& timer)
{
    int events = epoll_wait(mEpollFd, mEvents, SER_EVENTS_MAX, timer);

    uintptr_t instance;
    lpser_connection_t pConnection;
    uint32_t eventType;
    for(int i = 0; i < events; ++i)
    {
        pConnection = (lpser_connection_t)(mEvents[i].data.ptr);
        instance = (uintptr_t)pConnection & 1; //取出instance
        pConnection = (lpser_connection_t)((uintptr_t)pConnection & (uintptr_t)~1); //取得连接池真正的地址


        eventType = mEvents[i].events; //取得时间类型


        if(eventType & EPOLLIN) //如果是读事件，如：三次握手
        {
            //如果是监听套接字，一般是三路握手，走ser_event_accept
            //如果是正常tcp连接来数据，走ser_wait_request_handler
            (this->*(pConnection->mRHandler))(pConnection);
        }
    }

    return 1;
}

int SerSocket::ser_epoll_add_event(
    const int& fd,
    const int& readEvent,
    const int& writeEvent,
    const uint32_t& otherFlag,
    const uint32_t& eventType,
    lpser_connection_t& pConnection)
{
    struct epoll_event ev;
    memset(&ev,0,sizeof(ev));

    if(1 == readEvent)
    {
        //读事件
        ev.events = EPOLLIN|EPOLLRDHUP; //EPOLLIN读事件，也就是read ready【客户端三次握手连接进来，也属于一种可读事件】EPOLLRDHUP 客户端关闭连接，断连
    }
    else
    {
        //其他类型事件
    }

    if(0 != otherFlag)
    {
        ev.events |= otherFlag;
    }

    //因为指针的最后一位【二进制位】肯定不是1，所以 和 pConnection->instance做 |运算；到时候通过一些编码，既可以取得c的真实地址，又可以把此时此刻的pConnection->instance值取到
    //比如c是个地址，可能的值是 0x00af0578，对应的二进制是‭101011110000010101111000‬，而 | 1后是0x00af0579
    //把连接池对象弄进去，后续来事件时，用epoll_wait()后，这个对象能取出来用  //但同时把一个 标志位【不是0就是1】弄进去
    ev.data.ptr = (void*)((uintptr_t)pConnection | pConnection->mInstance);

    if(-1 == epoll_ctl(mEpollFd, eventType, fd, &ev)) //将socket及其事件添加到红黑树中
    {
        SER_LOG_STDERR(errno,"ngx_epoll_add_event()中epoll_ctl(%d,%d,%d,%u,%u)失败.",fd,readEvent,writeEvent,otherFlag,eventType);
        return -1;
    }

    return 0;
}

bool SerSocket::ser_set_nonblocking(const int& sockfd)
{
    int nb = 1; //0:清除，1:设置
    if(-1 == ioctl(sockfd, FIONBIO, &nb))//FIONBIO:设置/清除 非阻塞I/O标记
    {
        SER_LOG_STDERR(errno, "set nonblocking failed!");
        return false;
    }

    return true;

    // //如下也是一种写法，跟上边这种写法其实是一样的，但上边的写法更简单 
    // // fcntl:file control【文件控制】相关函数，执行各种描述符控制操作
    // // 参数1：所要设置的描述符，这里是套接字【也是描述符的一种】
    // int opts = fcntl(sockfd, F_GETFL);  //用F_GETFL先获取描述符的一些标志信息
    // if(opts < 0) 
    // {
    //     SER_LOG_STDERR(errno,"SerSocket::ser_set_nonblocking()中fcntl(F_GETFL)失败.");
    //     return false;
    // }
    // opts |= O_NONBLOCK; //把非阻塞标记加到原来的标记上，标记这是个非阻塞套接字【如何关闭非阻塞呢？opts &= ~O_NONBLOCK,然后再F_SETFL一下即可】
    // if(fcntl(sockfd, F_SETFL, opts) < 0) 
    // {
    //     SER_LOG_STDERR(errno,"SerSocket::ser_set_nonblocking()中fcntl(F_SETFL)失败.");
    //     return false;
    // }
    // return true;
}

void SerSocket::ser_close_listening_sockets()
{
    for(auto& pSocketItem : mListenSocketList)
    {
        close(pSocketItem->mFd);
        SER_LOG(SER_LOG_INFO, 0, "关闭监听端口:%d", pSocketItem->mFd);
    }
    return;
}