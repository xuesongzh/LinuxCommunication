#include <string.h>
#include <stdio.h>
#include <stdarg.h> //va_list
#include <unistd.h> //STDERR_FILENO...

#include "ser_log.h"
#include "ser_macros.h"

//只用于本文件的函数声明
//打印错误码
static void ser_log_errno(
    uint8_t*& pWrite,
    uint8_t*& pLast, 
    const int& err);
//实现可变参数的打印，定制输出格式
static void ser_vslprintf(
    uint8_t*& pWrite, 
    uint8_t*& pLast, 
    const char*& pfmt,
    const va_list& args);

void ser_log_stderr(const int& errNum, const char* pfmt, ...)
{
    va_list args; //可变参数
    uint8_t errstr[SER_MAX_ERROR_STR + 1]; //日志信息
    uint8_t* pWrite = errstr; //指向可写的内存
    uint8_t* pLast = errstr + SER_MAX_ERROR_STR; //指向最后一个字符，用于判断是否超过日志最大长度
    memset(errstr, 0, sizeof(errstr));

    SER_MEMCPY(pWrite, "server: ", sizeof("server: "));

    //实现可变参数的打印，定制输出格式
    va_start(args, pfmt); //使args指向可变参数起始参数
    ser_vslprintf(pWrite, pLast, pfmt, args);
    va_end(args); //释放args

    //追加错误码对应的信息
    if (0 != errNum)
    {
        ser_log_errno(pWrite, pLast, errNum);
    }

    //如果最后位置不够，强制换行
    if (pWrite > pLast -1)
    {
        pWrite = pLast - 1; //pLast - 1是可写的最后一个有效字符
    }
    *pWrite++ = '\n'; //插入换行符

    //往屏幕(STDERR_FILENO)输出错误信息
    write(STDERR_FILENO, errstr, pWrite - errstr);

    return;
}


static void ser_vslprintf(
    uint8_t*& pWrite, 
    uint8_t*& pLast, 
    const char*& pfmt,
    const va_list& args)
{
    while(*pfmt && pWrite < pLast)
    {
        if (*pfmt == '%') //碰到需要转换的格式
        {

        }
        else //纯字符串
        {
            *pWrite++ = *pfmt++; //*与++优先级相同，从右往左运算
        }
    }
}


static void ser_log_errno(
    uint8_t*& pWrite, 
    uint8_t*& pLast, 
    const int& err)
{
    //错误码信息
    char* pErrorInfo = strerror(err); //这个不会反回nullptr
    size_t errorLen = strlen(pErrorInfo);

    //定义一些格式信息
    char leftStr[10] = {0};
    sprintf(leftStr, "(%d: ", err);
    size_t leftLen = strlen(leftStr);

    char rightStr[] = ") ";
    size_t rightLen = strlen(rightStr);

    //向buffer写入信息：如果长度不够就不写，直接舍弃
    if ((pWrite + errorLen + leftLen + rightLen) < pLast)
    {
        SER_MEMCPY(pWrite, leftStr, leftLen);
        SER_MEMCPY(pWrite, pErrorInfo, errorLen);
        SER_MEMCPY(pWrite, rightStr, rightLen);
    }
}