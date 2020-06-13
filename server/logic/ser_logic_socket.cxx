#include "ser_logic_socket.h"

//成员函数指针
typedef bool (SerLogicSocket::*handler)(
        lpser_connection_t const& pConnection,
        LPMSG_HEADER const& pMsgHeader,
        char* const& pPkgBody,
        const unsigned short& pkgBodyLength);

//下标直接对于消息头中的消息码
static const handler msgHandler[] = 
{
    //保留消息处理，用于增加一些基本的功能
    nullptr, //[0]
    nullptr, //[1]
    nullptr, //[2]
    nullptr, //[3]
    nullptr, //[4]
    //业务逻辑处理
    &SerLogicSocket::RegisterHandler, //[5]注册
    &SerLogicSocket::LoginHandler, //[6]登录
};
#define AUTH_TOTAL_COMMANDS (sizeof(msgHandler) / sizeof(handler)) //命令个数


SerLogicSocket::SerLogicSocket()
{

}

SerLogicSocket::~SerLogicSocket()
{

}

bool SerLogicSocket::Initialize()
{
    return SerSocket::Initialize();
}

//处理收到的消息包：
void SerLogicSocket::ser_thread_process_message(char* const& pPkgDat)
{

}



//业务逻辑-----------------------------
bool SerLogicSocket::RegisterHandler(
        lpser_connection_t const& pConnection,
        LPMSG_HEADER const& pMsgHeader,
        char* const& pPkgBody,
        const unsigned short& pkgBodyLength)
{
    return true;
}

bool SerLogicSocket::LoginHandler(
        lpser_connection_t const& pConnection,
        LPMSG_HEADER const& pMsgHeader,
        char* const& pPkgBody,
        const unsigned short& pkgBodyLength)
{
    return true;
}