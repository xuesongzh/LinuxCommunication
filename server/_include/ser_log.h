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

//打印日志 输出到屏幕
void ser_log_stderr(const int& errNum, const char* pfmt, ...);

//初始化日志文件，打开日志文件
void ser_log_init();

//打印日志 输出到log文件
void ser_log_error_core(const int& logLevel, const int& errNum, const char* pfmt, ...);

#endif