/*
==============================
*Copyright(c): huan.liu, 2019
*All rights reversed

*File name: nginx_logger.h
*Brief: nginx function about logger

*Data:2019.10.30
*Version: 1.0
==============================
*/

#ifndef __NGX_LOGGER_H__
#define __NGX_LOGGER_H__

void ngx_log_init();
void ngx_log_stderr(int err, const char* fmt, ...);
void ngx_log_error_core(int level, int err, const char* fmt, ...);

u_char* ngx_log_errno(u_char* buf, u_char* last, int err);
u_char* ngx_slprintf(u_char* buf, u_char* last, const char* fmt, ...);
u_char* ngx_vslprintf(u_char* buf, u_char* last, const char* fmt, va_list args);


#endif