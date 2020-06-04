/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*Brief: 监听套接字读事件处理

*Data:2020.05.29
*Version: 1.0
=====================================*/

#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_datastruct.h"

//三路握手，建立TCP连接用
void SerSocket::ser_event_accept(lpser_connection_t listenConnection)
{
    struct sockaddr clientSockaddr; //客户端地址
    socklen_t sockLen = sizeof(clientSockaddr);
    int err;
    int logLevel;
    int tcpSockFd; //从已连接队列中取出的与客户端通信的套接字描述符
    static int useAccept4 = 1; //支持accept4()的使用;
    lpser_connection_t pTCPConnection; //连接池中的连接，用于和TCP连接做绑定

    do
    {
        if(useAccept4)
        {
            //因为监听套接字是非阻塞的。所以即便是已完成队列为空，也不会阻塞在这里
            //SOCK_NONBLOCK：返回的套接字设为非阻塞，不用主动调用ioctl
            tcpSockFd = accept4(listenConnection->mSockFd, &clientSockaddr, &sockLen, SOCK_NONBLOCK);
        }
        else
        {
            //因为监听套接字是非阻塞的。所以即便是已完成队列为空，也不会阻塞在这里
            tcpSockFd = accept(listenConnection->mSockFd, &clientSockaddr, &sockLen);
        }

        //在linux2.6内核上，accept系统调用已经不存在惊群了。这样，当新连接过来时，仅有一个子进程返回新建的连接，其他子进程继续休眠在accept调用上，没有被唤醒。

        if(tcpSockFd == -1)
        {
            err = errno;
            //事件未发生时errno通常被设置成EAGAIN（意为“再来一次”）或者EWOULDBLOCK（意为“期待阻塞”）
            if(err == EAGAIN) //accept没有准备好
            {
                return;
            }
            logLevel = SER_LOG_ALERT;
            //ECONNRESET错误则发生在对方意外关闭套接字后【主机中的软件放弃了一个已建立的连接--由于超时或者其它失败而中止接连(用户插拔网线就可能有这个错误出现)】
            if(err == ECONNABORTED)
            {
                //该错误被描述为“software caused connection abort”，即“软件引起的连接中止”。原因在于当服务和客户进程在完成用于 TCP 连接的“三次握手”后，
                //客户 TCP 却发送了一个 RST （复位）分节，在服务进程看来，就在该连接已由 TCP 排队，等着服务进程调用 accept 的时候 RST 却到达了。
                //POSIX 规定此时的 errno 值必须 ECONNABORTED。源自 Berkeley 的实现完全在内核中处理中止的连接，服务进程将永远不知道该中止的发生。
                //服务器进程忽略该错误，直接再次调用accept。
                logLevel = SER_LOG_ERR;
            }
            else if(err == EMFILE || err == ENFILE)
            {
                //EMFILE:进程的fd已用尽【已达到系统所允许单一进程所能打开的文件/套接字总数】。https://blog.csdn.net/sdn_prc/article/details/28661661以及 https://bbs.csdn.net/topics/390592927
                //ulimit -n ,看看文件描述符限制,如果是1024的话，需要改大;  打开的文件句柄数过多 ,把系统的fd软限制和硬限制都抬高.
                //ENFILE这个errno的存在，表明一定存在system-wide的resource limits，而不仅仅有process-specific的resource limits。按照常识，process-specific的resource limits，一定受限于system-wide的resource limits。
                logLevel = SER_LOG_CRIT;
            }
            SER_LOG(logLevel, errno, "SerSocket::ser_event_accept中accept或者accept4返回套接字错误!");

            if(useAccept4 && err == ENOSYS) //不支持accept4
            {
                useAccept4 = 0; //不使用accept4
                continue;
            }

            if(err == EMFILE || err == ENFILE)
            {
                //暂时不处理
            }

            return;
        }

        pTCPConnection = ser_get_free_connection(tcpSockFd); //取出一个空闲连接给TCP套接字用
        if(nullptr == pTCPConnection) //连接池空闲连接不够用
        {
            if(close(tcpSockFd) == -1)
            {
                SER_LOG(SER_LOG_ALERT, errno, "SerSocket::ser_event_accept中close tcp fd失败!");
            }

            return;
        }

        //判断连接是否超过最大连接数

        //拷贝客户端地址到连接对象
        memcpy(&(pTCPConnection->mSockAddr), &clientSockaddr, sockLen);
        if(!useAccept4) //如果accept返回的套接字，需要设置为 非阻塞
        {
            if(ser_set_nonblocking(tcpSockFd) == false)
            {
                ser_close_connection(pTCPConnection);//回收连接池中的连接，并关闭socket
            }
        }

        pTCPConnection->mListening = listenConnection->mListening;
        pTCPConnection->mWReady = 1;
        pTCPConnection->mRHandler = &SerSocket::ser_wait_request_handler;

        //将TCP连接以及对应的事件加入到epoll对象
        if(ser_epoll_add_event(tcpSockFd, 1, 0, 0, EPOLL_CTL_ADD, pTCPConnection) == -1)
        {
            //增加事件失败，回收连接池及关闭套接字
            ser_close_connection(pTCPConnection);
            return;
        }

        break; //循环一次就跳出去
    }while(true);

    return;
 }