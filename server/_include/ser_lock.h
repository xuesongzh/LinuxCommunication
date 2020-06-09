/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_lock.h
*Brief: 互斥量锁，用于自动释放互斥量

*Data:2020.06.10
*Version: 1.0
=====================================*/

#ifndef __SER_LOCK_H__
#define __SER_LOCK_H__

#include <pthread.h>

//自动释放互斥量，防止unlock的情况发生
class SerLock
{
public:
    SerLock(pthread_mutex_t* pMutex) : mMutex(pMutex)
    {
        pthread_mutex_lock(mMutex); //加锁互斥量
    }

    ~SerLock()
    {
        pthread_mutex_unlock(mMutex);
    }

private:
    pthread_mutex_t* mMutex;
};

#endif