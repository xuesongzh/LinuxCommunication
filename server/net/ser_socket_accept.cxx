/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*Brief: 监听套接字读事件处理

*Data:2020.05.29
*Version: 1.0
=====================================*/

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"

void SerSocket::ser_event_accept(lpser_connection_t listenConnection)
{
    SER_LOG_STDERR(0,"222222");
    return;
 }