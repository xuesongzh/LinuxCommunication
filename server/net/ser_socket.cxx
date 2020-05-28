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
    return 0;
}

int SerSocket::ser_epoll_add_event(
    const int &fd,
    const int &readEvent,
    const int &writeEvent,
    const uint32_t &otherFlag,
    const uint32_t &eventType,
    lpser_connection_t &pConnection)
{
    return 0;
}

void SerSocket::ser_event_accept(lpser_connection_t oldConnection)
{
    return;
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