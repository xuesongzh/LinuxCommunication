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
#include "ser_lock.h"

SerSocket::SerSocket():mListenPortCount(1), mWorkerConnections(1),mEpollFd(-1),
    mRecyWaiteTime(60)
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
    mRecyWaiteTime = pConfiger->GetIntDefault("RecyConnectionWaiteTime", mRecyWaiteTime);
    return;
}

bool SerSocket::ser_init_subproc()
{
    //发消息队列互斥量初始化
    if(pthread_mutex_init(&mSendQueueMutex, NULL) != 0)
    {
        SER_LOG(SER_LOG_STDERR, 0, "SerSocket::ser_init_subproc()中pthread_mutex_init(mSendQueueMutex)失败!");
        return false;
    }
    //连接池互斥量初始化
    if(pthread_mutex_init(&mConnectionMutex, NULL) != 0)
    {
        SER_LOG(SER_LOG_STDERR, 0, "SerSocket::ser_init_subproc()中pthread_mutex_init(mConnectionMutex)失败!");
        return false;
    }
    //延迟回收队列互斥量初始化
    if(pthread_mutex_init(&mRecyConnectionMutex, NULL) != 0)
    {
        SER_LOG(SER_LOG_STDERR, 0, "SerSocket::ser_init_subproc()中pthread_mutex_init(mRecyConnectionMutex)失败!");
        return false;
    }

    //初始化发消息相关信号量，信号量用于进程/线程 之间的同步
    //第二个参数=0，表示信号量在线程之间共享，确实如此 ，如果非0，表示在进程之间共享
    //第三个参数=0，表示信号量的初始值，为0时，调用sem_wait()就会卡在那里卡着
    if (sem_init(&mSendQueueSem, 0, 0) == -1)
    {
        SER_LOG(SER_LOG_STDERR, 0, "SerSocket::ser_init_subproc()中sem_init(mSendQueueSem)失败!");
        return false;
    }

    int err;
    //创建线程，用于发送数据
    ThreadItem* pSendQueueThread;
    mThreads.push_back(pSendQueueThread = new ThreadItem(this));
    err = pthread_create(&pSendQueueThread->mHandle, NULL, ser_send_msg_queue_thread, pSendQueueThread);
    if(err != 0)
    {
        SER_LOG(SER_LOG_STDERR, 0, "SerSocket::ser_init_subproc()中创建发送数据线程失败!");
        return false;
    }

    //创建线程用于延迟回收连接池对象
    ThreadItem* pRecyConnThread;
    mThreads.push_back(pRecyConnThread = new ThreadItem(this));
    err = pthread_create(&pRecyConnThread->mHandle, NULL, ser_recy_connection_thread, pRecyConnThread);
    if(err != 0)
    {
        SER_LOG(SER_LOG_STDERR, 0, "SerSocket::ser_init_subproc()中创建延迟回收线程失败!");
        return false;
    }
    return true;
}

void SerSocket::ser_shutdown_subproc()
{
    //用到信号量的，可能还需要调用一下sem_post,让发送数据的线程流程走下去
    if(sem_post(&mSendQueueSem )== -1)
    {
         SER_LOG(SER_LOG_STDERR, 0,"SerSocket::ser_shutdown_subprocc()中sem_post()失败!");
    }

    //停止线程
    for(auto& thread : mThreads)
    {
        pthread_join(thread->mHandle, NULL); //等待一个线程终止
    }

    for(auto& thread : mThreads)
    {
        DEL_PTR(thread); //释放new出来的线程
    }
    mThreads.clear();

    //队列相关
    ser_clear_send_queue();
    ser_clear_connection();

    //多线程相关
    pthread_mutex_destroy(&mConnectionMutex);
    pthread_mutex_destroy(&mRecyConnectionMutex);
    pthread_mutex_destroy(&mSendQueueMutex);
    sem_destroy(&mSendQueueSem);
}

void SerSocket::ser_clear_send_queue()
{
    char* pSendMsg = nullptr;
    auto pMemory = SerMemory::GetInstance();
    while(!mMsgSendQueue.empty())
    {
        pSendMsg = mMsgSendQueue.front();
        mMsgSendQueue.pop_front();
        pMemory->FreeMemory(pSendMsg);
    }
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

    //初始化连接池
    ser_init_connection();

    // //创建连接池
    // mConnectionLength = mWorkerConnections; //连接池大小等于能处理的TCP连接最大数目
    // mConnectionHeader = new ser_connection_t[mConnectionLength];
    // mFreeConnectionHeader = nullptr;
    // int i = mConnectionLength;

    // do
    // {
    //     --i;
    //     mConnectionHeader[i].mNext = mFreeConnectionHeader; //最后一个元素指向空
    //     mConnectionHeader[i].mSockFd = -1; //初始化TCP连接，无socket绑定
    //     // mConnectionHeader[i].mInstance = 1; //失效标志位：失效
    //     mConnectionHeader[i].mCurrsequence = 0; //序号从0开始

    //     //更新空闲链头指针
    //     mFreeConnectionHeader = &mConnectionHeader[i];

    // }while(i);
    // mFreeConnectionLegth = mConnectionLength; //刚开始空闲链大小等于连接池大小

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
        // instance = (uintptr_t)pConnection & 1; //取出instance
        // pConnection = (lpser_connection_t)((uintptr_t)pConnection & (uintptr_t)~1); //取得连接池真正的地址

        // //过滤过期事件
        // if(pConnection->mSockFd == -1)
        // {
        //     //一个套接字，当关联一个 连接池中的连接【对象】时，这个套接字值是要给到mSockFd的，关闭连接时这个mSockFd会被设置为-1
        //     //比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭，那我们应该会把mSockFd设置为-1；
        //     //第二个事件照常处理
        //     //第三个事件，假如这第三个事件，也跟第一个事件对应的是同一个连接，那这个条件就会成立；那么这种事件，属于过期事件，不该处理
        //     SER_LOG(SER_LOG_DEBUG,0,"SerSocket::ser_epoll_process_events中遇到了mSockFd == -1的过期事件!");
        //     continue;
        // }

        // if(instance != pConnection->mInstance)
        // {
        //     //比如我们用epoll_wait取得三个事件，处理第一个事件时，因为业务需要，我们把这个连接关闭【麻烦就麻烦在这个连接被服务器关闭上了】，但是恰好第三个事件也跟这个连接有关；
        //     //因为第一个事件就把socket连接关闭了，显然第三个事件我们是不应该处理的【因为这是个过期事件】，若处理肯定会导致错误；
        //     //那我们上述把mSockFd设置为-1，可以解决这个问题吗？ 能解决一部分问题，但另外一部分不能解决，不能解决的问题描述如下【这么离奇的情况应该极少遇到】：

        //     //a)处理第一个事件时，因为业务需要，我们把这个连接【假设套接字为50】关闭，同时设置mFd = -1;并且调用ser_free_connection将该连接归还给连接池；
        //     //b)处理第二个事件，恰好第二个事件是建立新连接事件，调用ser_get_free_connection从连接池中取出的连接非常可能就是刚刚释放的第一个事件对应的连接池中的连接；
        //     //c)又因为a中套接字50被释放了，所以会被操作系统拿来复用，复用给了b；
        //     //d)当处理第三个事件时，第三个事件其实是已经过期的，应该不处理，那怎么判断这第三个事件是过期的呢？ 【假设现在处理的是第三个事件，此时这个 连接池中的该连接 实际上已经被用作第二个事件对应的socket上了】；
        //         //依靠instance标志位能够解决这个问题，当调用ser_get_free_connection从连接池中获取一个新连接时，我们把instance标志位置反，所以这个条件如果不成立，说明这个连接已经被挪作他用了；
        //     SER_LOG(SER_LOG_DEBUG,0,"SerSocket::ser_epoll_process_events中遇到了instance != pConnection->mInstance的过期事件!");
        //     continue;           
        // }

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
            // SER_LOG_STDERR(0, "处理写事件.......");
            if(eventType & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) //客户端关闭，服务端还挂着可写通知
            {
                //EPOLLERR：对应的连接发生错误
                //EPOLLHUP：对应的连接被挂起
                //EPOLLRDHUP：表示TCP连接的远端关闭或者半关闭连接 
                //我们只有投递了 写事件，但对端断开时，程序流程才走到这里，投递了写事件意味着 iThrowsendCount标记肯定被+1了，这里我们减回去
                --pConnection->mThrowSendCount; 
            }
            else
            {
                SER_LOG(SER_LOG_DEBUG, 0, "epoll驱动发数据!");
                (this->*(pConnection->mWHandler))(pConnection);
            }
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
        // ev.data.ptr = (void*)pConnection;
        ev.events = events;
        pConnection->mEpollEvents = events; //连接对象上也记录一下这个事件
    }
    else if(eventType == EPOLL_CTL_MOD)
    {
        //修改红黑树中节点的事件
        ev.events = pConnection->mEpollEvents;
        if(supAction == 0) //增加
        {
            ev.events |= events;
        }
        else if(supAction == 1) //去掉
        {
            ev.events &= ~events;
        }
        else if(supAction == 2) //覆盖
        {
            ev.events = events;
        }

        pConnection->mEpollEvents = ev.events;
    }
    else //EPOLL_CTL_DEL
    {
        //目前没有这个需求
        return 1;
    }

    ev.data.ptr = (void*)pConnection; //修复bug,否则会崩溃，因为linux内核对EPOLL_CTL_MOD的实现方式
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

void SerSocket::ser_in_send_queue(char*& pSendBuffer)
{
    SER_LOG(SER_LOG_DEBUG, 0, "一个要发送的数据进入消息队列!");
    SerLock locker(&mSendQueueMutex);
    mMsgSendQueue.push_back(pSendBuffer);
    //sem_post将信号量加1，让卡在sem_wait的线程可以走下去
    if(sem_post(&mSendQueueSem) == -1)
    {
        SER_LOG(SER_LOG_STDERR, 0, "SerSocket::ser_in_send_queue()中sem_post()失败!");
    }

    return;
}

//发消息线程处理函数
void* SerSocket::ser_send_msg_queue_thread(void* pThreadData)
{
    //初始化一些变量
    auto pSendThread = static_cast<ThreadItem*>(pThreadData);
    auto pSocket = pSendThread->mThis;
    int err = 0;
    std::list<char*>::iterator iter, iterTemp, iterEnd;

    char* pMsgBuffer = nullptr;
    LPMSG_HEADER pMsgHeader = nullptr;
    LPPKG_HEADER pPkgHeader = nullptr;
    lpser_connection_t pConnection = nullptr;
    ssize_t sendedSize = 0;
    auto pMemory = SerMemory::GetInstance();

    //发数据
    while(g_stopEvent == 0) //程序不退出
    {
        //如果信号量值>0，则减1并走下去，否则卡这里卡着。
        //如果被某个信号中断，sem_wait也可能过早的返回，错误为EINTR；不算是错误
        //整个程序退出之前，也要sem_post()一下，确保如果本线程卡在sem_wait()，也能走下去从而让本线程成功返回
       if((sem_wait(&pSocket->mSendQueueSem) == -1) && (errno != EINTR))
       {
           SER_LOG(SER_LOG_STDERR, errno, "SerSocket::ser_send_msg_queue_thread()中sem_wait()失败!");
       }

        if(!pSocket->mMsgSendQueue.empty()) //发送消息队列不为空需要发送消息
        {
            err = pthread_mutex_lock(&pSocket->mSendQueueMutex);
            if(err != 0)
            {
                SER_LOG(SER_LOG_STDERR, err, "SerSocket::ser_send_msg_queue_thread()中pthread_mutex_lock失败！");
            }

            iter = pSocket->mMsgSendQueue.begin();
            iterEnd = pSocket->mMsgSendQueue.end();

            while(iter != iterEnd)
            {
                pMsgBuffer = *iter;
                pMsgHeader = (LPMSG_HEADER)pMsgBuffer;
                pPkgHeader = (LPPKG_HEADER)(pMsgBuffer + MSG_HEADER_LENGTH);
                pConnection = pMsgHeader->mConnection;

                //判断包是否过期,比如连接断开pConnection->mCurrsequence会加一
                if(pConnection->mCurrsequence != pMsgHeader->mCurrsequence)
                {
                    iterTemp = iter;
                    iter++;
                    pSocket->mMsgSendQueue.erase(iterTemp); //从队列中移走
                    pMemory->FreeMemory(pMsgBuffer); //释放内存
                    continue;
                }

                if(pConnection->mThrowSendCount > 0) //是否考epoll驱动来发消息
                {
                    //如果靠epoll来发送消息，则不走下面的流程
                    iter++;
                    continue;
                }

                pConnection->mSendPkgData = pMsgBuffer;
                iterTemp = iter;
                iter++;
                pSocket->mMsgSendQueue.erase(iterTemp); //移除要发送的数据
                pConnection->mSendLocation = (char*)pPkgHeader;
                pConnection->mSendLength = htons(pPkgHeader->mPkgLength);

                //我们采用 epoll水平触发的策略，能走到这里的，都应该是还没有投递 写事件 到epoll中
                //epoll水平触发发送数据的改进方案：
	            //开始不把socket写事件通知加入到epoll,当我需要写数据的时候，直接调用write/send发送数据；
	            //如果返回了EAGIN【发送缓冲区满了，需要等待可写事件才能继续往缓冲区里写数据】，此时，我再把写事件通知加入到epoll，
	            //此时，就变成了在epoll驱动下写数据，全部数据发送完毕后，再把写事件通知从epoll中干掉；
	            //优点：数据不多的时候，可以避免epoll的写事件的增加/删除，提高了程序的执行效率；

                //(1)直接调用write或者send发送数据
                SER_LOG(SER_LOG_DEBUG, 0, "即将发送数据大小为：%d", pConnection->mSendLength);
                sendedSize = pSocket->ser_send_pkg(pConnection->mSockFd, pConnection->mSendLocation, pConnection->mSendLength);
                if(sendedSize > 0)
                {
                    if(sendedSize == pConnection->mSendLength) //一次性成功发送完了数据
                    {
                        pMemory->FreeMemory(pConnection->mSendPkgData);
                        pConnection->mThrowSendCount = 0;
                        SER_LOG(SER_LOG_DEBUG, 0, "数据发送完毕");
                    }
                    else //数据没有发完，肯定是因为缓冲区满了，这是需要为epoll对象添加写事件，用内核驱动发数据
                    {
                        SER_LOG(SER_LOG_DEBUG, 0, "发送缓冲区满了，数据没有发完!此时靠epoll驱动发数据!");
                        pConnection->mSendLocation = pConnection->mSendLocation + sendedSize; //更新发送数据的位置
                        pConnection->mSendLength = pConnection->mSendLength - sendedSize; //更新发送数据大小
                        ++pConnection->mThrowSendCount; //标记需要通过epoll事件驱动来发送数据
                        if(pSocket->ser_epoll_oper_event(
                            pConnection->mSockFd,
                            EPOLL_CTL_MOD,
                            EPOLLOUT,
                            0,
                            pConnection
                        ) == -1)
                        {
                            SER_LOG(SER_LOG_STDERR, errno, "SerSocket::ser_send_msg_queue_thread()中ser_epoll_oper_event()失败!");
                        }
                    }
                    continue;
                }
                else if(sendedSize == 0)
                {
                    //发送0个字节，首先因为我发送的内容不是0个字节的；
                    //然后如果发送 缓冲区满则返回的应该是-1，而错误码应该是EAGAIN，所以我综合认为，这种情况我就把这个发送的包丢弃了【按对端关闭了socket处理】
                    pMemory->FreeMemory(pConnection->mSendPkgData);
                    pConnection->mThrowSendCount = 0;
                }
                else if(sendedSize == -1)
                {
                    //发送缓冲区满，一个字节都没有发出去
                    SER_LOG(SER_LOG_DEBUG, 0, "发送缓冲区满，一个字节都没有发出去!");
                    ++pConnection->mThrowSendCount;
                    if (pSocket->ser_epoll_oper_event(
                            pConnection->mSockFd,
                            EPOLL_CTL_MOD,
                            EPOLLOUT,
                            0,
                            pConnection) == -1)
                    {
                        SER_LOG(SER_LOG_STDERR, errno, "SerSocket::ser_send_msg_queue_thread()中ser_epoll_oper_event()失败!");
                    }
                }
                else //错误，释放内存
                {
                    SER_LOG(SER_LOG_DEBUG, 0, "发送数据错误!");
                    pMemory->FreeMemory(pConnection->mSendPkgData);
                    pConnection->mThrowSendCount = 0;
                }
            }

            err = pthread_mutex_unlock(&pSocket->mSendQueueMutex);
            if(err != 0)
            {
                SER_LOG(SER_LOG_STDERR, err, "SerSocket::ser_send_msg_queue_thread()中pthread_mutex_unlock失败！");
            }
        }
    }

    return (void*)0;
}