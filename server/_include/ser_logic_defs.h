/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_logic_defs.h
*Brief: 业务逻辑相关定义

*Data:2020.06.13
*Version: 1.0
=====================================*/

#ifndef __SER_LOGIC_DEFS_H__
#define __SER_LOGIC_DEFS_H__

//业务逻辑命令宏定义
#define CMD_START 0
#define CMD_PING CMD_START + 0  //ping命令【心跳包】
#define CMD_REGISTER CMD_START + 5
#define CMD_LOGIN CMD_START + 6

//结构体定义，在网络上传输的包需要一字节对齐

#pragma pack(1)

//注册
typedef struct STRUCT_REGISTER {
    int mType;
    char mUserName[56];
    char mPassWord[40];

} STRUCT_REGISTER, *LPSTRUCT_REGISTER;

//登录
typedef struct STRUCT_LOGIN {
    char mUserName[56];
    char mPassWord[40];

} STRUCT_LOGIN, *LPSTRUCT_LOGIN;

#pragma pack()

#endif