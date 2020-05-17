/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_function.h
*Brief: 一些辅助函数

*Data:2020.05.09
*Version: 1.0
=====================================*/

#ifndef __SER_FUNCTION_H__
#define __SER_FUNCTION_H__

#pragma region[字符串相关]

void Rtrim(char* const& string);

void Ltrim(char* const& string);

#pragma endregion

#pragma region[设置进程名称]

void MoveEnviron(char*& pNewEnviron);

//假设所有的命令行参数都不用才能调用次函数
void SetProcessTitle(const char* const& pTitle);

#pragma endregion

#pragma region[信号相关]

//用于注册信号处理函数
int ser_init_signals();

#pragma endregion

#endif