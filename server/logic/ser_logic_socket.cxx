#include<arpa/inet.h> //ntohl
#include<signal.h>

#include "ser_logic_socket.h"
#include "ser_crc32.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_datastruct.h"

//成员函数指针
typedef bool (SerLogicSocket::*handler)(
        lpser_connection_t const& pConnection,
        LPMSG_HEADER const& pMsgHeader,
        char* const& pPkgBody,
        const unsigned short& pkgBodyLength);

//下标直接对于消息头中的消息码
static const handler msgHandlers[] = 
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
#define AUTH_TOTAL_COMMANDS (sizeof(msgHandlers) / sizeof(handler)) //命令个数


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
void SerLogicSocket::ser_thread_process_message(char* const& pPkgData)
{
    LPMSG_HEADER pMsgHeader = (LPMSG_HEADER)pPkgData; //消息头
    LPPKG_HEADER pPkgHeader = (LPPKG_HEADER)(pPkgData + MSG_HEADER_LENGTH); //包头
    void* pPkgBody = nullptr; //包体
    unsigned short pkgLength = ntohs(pPkgHeader->mPkgLength); //包的长度（包头+包体）
    SER_LOG_STDERR(0, "处理消息队列时包的长度:%d",pkgLength);
    if(pkgLength == PKG_HEADER_LENGTH)
    {
        //没有包体的数据包crc32为0
        if(pPkgHeader->mCRC32 != 0)
        {
            return; //错误包，直接丢弃
        }
    }
    else
    {
        pPkgHeader->mCRC32 = ntohl(pPkgHeader->mCRC32);
        pPkgBody = (void*)(pPkgData + MSG_HEADER_LENGTH +PKG_HEADER_LENGTH);
        //校验CRC32值
        SER_LOG_STDERR(0, "包头长度：%d，包体长度：%d",PKG_HEADER_LENGTH, pkgLength - PKG_HEADER_LENGTH);
        int crc32 = SerCRC32::GetInstance()->Get_CRC((unsigned char*)pPkgBody, pkgLength - PKG_HEADER_LENGTH);
        SER_LOG_STDERR(0, "服务端的crc32值为:%d",crc32);
        if(crc32 != pPkgHeader->mCRC32)
        {
            //crc32校验不通过，直接丢弃
            SER_LOG_STDERR(0, "SerLogicSocket::ser_thread_process_message()中crc32校验不通过!");
            return;
        }
    }

    //CRC32校验通过，开始处理业务逻辑
    unsigned short int msgCode = ntohs(pPkgHeader->mMsgCode); //消息码
    lpser_connection_t pConnection = pMsgHeader->mConnection; //连接池对象
    //客服端已经断开连接，直接丢弃
    if(pConnection->mCurrsequence != pMsgHeader->mCurrsequence)
    {
        SER_LOG(SER_LOG_INFO, 0, "SerLogicSocket::ser_thread_process_message()中检测到客服端已经断开连接!");
        return;
    }

    //判断消息码是否正确
    if(msgCode > AUTH_TOTAL_COMMANDS)
    {
        //消息码不正确，直接丢弃
        SER_LOG_STDERR(0, "SerLogicSocket::ser_thread_process_message()中检测到消息码不正确!");
        return;
    }

    //消息码没有对应的处理函数
    if(msgHandlers[msgCode] == nullptr)
    {
        //消息码没有对应的处理函数
        SER_LOG_STDERR(0, "SerLogicSocket::ser_thread_process_message()中检测到消息码没有对应的处理函数!");
        return;
    }

    //调用对应的处理函数处理业务逻辑
    if(true != (this->*msgHandlers[msgCode])(pConnection, pMsgHeader,(char*)pPkgBody, pkgLength-PKG_HEADER_LENGTH))
    {
        SER_LOG_STDERR(0, "SerLogicSocket::ser_thread_process_message()消息码：%d处理失败!", msgCode);
    }

    return;
}



//业务逻辑-----------------------------
bool SerLogicSocket::RegisterHandler(
        lpser_connection_t const& pConnection,
        LPMSG_HEADER const& pMsgHeader,
        char* const& pPkgBody,
        const unsigned short& pkgBodyLength)
{
    SER_LOG_STDERR(0,"SerLogicSocket::RegisterHandler");
    return true;
}

bool SerLogicSocket::LoginHandler(
        lpser_connection_t const& pConnection,
        LPMSG_HEADER const& pMsgHeader,
        char* const& pPkgBody,
        const unsigned short& pkgBodyLength)
{
    SER_LOG_STDERR(0,"SerLogicSocket::LoginHandler");
    return true;
}