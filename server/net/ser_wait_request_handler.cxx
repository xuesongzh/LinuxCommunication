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