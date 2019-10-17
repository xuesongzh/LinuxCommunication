/*
==============================
*Copyright(c): huan.liu, 2019
*All rights reversed

*File name: nginx_defs.h
*Brief: common macros

*Data:2019.10.02
*Version: 1.0
==============================
*/

#ifndef __NGINX_DEFS_H__
#define __NGINX_DEFS_H__

typedef struct
{
	char ItemName[50];
	char ItemContent[500];
}ConfItem, *PConfItem;

extern char** g_os_argv;
extern char* gp_envmem;
extern int g_environlen;

//class Singleton Implementation
#undef DECLARE_SINGLETON
#define DECLARE_SINGLETON(T)\
public:\
	static T* GetInstance();\
	


#endif