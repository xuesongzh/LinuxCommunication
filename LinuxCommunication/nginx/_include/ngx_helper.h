/*
==============================
*Copyright(c): huan.liu, 2019
*All rights reversed

*File name: nginx_helper.h
*Brief: nginx function define

*Data:2019.10.24
*Version: 1.0
==============================
*/

#ifndef __NGX_HELPER_H__
#define __NGX_HELPER_H__

class NgxHelper
{
  public:
    //字符串处理相关
    static void Rtrim(char *string); //截取字符串尾部空格

    static void Ltrim(char *string); //截取字符串首部空格
};

#endif