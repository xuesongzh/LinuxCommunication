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
//打印数字
static void ser_sprinf_number(
    uint8_t*& pWrite,
    uint8_t* pLast,
    uint64_t& uint64,
    const uint8_t& charZero,
    const uint32_t& hex,
    const uint32_t& width);

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

static void ser_sprinf_number(
    uint8_t*& pWrite,
    uint8_t* pLast,
    uint64_t& uint64,
    const uint8_t& charZero,
    const uint32_t& hex,
    const uint32_t& width)
{
    //必要参数定义
    uint8_t temp[SER_INT64_STR_LEN_MAXA + 1]; //用于临时存放数字
    uint8_t* pTempWrite = temp + SER_INT64_STR_LEN_MAXA; //指向temp数组最后，为了填充不够的位数，需要反向填充
    size_t numLength = 0;//数字宽度
    static uint8_t hexChar[] = "0123456789abcdef"; //用于十六进制小写
    static uint8_t HEXChar[] = "0123456789ABCDEF"; //用于十六进制大写

    if(0 == hex)
    {
        //从末尾将数字依次从后往前插入到temp数组
        do
        {
            *--pTempWrite = (uint8_t)(uint64 % 10 + '0');
        }while(uint64 /= 10);
    }
    else if(1 == hex) //十六进制小写
    {
        do
        {
            *--pTempWrite = hexChar[(uint32_t)(uint64 & 0xf)]; //取出最后一个16进制数字
        }while(uint64 >>= 4); //循环取出每一个十六进制数字  
    }
    else if(2 == hex) //十六进制大写
    {
        do
        {
            *--pTempWrite = HEXChar[(uint32_t)(uint64 & 0xf)]; //取出最后一个16进制数字
        }while(uint64 >>= 4); 
    }

    numLength = temp + SER_INT64_STR_LEN_MAXA -  pTempWrite; //得到数字宽度

    //不够的位数用zero填充
    while(numLength++ < width && pWrite < pLast)
    {
        *pWrite++ = charZero;
    }

    numLength = temp + SER_INT64_STR_LEN_MAXA -  pTempWrite; //恢复数字长度，用于拷贝
    if((pWrite + numLength) > pLast)
    {
        numLength = pLast - pWrite; //如果数字的时候超过最大长度，剩多少位就拷贝多少位
    }

    SER_MEMCPY(pWrite, pTempWrite, numLength);
}

static void ser_vslprintf(
    uint8_t*& pWrite, 
    uint8_t*& pLast, 
    const char*& pfmt,
    const va_list& args)
{
    //必要参数声明
    uint32_t intWidth = 0; //整数部分长度
    uint8_t charZero = ' '; //用于填充不够长度的位数，'0'或者''
    bool isSign = true; //是否是有符号数
    uint64_t uint64 = 0; //保存无符号数参数
    int64_t int64 = 0; //保存有符号数参数
    uint32_t hex = 0; //是否十六进制显示，0:不是，1:以小写字母显示，2:以大写字母显示


    while(*pfmt && pWrite < pLast)
    {
        if (*pfmt == '%') //碰到需要转换的格式
        {
            charZero = (uint8_t)(*pfmt++ == '0' ? '0' : ' ');
            intWidth = 0;
            isSign = true;
            hex = 0;

            //读取%后面的数字，及整数部分长度,例如%016，就会取出16
            while(*pfmt >= '0' && *pfmt < '9')
            {
                intWidth = intWidth*10 + (*pfmt++ - '0');
            }

            switch (*pfmt)
            {
            case 'd':
                if(isSign)
                {
                    int64 = (int64_t)(va_arg(args, int32_t));
                }
                else
                {
                    uint64 = (uint64_t)(va_arg(args, uint32_t));
                }
                break;
            default: //不是定义的字符直接拷贝
                *pWrite++ = *pfmt++;
                continue;
            }

            //%d的会走到这里,把整数参数统一保存到uint64中去
            if(isSign)
            {
                if(int64 < 0)
                {
                    *pWrite++ = '-'; //填充'-'
                    uint64 = (uint64_t) -int64; //变成无符号数 
                }
                else
                {
                    uint64 = (uint64_t) int64;
                }              
            }

            ser_sprinf_number(pWrite, pLast, uint64, charZero, hex, intWidth);
            pfmt++;
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