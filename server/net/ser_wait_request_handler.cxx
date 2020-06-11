#include <string.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h> //ntohs
#include <unistd.h>

#include "ser_socket.h"
#include "ser_macros.h"
#include "ser_log.h"
#include "ser_pkg_defs.h"
#include "ser_log.h"
#include "ser_datastruct.h"
#include "ser_memory.h"
#include "ser_lock.h"

#define PKG_HEADER_LENGTH (sizeof(struct PKG_HEADER)) //包头长度
#define MSG_HEADER_LENGTH (sizeof(struct PKG_HEADER)) //消息头长度

void SerSocket::ser_wait_request_handler(lpser_connection_t tcpConnection)
{
    ssize_t recvLength = ser_recv_pkg(tcpConnection, tcpConnection->mRecvLocation, tcpConnection->mRecvLength);
    if(recvLength <= 0)
    {
        //日志已经在ser_recv_pkg处理，这里直接返回
        return;
    }
    //走到这里说明成功接收到一些字节
    if(tcpConnection->mRecvStat == PKG_HD_INIT)
    {
        if(recvLength == tcpConnection->mRecvLength)//正好收完包体
        {
            ser_wait_request_process_pkg(tcpConnection); //处理数据
        }
        else //没有收完，继续收包
        {
            tcpConnection->mRecvStat = PKG_HD_RECVING;
            tcpConnection->mRecvLocation +=  recvLength;
            tcpConnection->mRecvLength -= recvLength;
        }
    }
    else if(tcpConnection->mRecvStat == PKG_HD_RECVING)
    {
        if(recvLength == tcpConnection->mRecvLength)//正好收完包体
        {
            ser_wait_request_process_pkg(tcpConnection); //处理数据
        }
        else //没有收完，继续收包
        {
            tcpConnection->mRecvLocation +=  recvLength;
            tcpConnection->mRecvLength -= recvLength;
        }       
    }
    else if(tcpConnection->mRecvStat == PKG_BD_INIT) //收包体
    {
        if(recvLength == tcpConnection->mRecvLength)
        {
            ser_wait_request_in_msgqueue(tcpConnection); //收完包体如消息队列
        }
        else
        {
            tcpConnection->mRecvStat = PKG_BD_RECVING;
            tcpConnection->mRecvLocation +=  recvLength;
            tcpConnection->mRecvLength -= recvLength; 
        }
    }
    else if(tcpConnection->mRecvStat == PKG_BD_INIT) //包体没有收完继续收包体
    {
        if(recvLength == tcpConnection->mRecvLength)
        {
            ser_wait_request_in_msgqueue(tcpConnection); //收完包体如消息队列
        }
        else
        {
            tcpConnection->mRecvLocation +=  recvLength;
            tcpConnection->mRecvLength -= recvLength; 
        }
    }

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

void SerSocket::ser_wait_request_process_pkg(lpser_connection_t const& pConnection)
{
    auto pMemory = SerMemory::GetInstance();

    auto pPkgHeader = (LPPKG_HEADER)pConnection->mPkgHeadInfo; //包头所在位置
    unsigned int pkgLength = ntohs(pPkgHeader->mPkgLength); //整个包的长度，网络序转主机序
    //恶意包判断
    if(pkgLength < PKG_HEADER_LENGTH || pkgLength > (PKG_MAX_LENGTH - 1000))
    {
        //包太小或者太大都被判断为恶意包，复原接收数据的状态和位置
        pConnection->mRecvStat = PKG_HD_INIT;
        pConnection->mRecvLocation = pConnection->mPkgHeadInfo;
        pConnection->mRecvLength = PKG_HEADER_LENGTH;
    }
    else
    {
        //合法包，继续处理
        //分配内存，放消息头，包体，以及将包头信息拷贝到这块新的内存中来
        auto pNewBuffer = (char*)pMemory->MallocMemory(MSG_HEADER_LENGTH + pkgLength);
        pConnection->mIfNewRecvMem  = true; //分配了新的内存
        pConnection->mPkgData = pNewBuffer;
        //1)填写消息头
        auto pMsgHeader = (LPMSG_HEADER)pNewBuffer;
        pMsgHeader->mConnection = pConnection;
        pMsgHeader->mCurrsequence = pConnection->mCurrsequence;
        //2)填写包头内容
        pNewBuffer += MSG_HEADER_LENGTH;
        memcpy(pNewBuffer, pConnection->mPkgHeadInfo, PKG_HEADER_LENGTH); //拷贝包头信息内容到新内存
        if(pkgLength == PKG_HEADER_LENGTH)
        {
            //没有包体信息，直接入消息队列
            ser_wait_request_in_msgqueue(pConnection);
        }
        else
        {
            //开始收包体
            pConnection->mRecvStat = PKG_BD_INIT;
            pConnection->mRecvLocation = pNewBuffer + PKG_HEADER_LENGTH; //之前已经+MSG_HEADER_LENGTH
            pConnection->mRecvLength = pkgLength - PKG_HEADER_LENGTH; //更新包体长度
        }
    }


}

void SerSocket::ser_wait_request_in_msgqueue(lpser_connection_t const& pConnection)
{
    //如消息队列
    ser_in_msgqueue(pConnection->mPkgData);

    //设置一些状态
    pConnection->mIfNewRecvMem = false; //放入消息队列后不释放，由之后的业务逻辑释放
    pConnection->mPkgData = nullptr;
    pConnection->mRecvStat = PKG_HD_INIT; //回到收包头状态
    pConnection->mRecvLocation = pConnection->mPkgHeadInfo; //收包位置
    pConnection->mRecvLength = PKG_HEADER_LENGTH; //设置收包长度
}

void SerSocket::ser_in_msgqueue(char* const& pBuffer)
{
    //加入消息队列
    mMsgRecvQueue.push_back(pBuffer);

    //防止消息队列过大
    // ser_temp_out_msgqueue();

    SER_LOG_STDERR(0, "收到一个完整的数据包!");
}

char* SerSocket::ser_get_one_message()
{
    SerLock locker(&mMsgQueueMutex);
    if(mMsgRecvQueue.empty())
    {
        return nullptr;
    }
    char* pTemp = mMsgRecvQueue.front();
    mMsgRecvQueue.pop_front();
    return pTemp;
}

//线程处理函数，处理业务逻辑。pPkgData：消息头+包头+包体
void SerSocket::ser_thread_process_message(char* const& pPkgData)
{
    pthread_t tid = pthread_self();
    SER_LOG_STDERR(0,"执行开始---begin,tid=%ui!",tid);
    sleep(5); //临时测试代码
    SER_LOG_STDERR(0,"执行结束---end,tid=%ui!",tid);

    return;
}

// void SerSocket::ser_temp_out_msgqueue()
// {
//     int size = mMsgRecvQueue.size();
//     if(size >= 1000) //消息超过1000条就处理
//     {
//         auto pMemory = SerMemory::GetInstance();
//         int outSize = size - 500; //干掉500条
//         while(outSize--)
//         {
//             auto pBuffer = mMsgRecvQueue.front();
//             mMsgRecvQueue.pop_front();
//             pMemory->FreeMemory(pBuffer);
//         }
//     }

//     return;   
// }