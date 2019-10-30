/*
==============================
*Copyright(c): huan.liu, 2019
*All rights reversed

*File name: nginx_datamodel.h
*Brief: data model

*Data:2019.10.22
*Version: 1.0
==============================
*/

#ifndef __NGX_DATAMODEL_H__
#define __NGX_DATAMODEL_H__

typedef struct
{
	char ItemName[50];
	char ItemContent[500];
}ConfItem, *PConfItem;

//日志相关
typedef struct
{
	int log_level; //日志级别
	int fd; //日志文件描述符
}ngx_log_t;

extern char** g_os_argv;
extern char* gp_envmem;
extern int g_environlen;

#endif