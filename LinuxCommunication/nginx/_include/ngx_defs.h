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

/*---------------日志相关-----------------*/
#define NGX_LOG_PATH "logs/error1.log" //缺省log路径

#define NGX_MAX_ERROR_STR 2048 //错误信息最大长度

//日志等级：0级别最高，8级最低
#define NGX_LOG_STDERR      0 //严重错误，不输出到日志文件，直接打印到控制台或者屏幕上
#define NGX_LOG_EMERG       1 //紧急[meerg]
#define NGX_LOG_ALERT       2 //警戒[alert]
#define NGX_LOG_CRIT        3 //严重[crit]
#define NGX_LOG_ERR         4 //错误[error] ：常用
#define NGX_LOG_WARNING     5 //警告[warning]：常用
#define NGX_LOG_NOTICE      6 //注意[notice]
#define NGX_LOG_INFO        7 //信息[info]
#define NGX_LOG_DEBUG       8 //调试[debug]






/*---------------Common-----------------*/
//类似memcpy，但常规memcpy返回的是指向目标dst的指针，而此处返回的是目标【拷贝数据后】的终点位置，连续复制多段数据时方便
#define NGX_CPYMEM(dst, src, n)  (((u_char*)memcpy(dst, src, n)) + (n))




//delete pointer
#ifndef DEL_PTR
#define DEL_PTR(ptr)        \
	do                      \
	{                       \
		if (ptr != nullptr) \
		{                   \
			delete (ptr);   \
			ptr = nullptr;  \
		}                   \
	} while (false)
#endif

//class Singleton Implementation
#undef DECLARE_SINGLETON
#define DECLARE_SINGLETON(T)\
public:\
	static T* GetInstance();\
	


#endif