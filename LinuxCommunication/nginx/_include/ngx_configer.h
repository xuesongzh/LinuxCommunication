/*
==============================
*Copyright(c): huan.liu, 2019
*All rights reversed

*File name: ngx_configer.h
*Brief: class Configer 用于读取配置文件, 单例类

*Data:2019.10.16
*Version: 1.0
==============================
*/

#ifndef __NGX_CONFIGER_H__
#define __NGX_CONFIGER_H__

class Configer
{

public:
	bool Load(const char* pConfName); //加载配置文件
};

#endif 
