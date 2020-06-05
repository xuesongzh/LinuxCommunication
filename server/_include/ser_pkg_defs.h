/*===================================
*Copyright(c): huan.liu, 2020
*All rights reversed

*File name: ser_pkg_defs.h
*Brief: 通信数据包相关定义

*Data:2020.06.04
*Version: 1.0
=====================================*/

#ifndef __SER_PKG_DEFS_H__
#define __SER_PKG_DEFS_H__

//数据包最大长度[包头+包体]，为了留出一些空间实际上编码的时候应该是30000-1000 = 29000个字节
#define PKG_MAX_LENGTH 30000 

//收包状态
enum PkgState
{
    PKG_HD_INIT = 0, //初始状态，准备收数据包头
    PKG_HD_RECVING, //收包头不完整，收包头中
    PKG_BD_INIT, //包头接受完毕，准备收包体
    PKG_BD_RECVING, //包体接受不完整，接收包体中
};

#pragma pack(1) //网络中传输的数据要使用1字节对齐的方式

typedef struct PKG_HEADER
{
    unsigned short mPkgLength; //包头加包体的长度，unsigned short能表示的大小超过30000，可行
    unsigned short mMsgCode; //消息码，区别不同的命令
    int mCRC32; //crc32校验，数据校验保证数据正确性

}PKG_HEADER, *LPPKG_HEADER;

#pragma pack()

#define PKG_HEAD_INFO_SIZE (sizeof(struct PKG_HEADER) + 10) //接受包体内存大小，一定要比包头的结构所占内存大

#endif