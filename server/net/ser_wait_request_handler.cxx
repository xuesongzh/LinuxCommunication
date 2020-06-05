#include <string.h>
#include <errno.h>
#include <signal.h>

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_pkg_defs.h"
#include "ser_log.h"
#include "ser_datastruct.h"

#define PKG_HEADER_LENGTH (sizeof(struct PKG_HEADER)) //包头长度
#define MSG_HEADER_LENGTH (sizeof(struct PKG_HEADER)) //消息头长度

void SerSocket::ser_wait_request_handler(lpser_connection_t tcpConnection)
{

    return;
}

//接收数据函数
//pConnection:连接池对象
//pBuffer:接收数据写入的位置
//bufferLength:接收数据字节数
//返回值：-1：有问题，>0正常返回实际接收的字节数
ssize_t SerSocket::ser_recv_pkg(lpser_connection_t const& pConnection, char* const& pBuffer, const ssize_t& bufferLength)
{
    ssize_t recvPkgLength;

    //最后一个参数，一般为0
    recvPkgLength = recv(pConnection->mSockFd, pBuffer, bufferLength, 0);

    if(0 == recvPkgLength)
    {
        SER_LOG(SER_LOG_INFO, 0, "客户端关闭，四次挥手结束，正常关闭!");
        ser_close_connection(pConnection);
        return -1;
    }

    if(recvPkgLength < 0) //有错误发生
    {
        //EAGAIN和EWOULDBLOCK[【这个应该常用在hp上】应该是一样的值，表示没收到数据，一般来讲，在ET模式下会出现这个错误，因为ET模式下是不停的recv肯定有一个时刻收到这个errno，但LT模式下一般是来事件才收，所以不该出现这个返回值
        if(EAGAIN == errno || EWOULDBLOCK == errno)
        {
            SER_LOG_STDERR(errno, "SerSocket::ser_recv_pkg()中EAGAIN == errno || EWOULDBLOCK == errno成立!");
            return -1; //不认为是错误，不释放连接池对象
        }

        if(EINTR == errno) //来信号了
        {
            SER_LOG_STDERR(errno, "SerSocket::ser_recv_pkg()中EINTR === errno成立!");
            return -1; //不认为是错误，不释放连接池对象       
        }

        //走到这里的错误都认为是异常，需要释放连接池

        if(ECONNRESET == errno)
        {
            //如果客户端没有正常关闭socket连接，却关闭了整个运行程序,应该是直接给服务器发送rst包而不是4次挥手包完成连接断开,那么会产生这个错误            
            //以后增加代码完善
        }
        else
        {
            SER_LOG_STDERR(errno, "SerSocket::ser_recv_pkg()中发生错误!");
        }

        ser_close_connection(pConnection); //关闭套接字和连接此对象
        return -1;
    }

    return recvPkgLength;
}