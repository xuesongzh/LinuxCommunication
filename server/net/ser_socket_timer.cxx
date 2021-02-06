#include <errno.h>   //errno
#include <fcntl.h>   //open
#include <stdarg.h>  //va_start....
#include <stdint.h>  //uintptr_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>  //gettimeofday
#include <time.h>      //localtime_r
#include <unistd.h>    //STDERR_FILENO等
//#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>  //ioctl

#include "ser_configer.h"
#include "ser_lock.h"
#include "ser_log.h"
#include "ser_macros.h"
#include "ser_memory.h"
#include "ser_socket.h"

//设置踢出时钟(向multimap表中增加内容)，用户三次握手成功连入，然后我们开启了踢人开关【WaitTimeEnable = 1】，那么本函数被调用；
void SerSocket::addToTimerQueue(lpser_connection_t pConnection) {
    auto pMemory = SerMemory::GetInstance();

    time_t futtime = time(NULL);
    futtime += mWaitTime;  //20秒之后的时间

    SerLock locker(&mTimerQueueMutex);
    LPMSG_HEADER tmpMsgHeader = (LPMSG_HEADER)pMemory->AllocMemory(MSG_HEADER_LENGTH, false);
    tmpMsgHeader->mConnection = pConnection;
    tmpMsgHeader->mCurrsequence = pConnection->mCurrsequence;
    mTimerQueuemap.insert(std::make_pair(futtime, tmpMsgHeader));  //按键 自动排序 小->大
    mTimerQueueSize++;                                             //计时队列尺寸+1
    mTimerValue = getEarliestTime();                               //计时队列头部时间值
    return;
}

//从multimap中取得最早的时间返回去，调用者负责互斥，所以本函数不用互斥
time_t SerSocket::getEarliestTime() {
    std::multimap<time_t, LPMSG_HEADER>::iterator pos;
    pos = mTimerQueuemap.begin();
    return pos->first;
}

LPMSG_HEADER SerSocket::removeFirstTimer() {
    std::multimap<time_t, LPMSG_HEADER>::iterator pos;
    LPMSG_HEADER p_tmp;
    if (mTimerQueueSize <= 0) {
        return NULL;
    }
    pos = mTimerQueuemap.begin();  //调用者负责互斥的，这里直接操作没问题的
    p_tmp = pos->second;
    mTimerQueuemap.erase(pos);
    --mTimerQueueSize;
    return p_tmp;
}

LPMSG_HEADER SerSocket::GetOverTimeTimer(time_t cur_time) {
    SerMemory *pMemory = SerMemory::GetInstance();
    LPMSG_HEADER ptmp;

    if (mTimerQueueSize == 0 || mTimerQueuemap.empty())
        return NULL;  //队列为空

    time_t earliesttime = GetEarliestTime();  //到multimap中去查询
    if (earliesttime <= cur_time) {
        //这回确实是有到时间的了【超时的节点】
        ptmp = RemoveFirstTimer();  //把这个超时的节点从 mTimerQueuemap 删掉，并把这个节点的第二项返回来；

        if (/*m_ifkickTimeCount == 1 && */ m_ifTimeOutKick != 1)  //能调用到本函数第一个条件肯定成立，所以第一个条件加不加无所谓，主要是第二个条件
        {
            //如果不是要求超时就提出，则才做这里的事：

            //因为下次超时的时间我们也依然要判断，所以还要把这个节点加回来
            time_t newinqueutime = cur_time + (mWaitTime);
            LPMSG_HEADER tmpMsgHeader = (LPMSG_HEADER)pMemory->AllocMemory(sizeof(STRUC_MSG_HEADER), false);
            tmpMsgHeader->pConnection = ptmp->pConnection;
            tmpMsgHeader->iCurrsequence = ptmp->iCurrsequence;
            mTimerQueuemap.insert(std::make_pair(newinqueutime, tmpMsgHeader));  //自动排序 小->大
            mTimerQueueSize++;
        }

        if (mTimerQueueSize > 0)  //这个判断条件必要，因为以后我们可能在这里扩充别的代码
        {
            mTimerValue = GetEarliestTime();  //计时队列头部时间值保存到mTimerValue里
        }
        return ptmp;
    }
    return NULL;
}

void SerSocket::DeleteFromTimerQueue(lpser_connection_t pConnection) {
    std::multimap<time_t, LPMSG_HEADER>::iterator pos, posend;
    SerMemory *pMemory = SerMemory::GetInstance();

    CLock lock(&m_timequeueMutex);

    //因为实际情况可能比较复杂，将来可能还扩充代码等等，所以如下我们遍历整个队列找 一圈，而不是找到一次就拉倒，以免出现什么遗漏
lblMTQM:
    pos = mTimerQueuemap.begin();
    posend = mTimerQueuemap.end();
    for (; pos != posend; ++pos) {
        if (pos->second->pConnection == pConnection) {
            pMemory->FreeMemory(pos->second);  //释放内存
            mTimerQueuemap.erase(pos);
            --mTimerQueueSize;  //减去一个元素，必然要把尺寸减少1个;
            goto lblMTQM;
        }
    }
    if (mTimerQueueSize > 0) {
        mTimerValue = GetEarliestTime();
    }
    return;
}

//清理时间队列中所有内容
void SerSocket::clearAllFromTimerQueue() {
    std::multimap<time_t, LPMSG_HEADER>::iterator pos, posend;

    SerMemory *pMemory = SerMemory::GetInstance();
    pos = mTimerQueuemap.begin();
    posend = mTimerQueuemap.end();
    for (; pos != posend; ++pos) {
        pMemory->FreeMemory(pos->second);
        --mTimerQueueSize;
    }
    mTimerQueuemap.clear();
}

//时间队列监视和处理线程，处理到期不发心跳包的用户踢出的线程
void *SerSocket::ser_timer_queue_monitor_thread(void *pThreadData) {
    ThreadItem *pThread = static_cast<ThreadItem *>(pThreadData);
    SerSocket *pSocketObj = pThread->mThis;

    time_t absolute_time, cur_time;
    int err;

    while (g_stopEvent == 0)  //不退出
    {
        //这里没互斥判断，所以只是个初级判断，目的至少是队列为空时避免系统损耗
        if (pSocketObj->mTimerQueueSize > 0)  //队列不为空，有内容
        {
            //时间队列中最近发生事情的时间放到 absolute_time里；
            absolute_time = pSocketObj->mTimerValue;  //这个可是省了个互斥，十分划算
            cur_time = time(NULL);
            if (absolute_time < cur_time) {
                //时间到了，可以处理了
                std::list<LPMSG_HEADER> m_lsIdleList;  //保存要处理的内容
                LPMSG_HEADER result;

                err = pthread_mutex_lock(&pSocketObj->m_timequeueMutex);
                if (err != 0) ngx_log_stderr(err, "SerSocket::ServerTimerQueueMonitorThread()中pthread_mutex_lock()失败，返回的错误码为%d!", err);  //有问题，要及时报告
                while ((result = pSocketObj->GetOverTimeTimer(cur_time)) != NULL)                                                                   //一次性的把所有超时节点都拿过来
                {
                    m_lsIdleList.push_back(result);
                }  //end while
                err = pthread_mutex_unlock(&pSocketObj->m_timequeueMutex);
                if (err != 0) ngx_log_stderr(err, "SerSocket::ServerTimerQueueMonitorThread()pthread_mutex_unlock()失败，返回的错误码为%d!", err);  //有问题，要及时报告
                LPMSG_HEADER tmpmsg;
                while (!m_lsIdleList.empty()) {
                    tmpmsg = m_lsIdleList.front();
                    m_lsIdleList.pop_front();
                    pSocketObj->procPingTimeOutChecking(tmpmsg, cur_time);  //这里需要检查心跳超时问题
                }                                                           //end while(!m_lsIdleList.empty())
            }
        }  //end if(pSocketObj->mTimerQueueSize > 0)

        usleep(500 * 1000);  //为简化问题，我们直接每次休息500毫秒
    }                        //end while

    return (void *)0;
}

//心跳包检测时间到，该去检测心跳包是否超时的事宜，本函数只是把内存释放，子类应该重新事先该函数以实现具体的判断动作
void SerSocket::procPingTimeOutChecking(LPMSG_HEADER tmpmsg, time_t cur_time) {
    SerMemory *pMemory = SerMemory::GetInstance();
    pMemory->FreeMemory(tmpmsg);
}
