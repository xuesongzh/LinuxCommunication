#include "ser_log.h"

#include <fcntl.h>  //open
#include <signal.h>
#include <stdarg.h>  //va_list
#include <stdio.h>
#include <string.h>
#include <sys/time.h>  //gettimeofday
#include <unistd.h>    //STDERR_FILENO...

#include "ser_configer.h"
#include "ser_macros.h"

//必要变量定义
ser_log_t ser_log;
static const char SerLogLevels[][20] = {
    {"stderr"},  //0：控制台错误
    {"emerg"},   //1：紧急
    {"alert"},   //2：警戒
    {"crit"},    //3：严重
    {"error"},   //4：错误
    {"warn"},    //5：警告
    {"notice"},  //6：注意
    {"info"},    //7：信息
    {"debug"}    //8：调试
};

//只用于本文件的函数声明
//打印错误码
static void ser_log_errno(
    uint8_t* pWrite,
    uint8_t* pLast,
    const int& err);
//实现可变参数的打印，定制输出格式
static void ser_vslprintf(
    uint8_t* pWrite,
    uint8_t* pLast,
    const char* pfmt,
    const va_list& args);
//打印数字
static void ser_sprinf_number(
    uint8_t* pWrite,
    uint8_t* pLast,
    uint64_t& uint64,
    const uint8_t& charZero,
    const uint32_t& hex,
    const uint32_t& width);

//对ser_vslprintf，以适应自己的数据结构
static void ser_log_format(
    uint8_t* pWrite,
    uint8_t* pLast,
    const char* pfmt,
    ...);

void ser_log_stderr(const int& errNum, const char* pfmt, ...) {
    va_list args;                                 //可变参数
    uint8_t errstr[SER_MAX_ERROR_STR + 1];        //日志信息
    uint8_t* pWrite = errstr;                     //指向可写的内存
    uint8_t* pLast = errstr + SER_MAX_ERROR_STR;  //指向最后一个字符，用于判断是否超过日志最大长度
    memset(errstr, 0, sizeof(errstr));

    SER_MEMCPY(pWrite, "server: ", strlen("server: "));

    //实现可变参数的打印，定制输出格式
    va_start(args, pfmt);  //使args指向可变参数起始参数
    ser_vslprintf(pWrite, pLast, pfmt, args);
    va_end(args);  //释放args

    //追加错误码对应的信息
    if (0 != errNum) {
        ser_log_errno(pWrite, pLast, errNum);
    }

    //如果最后位置不够，强制换行
    if (pWrite > pLast - 1) {
        pWrite = pLast - 1;  //pLast - 1是可写的最后一个有效字符
    }

    //如果是守护进程，需要往日志中写，如果不是通过终端启动进程，那么将在屏幕上看不到信息
    if (ser_log.mFd > STDERR_FILENO) {
        ser_log_error_core(SER_LOG_STDERR, errNum, (const char*)errstr);
    }

    *pWrite++ = '\n';  //插入换行符
    //往屏幕(STDERR_FILENO)输出错误信息
    write(STDERR_FILENO, errstr, pWrite - errstr);

    return;
}

void ser_log_error_core(const int& logLevel, const int& errNum, const char* pfmt, ...) {
    uint8_t errstr[SER_MAX_ERROR_STR + 1];
    memset(errstr, 0, sizeof(errstr));
    uint8_t* pWrite = errstr;
    uint8_t* pLast = errstr + SER_MAX_ERROR_STR;

    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));

    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));

    time_t sec;  //秒
    va_list args;
    //获取当前时间，返回自1970-01-01 00:00:00到现在经历的秒数
    gettimeofday(&tv, NULL);
    sec = tv.tv_sec;
    localtime_r(&sec, &tm);  //把参数1的time_t转换为本地时间保存到参数2中去,_r表示线程安全版本
    tm.tm_mon++;             //月份调整才正常
    tm.tm_year += 1900;      //年份调整才正常

    //组合出一个当前时间字符串，格式形如：2020/05/16 19:57:11
    uint8_t currentTime[30] = {0};
    uint8_t* pTimeWrite = currentTime;
    uint8_t* pTimeLast = currentTime + 30;
    ser_log_format(pTimeWrite, pTimeLast,
                   "%4d/%02d/%02d %02d:%02d:%02d",
                   tm.tm_year, tm.tm_mon, tm.tm_mday,
                   tm.tm_hour, tm.tm_min, tm.tm_sec);

    SER_MEMCPY(pWrite, currentTime, strlen((const char*)currentTime));
    ser_log_format(pWrite, pLast, " [%s] ", SerLogLevels[logLevel]);  //打印日志等级
    ser_log_format(pWrite, pLast, " pid:%p ", ser_pid);               //打印进程ID

    va_start(args, pfmt);
    ser_vslprintf(pWrite, pLast, pfmt, args);  //打印用户日志
    va_end(args);

    if (0 != errNum) {
        ser_log_errno(pWrite, pLast, errNum);  //打印错误码
    }

    if (pWrite > pLast - 1) {
        pWrite = pLast - 1;
    }
    *pWrite++ = '\n';

    ssize_t writeFlag;
    while (1) {
        if (logLevel > ser_log.mLogLevel) {
            break;  //大于配置文件中的日志等级不打印
        }

        writeFlag = write(ser_log.mFd, errstr, pWrite - errstr);
        if (-1 == writeFlag) {  //写入文件失败
            if (errno == ENOSPC) {
                //磁盘没空间了，暂不处理
            } else {  //其他错误
                if (STDERR_FILENO != ser_log.mFd) {
                    //直接显示到屏幕
                    writeFlag = write(STDERR_FILENO, errstr, pWrite - errstr);
                }
            }
            break;
        }

        break;
    }

    return;
}

void ser_log_init() {
    const char* pLogFilePath = nullptr;
    //读取配置文件中log文件的存放位置
    SerConfiger* pConfiger = SerConfiger::GetInstance();
    pLogFilePath = pConfiger->GetString("LogFilePath");
    if (nullptr == pLogFilePath) {
        //读不到就给默认路径
        pLogFilePath = (const char*)SER_LOG_DEFAULT_PATH;
    }
    //读取日志等级，读不到就给默认值SER_LOG_NOTICE
    ser_log.mLogLevel = pConfiger->GetIntDefault("LogLevel", SER_LOG_NOTICE);

    //O_WRONLY(只写打开)|O_APPEND(追加到末尾)|O_CREAT(文件不存在就创建)
    //0644:当前用户110(可读，可写，不可执行)，(所在组)100,(其他用户)100
    ser_log.mFd = open(pLogFilePath, O_WRONLY | O_APPEND | O_CREAT, 0644);

    if (-1 == ser_log.mFd) {  //如果有错误直接打印到屏幕
        SER_LOG_STDERR(errno, "can not open log file path:%s", pLogFilePath);
        ser_log.mFd = STDERR_FILENO;  //定位到标准错误
    }
}

static void ser_log_format(
    uint8_t* pWrite,
    uint8_t* pLast,
    const char* pfmt,
    ...) {
    va_list args;
    va_start(args, pfmt);
    ser_vslprintf(pWrite, pLast, pfmt, args);
    va_end(args);
}

static void ser_sprinf_number(
    uint8_t* pWrite,
    uint8_t* pLast,
    uint64_t& uint64,
    const uint8_t& charZero,
    const uint32_t& hex,
    const uint32_t& width) {
    //必要参数定义
    uint8_t temp[SER_INT64_STR_LEN_MAXA + 1];             //用于临时存放数字
    uint8_t* pTempWrite = temp + SER_INT64_STR_LEN_MAXA;  //指向temp数组最后，为了填充不够的位数，需要反向填充
    size_t numLength = 0;                                 //数字宽度
    static uint8_t hexChar[] = "0123456789abcdef";        //用于十六进制小写
    static uint8_t HEXChar[] = "0123456789ABCDEF";        //用于十六进制大写

    if (0 == hex) {  //从末尾将数字依次从后往前插入到temp数组
        do {
            *--pTempWrite = (uint8_t)(uint64 % 10 + '0');
        } while (uint64 /= 10);
    } else if (1 == hex) {  //十六进制小写
        do {
            *--pTempWrite = hexChar[(uint32_t)(uint64 & 0xf)];  //取出最后一个16进制数字
        } while (uint64 >>= 4);                                 //循环取出每一个十六进制数字
    } else if (2 == hex) {                                      //十六进制大写
        do {
            *--pTempWrite = HEXChar[(uint32_t)(uint64 & 0xf)];  //取出最后一个16进制数字
        } while (uint64 >>= 4);
    }

    numLength = temp + SER_INT64_STR_LEN_MAXA - pTempWrite;  //得到数字宽度

    //不够的位数用zero填充
    while (numLength++ < width && pWrite < pLast) {
        *pWrite++ = charZero;
    }

    numLength = temp + SER_INT64_STR_LEN_MAXA - pTempWrite;  //恢复数字长度，用于拷贝
    if ((pWrite + numLength) > pLast) {
        numLength = pLast - pWrite;  //如果数字的时候超过最大长度，剩多少位就拷贝多少位
    }

    SER_MEMCPY(pWrite, pTempWrite, numLength);
}

static void ser_vslprintf(uint8_t* pWrite, uint8_t* pLast, const char* pfmt, const va_list& args) {
    //必要参数声明
    uint32_t intWidth = 0;   //整数部分长度
    uint8_t charZero = ' ';  //用于填充不够长度的位数，'0'或者''
    bool isSign = true;      //是否是有符号数
    uint64_t uint64 = 0;     //保存无符号数参数
    int64_t int64 = 0;       //保存有符号数参数
    uint32_t hex = 0;        //是否十六进制显示，0:不是，1:以小写字母显示，2:以大写字母显示
    uint32_t fracWidth = 0;  //浮点数小数位长度

    while (*pfmt && pWrite < pLast) {
        if (*pfmt == '%') {  //碰到需要转换的格式
            charZero = (uint8_t)(*++pfmt == '0' ? '0' : ' ');
            intWidth = 0;
            isSign = true;
            hex = 0;

            //读取%后面的数字，及整数部分长度,例如%016，就会取出16
            while (*pfmt >= '0' && *pfmt < '9') {
                intWidth = intWidth * 10 + (*pfmt++ - '0');
            }
            //如果遇到u,x,X,.需要重置一些标志位去表示当前是无符号，十进制小写，十六进制大写，浮点数等显示格式
            while (true) {
                switch (*pfmt) {
                    case 'u':  //无符号数
                        isSign = false;
                        pfmt++;
                        continue;
                    case 'X':  //十六进制大写显示
                        hex = 2;
                        pfmt++;
                        continue;
                    case 'x':  //十六进制小写显示
                        hex = 1;
                        pfmt++;
                        continue;
                    case '.':  //浮点数显示
                        pfmt++;
                        while (*pfmt >= '0' && *pfmt <= '9') {
                            //取出小数位长度
                            fracWidth = fracWidth * 10 + (*pfmt++ - '0');
                        }
                        break;
                    default:
                        break;
                }
                break;
            }

            switch (*pfmt) {
                case '%':
                    *pWrite++ = *pfmt++;
                    continue;
                case 'd':  //整数显示
                    if (isSign) {
                        int64 = (int64_t)(va_arg(args, int32_t));
                    } else {
                        uint64 = (uint64_t)(va_arg(args, uint32_t));
                    }
                    break;
                case 'i':  //支持线程ID,条件变量等打印
                    if (isSign) {
                        int64 = (int64_t)va_arg(args, intptr_t);
                    } else {
                        uint64 = (uint64_t)va_arg(args, uintptr_t);
                    }
                    break;
                case 'L':  //转换int64_t数据
                    if (isSign) {
                        int64 = va_arg(args, int64_t);
                    } else {
                        uint64 = va_arg(args, uint64_t);
                    }
                    break;
                case 's':  //字符串打印
                {          //在switch case中如果使用局部变量，必须设置他的生命周期，加{}。否侧报错
                    const char* pTemp = va_arg(args, const char*);
                    while (*pTemp && pWrite < pLast) {
                        *pWrite++ = *pTemp++;
                    }
                    pfmt++;
                    continue;
                }
                case 'p':  //打印进程id
                    int64 = (int64_t)(va_arg(args, pid_t));
                    isSign = true;
                    break;
                case 'f':  //打印浮点数
                {
                    double floatValue = va_arg(args, double);
                    if (floatValue < 0) {
                        *pWrite++ = '-';           //显示负号
                        floatValue = -floatValue;  //将负数变为正数
                    }
                    uint64 = (uint32_t)floatValue;  //保存正整数部分

                    uint64_t fracValue = 0;  //要显示的小数部分，比如15.4保留3位，这里求出来的结果应该是400
                    if (fracWidth)           //如果小数有位数要求
                    {
                        uint32_t scale = 1;
                        uint32_t n = fracWidth;
                        while (n--) {
                            scale *= 10;  //这里可能溢出
                        }

                        fracValue = (uint64_t)((floatValue - (double)uint64) * scale + 0.5);  //有四舍五入
                        if (fracValue == scale)                                               //比如12.999这种直接进位
                        {
                            uint64++;
                            fracValue = 0;
                        }
                    }

                    //显示整数部分
                    ser_sprinf_number(pWrite, pLast, uint64, charZero, 0, intWidth);
                    if (fracWidth)  //指定了显示多少位小数
                    {
                        if (pWrite < pLast) {
                            *pWrite++ = '.';
                        }
                        ser_sprinf_number(pWrite, pLast, fracValue, '0', 0, fracWidth);
                    }
                    pfmt++;
                    continue;
                }
                default:  //不是定义的字符直接拷贝
                    *pWrite++ = *pfmt++;
                    continue;
            }

            //%d的会走到这里,把整数参数统一保存到uint64中去
            if (isSign) {
                if (int64 < 0) {
                    *pWrite++ = '-';            //填充'-'
                    uint64 = (uint64_t)-int64;  //变成无符号数
                } else {
                    uint64 = (uint64_t)int64;
                }
            }

            ser_sprinf_number(pWrite, pLast, uint64, charZero, hex, intWidth);
            pfmt++;
        } else {                  //纯字符串
            *pWrite++ = *pfmt++;  //*与++优先级相同，从右往左运算
        }
    }
}

static void ser_log_errno(uint8_t* pWrite, uint8_t* pLast, const int& err) {
    //错误码信息
    char* pErrorInfo = strerror(err);  //这个不会反回nullptr
    size_t errorLen = strlen(pErrorInfo);

    //定义一些格式信息
    char leftStr[10] = {0};
    sprintf(leftStr, "(%d: ", err);
    size_t leftLen = strlen(leftStr);

    char rightStr[] = ") ";
    size_t rightLen = strlen(rightStr);

    //向buffer写入信息：如果长度不够就不写，直接舍弃
    if ((pWrite + errorLen + leftLen + rightLen) < pLast) {
        SER_MEMCPY(pWrite, leftStr, leftLen);
        SER_MEMCPY(pWrite, pErrorInfo, errorLen);
        SER_MEMCPY(pWrite, rightStr, rightLen);
    }
}