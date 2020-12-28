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
    bool RegisterHandler(
        lpser_connection_t& pConnection,
        LPMSG_HEADER const& pMsgHeader,
        char* const& pPkgBody,
        const unsigned short& pkgBodyLength);

    bool LoginHandler(
        lpser_connection_t& pConnection,
        LPMSG_HEADER const& pMsgHeader,
        char* const& pPkgBody,
        const unsigned short& pkgBodyLength);

 public:
    virtual void ser_thread_process_message(char* const& pPkgData);
};

#endif