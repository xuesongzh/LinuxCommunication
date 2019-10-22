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

extern char** g_os_argv;
extern char* gp_envmem;
extern int g_environlen;

#endif