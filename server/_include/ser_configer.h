/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_configer.h
*Brief: 配置文件管理类

*Data:2020.05.07
*Version: 1.0
=====================================*/

#ifndef __SER_CONFIGER_H__
#define __SER_CONFIGER_H__

#include <mutex>
#include <vector>

#include "ser_datastruct.h"
#include "ser_macros.h"

class SerConfiger {
    DECLEAR_SINGLETON(SerConfiger)

 public:
    ~SerConfiger();

 public:
    bool Load(const char* pConfFileName);
    const char* GetString(const char* pItemName);
    int GetIntDefault(const char* pItemName, int def);

 private:
    std::vector<LPConfItem> mConfItemList;
};

#endif