#include <string.h>
#include <errno.h>

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_pkg_defs.h"

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

    }
}