#include <string.h>
#include <errno.h>

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"


void SerSocket::ser_wait_request_handler(lpser_connection_t tcpConnection)
{
    // unsigned char buf[10];
    // memset(buf, 0, sizeof(buf));
    // //ET测试代码
    // do
    // {
    //     int n = recv(tcpConnection->mSockFd, buf, 2, 0);
    //     if(n == -1 && errno == EAGAIN)
    //     {
    //         break;
    //     }
    //     else if(n == 0)
    //     {
    //         break;
    //     }

    //     SER_LOG_STDERR(0, "收到的数据为:%s", buf);
    // } while (true);

    // //LT测试代码
    // int n = recv(tcpConnection->mSockFd, buf, 2, 0);
    // if(n == 0)
    // {
    //     ser_close_accepted_connection(tcpConnection);
    // }
    // SER_LOG_STDERR(0, "收到的数据为:%s", buf);

    return;
}