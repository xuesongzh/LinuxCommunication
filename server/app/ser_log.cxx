#include <string.h>
#include <stdio.h>

#include "ser_log.h"
#include "ser_macros.h"









void ser_log_errno(uint8_t*& pWrite, uint8_t*& pLast, const int& err)
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