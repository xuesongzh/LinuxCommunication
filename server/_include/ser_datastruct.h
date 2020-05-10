/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_datastruct.h
*Brief: 一些结构体和全局变量声明

*Data:2020.05.08
*Version: 1.0
=====================================*/

#ifndef __SER_DATASTRUCT_H__
#define __SER_DATASTRUCT_H__

//配置文件
typedef struct
{
    char mName[50];
    char mContent[500];
}ConfItem, *LPConfItem;


/*-------------全局变量-------------*/
//设置进程标题相关
extern char ** pArgv; //命令行参数
extern char* pNewEnviron; //指向新的环境变量内存
extern int EnvironLength; //环境变量的长度

#endif