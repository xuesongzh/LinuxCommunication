#include<signal.h>
#include<unistd.h> //close
#include<errno.h>
#include<sys/socket.h>
#include<sys/ioctl.h> //ioctl
#include<arpa/inet.h> //sockaddr_in
#include<string.h>
#include<pthread.h>

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_configer.h"
#include "ser_memory.h"

SerSocket::SerSocket():mListenPortCount(1), mWorkerConnections(1),mEpollFd(-1),mConnectionHeader(nullptr), 
    mFreeConnectionHeader(nullptr),mConnectionLength(1),mFreeConnectionLegth(1)
{
    // pthread_mutex_init(&mMsgQueueMutex, NULL); //互斥量初始化
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

    // ser_clear_msgqueue(); //释放消息队列

    // pthread_mutex_destroy(&mMsgQueueMutex); //互斥量释放

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
        if(-1 == ser_epoll_oper_event(
            sockListen->mFd, 
            EPOLL_CTL_ADD,
            EPOLLIN|EPOLLRDHUP,
            0, 
            pFreeConnection))
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
    //等待事件，事件会返回到mEvents里，最多返回SER_EVENTS_MAX个事件【我们只提供了这些内存】；
    //如果两次调用epoll_wait()的事件间隔比较长，则可能在epoll的双向链表中，积累了多个事件，所以调用epoll_wait，可能取到多个事件
    //阻塞timer这么长时间除非：a)阻塞时间到达 b)阻塞期间收到事件【比如新用户连入】会立刻返回c)调用时有事件也会立刻返回d)如果来个信号，比如你用kill -1 pid
    //如果timer为-1则一直阻塞，如果timer为0则立即返回，即便没有任何事件
    //返回值：有错误发生返回-1，错误在errno中，比如你发个信号过来，就返回-1，错误信息是(4: Interrupted system call)
    //       如果你等待的是一段时间，并且超时了，则返回0；
    //       如果返回>0则表示成功捕获到这么多个事件【返回值里】
    int events = epoll_wait(mEpollFd, mEvents, SER_EVENTS_MAX, timer);
    if(events == -1) //有错误发生
    {
        //有错误发生，发送某个信号给本进程就可以导致这个条件成立，而且错误码根据观察是4；
        //#define EINTR  4，EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。
        //例如：在socket服务器端，设置了信号捕获机制，有子进程，当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        if(errno == EINTR)
        {
            SER_LOG(SER_LOG_INFO, errno, "SerSocket::ser_epoll_process_events中epoll_wait失败!");
            return 1; //信号所致，正常返回
        }
        else
        {
            SER_LOG(SER_LOG_ALERT, errno, "SerSocket::ser_epoll_process_events中epoll_wait失败!");
            return 0; //有问题返回
        }
    }

    if(events == 0) //超时，没来事件
    {
        if(-1 != timer)//没有一直阻塞
        {
            //阻塞一段时间，正常返回
            return 1;
        }
        //一直阻塞
        SER_LOG(SER_LOG_ALERT,0, "SerSocket::ser_epoll_process_events中epoll_wait一直阻塞但发生了超时，不正常!");
        return 0;
    }

    uintptr_t instance;
    lpser_connection_t pConnection;
    uint32_t eventType;
    for(int i = 0; i < events; ++i)
    {
        pConnection = (lpser_connection_t)(mEvents[i].data.ptr);
        instance = (uintptr_t)pConnection & 1; //取出instance
        pConnection = (lpser_connection_t)((uintptr_t)pConnection & (uintptr_t)~1); //取得连接池真正的地址

        //过滤过期事件
        if(pConnection->mSockFd == -1)
        {
            //一个套接字，当关联一个 连接池中的连接【对象】时，这个套接字值是要给到mSockFd的，关闭连接时这个mSockFd会被设置为-1
            //比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭，那我们应该会把mSockFd设置为-1；
            //第二个事件照常处理
            //第三个事件，假如这第三个事件，也跟第一个事件对应的是同一个连接，那这个条件就会成立；那么这种事件，属于过期事件，不该处理
            SER_LOG(SER_LOG_DEBUG,0,"SerSocket::ser_epoll_process_events中遇到了mSockFd == -1的过期事件!");
            continue;
        }

        if(instance != pConnection->mInstance)
        {
            //比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭【麻烦就麻烦在这个连接被服务器关闭上了】，但是恰好第三个事件也跟这个连接有关；
            //因为第一个事件就把socket连接关闭了，显然第三个事件我们是不应该处理的【因为这是个过期事件】，若处理肯定会导致错误；
            //那我们上述把mSockFd设置为-1，可以解决这个问题吗？ 能解决一部分问题，但另外一部分不能解决，不能解决的问题描述如下【这么离奇的情况应该极少遇到】：

            //a)处理第一个事件时，因为业务需要，我们把这个连接【假设套接字为50】关闭，同时设置mFd = -1;并且调用ser_free_connection将该连接归还给连接池；
            //b)处理第二个事件，恰好第二个事件是建立新连接事件，调用ser_get_free_connection从连接池中取出的连接非常可能就是刚刚释放的第一个事件对应的连接池中的连接；
            //c)又因为a中套接字50被释放了，所以会被操作系统拿来复用，复用给了b；
            //d)当处理第三个事件时，第三个事件其实是已经过期的，应该不处理，那怎么判断这第三个事件是过期的呢？ 【假设现在处理的是第三个事件，此时这个 连接池中的该连接 实际上已经被用作第二个事件对应的socket上了】；
                //依靠instance标志位能够解决这个问题，当调用ser_get_free_connection从连接池中获取一个新连接时，我们把instance标志位置反，所以这个条件如果不成立，说明这个连接已经被挪作他用了；
            SER_LOG(SER_LOG_DEBUG,0,"SerSocket::ser_epoll_process_events中遇到了instance != pConnection->mInstance的过期事件!");
            continue;           
        }

        //处理正常事件
        eventType = mEvents[i].events; //取得时间类型

        if(eventType & (EPOLLERR|EPOLLHUP)) //生了错误或者客户端断连
        {
            //EPOLLIN：表示对应的链接上有数据可以读出（TCP链接的远端主动关闭连接，也相当于可读事件，因为本服务器要处理发送来的FIN包）
            //EPOLLOUT：表示对应的连接上可以写入数据发送【写准备好】   
            eventType |= EPOLLIN|EPOLLOUT;
        }

        if(eventType & EPOLLIN) //如果是读事件，如：三次握手
        {
            //如果是监听套接字，一般是三路握手，走ser_event_accept
            //如果是正常tcp连接来数据，走ser_wait_request_handler
            (this->*(pConnection->mRHandler))(pConnection);
        }

        if(eventType & EPOLLOUT) //如果是写事件
        {
            SER_LOG_STDERR(0, "处理写事件.......");
        }
    }

    return 1;
}

// int SerSocket::ser_epoll_add_event(
//     const int& fd,
//     const int& readEvent,
//     const int& writeEvent,
//     const uint32_t& otherFlag,
//     const uint32_t& eventType,
//     lpser_connection_t& pConnection)
// {
//     struct epoll_event ev;
//     memset(&ev,0,sizeof(ev));

//     if(1 == readEvent)
//     {
//         //读事件
//         ev.events = EPOLLIN|EPOLLRDHUP; //EPOLLIN读事件，也就是read ready【客户端三次握手连接进来，也属于一种可读事件】EPOLLRDHUP 客户端关闭连接，断连
//     }
//     else
//     {
//         //其他类型事件
//     }

//     if(0 != otherFlag)
//     {
//         ev.events |= otherFlag;
//     }

//     //因为指针的最后一位【二进制位】肯定不是1，所以 和 pConnection->instance做 |运算；到时候通过一些编码，既可以取得c的真实地址，又可以把此时此刻的pConnection->instance值取到
//     //比如c是个地址，可能的值是 0x00af0578，对应的二进制是‭101011110000010101111000‬，而 | 1后是0x00af0579
//     //把连接池对象弄进去，后续来事件时，用epoll_wait()后，这个对象能取出来用  //但同时把一个 标志位【不是0就是1】弄进去
//     ev.data.ptr = (void*)((uintptr_t)pConnection | pConnection->mInstance);

//     if(-1 == epoll_ctl(mEpollFd, eventType, fd, &ev)) //将socket及其事件添加到红黑树中
//     {
//         SER_LOG_STDERR(errno,"ngx_epoll_add_event()中epoll_ctl(%d,%d,%d,%u,%u)失败.",fd,readEvent,writeEvent,otherFlag,eventType);
//         return -1;
//     }

//     return 0;
// }

int SerSocket::ser_epoll_oper_event(
    const int &fd,                   
    const uint32_t &eventType,       
    const uint32_t &events,          
    const int supAction,             
    lpser_connection_t &pConnection) 
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));

    if(eventType == EPOLL_CTL_ADD)
    {
        //增加红黑树中的节点
        ev.data.ptr = (void*)pConnection;
        ev.events = events;
        pConnection->mEpollEvents = events; //连接对象上也记录一下这个事件
    }
    else if(eventType == EPOLL_CTL_MOD)
    {
        //修改红黑树中节点的事件
    }
    else //EPOLL_CTL_DEL
    {
        //目前没有这个需求
        return 1;
    }

    if(epoll_ctl(mEpollFd, eventType, fd, &ev) == -1)
    {
        SER_LOG(SER_LOG_STDERR, errno, "SerSocket::ser_epoll_oper_event()中epoll_ctl()执行失败!");
        return -1;
    }

    return 1;
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

// void SerSocket::ser_clear_msgqueue()
// {
//     char* pTemp = nullptr;
//     auto pMemory = SerMemory::GetInstance();
//     while(!mMsgRecvQueue.empty())
//     {
//         pTemp = mMsgRecvQueue.front();
//         mMsgRecvQueue.pop_front();
//         pMemory->FreeMemory(pTemp);
//     }
// }