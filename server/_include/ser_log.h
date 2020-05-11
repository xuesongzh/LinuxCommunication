/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_log.h
*Brief: 服务器日志相关函数

*Data:2020.05.12
*Version: 1.0
=====================================*/

#ifndef __SERVER_LOG_H__
#define __SERVER_LOG_H__

#include <stdint.h>




//打印错误码
void ser_log_errno(uint8_t*& pWrite, uint8_t*& pLast, const int& err);

#endif