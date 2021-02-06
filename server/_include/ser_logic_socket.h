/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_logic_socket.h
*Brief: 用于业务逻辑

*Data:2020.06.13
*Version: 1.0
=====================================*/

#ifndef __SER_LOGIC_SOCKET_H__
#define __SER_LOGIC_SOCKET_H__

#include "ser_socket.h"

class SerLogicSocket : public SerSocket {
 public:
    SerLogicSocket();
    virtual ~SerLogicSocket();
    virtual bool Initialize();

 public:
    void SendNoBodyPkg(LPMSG_HEADER pMsgHeader, unsigned short msgCode);

    bool RegisterHandler(lpser_connection_t pConnection,
                         LPMSG_HEADER pMsgHeader,
                         char* pPkgBody,
                         unsigned short pkgBodyLength);

    bool LoginHandler(lpser_connection_t pConnection,
                      LPMSG_HEADER pMsgHeader,
                      char* pPkgBody,
                      unsigned short pkgBodyLength);

    bool PingHandler(lpser_connection_t pConnection,
                     LPMSG_HEADER pMsgHeader,
                     char* pPkgBody,
                     unsigned short pkgBodyLength);

    //检测心跳包是否超时，内存释放，子类应该实现具体的判断动作
    virtual void pingTimeOutChecking(LPMSG_HEADER pMsgHeader, time_t time);

 public:
    virtual void ser_thread_process_message(char* pPkgData);
};

#endif